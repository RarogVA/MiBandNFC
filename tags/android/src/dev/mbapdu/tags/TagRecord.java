package dev.mbapdu.tags;

import android.nfc.NdefMessage;
import android.nfc.NdefRecord;
import android.nfc.Tag;
import android.nfc.tech.IsoDep;
import android.nfc.tech.Ndef;
import android.nfc.tech.NfcA;
import android.util.Base64;
import org.json.JSONException;
import org.json.JSONObject;
import java.nio.charset.StandardCharsets;
import java.util.UUID;

final class TagRecord {
    final String id,uid,technology,details;String name;final byte[] ndef;long revision=1;
    TagRecord(String id,String name,String uid,String technology,String details,byte[] ndef) {
        this.id=id;this.name=name;this.uid=uid;this.technology=technology;this.details=details;
        this.ndef=ndef==null?null:ndef.clone();
    }
    boolean emulatable(){return ndef!=null && ndef.length>0 && ndef.length<=Type4Ndef.MAX_NDEF;}
    byte[] metadata(){return Protocol.metadata(name,uid.length()<=20?uid:"",technology,emulatable());}
    CatalogWire.Entry wire(){return new CatalogWire.Entry(id,revision,CatalogWire.displayName(name),uid.length()<=20?uid:"",Protocol.ascii(technology,48),emulatable()?ndef:null);}
    static TagRecord create(String name,NdefMessage message) {
        byte[] bytes=message.toByteArray();
        if(bytes.length>Type4Ndef.MAX_NDEF)throw new IllegalArgumentException("Лимит NDEF — 1024 байта");
        return new TagRecord(UUID.randomUUID().toString(),name,"","NDEF", "Создана на телефоне; эмуляция содержимого Type 4",bytes);
    }
    static TagRecord read(Tag tag) {
        StringBuilder tech=new StringBuilder(),detail=new StringBuilder();
        for(String t:tag.getTechList()){String shortName=t.substring(t.lastIndexOf('.')+1);if(tech.length()>0)tech.append(" / ");tech.append(shortName);}
        NfcA a=NfcA.get(tag);if(a!=null)detail.append("ATQA ").append(Protocol.hex(a.getAtqa())).append(" · SAK ").append(String.format(java.util.Locale.ROOT,"%02X",a.getSak()&255)).append('\n');
        IsoDep iso=IsoDep.get(tag);if(iso!=null){detail.append("ISO-DEP");byte[] historical=iso.getHistoricalBytes(),higher=iso.getHiLayerResponse();
            if(historical!=null)detail.append(" · historical ").append(Protocol.hex(historical));
            if(higher!=null)detail.append(" · higher-layer ").append(Protocol.hex(higher));detail.append('\n');}
        byte[] bytes=null;Ndef n=Ndef.get(tag);
        if(n!=null) {
            try {
                n.connect();NdefMessage message=n.getNdefMessage();
                if(message!=null){byte[] payload=message.toByteArray();if(payload.length>0 && payload.length<=Type4Ndef.MAX_NDEF){bytes=payload;detail.append("NDEF: ").append(bytes.length).append(" байт");}
                    else detail.append("NDEF ").append(payload.length).append(" байт: превышен лимит 1024; сохранено описание");}
                else detail.append("Нет сообщения NDEF; сохранено описание");
            } catch(Exception e){detail.append("NDEF не прочитан: ").append(e.getClass().getSimpleName());}
            finally {try{n.close();}catch(Exception ignored){}}
        } else detail.append("Нет доступного NDEF. UID и технология не являются копией карты.");
        String uid=Protocol.hex(tag.getId());
        return new TagRecord(UUID.randomUUID().toString(),"Метка "+(uid.isEmpty()?"без UID":uid.substring(Math.max(0,uid.length()-8))),uid,tech.toString(),detail.toString(),bytes);
    }
    String preview() {
        if(!emulatable())return "Эмуляция не поддерживается: сохранены только доступные сведения.";
        try {
            NdefMessage m=new NdefMessage(ndef);StringBuilder out=new StringBuilder();
            for(NdefRecord r:m.getRecords()) {
                if(out.length()>0)out.append('\n');
                if(r.toUri()!=null)out.append(r.toUri().toString());
                else if(r.getTnf()==NdefRecord.TNF_WELL_KNOWN && java.util.Arrays.equals(r.getType(),NdefRecord.RTD_TEXT)) {
                    byte[] p=r.getPayload();int prefix=p.length==0?1:1+(p[0]&63);
                    if(prefix<=p.length)out.append(new String(p,prefix,p.length-prefix,(p[0]&128)==0?StandardCharsets.UTF_8:StandardCharsets.UTF_16));
                    else out.append("NDEF Text");
                } else out.append("NDEF запись · ").append(r.getPayload().length).append(" байт");
                if(out.length()>350){out.setLength(350);out.append('…');break;}
            }
            return out.toString();
        }catch(Exception e){return "NDEF · "+ndef.length+" байт";}
    }
    JSONObject json() throws JSONException {
        JSONObject o=new JSONObject();o.put("id",id);o.put("revision",revision);o.put("name",name);o.put("uid",uid);o.put("technology",technology);o.put("details",details);
        if(ndef!=null)o.put("ndef",Base64.encodeToString(ndef,Base64.NO_WRAP));return o;
    }
    static TagRecord fromJson(JSONObject o) throws Exception {
        String name=o.getString("name"),uid=o.getString("uid"),id=o.getString("id");
        if(name.isEmpty() || name.length()>80 || !UUID.fromString(id).toString().equals(id) || uid.length()>64 || (uid.length()&1)!=0 || !uid.matches("[0-9A-F]*"))throw new IllegalArgumentException("Invalid catalogue record");
        byte[] bytes=o.has("ndef")?Base64.decode(o.getString("ndef"),Base64.NO_WRAP):null;
        if(bytes!=null){if(bytes.length==0 || bytes.length>Type4Ndef.MAX_NDEF)throw new IllegalArgumentException("NDEF size");new NdefMessage(bytes);}
        long revision=o.optLong("revision",1);
        if(revision<1 || revision>0xffffffffL)throw new IllegalArgumentException("Invalid record revision");
        TagRecord result=new TagRecord(id,name,uid,o.getString("technology"),o.getString("details"),bytes);
        result.revision=revision;return result;
    }
}
