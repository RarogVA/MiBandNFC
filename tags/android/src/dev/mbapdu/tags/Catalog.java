package dev.mbapdu.tags;

import android.content.Context;
import android.util.AtomicFile;
import org.json.JSONArray;
import org.json.JSONObject;
import java.io.File;
import java.io.FileOutputStream;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.HashSet;

final class Catalog {
    static final int MAX=32;
    final ArrayList<TagRecord> records=new ArrayList<>();String selectedId="";long revision=1;
    private final AtomicFile file;
    Catalog(Context context){file=new AtomicFile(new File(context.getFilesDir(),"tag-catalog.json"));}
    void load() throws Exception {
        if(!file.getBaseFile().exists())return;
        byte[] bytes=file.readFully();if(bytes.length>150000)throw new IllegalArgumentException("Слишком большой каталог");
        JSONObject data=new JSONObject(new String(bytes,StandardCharsets.UTF_8));
        int version=data.getInt("version");
        if(version!=1 && version!=2)throw new IllegalArgumentException("Неизвестная версия каталога");
        long loadedRevision=version==1?1:data.getLong("revision");
        if(loadedRevision<1 || loadedRevision>0xffffffffL)throw new IllegalArgumentException("Неверная ревизия каталога");
        JSONArray items=data.getJSONArray("records");if(items.length()>MAX)throw new IllegalArgumentException("Лимит каталога");
        ArrayList<TagRecord> loaded=new ArrayList<>();HashSet<String> ids=new HashSet<>();
        for(int i=0;i<items.length();i++){
            TagRecord record=TagRecord.fromJson(items.getJSONObject(i));
            if(!ids.add(record.id))throw new IllegalArgumentException("Повторяющийся ID метки");loaded.add(record);
        }
        records.clear();records.addAll(loaded);revision=loadedRevision;selectedId=data.optString("selected","");
        if(selected()==null)selectedId="";
    }
    TagRecord selected(){for(TagRecord record:records)if(record.id.equals(selectedId))return record;return null;}
    void save() throws Exception {
        JSONObject out=new JSONObject();out.put("version",2);out.put("revision",revision);out.put("selected",selectedId);JSONArray items=new JSONArray();
        for(TagRecord record:records)items.put(record.json());out.put("records",items);
        FileOutputStream stream=null;
        try {stream=file.startWrite();stream.write(out.toString().getBytes(StandardCharsets.UTF_8));file.finishWrite(stream);}
        catch(Exception e){if(stream!=null)file.failWrite(stream);throw e;}
    }
    void changed(TagRecord modified) {
        if(revision==0xffffffffL || (modified!=null && modified.revision==0xffffffffL))throw new IllegalStateException("Исчерпана ревизия каталога");
        revision++;if(modified!=null)modified.revision++;
    }
    TagRecord find(String id){for(TagRecord record:records)if(record.id.equals(id))return record;return null;}
    CatalogWire.Snapshot snapshot(){ArrayList<CatalogWire.Entry> entries=new ArrayList<>();for(TagRecord record:records)entries.add(record.wire());return new CatalogWire.Snapshot(revision,entries);}
    void advanceAbove(long observed) {if(observed>=0xffffffffL)throw new IllegalStateException("Исчерпана ревизия каталога браслета");if(revision<=observed)revision=observed+1;}
}
