#!/usr/bin/env python3
"""Build UI4's owned Pult surfaces, bounded spinner frames and native preview.

No external images/fonts are downloaded. Material arrow/reload sources retain
icons/LICENSE. Other geometry is original. Dynamic text uses existing native
MiSans-Demibold styles (26/20 px); the preview approximates them with Arial Bold.
"""
from pathlib import Path
import hashlib, json, math, re, struct
from PIL import Image, ImageChops, ImageDraw, ImageFont
ROOT = Path(__file__).resolve().parents[2]
HERE = Path(__file__).resolve().parent
VALUES = {name: int(value.rstrip('u'), 0) for name, value in re.findall(
    r'^#define (\w+) (0x[0-9a-f]+u|\d+)$', (HERE/'catalog_layout.h').read_text(), re.M)}
SCALE = 4
FONT = '/System/Library/Fonts/Supplemental/Arial Bold.ttf'
ASSET_NAMES = ('card-v4', 'controls-v4', 'status-ok-v4', 'status-fail-v4',
               'status-busy0-v4', 'status-busy1-v4', 'status-busy2-v4', 'status-busy3-v4',
               'left-v4', 'right-v4', 'left-off-v4', 'right-off-v4')

def rgb(name):
    c = VALUES[name]
    return ((c >> 16) & 255, (c >> 8) & 255, c & 255, 255)

def rounded_mask(size, radii, inset=0):
    w,h=size; points=[]
    for (cx,cy),radius,start in zip(
        [(inset+radii[0],inset+radii[0]),(w-1-inset-radii[1],inset+radii[1]),
         (w-1-inset-radii[2],h-1-inset-radii[2]),(inset+radii[3],h-1-inset-radii[3])],
        radii,[180,270,0,90]):
        for k in range(65):
            a=math.radians(start+k*90/64)
            points.append(((cx+radius*math.cos(a))*SCALE,(cy+radius*math.sin(a))*SCALE))
    mask=Image.new('L',(w*SCALE,h*SCALE));ImageDraw.Draw(mask).polygon(points,fill=255)
    return mask

def painted(outer, inner, fill, stroke):
    im=Image.new('RGBA',outer.size,stroke);im.putalpha(outer)
    paint=Image.new('RGBA',outer.size,fill);paint.putalpha(inner);im.alpha_composite(paint)
    return im

def label(im, value, box, size, color, anchor='mm'):
    x,y,w,h=box
    ImageDraw.Draw(im).text(((x+w/2)*SCALE,(y+h/2)*SCALE), value,
        font=ImageFont.truetype(FONT,size*SCALE),fill=color,anchor=anchor)

def encode(name, im):
    im=im.resize((im.width//SCALE,im.height//SCALE),Image.Resampling.LANCZOS)
    # Raster clipping uses the same conservative physical-pixel envelope as
    # validation; subpixel antialiasing must not leak beyond the 5px inset.
    v=VALUES
    position=(v['CARD_X'],v['CARD_Y']) if name=='card-v4' else (v['CONTROLS_X'],v['CONTROLS_Y']) if name=='controls-v4' else (v['STATUS_X'],v['STATUS_Y']) if name.startswith('status-') else None
    if position:
        safe=Image.new('L',(336,480));ImageDraw.Draw(safe).rounded_rectangle((5,5,330,474),radius=51,fill=255)
        x,y=position;im.putalpha(ImageChops.darker(im.getchannel('A'),safe.crop((x,y,x+im.width,y+im.height))))
    im=im.quantize(colors=256,method=Image.Quantize.FASTOCTREE).convert('RGBA')
    palette={};indices=bytearray()
    for c in im.getdata():
        if c not in palette:palette[c]=len(palette)
        indices.append(palette[c])
    assert len(palette)<=256
    colors=bytearray()
    for r,g,b,a in palette:colors.extend((b,g,r,a))
    colors.extend(bytes(1024-len(colors)))
    w,h=im.size
    data=struct.pack('<BBHHHHH',0x19,0x0a,0,w,h,w,0)+colors+indices
    (HERE/'icons'/f'{name}.bin').write_bytes(data);im.save(HERE/'icons'/f'{name}.png')
    # Independent I8 readback of every pixel, dimensions and palette.
    assert struct.unpack_from('<BBHHHHH',data)==(0x19,0x0a,0,w,h,w,0)
    assert len(data)==1036+w*h
    decoded=bytearray()
    for index in data[1036:]:
        b,g,r,a=data[12+index*4:16+index*4];decoded.extend((r,g,b,a))
    assert bytes(decoded)==im.tobytes()
    return im,dict(name=name+'.bin',bytes=len(data),sha256=hashlib.sha256(data).hexdigest(),size=[w,h])

def main():
    v=VALUES;assets={};manifest=[]
    def save(name,im):
        assets[name],record=encode(name,im);manifest.append(record)
    size=(v['CARD_W'],v['CARD_H']);r=v['CARD_RADIUS']
    card=painted(rounded_mask(size,[r]*4),rounded_mask(size,[r-1]*4,1),rgb('CARD_COLOR'),rgb('CARD_BORDER_COLOR'))
    d=ImageDraw.Draw(card)
    d.rounded_rectangle((0,97*SCALE,3*SCALE,157*SCALE),radius=1.5*SCALE,fill='#d3ba67')
    d.rounded_rectangle((v['TYPE_X']*SCALE,v['TYPE_Y']*SCALE,(v['TYPE_X']+v['TYPE_W'])*SCALE,(v['TYPE_Y']+v['TYPE_H'])*SCALE),radius=5*SCALE,fill=rgb('TYPE_COLOR'))
    # Original contactless wave illustration, distinct from controls.
    for radius in [9,15,21]:
        cx,cy=271,38
        d.arc(((cx-radius)*SCALE,(cy-radius)*SCALE,(cx+radius)*SCALE,(cy+radius)*SCALE),-42,42,fill='#c5b278',width=3*SCALE)
    save('card-v4',card)
    size=(v['CONTROLS_W'],v['CONTROLS_H'])
    outer=rounded_mask(size,[20,20,51,51]);inner=rounded_mask(size,[19,19,50,50],1)
    panel=painted(outer,inner,rgb('BUTTON_COLOR'),rgb('CONTROL_BORDER_COLOR'))
    bottom=Image.new('RGBA',panel.size,rgb('REFRESH_COLOR'));mask=inner.copy()
    ImageDraw.Draw(mask).rectangle((0,0,mask.width,v['NAV_H']*SCALE-1),fill=0)
    bottom.putalpha(mask);panel.alpha_composite(bottom);d=ImageDraw.Draw(panel)
    d.line((163*SCALE,0,163*SCALE,72*SCALE),fill=rgb('CONTROL_BORDER_COLOR'),width=SCALE)
    d.line((SCALE,72*SCALE,325*SCALE,72*SCALE),fill=rgb('CONTROL_BORDER_COLOR'),width=SCALE)
    label(panel,'Обновить',(0,72,326,67),v['REFRESH_FONT'],'#f4f5f7');save('controls-v4',panel)
    # Crop concentric R51 upper corners, then round the lower six pixels.
    w,h=v['STATUS_W'],v['STATUS_H']
    outer=rounded_mask((w,104),[51]*4).crop((0,0,w*SCALE,h*SCALE))
    inner=rounded_mask((w,104),[50]*4,1).crop((0,0,w*SCALE,h*SCALE))
    outer=ImageChops.darker(outer,rounded_mask((w,h),[0,0,6,6]))
    inner=ImageChops.darker(inner,rounded_mask((w,h),[0,0,5,5],1))
    refresh=Image.open(HERE/'icons/refresh.png').convert('RGBA').resize((24*SCALE,24*SCALE),Image.Resampling.LANCZOS)
    states=[('ok','#183e2c','#315b42','#95e6b9','OK'),('fail','#4b222a','#6a3941','#ffb0bc','FAIL')]
    states += [(f'busy{i}','#2b2e33','#42464e','#d1d4d9','Проверка') for i in range(4)]
    for state,fill,stroke,fg,value in states:
        im=painted(outer,inner,fill,stroke)
        label_width=w if state=='ok' else w-36
        text='Статус сервиса: '+value
        assert ImageFont.truetype(FONT,v['STATUS_FONT']*SCALE).getlength(text)<(label_width-20)*SCALE
        label(im,text,(0,0,label_width,h),v['STATUS_FONT'],fg)
        if state!='ok':
            icon=Image.new('RGBA',refresh.size,fg);icon.putalpha(refresh.getchannel('A'))
            if state.startswith('busy'):icon=icon.rotate(-int(state[-1])*90)
            im.alpha_composite(icon,((w-18-24)*SCALE,((h-24)*SCALE)//2))
        save('status-'+state+'-v4',im)
    for direction in ['left','right']:
        source=Image.open(HERE/'icons'/f'{direction}.png').convert('RGBA').resize((32*SCALE,32*SCALE),Image.Resampling.LANCZOS)
        for suffix,color in [('', '#d6d9df'),('-off','#52555b')]:
            im=Image.new('RGBA',source.size,color);im.putalpha(source.getchannel('A'));save(direction+suffix+'-v4',im)
    assert set(assets)==set(ASSET_NAMES)
    (HERE/'icons/ui4-manifest.json').write_text(json.dumps({'profile':'catalog-ui-v4','assets':manifest},indent=2)+'\n')
    lines=['/* Generated by build_card.py. No unverified firmware APIs. */', '#ifndef CATALOG_ASSETS_H', '#define CATALOG_ASSETS_H',
           'static const char *const ui4_assets[]={']
    lines += ['    "/data/tag_catalog/'+n+'.bin",' for n in ASSET_NAMES]
    lines += ['};','#endif','']
    (HERE/'catalog_assets.h').write_text('\n'.join(lines))
    # Inspect actual generated pixels, including all six status states.
    safe=Image.new('L',(336,480));ImageDraw.Draw(safe).rounded_rectangle((5,5,330,474),radius=51,fill=255)
    for state,_,_,_,_ in states:
        occupied=Image.new('L',(336,480))
        for name,x,y in [('card-v4',v['CARD_X'],v['CARD_Y']),('controls-v4',5,336),('status-'+state+'-v4',5,5)]:
            occupied.paste(assets[name].getchannel('A').point(lambda a:255 if a>=32 else 0),(x,y))
        assert not ImageChops.subtract(occupied,safe).getbbox(), 'Surface outside rounded display inset'
    assert v['CARD_Y']-(v['STATUS_Y']+v['STATUS_H'])==33
    assert v['CARD_Y']+v['CARD_H']+33==v['CONTROLS_Y']
    assert v['CONTROLS_Y']+v['CONTROLS_H']==475
    out=ROOT/'build/catalog-ui4';out.mkdir(parents=True,exist_ok=True)
    for state in ['ok','fail','busy0']:
        preview=Image.new('RGBA',(336*SCALE,480*SCALE),'#000000')
        for name,x,y in [('card-v4',v['CARD_X'],v['CARD_Y']),('controls-v4',5,336),('status-'+state+'-v4',5,5),('left-v4',70,356),('right-v4',233,356)]:
            preview.alpha_composite(assets[name].resize((assets[name].width*SCALE,assets[name].height*SCALE)),(x*SCALE,y*SCALE))
        # Left aligned, bounded native title. All other static labels use
        # exactly the pixels embedded in the FACE, not recreated text.
        ImageDraw.Draw(preview).text(((v['CARD_X']+v['TITLE_X'])*SCALE,(v['CARD_Y']+v['TITLE_Y']+8)*SCALE),'NFC-Test',font=ImageFont.truetype(FONT,v['TITLE_FONT']*SCALE),fill='#f0f3f8')
        label(preview,'NDEF',(v['CARD_X']+v['TYPE_X'],v['CARD_Y']+v['TYPE_Y'],v['TYPE_W'],v['TYPE_H']),v['TYPE_FONT'],'#f0f3f8')
        preview=preview.resize((672,960),Image.Resampling.LANCZOS)
        preview.save(out/('screen-'+state+'-2x.png'))
        if state=='ok':preview.save(out/'screen-2x.png');preview.resize((336,480)).save(out/'screen.png')
    report={'profile':'catalog-ui-v4','display':[336,480],'status':[5,5,326,49],
            'card':[5,87,326,216],'controls':[5,336,326,139],'navigation_height':72,'refresh_height':67,
            'side_margin':5,'status_to_card_gap':33,'card_to_controls_gap':33,
            'fonts':{'title':v['TITLE_FONT'],'type':v['TYPE_FONT'],'status':v['STATUS_FONT'],'refresh':v['REFRESH_FONT']},'rounded_safe_area':'PASS',
            'asset_roundtrip':'PASS','asset_count':len(manifest),'asset_bytes':sum(a['bytes'] for a in manifest),
            'preview':'offline render with actual assets; dynamic system text approximate, not a device screenshot',
            'hardware_tested':False}
    (out/'preview-validation.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
if __name__=='__main__':main()
