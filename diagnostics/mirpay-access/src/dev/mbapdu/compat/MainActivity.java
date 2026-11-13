package dev.mbapdu.compat;

import android.app.Activity;
import android.content.ComponentName;
import android.content.Intent;
import android.content.ServiceConnection;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.PermissionInfo;
import android.content.pm.ResolveInfo;
import android.content.pm.ServiceInfo;
import android.content.pm.Signature;
import android.graphics.Insets;
import android.os.Bundle;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.os.SystemClock;
import android.util.Log;
import android.view.WindowInsets;
import android.widget.ScrollView;
import android.widget.TextView;
import java.io.FileOutputStream;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.util.ArrayList;
import java.util.List;
import org.json.JSONArray;
import org.json.JSONObject;

/** Ordinary-app access check. Never invokes or sends data through the binder. */
public final class MainActivity extends Activity {
    private static final String TARGET = "ru.nspk.mirpay";
    private static final String ACTION = "android.nfc.cardemulation.action.HOST_APDU_SERVICE";
    private static final String PERMISSION = "android.permission.BIND_NFC_SERVICE";
    private static final String TAG = "MBAPDU_COMPAT";
    private final Handler main = new Handler(Looper.getMainLooper());
    private final JSONObject report = new JSONObject();
    private TextView output;
    private boolean bindingAccepted;
    private boolean finished;
    private final Runnable timeout = () -> complete("timeout_no_connection");
    private final ServiceConnection connection = new ServiceConnection() {
        @Override public void onServiceConnected(ComponentName name, IBinder binder) {
            // Do not inspect the binder or invoke any transaction on it.
            if (finished) return;
            put("connected_component", name.flattenToString());
            complete("connected_without_invoking_service");
        }
        @Override public void onServiceDisconnected(ComponentName name) {
            if (!finished) complete("disconnected_before_probe_completed");
        }
        @Override public void onBindingDied(ComponentName name) {
            if (!finished) complete("binding_died");
        }
        @Override public void onNullBinding(ComponentName name) {
            if (!finished) complete("null_binding");
        }
    };

    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        output = new TextView(this);
        output.setTextSize(14);
        output.setTextIsSelectable(true);
        int pad = (int) (16 * getResources().getDisplayMetrics().density);
        output.setPadding(pad, pad, pad, pad);
        output.setOnApplyWindowInsetsListener((v, insets) -> {
            Insets i = insets.getInsets(WindowInsets.Type.systemBars());
            output.setPadding(pad + i.left, pad + i.top, pad + i.right, pad + i.bottom);
            return insets;
        });
        ScrollView page = new ScrollView(this);
        page.addView(output);
        setContentView(page);
        put("schema", 1);
        put("probe_package", getPackageName());
        put("probe_uid", android.os.Process.myUid());
        put("probe_version", "0.1");
        put("target_package", TARGET);
        put("elapsed_realtime_ms", SystemClock.elapsedRealtime());
        put("scope", "Public package metadata and ordinary explicit HCE bind only; no APDUs, no binder transactions, no card queries, no NFC/Bluetooth/network access");
        put("payment_commands_sent", false);
        main.post(this::probe);
    }

    private void put(String key, Object value) {
        try { report.put(key, value == null ? JSONObject.NULL : value); }
        catch (org.json.JSONException e) { throw new IllegalStateException(e); }
    }

    private static String hex(byte[] bytes) {
        StringBuilder text = new StringBuilder(bytes.length * 2);
        for (byte b : bytes) text.append(String.format(java.util.Locale.ROOT, "%02x", b & 255));
        return text.toString();
    }

    private void probe() {
        if (finished || isFinishing()) return;
        PackageManager pm = getPackageManager();
        put("caller_bind_nfc_permission", pm.checkPermission(PERMISSION, getPackageName()));
        try {
            put("probe_stage", "permission_definition");
            PermissionInfo permission = pm.getPermissionInfo(PERMISSION, 0);
            JSONObject permissionDefinition = new JSONObject();
            permissionDefinition.put("name", permission.name);
            permissionDefinition.put("owner_package", permission.packageName);
            permissionDefinition.put("protection_level", permission.getProtection() | permission.getProtectionFlags());
            permissionDefinition.put("protection_base", permission.getProtection());
            permissionDefinition.put("protection_flags", permission.getProtectionFlags());
            permissionDefinition.put("signature_protection", permission.getProtection() == PermissionInfo.PROTECTION_SIGNATURE);
            put("bind_nfc_permission_definition", permissionDefinition);
            put("probe_stage", "caller_package_metadata");
            PackageInfo own = pm.getPackageInfo(getPackageName(), PackageManager.PackageInfoFlags.of(PackageManager.GET_PERMISSIONS));
            JSONArray permissions = new JSONArray();
            if (own.requestedPermissions != null)
                for (String requested : own.requestedPermissions) permissions.put(requested);
            put("caller_requested_permissions", permissions);
            put("probe_stage", "target_package_metadata");
            PackageInfo target = pm.getPackageInfo(TARGET,
                    PackageManager.PackageInfoFlags.of(PackageManager.GET_SERVICES | PackageManager.GET_SIGNING_CERTIFICATES));
            put("target_metadata_result", "visible");
            put("target_version_name", target.versionName);
            put("target_version_code", target.getLongVersionCode());
            put("target_application_enabled", target.applicationInfo != null && target.applicationInfo.enabled);
            JSONArray signatures = new JSONArray();
            if (target.signingInfo != null) {
                for (Signature signature : target.signingInfo.getApkContentsSigners())
                    signatures.put(hex(MessageDigest.getInstance("SHA-256").digest(signature.toByteArray())));
            }
            put("target_signer_sha256", signatures);
            put("probe_stage", "hce_service_query");
            List<ResolveInfo> found = pm.queryIntentServices(new Intent(ACTION).setPackage(TARGET),
                    PackageManager.ResolveInfoFlags.of(PackageManager.GET_META_DATA));
            List<ServiceInfo> candidates = new ArrayList<>();
            JSONArray services = new JSONArray();
            for (ResolveInfo item : found) {
                ServiceInfo info = item.serviceInfo;
                if (info == null || !TARGET.equals(info.packageName)) continue;
                JSONObject service = new JSONObject();
                service.put("component", new ComponentName(info.packageName, info.name).flattenToString());
                service.put("permission", info.permission == null ? JSONObject.NULL : info.permission);
                service.put("exported", info.exported);
                service.put("enabled", info.enabled);
                services.put(service);
                candidates.add(info);
            }
            put("query_action", ACTION);
            put("hce_candidates", services);
            if (candidates.isEmpty()) { complete("no_resolved_hce_service"); return; }
            if (candidates.size() != 1) { complete("ambiguous_hce_services_no_bind_attempt"); return; }
            ServiceInfo service = candidates.get(0);
            if (!service.exported || !service.enabled) {
                complete("hce_not_exported_or_not_enabled_no_bind_attempt"); return;
            }
            ComponentName component = new ComponentName(service.packageName, service.name);
            put("probe_stage", "explicit_hce_bind");
            put("bind_component", component.flattenToString());
            put("bind_attempted", true);
            try {
                bindingAccepted = bindService(new Intent(ACTION).setComponent(component), connection, BIND_AUTO_CREATE);
                put("bind_returned", bindingAccepted);
                if (!bindingAccepted) { complete("bind_returned_false"); return; }
                put("result", "binding_accepted_waiting_callback");
                persist();
                main.postDelayed(timeout, 3000);
            } catch (SecurityException e) {
                put("exception_class", e.getClass().getName());
                put("exception_message", e.getMessage());
                complete("bind_security_exception");
            }
        } catch (PackageManager.NameNotFoundException e) {
            if ("target_package_metadata".equals(report.optString("probe_stage")))
                put("target_metadata_result", "name_not_found_or_not_visible");
            put("exception_class", e.getClass().getName());
            complete("metadata_name_not_found");
        } catch (Exception e) {
            put("exception_class", e.getClass().getName());
            put("exception_message", e.getMessage());
            complete("probe_error");
        }
    }

    private void complete(String result) {
        if (finished) return;
        finished = true;
        main.removeCallbacks(timeout);
        put("result", result);
        if (bindingAccepted) {
            bindingAccepted = false;
            try { unbindService(connection); put("unbind_result", "unbound"); }
            catch (RuntimeException e) { put("unbind_result", e.getClass().getSimpleName()); }
        }
        put("completed", true);
        persist();
    }

    private void persist() {
        try {
            String text = report.toString(2);
            try (FileOutputStream stream = openFileOutput("report.json", MODE_PRIVATE)) {
                stream.write((text + "\n").getBytes(StandardCharsets.UTF_8));
            }
            output.setText("Проверка доступа Mir Pay\n\nТолько проверка разрешений и подключения к HCE.\nAPDU и платёжные команды не отправляются.\n\n" + text);
            Log.i(TAG, "result=" + report.optString("result", "metadata") +
                    " completed=" + finished + " report=files/report.json");
        } catch (Exception e) {
            output.setText("Не удалось сохранить отчёт: " + e.getClass().getSimpleName());
            Log.e(TAG, "report_write_failed " + e.getClass().getSimpleName());
        }
    }

    @Override public void onDestroy() {
        if (!finished) complete("activity_destroyed_before_completion");
        main.removeCallbacks(timeout);
        super.onDestroy();
    }
}
