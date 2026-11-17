package dev.mbapdu.tags;

import android.Manifest;
import android.annotation.SuppressLint;
import android.app.Activity;
import android.app.Dialog;
import android.app.AlertDialog;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothManager;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.nfc.NdefMessage;
import android.nfc.NdefRecord;
import android.nfc.NfcAdapter;
import android.nfc.tech.IsoDep;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.CheckBox;
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
    private final SelectionGate selectionGate=new SelectionGate();
    private final StringBuilder logBuffer=new StringBuilder();
    private NfcAdapter nfc;private BleTransport transport;private Catalog catalog;
    private TextView status,log,readbackResult,connectionTitle,connectionSubtitle,count,debugStatus,connectionStatus;
    private LinearLayout list;private EditText address;
    private Button scan,start,sync,stop,verify,debugStart,debugQuery,bleDisconnect;
    private Dialog connectionDialog,debugDialog;
    private String rawStatus="",lastAddress="";
    private volatile Type4Ndef engine;private volatile IsoDep readbackLink;private byte[] sessionNdef;private volatile boolean foreground;
    private boolean ready,stopped,active,scanning,checking,closing,catalogWritable=true,capabilities,transferring,choosing;
    private CatalogWire.Snapshot localSnapshot;
    private CatalogWire.Summary bandSummary;
    private Protocol.Frame waitingSelection;
    private int pendingKind,pendingOpcode;
    private String lastCommandReply="",startAfterSelection="";
    private String awaitingReadbackCleanup;
    private long controlSequence=0x50000000L|(new SecureRandom().nextInt()&0x0fffffffL),pending=-1,scanEpoch,commandEpoch,selectionAttempt;
    private String operation="";private Runnable completed;

    @Override public void onCreate(Bundle state) {
        super.onCreate(state);nfc=NfcAdapter.getDefaultAdapter(this);catalog=new Catalog(this);buildUi();
        try{catalog.load();}catch(Exception e){catalogWritable=false;setStatus("Каталог не прочитан; исходный файл сохранён. "+e.getMessage());}
        if(catalogWritable)updateSnapshot();
        transport=new BleTransport(this,new BridgeTransport.Listener(){
            @Override public void state(boolean connected,String message){
                if(!connected){engine=null;readbackProof.reset();}
                main.post(()->{ready=connected;pending=-1;commandEpoch++;selectionAttempt++;gate.reset();stopped=active=false;capabilities=transferring=choosing=false;
                    selectionGate.disconnected();bandSummary=null;waitingSelection=null;startAfterSelection="";completed=null;sessionNdef=null;
                    if(!connected&&awaitingReadbackCleanup!=null){readbackResult.setText("FAIL · NFC-очистка не подтверждена");awaitingReadbackCleanup=null;}
                    setStatus(message);append(message);render();if(connected){closing=false;query();}});
            }
            @Override public void log(String message){append(message);}
            @Override public void received(Protocol.Frame frame){
                if(frame.kind==Protocol.STATUS){String value=new String(frame.payload,StandardCharsets.UTF_8);main.post(()->handleStatus(frame.sequence,value));return;}
                if(frame.kind==Protocol.SELECTION){engine=null;main.post(()->handleSelection(frame));return;}
                if(frame.kind!=Protocol.REQUEST)return;
                Type4Ndef handler=engine;
                byte[] response=foreground && handler!=null?handler.respond(frame.payload):Protocol.hex("6985");
                ReadbackProof.Receipt receipt=readbackProof.received(frame.payload,response);
                transport.send(new Protocol.Frame(Protocol.RESPONSE,frame.sequence,response),ok->{
                    readbackProof.written(receipt,ok);
                    append("APDU #"+frame.sequence+": "+frame.payload.length+" → "+response.length+" байт · "+(ok?"ATT подтверждён":"ошибка ATT"));
                });
            }
        });
        render();append("Tag Bridge 0.4.1 · постоянный каталог и подтверждение выбранной метки. Нужен модуль CATALOG_V1; версия ещё проходит аппаратную проверку.");
    }
    private TextView text(String value,int size){return AppStyle.text(this,value,size,AppStyle.TEXT);}
    private Button button(LinearLayout parent,String title,View.OnClickListener action){return AppStyle.addButton(parent,title,false,action);}
    private int dp(float n){return AppStyle.dp(this,n);}
    private void buildUi(){
        AppStyle.window(getWindow());lastAddress=getPreferences(MODE_PRIVATE).getString("band_address","");
        LinearLayout root=AppStyle.column(this);root.setBackgroundColor(AppStyle.BG);AppStyle.insets(root);
        AppStyle.gap(root,8);TextView title=text("NFC-метки",32);AppStyle.medium(title);root.addView(title);AppStyle.gap(root,24);
        LinearLayout connection=AppStyle.row(this);connection.setPadding(dp(18),dp(16),dp(16),dp(16));connection.setBackground(AppStyle.shape(AppStyle.SURFACE,20,this));
        LinearLayout labels=AppStyle.column(this);connectionTitle=text("Браслет",17);AppStyle.medium(connectionTitle);labels.addView(connectionTitle);AppStyle.gap(labels,6);
        connectionSubtitle=AppStyle.text(this,"Не подключён",13,AppStyle.MUTED);labels.addView(connectionSubtitle);connection.addView(labels,new LinearLayout.LayoutParams(0,-2,1));
        Button connectButton=AppStyle.button(this,"BLE",false,v->connectionWindow());connectButton.setContentDescription("Подключение по BLE");connection.addView(connectButton,new LinearLayout.LayoutParams(dp(76),dp(48)));root.addView(connection);AppStyle.gap(root,28);
        LinearLayout heading=AppStyle.row(this);TextView label=text("Мои метки",20);AppStyle.medium(label);heading.addView(label,new LinearLayout.LayoutParams(0,-2,1));count=AppStyle.text(this,"0",14,AppStyle.MUTED);heading.addView(count);root.addView(heading);AppStyle.gap(root,14);
        list=AppStyle.column(this);root.addView(list);
        scan=AppStyle.addButton(root,"Добавить метку",false,v->{if(scanning)beginScan();else addDialog();});AppStyle.gap(root,24);
        sync=AppStyle.addButton(root,"Синхронизировать с браслетом",false,v->syncChecked());AppStyle.gap(root,10);
        start=AppStyle.addButton(root,"Включить NFC",true,v->{if(active)requestStop(false);else startEmulation();});
        status=AppStyle.text(this,"",14,AppStyle.MUTED);status.setPadding(dp(4),dp(14),dp(4),0);status.setVisibility(View.GONE);root.addView(status);
        AppStyle.gap(root,24);AppStyle.addButton(root,"Mi Home (тест)",false,v->startActivity(new Intent(this,MiHomeActivity.class)));AppStyle.gap(root,10);
        Button debug=AppStyle.addButton(root,"Запустить Debug",false,v->debugWindow());debug.setTextColor(AppStyle.MUTED);
        readbackResult=text("Тест NFC ещё не запускался",14);log=text("",12);log.setTypeface(android.graphics.Typeface.MONOSPACE);log.setTextIsSelectable(true);
        ScrollView page=new ScrollView(this);page.setBackgroundColor(AppStyle.BG);page.setFillViewport(true);page.addView(root);AppStyle.insets(page,root);setContentView(page);page.requestApplyInsets();
    }
    private void addDialog(){if(!editable())return;new AlertDialog.Builder(this).setTitle("Добавить метку").setItems(new String[]{"Считать NFC-метку","Создать текст или ссылку"},(d,i)->{if(i==0)beginScan();else createDialog();}).setNegativeButton("Отмена",null).show();}
    private void recordMenu(TagRecord record){new AlertDialog.Builder(this).setTitle(record.name).setItems(new String[]{"Сведения","Переименовать","Удалить"},(d,i)->{if(i==0){recordDetails(record);return;}if(!editable())return;if(i==1)renameDialog(record);else deleteRecord(record);}).show();}
    private void recordDetails(TagRecord record){TextView body=text((record.uid.isEmpty()?"":"UID: "+record.uid+"\n\n")+record.technology+"\n\n"+record.details+(record.emulatable()?"\n\n"+record.preview():""),15);body.setTextIsSelectable(true);body.setPadding(dp(20),dp(8),dp(20),dp(20));ScrollView content=new ScrollView(this);content.addView(body);new AlertDialog.Builder(this).setTitle(record.name).setView(content).setPositiveButton("Закрыть",null).show();}
    private void connectionWindow(){
        if(connectionDialog!=null&&connectionDialog.isShowing())return;
        LinearLayout body=AppStyle.column(this);connectionStatus=AppStyle.text(this,ready?"Браслет подключён":"Выберите браслет",14,AppStyle.MUTED);body.addView(connectionStatus);AppStyle.gap(body,16);
        button(body,"Сопряжённые устройства",v->chooseBonded());AppStyle.gap(body,28);body.addView(AppStyle.text(this,"Подключить по BLE-адресу",16,AppStyle.TEXT));AppStyle.gap(body,12);
        address=AppStyle.input(this,"AA:BB:CC:DD:EE:FF");address.setSingleLine(true);address.setInputType(android.text.InputType.TYPE_CLASS_TEXT|android.text.InputType.TYPE_TEXT_FLAG_CAP_CHARACTERS|android.text.InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS);address.setText(lastAddress);body.addView(address);AppStyle.gap(body,12);
        AppStyle.addButton(body,"Подключить",true,v->connect(address.getText().toString().trim()));AppStyle.gap(body,24);
        bleDisconnect=button(body,"Отключить браслет",v->{requestStop(true);connectionDialog.dismiss();});AppStyle.enable(bleDisconnect,ready);
        LinearLayout wrapper=AppStyle.column(this);ScrollView scroll=new ScrollView(this);scroll.addView(body);wrapper.addView(scroll,new LinearLayout.LayoutParams(-1,-1));
        connectionDialog=AppStyle.dialog(this,"Подключение",wrapper);connectionDialog.setOnDismissListener(d->{lastAddress=address.getText().toString();bleDisconnect=null;connectionStatus=null;connectionDialog=null;});connectionDialog.show();
    }
    private static void detach(View v){if(v.getParent() instanceof ViewGroup)((ViewGroup)v.getParent()).removeView(v);}
    private void debugWindow(){
        if(debugDialog!=null&&debugDialog.isShowing())return;
        LinearLayout body=AppStyle.column(this);debugStatus=AppStyle.text(this,rawStatus,14,AppStyle.MUTED);body.addView(debugStatus);AppStyle.gap(body,16);
        debugQuery=button(body,"Проверить связь",v->query());AppStyle.gap(body,8);
        debugStart=button(body,"Включить NFC",v->startEmulation());AppStyle.gap(body,8);
        verify=button(body,"Проверить NFC этим Pixel",v->beginReadback());AppStyle.gap(body,8);
        stop=button(body,"Остановить NFC",v->requestStop(false));AppStyle.gap(body,20);
        detach(readbackResult);body.addView(readbackResult);AppStyle.gap(body,20);
        TextView logHeading=text("Журнал",18);AppStyle.medium(logHeading);body.addView(logHeading);AppStyle.gap(body,12);detach(log);log.setText(logBuffer.toString());body.addView(log);
        ScrollView scroller=new ScrollView(this);scroller.setFillViewport(true);scroller.addView(body);LinearLayout wrapper=AppStyle.column(this);wrapper.addView(scroller,new LinearLayout.LayoutParams(-1,-1));
        debugDialog=AppStyle.dialog(this,"Debug",wrapper);debugDialog.setOnDismissListener(d->{detach(log);detach(readbackResult);debugStatus=null;verify=stop=debugQuery=debugStart=null;debugDialog=null;if(checking){endScan();requestStop(false);}});debugDialog.show();render();
    }
    private void setStatus(String message){
        rawStatus=message;if(debugStatus!=null)debugStatus.setText(message);
        String visible=message;
        if(message.startsWith("BT:"))visible=ready?"Браслет подключён":message.contains("остановлен")?"Браслет отключён":"Подключение к браслету…";
        if(message.startsWith("ERROR:")||message.contains("очистка NFC не подтверждена"))visible="Ошибка связи. Подробности — в Debug.";
        status.setText(visible);status.setVisibility(visible.isEmpty()?View.GONE:View.VISIBLE);if(connectionStatus!=null)connectionStatus.setText(visible);
    }
    private boolean busy(){return active || pending>=0 || scanning || transferring || choosing;}
    private boolean editable(){if(!catalogWritable){setStatus("Каталог повреждён: изменение отключено, чтобы не перезаписать файл.");return false;}if(busy()){setStatus("Сначала завершите сканирование или остановите текущий NFC-сеанс.");return false;}return true;}
    private void render(){
        if(list==null)return;list.removeAllViews();TagRecord selected=catalog.selected();count.setText("На браслет: "+catalog.syncCount()+" / "+catalog.records.size());
        connectionTitle.setText(ready?"Браслет подключён":"Браслет");connectionSubtitle.setText(active?"NFC активен":ready?"Bluetooth · подключён":"Не подключён");connectionSubtitle.setTextColor(ready?AppStyle.GREEN:AppStyle.MUTED);
        if(catalog.records.isEmpty()){TextView empty=AppStyle.text(this,"Добавьте первую метку",16,AppStyle.MUTED);empty.setPadding(dp(4),dp(24),dp(4),dp(32));list.addView(empty);}
        for(TagRecord r:catalog.records){
            boolean chosen=r==selected;LinearLayout card=AppStyle.row(this);card.setGravity(android.view.Gravity.TOP);card.setMinimumHeight(dp(148));card.setPadding(dp(18),dp(18),dp(14),dp(18));android.graphics.drawable.GradientDrawable background=AppStyle.shape(AppStyle.SURFACE,20,this);if(chosen)background.setStroke(dp(1),0xff6789b1);card.setBackground(background);
            LinearLayout content=AppStyle.column(this);content.setPadding(0,dp(6),dp(12),0);card.addView(content,new LinearLayout.LayoutParams(0,-2,1));
            TextView name=text(r.name,19);AppStyle.medium(name);name.setMaxLines(2);name.setEllipsize(android.text.TextUtils.TruncateAt.END);content.addView(name);AppStyle.gap(content,16);
            TextView badge=AppStyle.text(this,r.emulatable()?"NDEF":"Только сведения",12,r.emulatable()?AppStyle.BLUE:AppStyle.MUTED);badge.setPadding(dp(9),dp(4),dp(9),dp(4));badge.setBackground(AppStyle.shape(r.emulatable()?AppStyle.BLUE_BG:AppStyle.RAISED,7,this));content.addView(badge,new LinearLayout.LayoutParams(-2,-2));
            if(chosen&&r.emulatable()){AppStyle.gap(content,14);TextView preview=AppStyle.text(this,r.preview(),14,AppStyle.MUTED);preview.setMaxLines(2);preview.setEllipsize(android.text.TextUtils.TruncateAt.END);content.addView(preview);}
            LinearLayout controls=AppStyle.column(this);card.addView(controls,new LinearLayout.LayoutParams(dp(48),-2));
            Button menu=AppStyle.button(this,"⋮",false,v->recordMenu(r));menu.setTextSize(25);menu.setMinHeight(0);menu.setMinimumHeight(0);menu.setPadding(0,0,0,0);menu.setContentDescription("Действия: "+r.name);menu.setBackground(new android.graphics.drawable.RippleDrawable(android.content.res.ColorStateList.valueOf(0x338eadd1),AppStyle.shape(AppStyle.RAISED,12,this),null));controls.addView(menu,new LinearLayout.LayoutParams(dp(48),dp(48)));AppStyle.gap(controls,12);
            CheckBox include=AppStyle.checkBox(this);include.setChecked(r.syncToBand);include.setContentDescription("Передавать на браслет: "+r.name);controls.addView(include,new LinearLayout.LayoutParams(dp(48),dp(48)));include.setOnCheckedChangeListener((v,checked)->setSyncIncluded(r,checked));
            card.setContentDescription(r.name+(chosen?", выбрана":""));card.setOnClickListener(v->selectOnPhone(r));
            boolean selectable=catalogWritable&&!scanning&&pending<0&&!transferring&&!choosing;AppStyle.enable(card,selectable);AppStyle.enable(menu,!busy());AppStyle.enable(include,catalogWritable&&!busy());
            list.addView(card,new LinearLayout.LayoutParams(-1,-2));AppStyle.gap(list,10);
        }
        scan.setText(scanning?"Отменить считывание":"Добавить метку");AppStyle.enable(scan,scanning||!busy());
        AppStyle.enable(sync,catalogWritable&&ready&&capabilities&&!scanning&&pending<0&&!transferring&&!choosing);
        boolean canStart=catalogWritable&&foreground&&ready&&capabilities&&stopped&&!busy()&&selected!=null&&selected.syncToBand&&selected.emulatable()&&selectionGate.catalogMatches();
        start.setText(active?"Остановить NFC":"Включить NFC");AppStyle.enable(start,(active&&pending<0)||canStart);AppStyle.enable(debugStart,canStart);
        AppStyle.enable(verify,foreground&&ready&&active&&!checking&&pending<0);AppStyle.enable(debugQuery,ready&&!active&&pending<0&&!transferring&&!choosing);AppStyle.enable(stop,ready);AppStyle.enable(bleDisconnect,ready);
        getWindow().clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        if(active||scanning||pending>=0||transferring||choosing)getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
    }
    private boolean save(){try{catalog.save();return true;}catch(Exception e){catalogWritable=false;setStatus("Не удалось сохранить каталог: "+e.getMessage());append("Ошибка сохранения каталога");return false;}}
    private void setSyncIncluded(TagRecord record,boolean included){
        if(!editable()){render();return;}if(record.syncToBand==included)return;
        long previousRevision=catalog.revision;boolean previous=record.syncToBand;
        try{catalog.changed(null);record.syncToBand=included;catalog.save();}
        catch(Exception e){record.syncToBand=previous;catalog.revision=previousRevision;catalogWritable=false;setStatus("Не удалось сохранить отметку");append("Сохранение отметки: "+e.getMessage());render();return;}
        waitingSelection=null;startAfterSelection="";selectionAttempt++;updateSnapshot();render();
    }
    private void syncChecked(){
        if(!catalogWritable||!ready||!capabilities||scanning||pending>=0||transferring||choosing)return;
        if(catalog.syncCount()==0&&bandSummary!=null&&bandSummary.count>0){
            new AlertDialog.Builder(this).setTitle("Убрать метки с браслета?").setMessage("Ни одна метка не отмечена. На Pixel все записи останутся.")
                .setPositiveButton("Убрать",(d,w)->synchronizeCatalog()).setNegativeButton("Отмена",null).show();
        }else synchronizeCatalog();
    }
    private void add(TagRecord record){
        if(!catalogWritable)return;if(catalog.records.size()>=Catalog.MAX){setStatus("В каталоге уже 32 метки. Удалите ненужную запись.");return;}
        catalog.records.add(record);catalog.selectedId=record.id;catalog.changed(null);updateSnapshot();
        if(save())setStatus(record.emulatable()?"Метка сохранена":"Сохранены сведения. Эмуляция этой карты недоступна.");
        render();
    }
    private void createDialog(){
        if(!editable())return;new AlertDialog.Builder(this).setTitle("Создать NDEF-метку").setItems(new String[]{"Текст","Ссылка"},(d,which)->{
            LinearLayout form=new LinearLayout(this);form.setOrientation(LinearLayout.VERTICAL);form.setPadding(dp(20),dp(8),dp(20),dp(8));
            EditText name=AppStyle.input(this,"Название метки");name.setSingleLine(true);form.addView(name);
            AppStyle.gap(form,12);EditText value=AppStyle.input(this,which==0?"Текст":"https://example.com");form.addView(value);
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
    private void renameDialog(TagRecord record){
        if(!editable())return;EditText value=AppStyle.input(this,"Название метки");value.setText(record.name);value.setSingleLine(true);
        AlertDialog dialog=new AlertDialog.Builder(this).setTitle("Название метки").setView(value).setPositiveButton("Сохранить",null).setNegativeButton("Отмена",null).create();
        dialog.setOnShowListener(x->dialog.getButton(AlertDialog.BUTTON_POSITIVE).setOnClickListener(v->{
            if(!editable())return;String name=value.getText().toString().trim();if(name.isEmpty()||name.length()>80){value.setError("От 1 до 80 символов");return;}
            if(!record.name.equals(name)){record.name=name;catalog.changed(record);updateSnapshot();save();}render();dialog.dismiss();setStatus("Название сохранено. Синхронизируйте метки.");
        }));dialog.show();
    }
    private void deleteRecord(TagRecord record){
        if(!editable())return;
        new AlertDialog.Builder(this).setTitle("Удалить «"+record.name+"»?").setMessage("Удаляется только запись в этом приложении.")
            .setPositiveButton("Удалить",(d,w)->{if(!editable())return;catalog.records.remove(record);if(catalog.selectedId.equals(record.id))catalog.selectedId="";catalog.changed(null);updateSnapshot();save();render();setStatus("Метка удалена. Синхронизируйте изменения.");}).setNegativeButton("Отмена",null).show();
    }
    private void beginScan(){
        if(scanning){endScan();setStatus("Считывание отменено");return;}if(!editable())return;
        if(nfc==null || !nfc.isEnabled()){setStatus("Включите NFC в настройках Pixel");return;}
        if(catalog.records.size()>=Catalog.MAX){setStatus("Лимит — 32 метки");return;}
        scanning=true;final long epoch=++scanEpoch;setStatus("Приложите метку к задней стороне Pixel");render();
        try{nfc.enableReaderMode(this,tag->{if(!reading.compareAndSet(false,true))return;worker.execute(()->{
            TagRecord result=null;String error=null;try{result=TagRecord.read(tag);}catch(Exception e){error=e.getClass().getSimpleName();}
            final TagRecord captured=result;final String failure=error;reading.set(false);
            main.post(()->{if(!foreground||!scanning||scanEpoch!=epoch)return;endScan();if(captured!=null)add(captured);else setStatus("Не удалось считать метку: "+failure);});
        });},NfcAdapter.FLAG_READER_NFC_A|NfcAdapter.FLAG_READER_NFC_B|NfcAdapter.FLAG_READER_NFC_F|NfcAdapter.FLAG_READER_NFC_V,null);}
        catch(RuntimeException e){endScan();setStatus("NFC: "+e.getMessage());}
    }
    private void endScan(){scanEpoch++;scanning=false;checking=false;IsoDep link=readbackLink;readbackLink=null;if(link!=null)try{link.close();}catch(Exception ignored){}if(nfc!=null)try{nfc.disableReaderMode(this);}catch(RuntimeException ignored){}render();}
    private boolean permissions(){return checkSelfPermission(Manifest.permission.BLUETOOTH_SCAN)==PackageManager.PERMISSION_GRANTED && checkSelfPermission(Manifest.permission.BLUETOOTH_CONNECT)==PackageManager.PERMISSION_GRANTED;}
    private BluetoothAdapter adapter(){
        if(!permissions()){requestPermissions(new String[]{Manifest.permission.BLUETOOTH_SCAN,Manifest.permission.BLUETOOTH_CONNECT},10);return null;}
        BluetoothManager manager=getSystemService(BluetoothManager.class);BluetoothAdapter result=manager==null?null:manager.getAdapter();
        if(result==null){setStatus("Bluetooth недоступен");return null;}if(!result.isEnabled()){startActivity(new Intent(BluetoothAdapter.ACTION_REQUEST_ENABLE));return null;}return result;
    }
    private void chooseBonded(){
        if(busy()){setStatus("Сначала остановите текущую операцию");return;}BluetoothAdapter a=adapter();if(a==null)return;
        ArrayList<String> labels=new ArrayList<>(),addresses=new ArrayList<>();
        for(BluetoothDevice d:a.getBondedDevices()){String name=d.getName();if(name==null)name="Bluetooth";labels.add(name+" · "+d.getAddress());addresses.add(d.getAddress());}
        if(labels.isEmpty()){setStatus("Нет сопряжённых устройств. Сопрягите браслет или укажите его BLE-адрес.");return;}
        new AlertDialog.Builder(this).setTitle("Выберите браслет").setItems(labels.toArray(new String[0]),(d,index)->connect(addresses.get(index))).setNegativeButton("Отмена",null).show();
    }
    private void connect(String value){
        if(!foreground||busy()){setStatus("Сначала остановите текущую операцию");return;}
        if(adapter()==null)return;String normalized=value.trim().toUpperCase(Locale.ROOT);
        if(!BluetoothAdapter.checkBluetoothAddress(normalized)){address.setError("Введите BLE-адрес вида AA:BB:CC:DD:EE:FF");return;}
        address.setText(normalized);lastAddress=normalized;getPreferences(MODE_PRIVATE).edit().putString("band_address",normalized).apply();
        if(connectionDialog!=null)connectionDialog.dismiss();transport.connect(normalized);
    }
    @Override public void onRequestPermissionsResult(int code,String[] names,int[] grants){super.onRequestPermissionsResult(code,names,grants);setStatus(permissions()?"Bluetooth разрешён. Нажмите подключение ещё раз.":"Доступ к Bluetooth не выдан");}
    private long nextSequence(){controlSequence=0x50000000L|((controlSequence+1)&0x0fffffffL);return controlSequence;}
    private void issue(int kind,byte[] payload,String expected,String name,int timeout,Runnable done){
        issue(nextSequence(),kind,payload,new String[]{expected},false,name,timeout,done);
    }
    private void issue(long seq,int kind,byte[] payload,String[] expected,boolean prefix,String name,int timeout,Runnable done){
        if(!ready){setStatus("Браслет не подключён");return;}
        final long epoch=++commandEpoch;pending=seq;pendingKind=kind;pendingOpcode=payload[0]&255;operation=name;completed=done;gate.begin(seq,expected,prefix);render();append(name+" #"+seq);
        transport.send(new Protocol.Frame(kind,seq,payload),ok->main.post(()->{if(pending!=seq||commandEpoch!=epoch)return;gate.write(seq,ok);checkCommand();}));
        main.postDelayed(()->{if(pending==seq&&commandEpoch==epoch)failCommand(name+": нет подтверждения за "+timeout/1000+" с");},timeout);
    }
    private void handleStatus(long seq,String value){
        append("STATUS #"+seq+" "+value);
        if(pending==seq){gate.status(seq,value);checkCommand();return;}
        if(value.startsWith("ERROR:") || (active && value.equals("STOPPED"))){if(checking){endScan();readbackResult.setText("FAIL · NFC-сеанс браслета завершился: "+value);}engine=null;active=false;stopped=false;selectionGate.invalidate();startAfterSelection="";setStatus(value);render();if(ready&&pending<0)query();}
    }
    private void checkCommand(){
        if(gate.failed()){failCommand(operation+": команда отклонена"+(pendingKind==Protocol.CONTROL&&pendingOpcode==4?". Установите модуль каталога CATALOG_V1":""));return;}
        if(!gate.complete())return;lastCommandReply=gate.result();pending=-1;gate.reset();Runnable done=completed;completed=null;if(done!=null)done.run();drainSelection();render();
    }
    private void failCommand(String message){if(checking||scanning)endScan();readbackProof.reset();pending=-1;commandEpoch++;selectionAttempt++;gate.reset();completed=null;engine=null;ready=capabilities=active=stopped=transferring=choosing=false;selectionGate.disconnected();waitingSelection=null;startAfterSelection="";if(awaitingReadbackCleanup!=null){readbackResult.setText("FAIL · NFC-очистка не подтверждена");awaitingReadbackCleanup=null;}
        setStatus(message+". Соединение закрывается; очистка NFC не подтверждена.");append(message);transport.disconnect();render();}
    private void query(){
        if(!ready||pending>=0||active)return;stopped=false;selectionGate.invalidate();
        issue(Protocol.CONTROL,new byte[]{0},"STOPPED","Проверка состояния",5000,()->{
            stopped=true;
            if(awaitingReadbackCleanup!=null){readbackResult.setText(awaitingReadbackCleanup+"\nSTOPPED и повторный QUERY подтверждены");awaitingReadbackCleanup=null;}
            if(!foreground)return;
            if(capabilities)queryCatalog();
            else issue(Protocol.CONTROL,new byte[]{4},"CATALOG_V1","Проверка версии модуля",5000,()->{capabilities=true;queryCatalog();});
        });
    }
    private void updateSnapshot(){
        try{localSnapshot=catalog.snapshot();selectionGate.local(localSnapshot);}
        catch(RuntimeException e){catalogWritable=false;localSnapshot=null;selectionGate.local(null);setStatus("Каталог нельзя передать: "+e.getMessage());}
    }
    private void queryCatalog(){
        if(!ready||!capabilities||pending>=0)return;
        issue(nextSequence(),Protocol.CATALOG,new byte[]{CatalogWire.QUERY},new String[]{"CATALOG:"},true,"Чтение состояния каталога",5000,()->{
            try{bandSummary=CatalogWire.Summary.parse(lastCommandReply);selectionGate.observed(bandSummary);}
            catch(RuntimeException e){failCommand(e.getMessage());return;}
            setStatus(selectionGate.catalogMatches()?"Метки синхронизированы":
                "На браслете другой каталог. Нажмите «Синхронизировать с браслетом».");
        });
    }
    private void synchronizeCatalog(){
        if(!catalogWritable||!foreground||!ready||!capabilities||pending>=0||scanning||transferring||choosing)return;
        if(active||!stopped){requestStop(false,this::synchronizeCatalog);return;}
        if(bandSummary==null){queryCatalog();return;}
        if(selectionGate.catalogMatches()){setStatus("Метки уже синхронизированы");drainSelection();return;}
        try{catalog.advanceAbove(bandSummary.revision);if(!save())return;updateSnapshot();if(!catalogWritable)return;}
        catch(RuntimeException e){setStatus(e.getMessage());return;}
        final CatalogWire.Snapshot snapshot=localSnapshot;
        transferring=true;choosing=false;waitingSelection=null;startAfterSelection="";selectionGate.invalidate();
        issue(nextSequence(),Protocol.CATALOG,snapshot.begin(),new String[]{"CAT_BEGIN","CAT_SAME:"+snapshot.revision},false,"Начало синхронизации каталога",10000,()->{
            if(lastCommandReply.equals("CAT_SAME:"+snapshot.revision))catalogCommitted(snapshot);
            else sendCatalogChunk(snapshot,0);
        });
    }
    private void sendCatalogChunk(CatalogWire.Snapshot snapshot,int offset){
        if(!ready||!foreground||!transferring)return;
        if(offset==snapshot.length()){
            issue(Protocol.CATALOG,new byte[]{CatalogWire.COMMIT},"CAT_COMMIT:"+snapshot.revision,"Сохранение каталога на браслете",20000,()->catalogCommitted(snapshot));return;
        }
        int next=Math.min(snapshot.length(),offset+CatalogWire.CHUNK_BYTES);
        issue(Protocol.CATALOG,snapshot.chunk(offset),"CAT_DATA:"+next,"Каталог "+next+" / "+snapshot.length()+" байт",8000,()->sendCatalogChunk(snapshot,next));
    }
    private void catalogCommitted(CatalogWire.Snapshot snapshot){
        transferring=false;bandSummary=CatalogWire.Summary.parse("CATALOG:"+snapshot.revision+":"+snapshot.count()+":"+snapshot.hashHex());selectionGate.observed(bandSummary);
        setStatus("Метки синхронизированы: "+snapshot.count());
    }
    private void selectOnPhone(TagRecord record){
        if(!catalogWritable||scanning||pending>=0||transferring||choosing)return;
        if(active||(!stopped&&ready)){requestStop(false,()->selectOnPhone(record));return;}
        catalog.selectedId=record.id;selectionGate.invalidate();if(!save())return;render();
        if(!record.syncToBand){setStatus("Отметьте метку и синхронизируйте её с браслетом");return;}
        if(!ready||!capabilities){setStatus("Метка выбрана. Подключите браслет.");return;}
        if(!selectionGate.catalogMatches()){setStatus("Сначала синхронизируйте метки с браслетом");return;}
        proposeSelection(record,false);
    }
    private void proposeSelection(TagRecord record,boolean startAfter){
        if(record==null||!selectionGate.catalogMatches()||!ready||!stopped||pending>=0)return;
        CatalogWire.Entry entry=localSnapshot.find(record.id);if(entry==null)return;
        final long attempt=++selectionAttempt;choosing=true;selectionGate.invalidate();startAfterSelection=startAfter?record.id:"";
        final long sequence=nextSequence();
        issue(sequence,Protocol.SELECTION,entry.selection(localSnapshot.revision).payload(CatalogWire.SELECT_PROPOSE),new String[]{"SELECT_PENDING"},false,"Выбор метки на браслете",20000,()->setStatus("Ожидается подтверждение выбранной записи"));
        main.postDelayed(()->{if(selectionAttempt==attempt&&choosing&&pending<0&&waitingSelection==null){choosing=false;startAfterSelection="";setStatus("Браслет не подтвердил выбор. Оставьте его экран открытым и выберите запись повторно.");render();}},25000);
    }
    private void handleSelection(Protocol.Frame frame){
        try{CatalogWire.Selection.parse(frame.payload,CatalogWire.SELECT_REQUEST);}
        catch(RuntimeException e){failCommand("Неверный запрос выбора: "+e.getMessage());return;}
        engine=null;sessionNdef=null;active=false;selectionAttempt++;selectionGate.invalidate();
        if(checking){endScan();readbackResult.setText("Проверка прервана: выбранная метка изменилась");}
        if(pending>=0 && ((pendingKind==Protocol.SELECTION&&pendingOpcode==CatalogWire.SELECT_ACK)||(pendingKind==Protocol.CONTROL&&pendingOpcode==1))){pending=-1;commandEpoch++;gate.reset();completed=null;}
        // A catalog-profile REQUEST is emitted only after native NFC cleanup.
        stopped=true;waitingSelection=frame;drainSelection();render();
    }
    private void drainSelection(){
        if(waitingSelection==null||pending>=0||transferring||!capabilities||!ready||!foreground)return;
        if(!selectionGate.catalogMatches()){choosing=false;startAfterSelection="";return;}
        Protocol.Frame frame=waitingSelection;waitingSelection=null;final SelectionGate.Request request;
        try{request=selectionGate.receive(frame.sequence,frame.payload);}
        catch(RuntimeException e){choosing=false;startAfterSelection="";setStatus("Выбор не подтверждён: "+e.getMessage());return;}
        if(!startAfterSelection.equals(request.selection.id))startAfterSelection="";
        choosing=true;
        issue(frame.sequence,Protocol.SELECTION,request.selection.payload(CatalogWire.SELECT_ACK),new String[]{"SELECT_OK"},false,"Подтверждение выбора метки",5000,()->{
            if(!selectionGate.confirm(request)){choosing=false;startAfterSelection="";setStatus("Выбор устарел; ожидается новый запрос браслета");return;}
            catalog.selectedId=request.selection.id;choosing=false;if(!save()){selectionGate.invalidate();startAfterSelection="";return;}
            TagRecord record=catalog.selected();setStatus("Выбрана «"+record.name+"» · "+(record.emulatable()?"NDEF готов к ручному запуску":"только сведения"));
            boolean launch=startAfterSelection.equals(record.id);startAfterSelection="";if(launch)startEmulation();
        });
    }
    private void beginReadback(){
        if(!foreground||!ready||!active||pending>=0||checking||sessionNdef==null)return;
        if(nfc==null||!nfc.isEnabled()){setStatus("Включите NFC на Pixel для проверки");return;}
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
        TagRecord record=catalog.selected();if(!catalogWritable||!foreground||!ready||!capabilities||!stopped||busy()||record==null||!record.syncToBand||!record.emulatable()||!selectionGate.catalogMatches())return;
        CatalogWire.Entry authorized=selectionGate.authorized(record.id);
        if(authorized==null){proposeSelection(record,true);return;}
        // Snapshot prevents catalogue edits or later reads from changing a live NFC transaction.
        sessionNdef=authorized.ndef();engine=new Type4Ndef(sessionNdef);stopped=false;readbackProof.reset();
        issue(Protocol.CONTROL,new byte[]{1},"READY","Запуск NFC",20000,()->{active=true;setStatus("NFC готов: «"+record.name+"». Приложите браслет к другому телефону / считывателю.");});
    }
    private void requestStop(boolean disconnect){
        requestStop(disconnect,null);
    }
    private void requestStop(boolean disconnect,Runnable continuation){
        if(scanning||checking)endScan();engine=null;sessionNdef=null;readbackProof.reset();active=false;selectionGate.invalidate();closing=disconnect;
        transferring=choosing=false;waitingSelection=null;startAfterSelection="";selectionAttempt++;
        if(!ready){transport.disconnect();render();return;}
        pending=-1;commandEpoch++;gate.reset();completed=null;stopped=false;
        issue(Protocol.CONTROL,new byte[]{2},"STOPPED","Остановка NFC",20000,()->{stopped=true;setStatus("NFC выключен");if(closing||!foreground)transport.disconnect();else if(continuation!=null)continuation.run();else query();});
    }
    private void append(String message){if(Looper.myLooper()!=Looper.getMainLooper()){main.post(()->append(message));return;}logBuffer.append(message).append('\n');if(logBuffer.length()>12000)logBuffer.delete(0,logBuffer.indexOf("\n",4000)+1);if(log!=null)log.setText(logBuffer.toString());}
    @Override protected void onResume(){super.onResume();foreground=true;render();}
    @Override protected void onPause(){foreground=false;endScan();if(transport!=null){if(ready)requestStop(true);else transport.disconnect();}super.onPause();}
    @Override protected void onDestroy(){checking=false;if(connectionDialog!=null)connectionDialog.dismiss();if(debugDialog!=null)debugDialog.dismiss();if(transport!=null)transport.shutdown();worker.shutdownNow();super.onDestroy();}
}
