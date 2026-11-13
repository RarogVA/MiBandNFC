package uc;

import android.os.Bundle;
import android.os.Message;
import com.ekassir.mirpay.app.ProtectedMirApplication;

/* JADX INFO: loaded from: <historical-project>/validation/decompiled/phone-native/recovered-4.dex */
public abstract class a {
    private static c.a a(int i) {
        switch (i) {
            case 0:
                return c.a.c;
            case 1:
                return c.a.f;
            case 2:
                return c.a.e;
            case 3:
                return c.a.a;
            case 4:
                return c.a.d;
            case 5:
                return c.a.h;
            case 6:
                return c.a.b;
            case 7:
                return c.a.g;
            default:
                throw new IllegalArgumentException(ProtectedMirApplication.s("๔") + i);
        }
    }

    public static c b(Message message) {
        String string;
        String string2;
        int i = message.what;
        Bundle data = message.getData();
        if (data != null) {
            string = data.getString(ProtectedMirApplication.s("๕"));
            string2 = data.getString(ProtectedMirApplication.s("๖"));
        } else {
            string = null;
            string2 = null;
        }
        return new c(a(i), string2, string);
    }
}
