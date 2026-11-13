package dev.mbapdu.probe;

import android.Manifest;
import android.annotation.SuppressLint;
import android.app.Activity;
import android.app.AlertDialog;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothManager;
import android.bluetooth.le.BluetoothLeScanner;
import android.bluetooth.le.ScanCallback;
import android.bluetooth.le.ScanFilter;
import android.bluetooth.le.ScanResult;
import android.bluetooth.le.ScanRecord;
import android.bluetooth.le.ScanSettings;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.graphics.Color;
import android.graphics.Insets;
import android.nfc.NfcAdapter;
import android.nfc.Tag;
import android.nfc.tech.IsoDep;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.ParcelUuid;
import android.view.View;
import android.view.WindowInsets;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.security.SecureRandom;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.atomic.AtomicBoolean;

@SuppressLint("MissingPermission")
public final class MainActivity extends Activity {
    private final Handler main = new Handler(Looper.getMainLooper());
    private final ExecutorService nfcWorker = Executors.newSingleThreadExecutor(r -> new Thread(r, "apdu-nfc-reader"));
    private final ProofJournal proof = new ProofJournal();
    private final ControlPreflight preflight = new ControlPreflight();
    private final InfoRequest infoRequest = new InfoRequest();
    private final SecureRandom random = new SecureRandom();
    private final AtomicBoolean transceiving = new AtomicBoolean();
    private final Map<String, String> scanDevices = new LinkedHashMap<>();
    private final StringBuilder logBuffer = new StringBuilder();
    private BleTransport transport;
    private NfcAdapter nfc;
    private BluetoothLeScanner scanner;
    private TextView status, result, log;
    private EditText address;
    private Button start, info;
    private volatile boolean btReady, readerActive, foreground;
    private volatile long readerEpoch;
    private volatile IsoDep activeIsoDep;
    private long nextControl = Integer.toUnsignedLong(random.nextInt());
    private long pendingStart = -1;
    private long pendingStop = -1;
    private boolean disconnectAfterStop;
    private boolean startWritten, startReady;
    private final Runnable scanFinish = this::finishScan;

    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        nfc = NfcAdapter.getDefaultAdapter(this);
        buildUi();
        transport = new BleTransport(this, new BridgeTransport.Listener() {
            @Override public void state(boolean ready, String message) {
                btReady = ready;
                if (ready) proof.connected(); else proof.disconnected();
                main.post(() -> {
                    status.setText(message); append(message);
                    if (!ready) {
                        preflight.reset();
                        infoRequest.reset();
                        pendingStart = -1;
                        pendingStop = -1; disconnectAfterStop = false;
                        if (readerActive) stopReaderLocal();
                        updateStartButton();
                    } else requestPreflight();
                });
            }
            @Override public void log(String message) { append(message); }
            @Override public void received(Protocol.Frame frame) {
                if (frame.kind == Protocol.STATUS) {
                    String value = new String(frame.payload, StandardCharsets.UTF_8);
                    main.post(() -> handleStatus(frame.sequence, value));
                    return;
                }
                // This is the only ingress for proof receipts. NFC worker never calls it.
                ProofJournal.Receipt receipt = proof.receivedOverBluetooth(frame.sequence, frame.payload);
                byte[] response = Protocol.respond(frame.payload);
                append("BT RX seq=" + frame.sequence + " CAPDU=" + Protocol.hex(frame.payload));
                transport.send(new Protocol.Frame(Protocol.RESPONSE, frame.sequence, response), success -> {
                    proof.completedBluetoothWrite(receipt, success);
                    append("BT TX seq=" + frame.sequence + (success ? " ATT подтверждён" : " ОШИБКА записи") +
                            " RAPDU=" + Protocol.hex(response));
                });
            }
        });
        append("Только синтетический AID F04D424150445501. Совместимость GATT с браслетом ещё не подтверждена.");
        append("Нет Mir Pay, карт, доступа в Интернет или фоновой службы.");
    }
    private TextView text(String value, int size) {
        TextView v = new TextView(this); v.setText(value); v.setTextSize(size); v.setTextColor(Color.rgb(25, 35, 45));
        v.setPadding(0, 6, 0, 6); return v;
    }
    private Button button(LinearLayout parent, String title, View.OnClickListener action) {
        Button b = new Button(this); b.setText(title); b.setAllCaps(false); b.setOnClickListener(action);
        parent.addView(b); return b;
    }
    private void buildUi() {
        LinearLayout root = new LinearLayout(this); root.setOrientation(LinearLayout.VERTICAL);
        root.setBackgroundColor(Color.rgb(245, 248, 250));
        int pad = (int) (16 * getResources().getDisplayMetrics().density);
        root.setPadding(pad, pad, pad, pad);
        root.setOnApplyWindowInsetsListener((v, insets) -> {
            Insets i = insets.getInsets(WindowInsets.Type.systemBars());
            root.setPadding(pad + i.left, pad + i.top, pad + i.right, pad + i.bottom); return insets;
        });
        root.addView(text("APDU · лаборатория", 24));
        root.addView(text("Pixel читает браслет по NFC; ответ должен вернуться через независимый Bluetooth-обработчик.", 14));
        status = text("BT не подключён", 15); root.addView(status);
        button(root, "1 · Разрешить Bluetooth", v -> requestBluetooth());
        address = new EditText(this); address.setSingleLine(true);
        address.setHint("BLE-адрес (необязательно; иначе поиск сервиса)");
        address.setTextSize(14); root.addView(address);
        button(root, "2 · Подключить браслет", v -> connectOrScan());
        button(root, "Диагностика · сопряжённый браслет", v -> chooseBondedBand());
        button(root, "Проверить BLE · без NFC", v -> requestPreflight());
        info = button(root, "Возможности NFC · только чтение", v -> requestInfo());
        info.setEnabled(false);
        start = button(root, "3 · Запустить один NFC-тест", v -> requestStart());
        start.setEnabled(false);
        button(root, "Остановить NFC и Bluetooth", v -> stopAll());
        result = text("Результат: тест не запускался", 16); root.addView(result);
        log = text("", 12); log.setTypeface(android.graphics.Typeface.MONOSPACE); log.setTextIsSelectable(true);
        root.addView(log);
        // A single scroll surface keeps every control reachable with large fonts
        // and leaves no zero-height log area on a shorter Pixel viewport.
        ScrollView page = new ScrollView(this); page.setFillViewport(true); page.addView(root);
        setContentView(page);
    }
    private void append(String message) {
        if (Looper.myLooper() != Looper.getMainLooper()) { main.post(() -> append(message)); return; }
        String time = String.format(Locale.ROOT, "%.3f", android.os.SystemClock.elapsedRealtime() / 1000.0);
        logBuffer.append(time).append(' ').append(message).append('\n');
        if (logBuffer.length() > 24000) logBuffer.delete(0, logBuffer.indexOf("\n", 8000) + 1);
        log.setText(logBuffer.toString());
    }
    private boolean permissions() {
        return checkSelfPermission(Manifest.permission.BLUETOOTH_CONNECT) == PackageManager.PERMISSION_GRANTED &&
                checkSelfPermission(Manifest.permission.BLUETOOTH_SCAN) == PackageManager.PERMISSION_GRANTED;
    }
    private void requestBluetooth() {
        if (!permissions()) requestPermissions(new String[]{Manifest.permission.BLUETOOTH_SCAN,
                Manifest.permission.BLUETOOTH_CONNECT}, 10);
        else append("Разрешения Bluetooth уже выданы");
    }
    @Override public void onRequestPermissionsResult(int requestCode, String[] names, int[] grants) {
        super.onRequestPermissionsResult(requestCode, names, grants);
        append(permissions() ? "Разрешения Bluetooth выданы" : "PERMISSION: разрешение Bluetooth не выдано");
    }
    private BluetoothAdapter connectionAdapter() {
        if (!permissions()) { requestBluetooth(); return null; }
        BluetoothManager manager = getSystemService(BluetoothManager.class);
        BluetoothAdapter adapter = manager == null ? null : manager.getAdapter();
        if (adapter == null) { append("BT: адаптер недоступен"); return null; }
        if (!adapter.isEnabled()) { startActivity(new Intent(BluetoothAdapter.ACTION_REQUEST_ENABLE)); return null; }
        if (readerActive || pendingStart >= 0 || pendingStop >= 0) {
            append("Сначала остановите текущий тест и дождитесь STOPPED"); return null;
        }
        return adapter;
    }
    private void connectSelected(String selectedAddress, String description) {
        if (!foreground || connectionAdapter() == null) return;
        stopScan();
        address.setText(selectedAddress);
        append("BT: выбран " + description);
        transport.connect(selectedAddress);
    }
    private void chooseBondedBand() {
        BluetoothAdapter adapter = connectionAdapter();
        if (adapter == null) return;
        stopScan();
        ArrayList<String> addresses = new ArrayList<>(), names = new ArrayList<>();
        try {
            for (BluetoothDevice device : adapter.getBondedDevices()) {
                String name = device.getName();
                if (name == null) continue;
                String normalized = name.toLowerCase(Locale.ROOT).replaceAll("[\\s_-]+", "");
                if (!normalized.contains("xiaomi") && !normalized.contains("band10pro")) continue;
                addresses.add(device.getAddress());
                names.add(name + " · " + device.getAddress());
            }
        } catch (RuntimeException e) { append("BT BONDED: " + e.getMessage()); return; }
        if (addresses.isEmpty()) {
            append("BT BONDED: среди сопряжённых устройств нет Xiaomi / Band 10 Pro"); return;
        }
        new AlertDialog.Builder(this).setTitle("Диагностика: выберите свой браслет A234")
                .setItems(names.toArray(new String[0]), (dialog, which) ->
                        connectSelected(addresses.get(which), "сопряжённый " + names.get(which)))
                .setNegativeButton("Отмена", null).show();
    }
    private void connectOrScan() {
        BluetoothAdapter adapter = connectionAdapter();
        if (adapter == null) return;
        stopScan();
        String explicit = address.getText().toString().trim().toUpperCase(Locale.ROOT);
        if (!explicit.isEmpty()) { transport.connect(explicit); return; }
        scanDevices.clear(); scanner = adapter.getBluetoothLeScanner();
        if (scanner == null) { append("BT: BLE-сканер недоступен"); return; }
        try {
            scanner.startScan(Arrays.asList(new ScanFilter.Builder()
                    .setServiceUuid(ParcelUuid.fromString(Protocol.SERVICE)).build(),
                    new ScanFilter.Builder().setDeviceName("MB-APDU").build()),
                    new ScanSettings.Builder().setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY).build(), scanCallback);
            append("BT: поиск UUID сервиса ИЛИ рекламного имени MB-APDU, 7 секунд…");
            main.postDelayed(scanFinish, 7000);
        } catch (RuntimeException e) { stopScan(); append("BT SCAN: " + e.getMessage()); }
    }
    private final ScanCallback scanCallback = new ScanCallback() {
        @Override public void onScanResult(int callbackType, ScanResult scanResult) {
            main.post(() -> {
                if (scanner == null) return;
                ScanRecord record = scanResult.getScanRecord();
                String name = record == null ? null : record.getDeviceName();
                List<ParcelUuid> uuids = record == null ? null : record.getServiceUuids();
                boolean serviceMatch = uuids != null && uuids.contains(ParcelUuid.fromString(Protocol.SERVICE));
                if (!serviceMatch && !"MB-APDU".equals(name)) return;
                String a = scanResult.getDevice().getAddress();
                if (!scanDevices.containsKey(a)) append("BT ADV: " + a + " name=" +
                        (name == null ? "(не объявлено)" : name) + " UUIDs=" +
                        (uuids == null ? "[]" : uuids.toString()));
                scanDevices.put(a, (name == null ? "Экспериментальный GATT" : name) + " · " + a);
            });
        }
        @Override public void onScanFailed(int errorCode) { main.post(() -> { stopScan(); append("BT SCAN: код " + errorCode); }); }
    };
    private void stopScan() {
        main.removeCallbacks(scanFinish);
        if (scanner != null) { try { scanner.stopScan(scanCallback); } catch (RuntimeException ignored) { } scanner = null; }
    }
    private void finishScan() {
        if (scanner == null) return;
        stopScan();
        if (!foreground) return;
        if (scanDevices.isEmpty()) {
            append("BT SCAN: UUID сервиса и имя MB-APDU не найдены. Для прямой проверки GATT нажмите «Диагностика · сопряжённый браслет»."); return;
        }
        ArrayList<String> addresses = new ArrayList<>(scanDevices.keySet());
        String[] names = scanDevices.values().toArray(new String[0]);
        new AlertDialog.Builder(this).setTitle("Выберите экспериментальный сервис")
                .setItems(names, (dialog, which) -> connectSelected(addresses.get(which), names[which]))
                .setNegativeButton("Отмена", null).show();
    }
    private long controlSequence() { nextControl = (nextControl + 1) & 0xffffffffL; return nextControl; }
    private void updateStartButton() {
        start.setEnabled(btReady && preflight.verified() && pendingStart < 0 && pendingStop < 0 &&
                !readerActive && !transceiving.get() && !infoRequest.pending());
        info.setEnabled(btReady && pendingStart < 0 && pendingStop < 0 && !readerActive &&
                !transceiving.get() && !preflight.pending() && !infoRequest.pending());
    }
    private void requestPreflight() {
        if (!btReady) { append("BLE: сначала подключите экспериментальный GATT-сервис"); return; }
        if (readerActive || pendingStart >= 0 || pendingStop >= 0) {
            append("BLE: сначала остановите текущий тест и дождитесь STOPPED"); return;
        }
        if (preflight.pending()) { append("BLE: ожидаем ответ на текущий QUERY"); return; }
        if (infoRequest.pending()) { append("BLE: ожидаем ответ INFO"); return; }
        final long sequence = controlSequence();
        preflight.begin(sequence); updateStartButton();
        result.setText("Проверяем BLE: QUERY → STOPPED. NFC не запускается.");
        append("CONTROL QUERY seq=" + sequence + " · без изменения NFC");
        transport.send(new Protocol.Frame(Protocol.CONTROL, sequence, new byte[]{0}), ok -> main.post(() -> {
            append("CONTROL QUERY seq=" + sequence + ": " + (ok ? "ATT подтверждён" : "ОШИБКА ATT записи"));
            if (!preflight.matches(sequence)) return;
            preflight.writeCompleted(sequence, ok);
            if (!ok) { updateStartButton(); fail("BLE: QUERY не записан; NFC не запускался"); return; }
            confirmPreflight();
        }));
        main.postDelayed(() -> {
            if (preflight.matches(sequence) && !preflight.verified()) {
                preflight.reset(); updateStartButton();
                fail("BLE: нет подтверждённого QUERY → STOPPED (5 с). NFC не запускался. Можно повторить «Проверить BLE».");
            }
        }, 5000);
    }
    private void confirmPreflight() {
        if (!preflight.verified()) return;
        status.setText("BLE проверен: QUERY → STOPPED");
        result.setText("BLE работает в обе стороны. Можно запустить один NFC-тест.");
        append("BLE PREFLIGHT: STOPPED с правильным seq + завершённый ATT write");
        updateStartButton();
    }
    private void requestInfo() {
        if (!btReady || !foreground) { append("INFO: сначала подключите GATT-сервис"); return; }
        if (readerActive || pendingStart >= 0 || pendingStop >= 0 || transceiving.get()) {
            append("INFO: дождитесь завершения текущего NFC-теста или STOP"); return;
        }
        if (preflight.pending() || infoRequest.pending()) {
            append("INFO: дождитесь ответа на текущий запрос BLE"); return;
        }
        final long sequence = controlSequence();
        infoRequest.begin(sequence); updateStartButton();
        result.setText("Читаем возможности NFC. Запуск и маршруты не меняются.");
        append("CONTROL INFO seq=" + sequence + " · payload=03, только чтение");
        transport.send(new Protocol.Frame(Protocol.CONTROL, sequence, new byte[]{3}), ok -> main.post(() -> {
            append("CONTROL INFO seq=" + sequence + ": " + (ok ? "ATT подтверждён" : "ОШИБКА ATT записи"));
            if (!infoRequest.matches(sequence) || !infoRequest.pending()) return;
            infoRequest.writeCompleted(sequence, ok);
            if (!ok) { updateStartButton(); fail("INFO: запрос не записан"); return; }
            confirmInfo();
        }));
        main.postDelayed(() -> {
            if (infoRequest.matches(sequence) && infoRequest.pending()) {
                infoRequest.finish(); updateStartButton();
                fail("INFO: нет совпавшего INFO и завершённого ATT write (5 с). NFC не запускался.");
            }
        }, 5000);
    }
    private void confirmInfo() {
        if (!infoRequest.verified()) return;
        String value = infoRequest.value();
        infoRequest.finish(); updateStartButton();
        result.setText("Возможности NFC · только чтение\n" + value);
        append("INFO подтверждён: совпавший seq + завершённый ATT write · " + value);
    }
    private void requestStart() {
        if (!btReady) { fail("BT: сначала подключите экспериментальный сервис"); return; }
        if (!preflight.verified()) {
            fail("BLE: сначала проверка QUERY → STOPPED. Нажмите «Проверить BLE · без NFC»."); return;
        }
        if (nfc == null || !nfc.isEnabled()) { fail("NFC: адаптер отсутствует или выключен"); return; }
        if (readerActive || pendingStart >= 0 || pendingStop >= 0 || transceiving.get() || infoRequest.pending()) {
            append("Дождитесь завершения текущего запуска/остановки"); return;
        }
        preflight.reset();
        pendingStart = controlSequence(); startWritten = false; startReady = false;
        final long sequence = pendingStart;
        updateStartButton(); result.setText("Результат: ожидаем готовность NFC на браслете…");
        append("CONTROL START_NFC seq=" + sequence);
        transport.send(new Protocol.Frame(Protocol.CONTROL, sequence, new byte[]{1}), successful -> main.post(() -> {
            append("CONTROL START_NFC seq=" + sequence + ": " +
                    (successful ? "ATT подтверждён" : "ОШИБКА ATT записи"));
            if (pendingStart != sequence) return;
            if (!successful) { pendingStart = -1; updateStartButton(); fail("CONTROL: START_NFC не записан"); return; }
            startWritten = true; maybeEnableReader();
        }));
        main.postDelayed(() -> {
            if (pendingStart == sequence) { stopReaderAndNative(); fail("CONTROL: нет STATUS READY от браслета (20 с)"); }
        }, 20000);
    }
    private void handleStatus(long sequence, String value) {
        append("STATUS seq=" + sequence + " " + value);
        if (infoRequest.matches(sequence)) {
            if (!infoRequest.pending()) return;
            infoRequest.status(sequence, value);
            if (!infoRequest.pending()) {
                updateStartButton(); fail("INFO: " + value);
            } else confirmInfo();
            return;
        }
        if (preflight.matches(sequence)) {
            preflight.status(sequence, value);
            if ("STOPPED".equals(value)) confirmPreflight();
            else {
                updateStartButton();
                fail("BLE: native активен или восстанавливается (" + value +
                        "). Нажмите «Остановить NFC и Bluetooth», затем подключитесь снова.");
            }
            return;
        }
        if (value.startsWith("ERROR") && pendingStop >= 0) { finishStop(value); return; }
        if (sequence == pendingStop) {
            if (value.startsWith("STOPPING:")) status.setText("NFC: остановка · " + value.substring(9));
            else if ("STOPPED".equals(value)) finishStop(null);
            else if (value.startsWith("ERROR")) finishStop(value);
            return;
        }
        if (value.startsWith("ERROR") && (readerActive || pendingStart >= 0)) {
            stopReaderAndNative(); fail("NATIVE: " + value); return;
        }
        if (sequence != pendingStart) return;
        if ("READY".equals(value)) { startReady = true; maybeEnableReader(); }
        else if (value.startsWith("STARTING:")) {
            result.setText("Результат: браслет подготавливает NFC…\n" + value.substring(9));
        }
        else { stopReaderAndNative(); fail("NATIVE: " + value); }
    }
    private void maybeEnableReader() {
        if (pendingStart < 0 || !startReady || !startWritten) return;
        pendingStart = -1;
        if (!foreground || !btReady) { stopReaderAndNative(); fail("START: приложение или Bluetooth уже не активны"); return; }
        readerEpoch++; readerActive = true;
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        try {
            nfc.enableReaderMode(this, this::onTag,
                    NfcAdapter.FLAG_READER_NFC_A | NfcAdapter.FLAG_READER_NFC_B | NfcAdapter.FLAG_READER_SKIP_NDEF_CHECK, null);
            result.setText("Приложите браслет к NFC-антенне Pixel");
            append("NFC: ReaderMode включён после STATUS READY и завершения START write");
        } catch (RuntimeException e) { stopReaderAndNative(); fail("NFC START: " + e.getMessage()); }
    }
    private void onTag(Tag tag) {
        if (!readerActive || !transceiving.compareAndSet(false, true)) return;
        long epoch = readerEpoch;
        nfcWorker.execute(() -> runNfc(tag, epoch));
    }
    private void runNfc(Tag tag, long epoch) {
        String stage = "NFC CONNECT", outcome;
        IsoDep iso = IsoDep.get(tag);
        try {
            if (!readerActive || epoch != readerEpoch) return;
            if (iso == null) throw new IOException("метка не поддерживает IsoDep");
            activeIsoDep = iso; iso.connect(); iso.setTimeout(4500);
            byte[] challenge = new byte[16]; random.nextBytes(challenge);
            ProofJournal.Run run = proof.begin(challenge);
            long before = System.nanoTime();
            stage = "NFC SELECT"; append("NFC TX SELECT " + Protocol.hex(Protocol.SELECT));
            byte[] selected = iso.transceive(Protocol.SELECT);
            append("NFC RX SELECT " + Protocol.hex(selected));
            if (!Arrays.equals(selected, Protocol.OK)) throw new IOException("ожидался 9000");
            stage = "NFC CHALLENGE";
            append("NFC TX challenge=" + Protocol.hex(challenge) + " run=" + run.id);
            byte[] response = iso.transceive(Protocol.challengeApdu(challenge));
            append("NFC RX " + Protocol.hex(response));
            stage = "PROOF";
            String error = proof.verify(run, response, 600);
            if (error != null) throw new IOException(error);
            double millis = (System.nanoTime() - before) / 1_000_000.0;
            outcome = String.format(Locale.ROOT, "PASS · NFC → BT → Pixel → BT → NFC\nrun=%d, seq=%d, %.1f мс", run.id, proof.sequence(run), millis);
        } catch (Exception e) { outcome = "FAIL · " + stage + ": " + e.getClass().getSimpleName() + " · " + e.getMessage(); }
        finally {
            if (iso != null) try { iso.close(); } catch (IOException ignored) { }
            if (activeIsoDep == iso) activeIsoDep = null;
            transceiving.set(false);
        }
        final String completed = outcome;
        main.post(() -> {
            if (epoch != readerEpoch || !readerActive) return;
            stopReaderAndNative(); result.setText(completed); append(completed);
        });
    }
    private void fail(String text) { result.setText("FAIL · " + text); append("FAIL · " + text); }
    private void stopReaderLocal() {
        readerEpoch++; readerActive = false; pendingStart = -1;
        proof.invalidateRun(); updateStartButton();
        getWindow().clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        if (nfc != null) try { nfc.disableReaderMode(this); } catch (RuntimeException ignored) { }
        IsoDep inProgress = activeIsoDep;
        if (inProgress != null) {
            // close() cancels transceive without waiting for the NFC worker or Bluetooth thread.
            new Thread(() -> { try { inProgress.close(); } catch (IOException ignored) { } }, "apdu-nfc-cancel").start();
        }
    }
    private void stopReaderAndNative() {
        boolean wasStarted = readerActive || pendingStart >= 0;
        stopReaderLocal();
        if (btReady && wasStarted) sendStop(false);
    }
    private void sendStop(boolean disconnect) {
        preflight.reset();
        infoRequest.finish();
        disconnectAfterStop |= disconnect;
        if (pendingStop >= 0) return;
        pendingStop = controlSequence(); updateStartButton();
        final long sequence = pendingStop;
        append("CONTROL STOP_NFC seq=" + sequence);
        transport.send(new Protocol.Frame(Protocol.CONTROL, sequence, new byte[]{2}), ok -> main.post(() -> {
            if (pendingStop != sequence) return;
            if (!ok) finishStop("STOP_NFC не записан");
            else append("CONTROL STOP_NFC: ATT подтверждён, ждём STOPPED");
        }));
        main.postDelayed(() -> {
            if (pendingStop == sequence) finishStop("нет STATUS STOPPED (20 с); завершение NFC на браслете не подтверждено");
        }, 20000);
    }
    private void finishStop(String error) {
        boolean disconnect = disconnectAfterStop || error != null;
        pendingStop = -1; disconnectAfterStop = false; updateStartButton();
        if (error != null) fail("NATIVE STOP: " + error);
        else { status.setText("NFC на браслете остановлен; GATT подключён"); append("NATIVE: STOPPED подтверждён"); }
        if (disconnect) transport.disconnect();
        else requestPreflight();
    }
    private void stopAll() {
        stopScan();
        boolean shouldStop = btReady;
        stopReaderLocal();
        if (shouldStop) sendStop(true);
        else transport.disconnect();
        result.setText("Результат: остановлено пользователем");
    }
    @Override public void onResume() { super.onResume(); foreground = true; }
    @Override public void onPause() { foreground = false; stopScan(); stopReaderAndNative(); super.onPause(); }
    @Override public void onDestroy() {
        transport.shutdown(); nfcWorker.shutdownNow(); super.onDestroy();
    }
}
