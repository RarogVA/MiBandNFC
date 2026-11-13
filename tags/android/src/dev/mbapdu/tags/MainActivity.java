package dev.mbapdu.tags;

import android.Manifest;
import android.annotation.SuppressLint;
import android.app.Activity;
import android.app.AlertDialog;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothManager;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.graphics.Color;
import android.graphics.Insets;
import android.nfc.NdefMessage;
import android.nfc.NdefRecord;
import android.nfc.NfcAdapter;
import android.nfc.tech.IsoDep;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.View;
import android.view.WindowInsets;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;
import java.nio.charset.StandardCharsets;
import java.security.SecureRandom;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Locale;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.atomic.AtomicBoolean;

@SuppressLint("MissingPermission")
public final class MainActivity extends Activity {
    private final Handler main=new Handler(Looper.getMainLooper());
    private final ExecutorService worker=Executors.newSingleThreadExecutor();
    private final AtomicBoolean reading=new AtomicBoolean();
    private final CommandGate gate=new CommandGate();
    private final ReadbackProof readbackProof=new ReadbackProof();
    private final StringBuilder logBuffer=new StringBuilder();
    private NfcAdapter nfc;private BleTransport transport;private Catalog catalog;
    private TextView status,detail,log,readbackResult;private LinearLayout list;private EditText address;
    private Button scan,start,sync,stop,verify;
    private volatile Type4Ndef engine;private volatile IsoDep readbackLink;private byte[] sessionNdef;private volatile boolean foreground;
    private boolean ready,stopped,active,scanning,checking,closing,catalogWritable=true;
    private String awaitingReadbackCleanup;
    private long controlSequence=Integer.toUnsignedLong(new SecureRandom().nextInt()),pending=-1,scanEpoch;
    private String operation="",syncedId="";private Runnable completed;

    @Override public void onCreate(Bundle state) {
        super.onCreate(state);nfc=NfcAdapter.getDefaultAdapter(this);catalog=new Catalog(this);buildUi();
        try{catalog.load();}catch(Exception e){catalogWritable=false;status.setText("Каталог не прочитан; исходный файл сохранён. "+e.getMessage());}
        transport=new BleTransport(this,new BridgeTransport.Listener(){
            @Override public void state(boolean connected,String message){
                if(!connected){engine=null;readbackProof.reset();}
                main.post(()->{ready=connected;pending=-1;gate.reset();stopped=active=false;syncedId="";
                    if(!connected&&awaitingReadbackCleanup!=null){readbackResult.setText("FAIL · NFC-очистка не подтверждена");awaitingReadbackCleanup=null;}
                    status.setText(message);append(message);render();if(connected){closing=false;query();}});
            }
            @Override public void log(String message){append(message);}
            @Override public void received(Protocol.Frame frame){
                if(frame.kind==Protocol.STATUS){String value=new String(frame.payload,StandardCharsets.UTF_8);main.post(()->handleStatus(frame.sequence,value));return;}
                Type4Ndef handler=engine;
                byte[] response=foreground && handler!=null?handler.respond(frame.payload):Protocol.hex("6985");
                ReadbackProof.Receipt receipt=readbackProof.received(frame.payload,response);
                transport.send(new Protocol.Frame(Protocol.RESPONSE,frame.sequence,response),ok->{
                    readbackProof.written(receipt,ok);
                    append("APDU #"+frame.sequence+": "+frame.payload.length+" → "+response.length+" байт · "+(ok?"ATT подтверждён":"ошибка ATT"));
                });
            }
        });
        render();append("Tag Bridge 0.3 · refresh с сохранением GATT-соединения; аппаратная проверка версии ещё не завершена.");
    }
    private TextView text(String value,int size){TextView v=new TextView(this);v.setText(value);v.setTextSize(size);v.setTextColor(Color.rgb(27,39,52));v.setPadding(0,7,0,7);return v;}
    private Button button(LinearLayout parent,String title,View.OnClickListener action){Button b=new Button(this);b.setText(title);b.setAllCaps(false);b.setOnClickListener(action);parent.addView(b);return b;}
    private void buildUi(){
        LinearLayout root=new LinearLayout(this);root.setOrientation(LinearLayout.VERTICAL);root.setBackgroundColor(Color.rgb(246,248,252));
        int pad=(int)(16*getResources().getDisplayMetrics().density);root.setPadding(pad,pad,pad,pad);
        root.setOnApplyWindowInsetsListener((v,insets)->{Insets i=insets.getInsets(WindowInsets.Type.systemBars());root.setPadding(pad+i.left,pad+i.top,pad+i.right,pad+i.bottom);return insets;});
        root.addView(text("Метки · NFC",27));
        root.addView(text("Сохраните метку на Pixel и выберите её для браслета. Эмуляция переносит содержимое NDEF в Type 4; UID и права доступа карты не копируются.",14));
        status=text("Выберите или создайте метку",15);root.addView(status);
        scan=button(root,"Считать метку телефоном",v->beginScan());
        button(root,"Создать текст или ссылку",v->createDialog());
        root.addView(text("Сохранённые метки",20));list=new LinearLayout(this);list.setOrientation(LinearLayout.VERTICAL);root.addView(list);
        detail=text("",14);detail.setTextIsSelectable(true);root.addView(detail);
        button(root,"Переименовать выбранную",v->renameDialog());button(root,"Удалить выбранную",v->deleteSelected());
        root.addView(text("Браслет",20));
        root.addView(text("Нужен отдельный циферблат Tag Bridge. На браслете отображается выбранная запись; полный каталог хранится на Pixel.",14));
        button(root,"Подключить сопряжённый браслет",v->chooseBonded());
        address=new EditText(this);address.setSingleLine(true);address.setHint("BLE-адрес браслета, если его нет в списке");root.addView(address);
        button(root,"Подключить по BLE-адресу",v->connect(address.getText().toString().trim()));
        sync=button(root,"Показать выбранную метку на браслете",v->syncSelected());
        start=button(root,"Включить NFC-эмуляцию NDEF",v->startEmulation());
        verify=button(root,"Проверить браслет этим Pixel",v->beginReadback());
        readbackResult=text("Проверка NFC: ещё не запускалась",14);root.addView(readbackResult);
        stop=button(root,"Остановить NFC и отключиться",v->requestStop(true));
        root.addView(text("Во время эмуляции оставьте приложение и циферблат открытыми. Браслет прикладывают к другому телефону или считывателю. Сеанс ограничен двумя минутами.",14));
        log=text("",12);log.setTypeface(android.graphics.Typeface.MONOSPACE);log.setTextIsSelectable(true);root.addView(log);
        ScrollView page=new ScrollView(this);page.setFillViewport(true);page.addView(root);setContentView(page);
    }
    private boolean busy(){return active || pending>=0 || scanning;}
    private boolean editable(){if(!catalogWritable){status.setText("Каталог повреждён: изменение отключено, чтобы не перезаписать файл.");return false;}if(busy()){status.setText("Сначала завершите сканирование или остановите текущий NFC-сеанс.");return false;}return true;}
    private void render(){
        if(list==null)return;list.removeAllViews();TagRecord selected=catalog.selected();
        if(catalog.records.isEmpty())list.addView(text("Пока пусто. Считайте метку или создайте текст/ссылку.",14));
        for(TagRecord r:catalog.records){Button b=button(list,(r==selected?"● ":"○ ")+r.name+"\n"+(r.emulatable()?"NDEF · доступна эмуляция":"Сведения · без эмуляции"),v->{
            if(!editable())return;catalog.selectedId=r.id;syncedId="";save();render();if(ready&&stopped)syncSelected();});b.setEnabled(!busy());}
        if(selected==null)detail.setText("");else detail.setText(selected.name+"\nUID: "+(selected.uid.isEmpty()?"нет / создана на телефоне":selected.uid)+"\n"+selected.technology+"\n"+selected.details+"\n\n"+selected.preview());
        scan.setText(scanning?"Отменить считывание":"Считать метку телефоном");scan.setEnabled(scanning || (!active && pending<0));
        sync.setEnabled(catalogWritable&&ready&&stopped&&!busy()&&selected!=null);
        start.setEnabled(catalogWritable&&foreground&&ready&&stopped&&!busy()&&selected!=null&&selected.emulatable()&&selected.id.equals(syncedId));
        verify.setEnabled(foreground&&ready&&active&&!checking&&pending<0);
        stop.setEnabled(ready);getWindow().clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        if(active || scanning || pending>=0)getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
    }
    private boolean save(){try{catalog.save();return true;}catch(Exception e){catalogWritable=false;status.setText("Не удалось сохранить каталог: "+e.getMessage());append("Ошибка сохранения каталога");return false;}}
    private void add(TagRecord record){
        if(!catalogWritable)return;if(catalog.records.size()>=Catalog.MAX){status.setText("В каталоге уже 32 метки. Удалите ненужную запись.");return;}
        catalog.records.add(record);catalog.selectedId=record.id;syncedId="";
        if(save())status.setText(record.emulatable()?"NDEF сохранён. Можно передать метку на браслет.":"Сохранено описание карты. Эмуляция этой технологии пока не поддерживается.");
        render();if(catalogWritable&&ready&&stopped)syncSelected();
    }
    private void createDialog(){
        if(!editable())return;new AlertDialog.Builder(this).setTitle("Создать NDEF-метку").setItems(new String[]{"Текст","Ссылка"},(d,which)->{
            LinearLayout form=new LinearLayout(this);form.setOrientation(LinearLayout.VERTICAL);form.setPadding(24,8,24,8);
            EditText name=new EditText(this);name.setHint("Название метки");name.setSingleLine(true);form.addView(name);
            EditText value=new EditText(this);value.setHint(which==0?"Текст":"https://example.com");form.addView(value);
            AlertDialog dialog=new AlertDialog.Builder(this).setTitle(which==0?"Текстовая метка":"Метка со ссылкой").setView(form).setPositiveButton("Сохранить",null).setNegativeButton("Отмена",null).create();
            dialog.setOnShowListener(x->dialog.getButton(AlertDialog.BUTTON_POSITIVE).setOnClickListener(v->{
                if(!editable())return;String title=name.getText().toString().trim(),content=value.getText().toString().trim();
                if(title.isEmpty()||title.length()>80){name.setError("Введите название до 80 символов");return;}
                if(content.isEmpty()){value.setError("Введите содержимое");return;}
                try{NdefRecord record=which==0?NdefRecord.createTextRecord("ru",content):NdefRecord.createUri(content);TagRecord tag=TagRecord.create(title,new NdefMessage(new NdefRecord[]{record}));add(tag);dialog.dismiss();}
                catch(Exception e){value.setError(e.getMessage()==null?"Не удалось создать NDEF":e.getMessage());}
            }));dialog.show();
        }).show();
    }
    private void renameDialog(){
        if(!editable())return;TagRecord record=catalog.selected();if(record==null)return;EditText value=new EditText(this);value.setText(record.name);value.setSingleLine(true);
        AlertDialog dialog=new AlertDialog.Builder(this).setTitle("Название метки").setView(value).setPositiveButton("Сохранить",null).setNegativeButton("Отмена",null).create();
        dialog.setOnShowListener(x->dialog.getButton(AlertDialog.BUTTON_POSITIVE).setOnClickListener(v->{
            if(!editable())return;String name=value.getText().toString().trim();if(name.isEmpty()||name.length()>80){value.setError("От 1 до 80 символов");return;}
            record.name=name;syncedId="";save();render();dialog.dismiss();if(ready&&stopped)syncSelected();
        }));dialog.show();
    }
    private void deleteSelected(){
        if(!editable())return;TagRecord record=catalog.selected();if(record==null)return;
        new AlertDialog.Builder(this).setTitle("Удалить «"+record.name+"»?").setMessage("Удаляется только запись в этом приложении.")
            .setPositiveButton("Удалить",(d,w)->{if(!editable())return;catalog.records.remove(record);catalog.selectedId=catalog.records.isEmpty()?"":catalog.records.get(0).id;syncedId="";save();render();if(ready&&stopped){if(catalog.selected()!=null)syncSelected();else requestStop(true);}}).setNegativeButton("Отмена",null).show();
    }
    private void beginScan(){
        if(scanning){endScan();status.setText("Считывание отменено");return;}if(!editable())return;
        if(nfc==null || !nfc.isEnabled()){status.setText("Включите NFC в настройках Pixel");return;}
        if(catalog.records.size()>=Catalog.MAX){status.setText("Лимит — 32 метки");return;}
        scanning=true;final long epoch=++scanEpoch;status.setText("Приложите метку к NFC-антенне Pixel. Данные на карте не изменяются.");render();
        try{nfc.enableReaderMode(this,tag->{if(!reading.compareAndSet(false,true))return;worker.execute(()->{
            TagRecord result=null;String error=null;try{result=TagRecord.read(tag);}catch(Exception e){error=e.getClass().getSimpleName();}
            final TagRecord captured=result;final String failure=error;reading.set(false);
            main.post(()->{if(!foreground||!scanning||scanEpoch!=epoch)return;endScan();if(captured!=null)add(captured);else status.setText("Не удалось считать метку: "+failure);});
        });},NfcAdapter.FLAG_READER_NFC_A|NfcAdapter.FLAG_READER_NFC_B|NfcAdapter.FLAG_READER_NFC_F|NfcAdapter.FLAG_READER_NFC_V,null);}
        catch(RuntimeException e){endScan();status.setText("NFC: "+e.getMessage());}
    }
    private void endScan(){scanEpoch++;scanning=false;checking=false;IsoDep link=readbackLink;readbackLink=null;if(link!=null)try{link.close();}catch(Exception ignored){}if(nfc!=null)try{nfc.disableReaderMode(this);}catch(RuntimeException ignored){}render();}
    private boolean permissions(){return checkSelfPermission(Manifest.permission.BLUETOOTH_SCAN)==PackageManager.PERMISSION_GRANTED && checkSelfPermission(Manifest.permission.BLUETOOTH_CONNECT)==PackageManager.PERMISSION_GRANTED;}
    private BluetoothAdapter adapter(){
        if(!permissions()){requestPermissions(new String[]{Manifest.permission.BLUETOOTH_SCAN,Manifest.permission.BLUETOOTH_CONNECT},10);return null;}
        BluetoothManager manager=getSystemService(BluetoothManager.class);BluetoothAdapter result=manager==null?null:manager.getAdapter();
        if(result==null){status.setText("Bluetooth недоступен");return null;}if(!result.isEnabled()){startActivity(new Intent(BluetoothAdapter.ACTION_REQUEST_ENABLE));return null;}return result;
    }
    private void chooseBonded(){
        if(busy()){status.setText("Сначала остановите текущую операцию");return;}BluetoothAdapter a=adapter();if(a==null)return;
        ArrayList<String> labels=new ArrayList<>(),addresses=new ArrayList<>();
        for(BluetoothDevice d:a.getBondedDevices()){String name=d.getName();if(name==null)name="Bluetooth";labels.add(name+" · "+d.getAddress());addresses.add(d.getAddress());}
        if(labels.isEmpty()){status.setText("Нет сопряжённых устройств. Сопрягите браслет или укажите его BLE-адрес.");return;}
        new AlertDialog.Builder(this).setTitle("Выберите браслет с Tag Bridge").setItems(labels.toArray(new String[0]),(d,index)->connect(addresses.get(index))).setNegativeButton("Отмена",null).show();
    }
    private void connect(String value){if(!foreground||busy()){status.setText("Сначала остановите текущую операцию");return;}if(adapter()==null)return;String normalized=value.toUpperCase(Locale.ROOT);if(!BluetoothAdapter.checkBluetoothAddress(normalized)){address.setError("Нужен BLE-адрес вида AA:BB:CC:DD:EE:FF");return;}address.setText(normalized);transport.connect(normalized);}
    @Override public void onRequestPermissionsResult(int code,String[] names,int[] grants){super.onRequestPermissionsResult(code,names,grants);status.setText(permissions()?"Bluetooth разрешён. Нажмите подключение ещё раз.":"Доступ к Bluetooth не выдан");}
    private long nextSequence(){controlSequence=(controlSequence+1)&0xffffffffL;return controlSequence;}
    private void issue(int kind,byte[] payload,String expected,String name,int timeout,Runnable done){
        if(!ready){status.setText("Браслет не подключён");return;}
        final long seq=nextSequence();pending=seq;operation=name;completed=done;gate.begin(seq,expected);render();append(name+" #"+seq);
        transport.send(new Protocol.Frame(kind,seq,payload),ok->main.post(()->{if(pending!=seq)return;gate.write(seq,ok);checkCommand();}));
        main.postDelayed(()->{if(pending==seq)failCommand(name+": нет подтверждения за "+timeout/1000+" с");},timeout);
    }
    private void handleStatus(long seq,String value){
        append("STATUS #"+seq+" "+value);
        if(pending==seq){gate.status(seq,value);checkCommand();return;}
        if(value.startsWith("ERROR:") || (active && value.equals("STOPPED"))){if(checking){endScan();readbackResult.setText("FAIL · NFC-сеанс браслета завершился во время проверки");}engine=null;active=false;stopped=false;syncedId="";status.setText(value);render();if(ready&&pending<0)query();}
    }
    private void checkCommand(){
        if(gate.failed()){failCommand(operation+": команда отклонена");return;}
        if(!gate.complete())return;pending=-1;gate.reset();Runnable done=completed;completed=null;if(done!=null)done.run();render();
    }
    private void failCommand(String message){if(checking||scanning)endScan();readbackProof.reset();pending=-1;gate.reset();completed=null;engine=null;active=stopped=false;syncedId="";if(awaitingReadbackCleanup!=null){readbackResult.setText("FAIL · NFC-очистка не подтверждена");awaitingReadbackCleanup=null;}
        status.setText(message+". Соединение закрывается; очистка NFC не подтверждена.");append(message);transport.disconnect();render();}
    private void query(){if(!ready||pending>=0||active)return;stopped=false;syncedId="";issue(Protocol.CONTROL,new byte[]{0},"STOPPED","Проверка состояния",5000,()->{stopped=true;status.setText("Браслет подключён, NFC остановлен");if(awaitingReadbackCleanup!=null){readbackResult.setText(awaitingReadbackCleanup+"\nSTOPPED и повторный QUERY подтверждены");awaitingReadbackCleanup=null;}if(foreground&&catalog.selected()!=null)syncSelected();});}
    private void syncSelected(){
        TagRecord record=catalog.selected();if(!catalogWritable||!ready||!stopped||busy()||record==null)return;
        final String id=record.id;syncedId="";
        issue(Protocol.TAG,record.metadata(),"TAG_OK","Передача выбранной метки",5000,()->{syncedId=id;status.setText("Запись передана на браслет. Нажмите REFRESH на циферблате.");});
    }
    private void beginReadback(){
        if(!foreground||!ready||!active||pending>=0||checking||sessionNdef==null)return;
        if(nfc==null||!nfc.isEnabled()){status.setText("Включите NFC на Pixel для проверки");return;}
        checking=true;final long epoch=++scanEpoch;final long proofToken=readbackProof.begin();
        final byte[] expected=sessionNdef.clone();
        readbackResult.setText("Приложите браслет к NFC-антенне этого Pixel и удерживайте. Проверяется физический NFC → BLE → Pixel → BLE → NFC.");render();
        try{nfc.enableReaderMode(this,tag->{
            if(!reading.compareAndSet(false,true))return;
            worker.execute(()->{
                String failure=null,summary=null;IsoDep link=IsoDep.get(tag);
                try{
                    if(link==null)throw new IllegalStateException("Считанное устройство не поддерживает ISO-DEP");
                    readbackLink=link;link.connect();link.setTimeout(4500);
                    NdefReadback reader=new NdefReadback();byte[] actual=reader.read(link::transceive);
                    if(!Arrays.equals(actual,expected))throw new IllegalStateException("NDEF по NFC не совпал с выбранной меткой");
                    long deadline=android.os.SystemClock.elapsedRealtime()+700;
                    while(!readbackProof.matches(proofToken,reader.commands,reader.responses)&&android.os.SystemClock.elapsedRealtime()<deadline)Thread.sleep(10);
                    if(!readbackProof.matches(proofToken,reader.commands,reader.responses))throw new IllegalStateException("Нет полного независимого BLE/ATT подтверждения");
                    summary="PASS · "+actual.length+" байт NDEF совпали; "+reader.commands.size()+" APDU прошли NFC и BLE с ATT-подтверждением";
                }catch(Exception e){failure=e.getClass().getSimpleName()+": "+e.getMessage();}
                finally{if(link!=null)try{link.close();}catch(Exception ignored){}if(readbackLink==link)readbackLink=null;reading.set(false);}
                final String error=failure,success=summary;
                main.post(()->{if(!foreground||!checking||scanEpoch!=epoch)return;endScan();
                    if(error==null){awaitingReadbackCleanup=success;readbackResult.setText(success+"\nОжидается подтверждение очистки NFC");append(success);}
                    else{readbackResult.setText("FAIL · "+error);append("NFC READBACK FAIL · "+error);}
                    requestStop(false);
                });
            });
        },NfcAdapter.FLAG_READER_NFC_A|NfcAdapter.FLAG_READER_SKIP_NDEF_CHECK,null);}
        catch(RuntimeException e){endScan();readbackResult.setText("FAIL · ReaderMode: "+e.getMessage());requestStop(false);return;}
        main.postDelayed(()->{if(checking&&scanEpoch==epoch){endScan();readbackResult.setText("FAIL · за 30 секунд не завершено считывание браслета");requestStop(false);}},30000);
    }
    private void startEmulation(){
        TagRecord record=catalog.selected();if(!catalogWritable||!foreground||!ready||!stopped||busy()||record==null||!record.emulatable()||!record.id.equals(syncedId))return;
        // Snapshot prevents catalogue edits or later reads from changing a live NFC transaction.
        sessionNdef=record.ndef.clone();engine=new Type4Ndef(sessionNdef);stopped=false;readbackProof.reset();
        issue(Protocol.CONTROL,new byte[]{1},"READY","Запуск NFC",20000,()->{active=true;status.setText("NFC готов: «"+record.name+"». Приложите браслет к другому телефону / считывателю.");});
    }
    private void requestStop(boolean disconnect){
        if(scanning||checking)endScan();engine=null;readbackProof.reset();active=false;syncedId="";closing=disconnect;
        if(!ready){transport.disconnect();render();return;}
        pending=-1;gate.reset();completed=null;stopped=false;
        issue(Protocol.CONTROL,new byte[]{2},"STOPPED","Остановка NFC",20000,()->{stopped=true;status.setText("NFC остановлен · подтверждено браслетом");if(closing||!foreground)transport.disconnect();else query();});
    }
    private void append(String message){if(Looper.myLooper()!=Looper.getMainLooper()){main.post(()->append(message));return;}logBuffer.append(message).append('\n');if(logBuffer.length()>12000)logBuffer.delete(0,logBuffer.indexOf("\n",4000)+1);if(log!=null)log.setText(logBuffer.toString());}
    @Override protected void onResume(){super.onResume();foreground=true;render();}
    @Override protected void onPause(){foreground=false;endScan();if(transport!=null){if(ready)requestStop(true);else transport.disconnect();}super.onPause();}
    @Override protected void onDestroy(){if(transport!=null)transport.shutdown();worker.shutdownNow();super.onDestroy();}
}
