package dev.mbapdu.tags;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.List;
import org.json.JSONArray;
import org.json.JSONObject;

/** Import Vevs device logs locally; tokens never leave the phone. */
public final class TokenImportActivity extends Activity {
    private static final int PICK_FILES = 41;
    private TextView status;
    private LinearLayout results;

    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        LinearLayout root = AppStyle.column(this);
        root.setBackgroundColor(AppStyle.BG);
        AppStyle.insets(root);
        AppStyle.gap(root, 8);
        TextView title = AppStyle.text(this, "Получить токен", 30, AppStyle.TEXT);
        AppStyle.medium(title);
        root.addView(title);
        AppStyle.gap(root, 12);
        root.addView(AppStyle.text(this,
            "Токен нельзя получить из IP или модели — это секрет конкретного устройства.\n\n"
            + "Без Mi Home Cloud путь такой:\n"
            + "1. Установить Vevs (модифицированный Mi Home).\n"
            + "2. Включить логи: Профиль → Расширенные настройки → Лаборатория → "
            + "«Логировать полезные данные».\n"
            + "3. Открыть список устройств, затем найти JSON-файлы Vevs "
            + "(Android/data/com.xiaomi.smarthome/files/vevs/logs/devices).\n"
            + "4. Скопировать их в доступное место и выбрать здесь.\n\n"
            + "Файлы читаются локально; токены сохраняются в приватном каталоге приложения "
            + "и не отправляются в сеть.",
            14, AppStyle.MUTED));
        AppStyle.gap(root, 20);
        AppStyle.addButton(root, "Открыть Vevs", false, v -> openVevs());
        AppStyle.gap(root, 10);
        AppStyle.addButton(root, "Выбрать JSON-файлы устройств", true, v -> pickFiles());
        AppStyle.gap(root, 20);
        status = AppStyle.text(this, "", 14, AppStyle.MUTED);
        root.addView(status);
        AppStyle.gap(root, 12);
        results = AppStyle.column(this);
        root.addView(results);
        ScrollView page = new ScrollView(this);
        page.setFillViewport(true);
        page.addView(root);
        setContentView(page);
    }

    private void openVevs() {
        Intent intent = getPackageManager().getLaunchIntentForPackage("com.xiaomi.smarthome");
        if (intent == null) {
            status.setText("Vevs/Mi Home не установлен: com.xiaomi.smarthome не найден.");
            return;
        }
        startActivity(intent);
    }

    private void pickFiles() {
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.setType("*/*");
        intent.putExtra(Intent.EXTRA_ALLOW_MULTIPLE, true);
        startActivityForResult(intent, PICK_FILES);
    }

    @Override protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode != PICK_FILES || resultCode != RESULT_OK || data == null) return;
        List<Uri> uris = new ArrayList<>();
        if (data.getClipData() != null) {
            for (int i = 0; i < data.getClipData().getItemCount(); i++) {
                uris.add(data.getClipData().getItemAt(i).getUri());
            }
        } else if (data.getData() != null) {
            uris.add(data.getData());
        }
        importFiles(uris);
    }

    private void importFiles(List<Uri> uris) {
        results.removeAllViews();
        int imported = 0;
        List<String> names = new ArrayList<>();
        for (Uri uri : uris) {
            try (InputStream in = getContentResolver().openInputStream(uri)) {
                if (in == null) continue;
                String text = new String(readAll(in), StandardCharsets.UTF_8);
                JSONObject object = new JSONObject(text);
                String token = object.optString("token", "");
                String model = object.optString("model", "");
                String did = object.optString("did", "");
                String name = object.optString("name", model);
                String host = object.optString("localip", "");
                if (token.length() != 32 || did.isEmpty() || model.isEmpty()) continue;
                JSONObject device = new JSONObject();
                device.put("did", did);
                device.put("token", token);
                device.put("model", model);
                device.put("host", host);
                device.put("name", name);
                saveImported(device);
                names.add(name + " · " + model + (host.isEmpty() ? "" : " · " + host));
                imported++;
            } catch (Exception e) {
                results.addView(AppStyle.text(this, "Пропущен файл: " + e.getClass().getSimpleName(), 13, AppStyle.RED));
            }
        }
        status.setText(imported == 0 ? "Не найдено ни одного устройства с token/did/model."
                                     : "Импортировано устройств: " + imported + " (токены локально)");
        for (String name : names) {
            TextView line = AppStyle.text(this, "• " + name, 14, AppStyle.TEXT);
            line.setTextIsSelectable(false);
            results.addView(line);
        }
        if (imported > 0) {
            TextView note = AppStyle.text(this,
                "Дальше нужно назначить устройства на 5 слотов: это делается в конфигурации моста, "
                + "а не автоматически по имени.", 13, AppStyle.MUTED);
            results.addView(note);
        }
    }

    private static byte[] readAll(InputStream in) throws Exception {
        java.io.ByteArrayOutputStream out = new java.io.ByteArrayOutputStream();
        byte[] buffer = new byte[8192];
        int read;
        while ((read = in.read(buffer)) > 0) out.write(buffer, 0, read);
        return out.toByteArray();
    }

    private void saveImported(JSONObject device) throws Exception {
        java.io.File file = new java.io.File(getExternalFilesDir(null), "mihome-imported.json");
        JSONObject root = new JSONObject();
        if (file.exists()) {
            try (InputStream in = new java.io.FileInputStream(file)) {
                root = new JSONObject(new String(readAll(in), StandardCharsets.UTF_8));
            }
        }
        JSONObject devices = root.optJSONObject("devices");
        if (devices == null) devices = new JSONObject();
        devices.put(device.getString("name"), device);
        root.put("devices", devices);
        try (java.io.FileOutputStream out = new java.io.FileOutputStream(file)) {
            out.write(root.toString(2).getBytes(StandardCharsets.UTF_8));
        }
    }
}
