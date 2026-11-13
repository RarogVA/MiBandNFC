package cb0;

import android.content.Context;
import android.content.Intent;
import java.net.URI;
import kotlin.jvm.internal.m;
import kotlin.jvm.internal.v;
import ru.nspk.wallet.ui.activity.InAppActivity;

/* JADX INFO: loaded from: <historical-project>/validation/decompiled/phone-native/recovered-3.dex */
public final class c {
    public static final a Companion = new a(null);

    /* JADX INFO: renamed from: b, reason: collision with root package name */
    public static final int f12257b = 8;

    /* JADX INFO: renamed from: c, reason: collision with root package name */
    private static final URI f12258c = URI.create("mirpay://pay.mironline.ru/");

    /* JADX INFO: renamed from: a, reason: collision with root package name */
    private final Context f12259a;

    public static final class a {
        private a() {
        }

        public /* synthetic */ a(m mVar) {
            this();
        }
    }

    public c(Context context) {
        v.g(context, "context");
        this.f12259a = context;
    }

    public final Intent a(String paymentToken) {
        v.g(paymentToken, "paymentToken");
        Intent intent = new Intent(this.f12259a, (Class<?>) InAppActivity.class);
        URI uriD = sl.a.d(sl.a.f55368a, f12258c, "/inapp/" + paymentToken, null, 4, null);
        intent.setAction("android.intent.action.VIEW");
        intent.setData(iq.a.a(uriD));
        intent.addCategory("android.intent.category.DEFAULT");
        intent.addCategory("android.intent.category.BROWSABLE");
        return intent;
    }
}
