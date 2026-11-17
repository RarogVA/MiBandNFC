package dev.mbapdu.tags;

import android.app.Activity;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;
import java.io.File;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import org.json.JSONArray;
import org.json.JSONObject;

/** Five Mi Home actions over local miIO, triggered from the band over BLE. */
public final class MiHomeActivity extends Activity {
    private static final String LOCAL_NETWORK = "android.permission.ACCESS_LOCAL_NETWORK";
    private interface Done { void accept(int code); }

    private final Handler main = new Handler(Looper.getMainLooper());
    private final ExecutorService worker = Executors.newSingleThreadExecutor();
    private TextView status;
    private TextView connection;
    private LinearLayout buttons;
    private JSONObject config;
    private BleTransport transport;
    private boolean bleReady, polling, queryPending;
    private long sequence = 1;

    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        requestPermissionsIfNeeded();
        buildUi();
        load();
    }

    private void requestPermissionsIfNeeded() {
        java.util.ArrayList<String> missing = new java.util.ArrayList<>();
        if (checkSelfPermission(LOCAL_NETWORK) != PackageManager.PERMISSION_GRANTED)
            missing.add(LOCAL_NETWORK);
        if (checkSelfPermission(android.Manifest.permission.BLUETOOTH_SCAN) != PackageManager.PERMISSION_GRANTED)
            missing.add(android.Manifest.permission.BLUETOOTH_SCAN);
        if (checkSelfPermission(android.Manifest.permission.BLUETOOTH_CONNECT) != PackageManager.PERMISSION_GRANTED)
            missing.add(android.Manifest.permission.BLUETOOTH_CONNECT);
        if (!missing.isEmpty()) requestPermissions(missing.toArray(new String[0]), 7);
    }

    private void buildUi() {
        LinearLayout root = AppStyle.column(this);
        root.setBackgroundColor(AppStyle.BG);
        AppStyle.insets(root);
        AppStyle.gap(root, 8);
        TextView title = AppStyle.text(this, "Mi Home", 30, AppStyle.TEXT);
        AppStyle.medium(title);
        root.addView(title);
        AppStyle.gap(root, 8);
        TextView note = AppStyle.text(this,
            "Локальный miIO · 5 действий · команда с браслета\n6-й слот пустой",
            14, AppStyle.MUTED);
        root.addView(note);
        AppStyle.gap(root, 12);
        connection = AppStyle.text(this, "Браслет не подключён", 14, AppStyle.MUTED);
        root.addView(connection);
        AppStyle.gap(root, 8);
        AppStyle.addButton(root, "Подключить браслет", false, v -> connectBand());
        AppStyle.gap(root, 20);
        status = AppStyle.text(this, "Загрузка конфигурации…", 14, AppStyle.MUTED);
        root.addView(status);
        AppStyle.gap(root, 20);
        buttons = AppStyle.column(this);
        root.addView(buttons);
        AppStyle.gap(root, 20);
        AppStyle.addButton(root, "Получить токен", false,
            v -> startActivity(new android.content.Intent(this, TokenImportActivity.class)));
        AppStyle.gap(root, 10);
        AppStyle.addButton(root, "Включить fallback Vevs", false,
            v -> startActivity(new android.content.Intent(android.provider.Settings.ACTION_ACCESSIBILITY_SETTINGS)));
        ScrollView page = new ScrollView(this);
        page.setFillViewport(true);
        page.addView(root);
        setContentView(page);
    }

    private void connectBand() {
        String address = getPreferences(MODE_PRIVATE).getString("band_address", "");
        if (address == null || address.trim().isEmpty()) {
            connection.setText("Сначала подключите браслет в главном экране");
            return;
        }
        if (transport == null) transport = new BleTransport(this, listener);
        bleReady = false;
        connection.setText("Подключение браслета…");
        transport.connect(address.trim());
    }

    private final BridgeTransport.Listener listener = new BridgeTransport.Listener() {
        @Override public void state(boolean ready, String message) {
            bleReady = ready;
            main.post(() -> {
                connection.setText(message);
                if (ready) startPolling(); else stopPolling();
            });
        }
        @Override public void received(Protocol.Frame frame) {
            if (frame.kind != Protocol.MIHOME || frame.payload.length < 1) return;
            int opcode = frame.payload[0] & 255;
            if (opcode == 1) {
                queryPending = false;
                int index = frame.payload.length > 1 ? (frame.payload[1] & 255) : 0xff;
                if (index >= 1 && index <= 6) {
                    main.post(() -> runSlot(index - 1, code -> sendStatus(code)));
                } else {
                    main.post(() -> sendStatus(0));
                }
            }
        }
        @Override public void log(String message) {
            main.post(() -> connection.setText(message));
        }
    };

    private void startPolling() {
        if (polling || !bleReady) return;
        polling = true;
        main.post(poll);
    }

    private void stopPolling() {
        polling = false;
        queryPending = false;
        main.removeCallbacks(poll);
    }

    private final Runnable poll = new Runnable() {
        @Override public void run() {
            if (!polling || !bleReady || transport == null) return;
            if (!queryPending) {
                queryPending = true;
                try {
                    transport.send(new Protocol.Frame(Protocol.MIHOME, nextSequence(), new byte[]{1}),
                        ok -> { if (!ok) main.post(() -> { queryPending = false; polling = false; }); });
                } catch (RuntimeException e) {
                    queryPending = false;
                    connection.setText("BLE: " + e.getMessage());
                    return;
                }
                main.postDelayed(() -> queryPending = false, 2000);
            }
            main.postDelayed(this, 500);
        }
    };

    private long nextSequence() {
        long value = sequence++;
        if (sequence > 0xffffffffL) sequence = 1;
        return value;
    }

    private void sendStatus(int code) {
        if (transport == null || !bleReady) return;
        try {
            transport.send(new Protocol.Frame(Protocol.MIHOME, nextSequence(),
                new byte[]{2, (byte) (code == 0 ? 0 : 1)}), ok -> { });
        } catch (RuntimeException e) {
            connection.setText("BLE статус: " + e.getMessage());
        }
    }

    private void load() {
        try {
            File file = new File(getExternalFilesDir(null), "mihome.json");
            if (!file.exists()) {
                status.setText("Нет " + file.getAbsolutePath() + "\nПоложи mihome.json через adb.");
                return;
            }
            config = new JSONObject(new String(Files.readAllBytes(file.toPath()), StandardCharsets.UTF_8));
            render();
        } catch (Exception e) {
            status.setText("Ошибка конфигурации: " + e.getMessage());
        }
    }

    private void render() throws Exception {
        buttons.removeAllViews();
        JSONArray slots = config.getJSONArray("slots");
        for (int i = 0; i < slots.length(); i++) {
            JSONObject slot = slots.getJSONObject(i);
            String label = slot.optString("label", "Слот " + (i + 1));
            boolean enabled = slot.optBoolean("enabled", slot.has("action") && !slot.isNull("action"));
            final int index = i;
            android.widget.Button button = AppStyle.addButton(buttons, label, false,
                v -> runSlot(index));
            AppStyle.enable(button, enabled);
            if (!enabled) button.setText(label + "  ·  пусто");
            AppStyle.gap(buttons, 10);
        }
        status.setText("Готово. Команда с браслета выполнится на Pixel.");
    }

    private void runSlot(int index) {
        runSlot(index, code -> { });
    }

    private void runSlot(int index, Done done) {
        if (config == null) { done.accept(1); return; }
        try {
            JSONObject slot = config.getJSONArray("slots").getJSONObject(index);
            if (!slot.optBoolean("enabled", slot.has("action") && !slot.isNull("action"))) {
                done.accept(1); return;
            }
            String key = slot.getString("device_key");
            JSONObject device = config.getJSONObject("devices").getJSONObject(key);
            String host = device.getString("host");
            String token = device.getString("token");
            String did = device.optString("did", "");
            int timeoutMs = device.optInt("timeout_ms", 10000);
            boolean noAck = device.optBoolean("no_ack", false);
            JSONObject action = slot.getJSONObject("action");
            String method = action.getString("method");
            Object params = toParams(action.opt("params"));
            status.setText("Отправка: " + slot.optString("label", "слот " + (index + 1)) + "…");
            Log.d("MiHomeActivity", "slot " + (index + 1) + " " + method + " " + key);
            worker.execute(() -> {
                try {
                    MiioClient client = new MiioClient(host, token, did, timeoutMs);
                    try {
                        client.hello();
                    } catch (java.net.SocketTimeoutException e) {
                        String label = slot.optString("label", "слот");
                        main.post(() -> status.setText("Ошибка · " + label + "\nНет handshake с устройством"));
                        done.accept(fallback(label) ? 0 : 1); return;
                    }
                    try {
                        JSONObject result = client.send(method, params);
                        main.post(() -> status.setText("OK · " + slot.optString("label") + "\n" + shorten(result)));
                        done.accept(0);
                    } catch (java.net.SocketTimeoutException e) {
                        if (noAck) {
                            main.post(() -> status.setText("Отправлено · " + slot.optString("label")
                                + "\nКоманда ушла, устройство не прислало ответ"));
                            done.accept(0);
                        } else {
                            String label = slot.optString("label", "слот");
                            main.post(() -> status.setText("Ошибка · " + label
                                + "\nНет ответа за " + (timeoutMs / 1000) + " с"));
                            done.accept(fallback(label) ? 0 : 1);
                        }
                    }
                } catch (Exception e) {
                    String label = slot.optString("label", "слот");
                    main.post(() -> status.setText("Ошибка · " + label + "\n" + e.getClass().getSimpleName() + ": " + e.getMessage()));
                    done.accept(fallback(label) ? 0 : 1);
                }
            });
        } catch (Exception e) {
            status.setText("Ошибка слота: " + e.getMessage());
            done.accept(1);
        }
    }

    private boolean fallback(String label) {
        if (config == null || !"vevs".equals(config.optString("cloud_fallback", ""))) return false;
        if (!CloudFallbackService.enabled(this)) {
            main.post(() -> status.setText("Локально не прошло · fallback Vevs не включён\nВключи Accessibility для «Метки • NFC»"));
            return false;
        }
        main.post(() -> {
            status.setText("Локально не прошло · открываю Vevs: " + label);
            CloudFallbackService.request(this, label);
        });
        return true;
    }

    private static Object toParams(Object value) throws Exception {
        if (value == null || value == JSONObject.NULL) return new JSONArray();
        String text = value.toString();
        if (value instanceof JSONArray || text.trim().startsWith("[")) return new JSONArray(text);
        return new JSONObject(text);
    }

    private static String shorten(JSONObject value) {
        String text = value.toString();
        return text.length() > 500 ? text.substring(0, 500) + "…" : text;
    }

    @Override protected void onPause() {
        stopPolling();
        if (transport != null) transport.disconnect();
        bleReady = false;
        super.onPause();
    }

    @Override protected void onDestroy() {
        stopPolling();
        if (transport != null) transport.shutdown();
        worker.shutdownNow();
        super.onDestroy();
    }
}
