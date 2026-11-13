package dev.mbapdu.tags;

import android.annotation.SuppressLint;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothGatt;
import android.bluetooth.BluetoothGattCallback;
import android.bluetooth.BluetoothGattCharacteristic;
import android.bluetooth.BluetoothGattDescriptor;
import android.bluetooth.BluetoothGattService;
import android.bluetooth.BluetoothManager;
import android.bluetooth.BluetoothProfile;
import android.bluetooth.BluetoothStatusCodes;
import android.content.Context;
import android.os.Handler;
import android.os.HandlerThread;
import java.util.ArrayDeque;
import java.util.Arrays;
import java.util.HashSet;
import java.util.Set;
import java.util.UUID;

/** Experimental custom GATT client. Ready means CCCD enabled, not verified band interoperability. */
@SuppressLint("MissingPermission")
public final class BleTransport implements BridgeTransport {
    private static final UUID SERVICE = UUID.fromString(Protocol.SERVICE);
    private static final UUID RX = UUID.fromString(Protocol.RX), TX = UUID.fromString(Protocol.TX);
    private static final UUID LEGACY_SERVICE = UUID.fromString("7dbb0001-8e51-4f89-a9cb-9d40bb8c0e31");
    private static final UUID CCCD = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb");
    private final Context context;
    private final Listener listener;
    private final HandlerThread thread = new HandlerThread("tags-bluetooth");
    private final Handler handler;
    private final Protocol.Parser parser = new Protocol.Parser();
    private final CacheRefreshGate cacheRefresh = new CacheRefreshGate();
    private final ArrayDeque<Write> writes = new ArrayDeque<>();
    private final Set<Long> seen = new HashSet<>();
    private BluetoothGatt gatt;
    private BluetoothGattCharacteristic rx;
    private boolean ready;
    private long generation, writeCounter;
    private Write current;
    private Runnable partialTimeout;

    private static final class Write {
        final byte[] bytes;
        final Completion completion;
        int offset, inFlight;
        long ticket;
        Write(Protocol.Frame frame, Completion completion) {
            bytes = Protocol.encode(frame); this.completion = completion;
        }
    }
    public BleTransport(Context context, Listener listener) {
        this.context = context.getApplicationContext(); this.listener = listener;
        thread.start(); handler = new Handler(thread.getLooper());
    }
    @Override public void connect(String address) {
        handler.post(() -> {
            cacheRefresh.begin();
            open(address);
        });
    }
    private void open(String address) {
            close("BT: новый сеанс");
            try {
                BluetoothManager manager = context.getSystemService(BluetoothManager.class);
                BluetoothAdapter adapter = manager == null ? null : manager.getAdapter();
                if (adapter == null || !adapter.isEnabled()) { close("BT: Bluetooth выключен"); return; }
                if (!BluetoothAdapter.checkBluetoothAddress(address)) { close("BT: неверный BLE-адрес"); return; }
                listener.state(false, "BT: подключение GATT…");
                BluetoothDevice device = adapter.getRemoteDevice(address);
                gatt = device.connectGatt(context, false, callback, BluetoothDevice.TRANSPORT_LE,
                        BluetoothDevice.PHY_LE_1M_MASK, handler);
                if (gatt == null) { close("BT: connectGatt вернул null"); return; }
                final long session = generation;
                handler.postDelayed(() -> {
                    if (generation == session && !ready) close("BT: timeout подключения/обнаружения/CCCD (12 с)");
                }, 12000);
            } catch (RuntimeException e) { close("BT: " + e.getClass().getSimpleName() + ": " + e.getMessage()); }
    }
    /** Cache diagnostic only, before any characteristic/CCCD writes. No unpair or hidden-API bypass. */
    private boolean refreshLegacyCache(BluetoothGatt source) {
        if (!cacheRefresh.claim(true, source.getService(LEGACY_SERVICE) != null)) return false;
        listener.log("BT CACHE: найден старый APDU Lab UUID 0001; Tag Bridge 0011 отсутствует. Одна попытка обновить кэш выбранного браслета.");
        boolean accepted;
        try {
            Object result = BluetoothGatt.class.getMethod("refresh").invoke(source);
            accepted = Boolean.TRUE.equals(result);
            listener.log("BT CACHE: refresh() вернул " + result);
        } catch (ReflectiveOperationException | SecurityException | LinkageError e) {
            Throwable cause = e.getCause() == null ? e : e.getCause();
            listener.log("BT CACHE: refresh недоступен: " + cause.getClass().getSimpleName() + ": " + cause.getMessage());
            close("BT: обновление GATT-кэша недоступно; подключение закрыто без записи");
            return true;
        }
        if (!accepted) {
            close("BT: система не приняла обновление GATT-кэша; повтор не выполняется");
            return true;
        }
        final long attempt = cacheRefresh.token();
        final long connectedGeneration = generation;
        cacheRefresh.accepted(attempt);
        listener.log("BT CACHE: refresh принят, но обновление ещё не подтверждено. Соединение сохраняется; ожидается повторное обнаружение.");
        handler.postDelayed(() -> {
            if (source == gatt && generation == connectedGeneration && !ready && cacheRefresh.claimExplicitDiscovery(attempt)) {
                listener.log("BT CACHE: один явный discoverServices после 3 секунд ожидания; GATT остаётся подключённым");
                if (!source.discoverServices()) {
                    close("BT CACHE: повторное обнаружение не запустилось; запись не выполнялась");
                    return;
                }
                handler.postDelayed(() -> {
                    if (source == gatt && generation == connectedGeneration && cacheRefresh.current(attempt) && !ready) {
                        close("BT CACHE: timeout повторного discovery/CCCD (5 с); автоматических повторов нет");
                    }
                }, 5000);
            }
        }, 3000);
        return true;
    }
    @Override public void send(Protocol.Frame response, Completion completion) {
        handler.post(() -> {
            if (!ready || gatt == null) { completion.done(false); return; }
            if ((response.kind != Protocol.RESPONSE && response.kind != Protocol.CONTROL && response.kind != Protocol.TAG && response.kind != Protocol.CATALOG && response.kind != Protocol.SELECTION) || writes.size() >= 8) {
                completion.done(false); close("BT: переполнение очереди или неверный тип ответа"); return;
            }
            writes.add(new Write(response, completion));
            if (current == null) nextWrite();
        });
    }
    private void nextWrite() { current = writes.poll(); if (current != null) writeChunk(); }
    private void writeChunk() {
        if (current == null || gatt == null || !ready) return;
        final Write pending = current;
        int end = Math.min(pending.offset + 20, pending.bytes.length);
        pending.inFlight = end - pending.offset;
        pending.ticket = ++writeCounter;
        final long ticket = pending.ticket;
        int result;
        try {
            result = gatt.writeCharacteristic(rx, Arrays.copyOfRange(pending.bytes, pending.offset, end),
                    BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT);
        } catch (RuntimeException e) { close("BT TX: " + e.getMessage()); return; }
        if (result != BluetoothStatusCodes.SUCCESS) { close("BT TX: write не принят, код " + result); return; }
        handler.postDelayed(() -> {
            if (current == pending && pending.ticket == ticket) close("BT TX: timeout подтверждения ATT write (3 с)");
        }, 3000);
    }
    private final BluetoothGattCallback callback = new BluetoothGattCallback() {
        @Override public void onConnectionStateChange(BluetoothGatt source, int status, int newState) {
            if (source != gatt) return;
            if (status != BluetoothGatt.GATT_SUCCESS) { close("BT: ошибка соединения " + status); return; }
            if (newState == BluetoothProfile.STATE_CONNECTED) {
                listener.state(false, "BT: поиск экспериментального GATT-сервиса…");
                if (!source.discoverServices()) close("BT: discoverServices не запустился");
            } else if (newState == BluetoothProfile.STATE_DISCONNECTED) close("BT: соединение закрыто");
        }
        @Override public void onServicesDiscovered(BluetoothGatt source, int status) {
            if (source != gatt) return;
            // refresh may cause an automatic callback while an explicit discovery
            // is queued. Never issue the same CCCD write twice.
            if (cacheRefresh.setupStarted()) return;
            if (status != BluetoothGatt.GATT_SUCCESS) { close("BT: обнаружение сервисов, код " + status); return; }
            BluetoothGattService service = source.getService(SERVICE);
            if (service == null) {
                StringBuilder discovered = new StringBuilder();
                for (BluetoothGattService found : source.getServices()) {
                    if (discovered.length() != 0) discovered.append(", ");
                    discovered.append(found.getUuid());
                }
                listener.log("BT GATT: выбранный браслет " + source.getDevice().getAddress() +
                        " · обнаруженные service UUIDs=[" + discovered + "]");
                if (cacheRefresh.waitForRediscovery(cacheRefresh.token())) {
                    listener.log("BT CACHE: пока получен прежний список; ожидание ограниченного повторного discovery продолжается");
                    return;
                }
                if (refreshLegacyCache(source)) return;
                if (cacheRefresh.attempted()) listener.log("BT CACHE: после единственного refresh сервис Tag Bridge всё ещё отсутствует; автоматических повторов больше нет.");
                close("BT: GATT доступен, но экспериментального сервиса нет; запись не выполнялась"); return;
            }
            rx = service.getCharacteristic(RX);
            BluetoothGattCharacteristic tx = service.getCharacteristic(TX);
            if (rx == null || tx == null || (rx.getProperties() & BluetoothGattCharacteristic.PROPERTY_WRITE) == 0 ||
                    (tx.getProperties() & BluetoothGattCharacteristic.PROPERTY_NOTIFY) == 0) {
                close("BT: нет RX/write или TX/notify по согласованным UUID"); return;
            }
            BluetoothGattDescriptor descriptor = tx.getDescriptor(CCCD);
            if (descriptor == null || !source.setCharacteristicNotification(tx, true)) {
                close("BT: нет CCCD или уведомления недоступны"); return;
            }
            if (!cacheRefresh.claimSetup()) return;
            int result = source.writeDescriptor(descriptor, BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE);
            if (result != BluetoothStatusCodes.SUCCESS) close("BT: запись CCCD не принята, код " + result);
        }
        @Override public void onDescriptorWrite(BluetoothGatt source, BluetoothGattDescriptor descriptor, int status) {
            if (source != gatt || !CCCD.equals(descriptor.getUuid())) return;
            if (status != BluetoothGatt.GATT_SUCCESS) { close("BT: CCCD отклонён, код " + status); return; }
            ready = true;
            listener.state(true, "BT: GATT готов, RX/write + TX/notify, фрагменты ≤20 байт");
        }
        @Override public void onCharacteristicChanged(BluetoothGatt source,
                BluetoothGattCharacteristic characteristic, byte[] value) {
            if (source != gatt || !ready || !TX.equals(characteristic.getUuid())) return;
            if (partialTimeout != null) handler.removeCallbacks(partialTimeout);
            try {
                parser.feed(value, frame -> {
                    if (frame.kind != Protocol.REQUEST && frame.kind != Protocol.STATUS && frame.kind != Protocol.SELECTION)
                        throw new IllegalArgumentException("ожидался CAPDU, STATUS или запрос выбора");
                    if (frame.kind == Protocol.REQUEST && !seen.add(frame.sequence))
                        throw new IllegalArgumentException("повтор sequence в сеансе");
                    if (seen.size() > 4096) throw new IllegalArgumentException("лимит сеанса: переподключите браслет");
                    listener.received(frame);
                });
            } catch (RuntimeException e) { close("BT RX: " + e.getMessage()); return; }
            if (parser.hasPartialFrame()) {
                final long session = generation;
                partialTimeout = () -> {
                    if (generation == session && parser.hasPartialFrame()) close("BT RX: незавершённый кадр (3 с)");
                };
                handler.postDelayed(partialTimeout, 3000);
            }
        }
        @Override public void onCharacteristicWrite(BluetoothGatt source,
                BluetoothGattCharacteristic characteristic, int status) {
            if (source != gatt || !RX.equals(characteristic.getUuid()) || current == null) return;
            if (status != BluetoothGatt.GATT_SUCCESS) { close("BT TX: ATT отказ " + status); return; }
            current.offset += current.inFlight;
            if (current.offset < current.bytes.length) writeChunk();
            else {
                Write done = current; current = null;
                done.completion.done(true);
                nextWrite();
            }
        }
    };
    private void close(String reason) {
        generation++; ready = false;
        if (partialTimeout != null) handler.removeCallbacks(partialTimeout);
        parser.reset(); seen.clear();
        if (current != null) { current.completion.done(false); current = null; }
        while (!writes.isEmpty()) writes.poll().completion.done(false);
        BluetoothGatt previous = gatt; gatt = null; rx = null;
        if (previous != null) {
            try { previous.disconnect(); } catch (RuntimeException ignored) { }
            try { previous.close(); } catch (RuntimeException ignored) { }
        }
        listener.state(false, reason);
    }
    @Override public void disconnect() { handler.post(() -> { cacheRefresh.cancel(); close("BT: остановлен пользователем"); }); }
    @Override public void shutdown() { handler.post(() -> { cacheRefresh.cancel(); close("BT: приложение закрыто"); thread.quitSafely(); }); }
}
