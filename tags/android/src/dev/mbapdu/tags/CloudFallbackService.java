package dev.mbapdu.tags;

import android.accessibilityservice.AccessibilityService;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.os.Handler;
import android.os.Looper;
import android.provider.Settings;
import android.view.accessibility.AccessibilityEvent;
import android.view.accessibility.AccessibilityNodeInfo;
import java.util.Locale;

/** Experimental fallback: taps a Vevs scene, which uses Mi Home Cloud. */
public final class CloudFallbackService extends AccessibilityService {
    private static final String VEVS_PACKAGE = "com.xiaomi.smarthome";
    private static String pendingLabel;
    private static long pendingAt;
    private final Handler handler = new Handler(Looper.getMainLooper());
    private int attempts;
    private boolean showAllClicked;

    static void request(Context context, String label) {
        pendingLabel = normalize(label);
        pendingAt = System.currentTimeMillis();
        context.getSharedPreferences("cloud_fallback", MODE_PRIVATE).edit()
            .putString("label", pendingLabel).putLong("at", pendingAt).apply();
        android.util.Log.d("CloudFallbackService", "request " + label);
        Intent intent = context.getPackageManager().getLaunchIntentForPackage(VEVS_PACKAGE);
        if (intent == null) {
            intent = new Intent(Intent.ACTION_MAIN);
            intent.addCategory(Intent.CATEGORY_LAUNCHER);
            intent.setComponent(new ComponentName(VEVS_PACKAGE, VEVS_PACKAGE + ".VevsIcon"));
            android.util.Log.d("CloudFallbackService", "using explicit VevsIcon");
        }
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_CLEAR_TOP);
        try {
            context.startActivity(intent);
            android.util.Log.d("CloudFallbackService", "launched Vevs");
        } catch (Exception e) {
            android.util.Log.e("CloudFallbackService", "launch failed", e);
            android.widget.Toast.makeText(context, "Не удалось открыть Vevs: " + e.getMessage(), android.widget.Toast.LENGTH_LONG).show();
            pendingLabel = null;
        }
    }

    static boolean enabled(Context context) {
        String value = Settings.Secure.getString(context.getContentResolver(),
            Settings.Secure.ENABLED_ACCESSIBILITY_SERVICES);
        if (value == null) return false;
        String component = context.getPackageName() + "/" + CloudFallbackService.class.getName();
        String shortComponent = context.getPackageName() + "/.CloudFallbackService";
        return value.contains(component) || value.contains(shortComponent);
    }

    static String normalize(String value) {
        if (value == null) return "";
        return value.toLowerCase(Locale.ROOT)
            .replace('c', 'с')
            .replace("°", "")
            .replaceAll("\\s+", "");
    }

    @Override public void onServiceConnected() {
        super.onServiceConnected();
        android.util.Log.d("CloudFallbackService", "connected");
    }

    @Override public void onAccessibilityEvent(AccessibilityEvent event) {
        if (pendingLabel == null) {
            android.content.SharedPreferences prefs = getSharedPreferences("cloud_fallback", MODE_PRIVATE);
            long at = prefs.getLong("at", 0);
            if (System.currentTimeMillis() - at < 60000) {
                pendingLabel = prefs.getString("label", null);
                pendingAt = at;
            }
        }
        if (pendingLabel == null || System.currentTimeMillis() - pendingAt > 60000) {
            pendingLabel = null;
            return;
        }
        android.util.Log.d("CloudFallbackService", "event " + event.getPackageName() + " type " + event.getEventType());
        CharSequence packageName = event.getPackageName();
        if (packageName == null || !VEVS_PACKAGE.contentEquals(packageName)) return;
        if (attempts == 0) handler.postDelayed(this::attempt, 350);
    }

    private void attempt() {
        if (pendingLabel == null) return;
        if (System.currentTimeMillis() - pendingAt > 60000) {
            fail("Vevs не открылся вовремя");
            return;
        }
        AccessibilityNodeInfo root = getRootInActiveWindow();
        if (root == null) {
            android.util.Log.d("CloudFallbackService", "root null attempt " + attempts);
            retry();
            return;
        }
        AccessibilityNodeInfo target = find(root, pendingLabel);
        if (target != null) {
            if (click(target)) {
                String label = pendingLabel;
                pendingLabel = null;
                attempts = 0;
                showAllClicked = false;
                getSharedPreferences("cloud_fallback", MODE_PRIVATE).edit().clear().apply();
                android.widget.Toast.makeText(this, "Vevs: сцена " + label, android.widget.Toast.LENGTH_LONG).show();
                return;
            }
        }
        if (!showAllClicked) {
            AccessibilityNodeInfo showAll = find(root, normalize("Показать все"));
            if (showAll != null && click(showAll)) {
                showAllClicked = true;
                handler.postDelayed(this::attempt, 700);
                return;
            }
        }
        retry();
    }

    private void retry() {
        attempts++;
        if (attempts > 12) {
            fail("Сцена Vevs не найдена");
            return;
        }
        handler.postDelayed(this::attempt, 500);
    }

    private void fail(String message) {
        pendingLabel = null;
        attempts = 0;
        showAllClicked = false;
        getSharedPreferences("cloud_fallback", MODE_PRIVATE).edit().clear().apply();
        android.widget.Toast.makeText(this, message, android.widget.Toast.LENGTH_LONG).show();
    }

    private static AccessibilityNodeInfo find(AccessibilityNodeInfo root, String target) {
        if (root == null || target.isEmpty()) return null;
        CharSequence text = root.getText();
        CharSequence desc = root.getContentDescription();
        if ((text != null && normalize(text.toString()).contains(target))
                || (desc != null && normalize(desc.toString()).contains(target))) {
            return root;
        }
        for (int i = 0; i < root.getChildCount(); i++) {
            AccessibilityNodeInfo child = root.getChild(i);
            AccessibilityNodeInfo found = find(child, target);
            if (found != null) return found;
        }
        return null;
    }

    private static boolean click(AccessibilityNodeInfo node) {
        AccessibilityNodeInfo current = node;
        for (int i = 0; i < 8 && current != null; i++) {
            if (current.isClickable()) {
                return current.performAction(AccessibilityNodeInfo.ACTION_CLICK);
            }
            current = current.getParent();
        }
        return false;
    }

    @Override public void onInterrupt() {
        pendingLabel = null;
    }
}
