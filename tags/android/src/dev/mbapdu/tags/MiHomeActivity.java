package dev.mbapdu.tags;

import android.app.Activity;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
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

/** Test bridge for five Mi Home actions over local miIO. No cloud. */
public final class MiHomeActivity extends Activity {
    private static final String LOCAL_NETWORK = "android.permission.ACCESS_LOCAL_NETWORK";
    private final Handler main = new Handler(Looper.getMainLooper());
    private final ExecutorService worker = Executors.newSingleThreadExecutor();
    private TextView status;
    private LinearLayout buttons;
    private JSONObject config;

    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        if (checkSelfPermission(LOCAL_NETWORK) != PackageManager.PERMISSION_GRANTED) {
            requestPermissions(new String[]{LOCAL_NETWORK}, 7);
        }
        buildUi();
        load();
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
            "Локальный miIO · 5 действий · без Mi Home Cloud\n6-й слот пустой",
            14, AppStyle.MUTED);
        root.addView(note);
        AppStyle.gap(root, 20);
        status = AppStyle.text(this, "Загрузка конфигурации…", 14, AppStyle.MUTED);
        root.addView(status);
        AppStyle.gap(root, 20);
        buttons = AppStyle.column(this);
        root.addView(buttons);
        ScrollView page = new ScrollView(this);
        page.setFillViewport(true);
        page.addView(root);
        setContentView(page);
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
        status.setText("Готово. Нажатие отправит локальную команду.");
    }

    private void runSlot(int index) {
        if (config == null) return;
        try {
            JSONObject slot = config.getJSONArray("slots").getJSONObject(index);
            if (!slot.optBoolean("enabled", slot.has("action") && !slot.isNull("action"))) return;
            String key = slot.getString("device_key");
            JSONObject device = config.getJSONObject("devices").getJSONObject(key);
            String host = device.getString("host");
            String token = device.getString("token");
            String did = device.optString("did", "");
            int timeoutMs = device.optInt("timeout_ms", 10000);
            JSONObject action = slot.getJSONObject("action");
            String method = action.getString("method");
            Object params = toParams(action.opt("params"));
            status.setText("Отправка: " + slot.optString("label", "слот " + (index + 1)) + "…");
            worker.execute(() -> {
                try {
                    MiioClient client = new MiioClient(host, token, did, timeoutMs);
                    client.hello();
                    JSONObject result = client.send(method, params);
                    main.post(() -> status.setText("OK · " + slot.optString("label") + "\n" + shorten(result)));
                } catch (Exception e) {
                    main.post(() -> status.setText("Ошибка · " + slot.optString("label") + "\n" + e.getClass().getSimpleName() + ": " + e.getMessage()));
                }
            });
        } catch (Exception e) {
            status.setText("Ошибка слота: " + e.getMessage());
        }
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
}
