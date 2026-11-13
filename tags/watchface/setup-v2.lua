local lvgl=require("lvgl")
local function read(path,n)
 local f=io.open(path,"r");if not f then return nil end
 local s=f:read(n or 256);f:close();return s
end
local function prop(key,path)
 os.execute("getprop "..key.." > "..path)
 return (read(path) or ""):match("^%s*(.-)%s*$")
end
local model=prop("product.model","/tmp/tag_model")
local version=prop("ro.build.version","/tmp/tag_version"):gsub("_",".")
local supported=model=="miwear.watch.p67glt" and version=="3.201.026"
local root=lvgl.Object(nil,{w=336,h=480,bg_color=0x07111F,bg_opa=lvgl.OPA(100)})
root:clear_flag(lvgl.FLAG.SCROLLABLE)
local function label(text,size,y,h)
 return lvgl.Label(root,{text=text,width=304,height=h,text_color=0xFFFFFF,
 text_font=lvgl.Font("MiSans-Demibold",size,"normal"),
 align={type=lvgl.ALIGN.CENTER,x_ofs=0,y_ofs=y}})
end
label("NFC Tags Setup v2",24,-192,40)
local info=label(supported and "Установка экрана меток\nдля Roundabout RU NFC.\n\nПосле установки выберите\nновый циферблат." or
 ("Unsupported device\n"..model.."\n"..version),18,-55,190)
label("Модули работают до перезагрузки.\nNFC включается с Pixel.",15,200,50)
local function mark(path,value)
 local f=io.open(path,"w");if not f then return false end
 local ok=f:write(value.."\n");local closed=f:close();return ok and closed
end
local function load(resource,target,name)
 local attempted="/tmp/"..name.."_attempted"
 if read(attempted) then return false,"Restart the band before retrying" end
 local src=io.open(SCRIPT_PATH..resource,"rb")
 local dst=src and io.open(target,"wb")
 if not src or not dst then
  if src then src:close() end
  return false,"Cannot open module"
 end
 local ok=true
 while true do
  local data=src:read(4096);if not data then break end
  if not dst:write(data) then ok=false;break end
 end
 src:close();if not dst:close() then ok=false end
 if not ok then return false,"Copy failed" end
 -- Keep a pre-load latch even when insmod or the final marker write fails.
 if not mark(attempted,"1") then return false,"Cannot save load guard" end
 local a,_,code=os.execute("insmod "..target.." "..name)
 return a==true or a==0 or code==0,"Load failed; restart band"
end
local busy=false
local failed=false
local function button(text,y,cb)
 local b=lvgl.Object(root,{w=300,h=50,bg_color=0x14508A,bg_opa=lvgl.OPA(100),radius=16,
 align={type=lvgl.ALIGN.CENTER,x_ofs=0,y_ofs=y}})
 b:add_flag(lvgl.FLAG.CLICKABLE)
 lvgl.Label(b,{text=text,text_color=0xFFFFFF,text_font=lvgl.Font("MiSans-Demibold",20,"normal"),align=lvgl.ALIGN.CENTER})
 b:onClicked(cb)
end
button("УСТАНОВИТЬ",85,function()
 if busy or failed or not supported then return end
 local resident=read("/tmp/nfc_tags_ui_loaded")
 if resident and resident~="ui2\n" then
  info:set{text="Старая версия экрана.\nПерезагрузите браслет."};return
 end
 if read("/tmp/apdu_lab_loaded") and not read("/tmp/tag_catalog_loaded") then
  info:set{text="Другой модуль уже загружен.\nПерезагрузите браслет."};return
 end
 busy=true
 os.execute("mkdir -p /data/tag_catalog")
 local ok,msg=true,""
 if not read("/tmp/tag_catalog_loaded") then
  ok,msg=load("apdu_lab_probe_3.201.026.elf","/data/tag_catalog/probe.elf","Tag_Catalog")
  if ok then
   ok=mark("/tmp/tag_catalog_loaded","tag_catalog") and mark("/tmp/apdu_lab_loaded","tag_catalog")
   if not ok then msg="Marker failed; restart band" end
  end
 end
 if ok and not read("/tmp/nfc_tags_ui_loaded") then
  ok,msg=load("launcher.elf","/data/tag_catalog/launcher.elf","NFC_Tags_UI")
  if ok then
   ok=mark("/tmp/nfc_tags_ui_loaded","ui2")
   if not ok then msg="Marker failed; restart band" end
  end
 end
 info:set{text=ok and "Модули загружены.\nЧерез 2 секунды нажмите\nСТАТУС для проверки." or msg}
 busy=false
 failed=not ok
end)
button("СТАТУС",145,function()
 info:set{text=read("/tmp/nfc_tags_ui_state",384) or "Нет состояния UI.\nНажмите УСТАНОВИТЬ."}
end)
