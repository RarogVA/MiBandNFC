package dev.mbapdu.tags;

import android.app.Instrumentation;
import android.content.ContextWrapper;
import android.nfc.NdefMessage;
import android.nfc.NdefRecord;
import android.os.Bundle;
import java.io.File;
import java.nio.charset.StandardCharsets;
import org.json.JSONObject;

/** Runs against real Android JSON, AtomicFile and the production catalog. */
public final class CatalogDeviceTest extends Instrumentation {
    private int checks;
    private void check(boolean ok,String message){checks++;if(!ok)throw new AssertionError(message);}
    @Override public void onCreate(Bundle args){super.onCreate(args);start();}
    @Override public void onStart(){
        Bundle result=new Bundle();File directory=new File(getTargetContext().getCacheDir(),"catalog-checkbox-test-"+System.nanoTime());
        try{
            if(!directory.mkdirs())throw new IllegalStateException("Cannot create isolated test directory");
            ContextWrapper context=new ContextWrapper(getTargetContext()){@Override public File getFilesDir(){return directory;}};
            TagRecord first=TagRecord.create("Included",new NdefMessage(new NdefRecord[]{NdefRecord.createTextRecord("en","visible-payload")}));
            TagRecord second=TagRecord.create("Unchecked-private",new NdefMessage(new NdefRecord[]{NdefRecord.createTextRecord("en","excluded-secret-payload")}));
            TagRecord third=new TagRecord("00112233-4455-6677-8899-aabbccddeeff","Metadata","01020304","NfcA","Test",null);
            check(!first.syncToBand&&!second.syncToBand,"new tags require a checkbox");first.syncToBand=true;third.syncToBand=true;
            check(TagRecord.fromJson(first.json()).syncToBand,"checked JSON roundtrip");
            check(!TagRecord.fromJson(second.json()).syncToBand,"unchecked JSON roundtrip");
            JSONObject legacy=second.json();legacy.remove("syncToBand");check(TagRecord.fromJson(legacy).syncToBand,"legacy records preserve inclusion");
            Catalog catalog=new Catalog(context);catalog.records.add(first);catalog.records.add(second);catalog.records.add(third);catalog.selectedId=second.id;
            CatalogWire.Snapshot subset=catalog.snapshot();
            check(subset.count()==2&&catalog.syncCount()==2,"subset count");
            check(subset.find(first.id)!=null&&subset.find(third.id)!=null&&subset.find(second.id)==null,"only checked IDs in outgoing snapshot");
            String bytes=new String(subset.bytes(),StandardCharsets.ISO_8859_1);
            check(!bytes.contains("Unchecked-private")&&!bytes.contains("excluded-secret-payload"),"unchecked name and NDEF excluded from wire bytes");
            check(subset.find(first.id).supported()&&!subset.find(third.id).supported(),"metadata-only inclusion remains metadata-only");
            catalog.save();Catalog loaded=new Catalog(context);loaded.load();
            check(loaded.records.size()==3&&loaded.syncCount()==2,"all local records survive save/load");
            check(loaded.selectedId.equals(second.id)&&!loaded.selected().syncToBand,"active choice independent of checkbox");
            check(loaded.snapshot().hashHex().equals(subset.hashHex()),"persistent subset matches transmitted bytes");
            SelectionGate gate=new SelectionGate();gate.local(subset);gate.observed(CatalogWire.Summary.parse("CATALOG:1:2:"+subset.hashHex()));
            boolean rejected=false;try{gate.receive(1,second.wire().selection(1).payload(CatalogWire.SELECT_REQUEST));}catch(IllegalArgumentException expected){rejected=true;}
            check(rejected&&gate.authorized(second.id)==null,"unchecked tag cannot authorize NFC");
            long recordRevision=loaded.records.get(0).revision;loaded.changed(null);loaded.records.get(0).syncToBand=false;loaded.records.get(2).syncToBand=false;
            CatalogWire.Snapshot empty=loaded.snapshot();gate.local(empty);
            check(empty.count()==0&&empty.length()==12,"uncheck all emits a valid empty catalog");
            check(loaded.records.size()==3&&loaded.records.get(0).revision==recordRevision&&loaded.revision==2,"checkbox changes catalog revision without deleting or editing records");
            check(!gate.catalogMatches()&&gate.authorized(first.id)==null,"changed checkbox invalidates old authorization");
            loaded.save();Catalog restart=new Catalog(context);restart.load();
            check(restart.syncCount()==0&&restart.records.size()==3,"unchecked flags persist across restart");
            result.putString("CATALOG_TESTS","PASS "+checks);finish(-1,result);
        }catch(Throwable error){result.putString("CATALOG_TESTS","FAIL "+error);finish(1,result);}
        finally{File[] files=directory.listFiles();if(files!=null)for(File f:files)f.delete();directory.delete();}
    }
}
