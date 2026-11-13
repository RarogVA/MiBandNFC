package com.ekassir.mirpay.app.services;

import android.app.Service;
import android.content.ComponentCallbacks;
import android.content.Context;
import android.content.Intent;
import android.os.Handler;
import android.os.IBinder;
import android.os.Message;
import android.os.Messenger;
import android.os.RemoteException;
import com.ekassir.mirpay.app.ProtectedMirApplication;
import com.ekassir.mirpay.app.services.SdkHostService;
import com.ekassir.mirpay.domainservices.correlationid.CorrelationId;
import com.ekassir.mirpay.ui.activity.AddCardGreenCorridorActivity;
import ea.b0;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.UUID;
import java.util.concurrent.CancellationException;
import java.util.concurrent.Executor;
import kotlin.jvm.internal.r0;
import kotlin.jvm.internal.v;
import kq.l0;
import kq.u;
import lq.w;
import qr.o0;
import ru.nspk.wallet.core.abstractions.error.ScenarioData;
import ru.nspk.wallet.core.abstractions.error.ScenarioId;
import ru.nspk.wallet.core.abstractions.error.SourceId$Service$SdkHost;
import ru.nspk.wallet.core.data.model.bank.BankId;
import ru.nspk.wallet.core.data.model.bank.IssuerModel;
import ru.nspk.wallet.core.data.model.bank.IssuerSettingsModel;
import ru.nspk.wallet.core.data.model.payment.PaymentCardTokenReference;
import ru.nspk.wallet.core.data.model.registration.DeviceId;
import ru.nspk.wallet.core.data.model.registration.WalletId;
import ru.nspk.wallet.core.data.model.registration.token.ExpiryDateModel;
import ru.nspk.wallet.core.data.model.registration.token.TokenResponseModel;
import ru.nspk.wallet.core.data.model.registration.token.TokenStatus;
import ru.nspk.wallet.data.model.integrity.DexEnvironmentState;
import x60.d1;
import x60.f0;
import x60.u0;
import zq.p;

/* JADX WARN: Classes with same name are omitted, all sources:
  <historical-project>/validation/decompiled/phone-native/recovered-1.dex
  <historical-project>/validation/decompiled/phone-native/recovered-5.dex
 */
/* JADX INFO: loaded from: <historical-project>/validation/decompiled/phone-native/recovered-1.dex */
public final class SdkHostService extends Service {
    public static final a Companion = new a(null);

    /* JADX INFO: renamed from: n, reason: collision with root package name */
    public static final int f17911n = 8;

    /* JADX INFO: renamed from: o, reason: collision with root package name */
    private static final String f17912o = SdkHostService.class.getSimpleName();

    /* JADX INFO: renamed from: a, reason: collision with root package name */
    private final b0 f17913a = new b0();

    /* JADX INFO: renamed from: b, reason: collision with root package name */
    private final Messenger f17914b = new Messenger(new b(new f()));

    /* JADX INFO: renamed from: c, reason: collision with root package name */
    private final Object f17915c = new Object();

    /* JADX INFO: renamed from: d, reason: collision with root package name */
    private final Executor f17916d;

    /* JADX INFO: renamed from: e, reason: collision with root package name */
    private final HashMap f17917e;

    /* JADX INFO: renamed from: f, reason: collision with root package name */
    private final kq.l f17918f;

    /* JADX INFO: renamed from: g, reason: collision with root package name */
    private final kq.l f17919g;

    /* JADX INFO: renamed from: h, reason: collision with root package name */
    private final kq.l f17920h;

    /* JADX INFO: renamed from: i, reason: collision with root package name */
    private final kq.l f17921i;

    /* JADX INFO: renamed from: j, reason: collision with root package name */
    private final kq.l f17922j;

    /* JADX INFO: renamed from: k, reason: collision with root package name */
    private final kq.l f17923k;

    /* JADX INFO: renamed from: l, reason: collision with root package name */
    private final kq.l f17924l;

    /* JADX INFO: renamed from: m, reason: collision with root package name */
    private final kq.l f17925m;

    public static final class a {
        private a() {
        }

        public /* synthetic */ a(kotlin.jvm.internal.m mVar) {
            this();
        }
    }

    private static final class b extends Handler {

        /* JADX INFO: renamed from: a, reason: collision with root package name */
        private final c f17926a;

        public b(c cVar) {
            v.g(cVar, ProtectedMirApplication.s("Ŋ"));
            this.f17926a = cVar;
        }

        @Override // android.os.Handler
        public void handleMessage(Message message) {
            v.g(message, ProtectedMirApplication.s("ŋ"));
            this.f17926a.a(message);
        }
    }

    private interface c {
        void a(Message message);
    }

    public /* synthetic */ class d {

        /* JADX INFO: renamed from: a, reason: collision with root package name */
        public static final /* synthetic */ int[] f17927a;

        /* JADX INFO: renamed from: b, reason: collision with root package name */
        public static final /* synthetic */ int[] f17928b;

        static {
            int[] iArr = new int[uc.c.a.values().length];
            try {
                iArr[uc.c.a.f57555f.ordinal()] = 1;
            } catch (NoSuchFieldError unused) {
            }
            try {
                iArr[uc.c.a.f57554e.ordinal()] = 2;
            } catch (NoSuchFieldError unused2) {
            }
            try {
                iArr[uc.c.a.f57550a.ordinal()] = 3;
            } catch (NoSuchFieldError unused3) {
            }
            f17927a = iArr;
            int[] iArr2 = new int[TokenStatus.values().length];
            try {
                iArr2[TokenStatus.ACTIVE.ordinal()] = 1;
            } catch (NoSuchFieldError unused4) {
            }
            try {
                iArr2[TokenStatus.PENDING.ordinal()] = 2;
            } catch (NoSuchFieldError unused5) {
            }
            try {
                iArr2[TokenStatus.DISPOSED.ordinal()] = 3;
            } catch (NoSuchFieldError unused6) {
            }
            try {
                iArr2[TokenStatus.SUSPENDED.ordinal()] = 4;
            } catch (NoSuchFieldError unused7) {
            }
            f17928b = iArr2;
        }
    }

    static final class e extends kotlin.coroutines.jvm.internal.l implements p {

        /* JADX INFO: renamed from: a, reason: collision with root package name */
        int f17929a;

        /* JADX INFO: renamed from: c, reason: collision with root package name */
        final /* synthetic */ CorrelationId f17931c;

        /* JADX INFO: renamed from: d, reason: collision with root package name */
        final /* synthetic */ BankId f17932d;

        /* JADX INFO: renamed from: e, reason: collision with root package name */
        final /* synthetic */ g9.d f17933e;

        /* JADX WARN: 'super' call moved to the top of the method (can break code semantics) */
        e(CorrelationId correlationId, BankId bankId, g9.d dVar, pq.e eVar) {
            super(2, eVar);
            this.f17931c = correlationId;
            this.f17932d = bankId;
            this.f17933e = dVar;
        }

        @Override // zq.p
        /* JADX INFO: renamed from: a, reason: merged with bridge method [inline-methods] */
        public final Object invoke(o0 o0Var, pq.e eVar) {
            return ((e) create(o0Var, eVar)).invokeSuspend(l0.f39294a);
        }

        @Override // kotlin.coroutines.jvm.internal.a
        public final pq.e create(Object obj, pq.e eVar) {
            return SdkHostService.this.new e(this.f17931c, this.f17932d, this.f17933e, eVar);
        }

        @Override // kotlin.coroutines.jvm.internal.a
        public final Object invokeSuspend(Object obj) {
            Object objA;
            Object next;
            Object objF = qq.b.f();
            int i11 = this.f17929a;
            if (i11 == 0) {
                kq.v.b(obj);
                ScenarioData scenarioData = new ScenarioData(SourceId$Service$SdkHost.INSTANCE, ScenarioId.b.f50954b);
                z40.a aVarN = SdkHostService.this.n();
                CorrelationId correlationId = this.f17931c;
                this.f17929a = 1;
                objA = aVarN.a(scenarioData, correlationId, this);
                if (objA == objF) {
                    return objF;
                }
            } else {
                if (i11 != 1) {
                    throw new IllegalStateException(ProtectedMirApplication.s("Ō"));
                }
                kq.v.b(obj);
                objA = ((u) obj).j();
            }
            BankId bankId = this.f17932d;
            g9.d dVar = this.f17933e;
            Throwable thE = u.e(objA);
            if (thE != null) {
                if (thE instanceof CancellationException) {
                    sk.d.b(dVar.b(), ProtectedMirApplication.s("ō"));
                } else {
                    sk.d.b(dVar.b(), ProtectedMirApplication.s("Ŏ"));
                }
                return kotlin.coroutines.jvm.internal.b.a(false);
            }
            Iterator<T> it = ((IssuerSettingsModel) objA).getIssuers().iterator();
            do {
                if (!it.hasNext()) {
                    next = null;
                    break;
                }
                next = it.next();
            } while (!v.b(((IssuerModel) next).getBankId(), bankId));
            return kotlin.coroutines.jvm.internal.b.a(next != null);
        }
    }

    public static final class f implements c {
        f() {
        }

        @Override // com.ekassir.mirpay.app.services.SdkHostService.c
        public void a(Message message) {
            v.g(message, ProtectedMirApplication.s("ŏ"));
            SdkHostService.this.q(message);
        }
    }

    public static final class g implements zq.a {

        /* JADX INFO: renamed from: a, reason: collision with root package name */
        final /* synthetic */ ComponentCallbacks f17935a;

        /* JADX INFO: renamed from: b, reason: collision with root package name */
        final /* synthetic */ kv.a f17936b;

        /* JADX INFO: renamed from: c, reason: collision with root package name */
        final /* synthetic */ zq.a f17937c;

        public g(ComponentCallbacks componentCallbacks, kv.a aVar, zq.a aVar2) {
            this.f17935a = componentCallbacks;
            this.f17936b = aVar;
            this.f17937c = aVar2;
        }

        @Override // zq.a
        public final Object invoke() {
            ComponentCallbacks componentCallbacks = this.f17935a;
            return wu.a.a(componentCallbacks).f(r0.b(la.d.class), this.f17936b, this.f17937c);
        }
    }

    public static final class h implements zq.a {

        /* JADX INFO: renamed from: a, reason: collision with root package name */
        final /* synthetic */ ComponentCallbacks f17938a;

        /* JADX INFO: renamed from: b, reason: collision with root package name */
        final /* synthetic */ kv.a f17939b;

        /* JADX INFO: renamed from: c, reason: collision with root package name */
        final /* synthetic */ zq.a f17940c;

        public h(ComponentCallbacks componentCallbacks, kv.a aVar, zq.a aVar2) {
            this.f17938a = componentCallbacks;
            this.f17939b = aVar;
            this.f17940c = aVar2;
        }

        @Override // zq.a
        public final Object invoke() {
            ComponentCallbacks componentCallbacks = this.f17938a;
            return wu.a.a(componentCallbacks).f(r0.b(z40.a.class), this.f17939b, this.f17940c);
        }
    }

    public static final class i implements zq.a {

        /* JADX INFO: renamed from: a, reason: collision with root package name */
        final /* synthetic */ ComponentCallbacks f17941a;

        /* JADX INFO: renamed from: b, reason: collision with root package name */
        final /* synthetic */ kv.a f17942b;

        /* JADX INFO: renamed from: c, reason: collision with root package name */
        final /* synthetic */ zq.a f17943c;

        public i(ComponentCallbacks componentCallbacks, kv.a aVar, zq.a aVar2) {
            this.f17941a = componentCallbacks;
            this.f17942b = aVar;
            this.f17943c = aVar2;
        }

        @Override // zq.a
        public final Object invoke() {
            ComponentCallbacks componentCallbacks = this.f17941a;
            return wu.a.a(componentCallbacks).f(r0.b(pa.c.class), this.f17942b, this.f17943c);
        }
    }

    public static final class j implements zq.a {

        /* JADX INFO: renamed from: a, reason: collision with root package name */
        final /* synthetic */ ComponentCallbacks f17944a;

        /* JADX INFO: renamed from: b, reason: collision with root package name */
        final /* synthetic */ kv.a f17945b;

        /* JADX INFO: renamed from: c, reason: collision with root package name */
        final /* synthetic */ zq.a f17946c;

        public j(ComponentCallbacks componentCallbacks, kv.a aVar, zq.a aVar2) {
            this.f17944a = componentCallbacks;
            this.f17945b = aVar;
            this.f17946c = aVar2;
        }

        @Override // zq.a
        public final Object invoke() {
            ComponentCallbacks componentCallbacks = this.f17944a;
            return wu.a.a(componentCallbacks).f(r0.b(x60.p.class), this.f17945b, this.f17946c);
        }
    }

    public static final class k implements zq.a {

        /* JADX INFO: renamed from: a, reason: collision with root package name */
        final /* synthetic */ ComponentCallbacks f17947a;

        /* JADX INFO: renamed from: b, reason: collision with root package name */
        final /* synthetic */ kv.a f17948b;

        /* JADX INFO: renamed from: c, reason: collision with root package name */
        final /* synthetic */ zq.a f17949c;

        public k(ComponentCallbacks componentCallbacks, kv.a aVar, zq.a aVar2) {
            this.f17947a = componentCallbacks;
            this.f17948b = aVar;
            this.f17949c = aVar2;
        }

        @Override // zq.a
        public final Object invoke() {
            ComponentCallbacks componentCallbacks = this.f17947a;
            return wu.a.a(componentCallbacks).f(r0.b(a40.b.class), this.f17948b, this.f17949c);
        }
    }

    public static final class l implements zq.a {

        /* JADX INFO: renamed from: a, reason: collision with root package name */
        final /* synthetic */ ComponentCallbacks f17950a;

        /* JADX INFO: renamed from: b, reason: collision with root package name */
        final /* synthetic */ kv.a f17951b;

        /* JADX INFO: renamed from: c, reason: collision with root package name */
        final /* synthetic */ zq.a f17952c;

        public l(ComponentCallbacks componentCallbacks, kv.a aVar, zq.a aVar2) {
            this.f17950a = componentCallbacks;
            this.f17951b = aVar;
            this.f17952c = aVar2;
        }

        @Override // zq.a
        public final Object invoke() {
            ComponentCallbacks componentCallbacks = this.f17950a;
            return wu.a.a(componentCallbacks).f(r0.b(f0.class), this.f17951b, this.f17952c);
        }
    }

    public static final class m implements zq.a {

        /* JADX INFO: renamed from: a, reason: collision with root package name */
        final /* synthetic */ ComponentCallbacks f17953a;

        /* JADX INFO: renamed from: b, reason: collision with root package name */
        final /* synthetic */ kv.a f17954b;

        /* JADX INFO: renamed from: c, reason: collision with root package name */
        final /* synthetic */ zq.a f17955c;

        public m(ComponentCallbacks componentCallbacks, kv.a aVar, zq.a aVar2) {
            this.f17953a = componentCallbacks;
            this.f17954b = aVar;
            this.f17955c = aVar2;
        }

        @Override // zq.a
        public final Object invoke() {
            ComponentCallbacks componentCallbacks = this.f17953a;
            return wu.a.a(componentCallbacks).f(r0.b(u0.class), this.f17954b, this.f17955c);
        }
    }

    public static final class n implements zq.a {

        /* JADX INFO: renamed from: a, reason: collision with root package name */
        final /* synthetic */ ComponentCallbacks f17956a;

        /* JADX INFO: renamed from: b, reason: collision with root package name */
        final /* synthetic */ kv.a f17957b;

        /* JADX INFO: renamed from: c, reason: collision with root package name */
        final /* synthetic */ zq.a f17958c;

        public n(ComponentCallbacks componentCallbacks, kv.a aVar, zq.a aVar2) {
            this.f17956a = componentCallbacks;
            this.f17957b = aVar;
            this.f17958c = aVar2;
        }

        @Override // zq.a
        public final Object invoke() {
            ComponentCallbacks componentCallbacks = this.f17956a;
            return wu.a.a(componentCallbacks).f(r0.b(d1.class), this.f17957b, this.f17958c);
        }
    }

    public SdkHostService() {
        ok.d dVarA = ok.d.a();
        v.f(dVarA, ProtectedMirApplication.s("Ő"));
        this.f17916d = dVarA;
        this.f17917e = new HashMap();
        kq.p pVar = kq.p.SYNCHRONIZED;
        this.f17918f = kq.m.a(pVar, new g(this, null, null));
        this.f17919g = kq.m.a(pVar, new h(this, null, null));
        this.f17920h = kq.m.a(pVar, new i(this, null, null));
        this.f17921i = kq.m.a(pVar, new j(this, null, null));
        this.f17922j = kq.m.a(pVar, new k(this, null, null));
        this.f17923k = kq.m.a(pVar, new l(this, null, null));
        this.f17924l = kq.m.a(pVar, new m(this, null, null));
        this.f17925m = kq.m.a(pVar, new n(this, null, null));
    }

    private final tc.b A(TokenStatus tokenStatus) {
        int i11 = d.f17928b[tokenStatus.ordinal()];
        if (i11 == 1) {
            return tc.b.f56536b;
        }
        if (i11 == 2) {
            return tc.b.f56535a;
        }
        if (i11 == 3) {
            return tc.b.f56538d;
        }
        if (i11 != 4) {
            return null;
        }
        return tc.b.f56537c;
    }

    private final boolean d(TokenResponseModel tokenResponseModel, BankId bankId) {
        BankId bankIdQ1 = o().q1(tokenResponseModel.getTokenReference());
        return bankIdQ1 != null && v.b(bankIdQ1, bankId);
    }

    private final boolean e(BankId bankId, g9.d dVar) {
        g9.d dVar2;
        try {
            dVar2 = dVar;
            try {
                return ((Boolean) qr.j.b(null, new e(j().a(la.a.f39934d, 1, null), bankId, dVar2, null), 1, null)).booleanValue();
            } catch (Throwable unused) {
                sk.d.b(dVar2.b(), ProtectedMirApplication.s("ő"));
                return false;
            }
        } catch (Throwable unused2) {
            dVar2 = dVar;
        }
    }

    private final String f(ExpiryDateModel expiryDateModel) {
        String string = getString(com.ekassir.mirpay.app.j.r.X5, Integer.valueOf(expiryDateModel.getMonth()), Integer.valueOf(expiryDateModel.getYear()));
        v.f(string, ProtectedMirApplication.s("Œ"));
        return string;
    }

    private final List g(BankId bankId, g9.d dVar) {
        sk.d.a(dVar.b(), ProtectedMirApplication.s("œ") + bankId);
        List listX1 = i().X1();
        ArrayList arrayList = new ArrayList();
        for (Object obj : listX1) {
            if (d((TokenResponseModel) obj, bankId)) {
                arrayList.add(obj);
            }
        }
        ArrayList arrayList2 = new ArrayList(w.y(arrayList, 10));
        int size = arrayList.size();
        int i11 = 0;
        while (i11 < size) {
            Object obj2 = arrayList.get(i11);
            i11++;
            arrayList2.add(z((TokenResponseModel) obj2));
        }
        return arrayList2;
    }

    private final tc.a h(BankId bankId, String str, g9.d dVar) {
        TokenResponseModel tokenResponseModelR2 = i().R2(new PaymentCardTokenReference(str));
        if (tokenResponseModelR2 == null) {
            return null;
        }
        if (d(tokenResponseModelR2, bankId)) {
            return z(tokenResponseModelR2);
        }
        sk.d.m(dVar.b(), ProtectedMirApplication.s("Ŕ") + bankId + ProtectedMirApplication.s("ŕ") + str + ProtectedMirApplication.s("Ŗ"));
        return null;
    }

    private final x60.p i() {
        return (x60.p) this.f17921i.getValue();
    }

    private final la.d j() {
        return (la.d) this.f17918f.getValue();
    }

    private final a40.b k() {
        return (a40.b) this.f17922j.getValue();
    }

    private final pa.c l() {
        return (pa.c) this.f17920h.getValue();
    }

    private final f0 m() {
        return (f0) this.f17923k.getValue();
    }

    /* JADX INFO: Access modifiers changed from: private */
    public final z40.a n() {
        return (z40.a) this.f17919g.getValue();
    }

    private final u0 o() {
        return (u0) this.f17924l.getValue();
    }

    private final d1 p() {
        return (d1) this.f17925m.getValue();
    }

    /* JADX INFO: Access modifiers changed from: private */
    public final void q(Message message) {
        try {
            this.f17913a.u(message);
        } catch (UnsupportedOperationException unused) {
            r(message);
        }
    }

    private final void r(Message message) {
        final g9.d dVarE = g9.a.f32987a.e(this);
        sk.d.a(dVarE.b(), ProtectedMirApplication.s("ŗ"));
        final uc.c cVarB = uc.a.b(message);
        final Messenger messenger = message.replyTo;
        final int i11 = message.arg1;
        if (cVarB.a() != null) {
            this.f17916d.execute(new Runnable() { // from class: ea.z
                @Override // java.lang.Runnable
                public final void run() {
                    SdkHostService.s(cVarB, dVarE, this, messenger, i11);
                }
            });
            return;
        }
        sk.d.m(dVarE.b(), ProtectedMirApplication.s("Ř"));
        v.d(messenger);
        u(messenger, i11, uc.d.f57565g);
    }

    /* JADX INFO: Access modifiers changed from: private */
    public static final void s(uc.c cVar, g9.d dVar, SdkHostService sdkHostService, Messenger messenger, int i11) {
        uc.c.a aVarA = cVar.a();
        uc.c.a aVar = uc.c.a.f57552c;
        String strS = ProtectedMirApplication.s("ř");
        if (aVarA == aVar) {
            sk.d.a(dVar.b(), ProtectedMirApplication.s("Ś"));
            v.d(messenger);
            String strB = cVar.b();
            v.f(strB, strS);
            sdkHostService.y(messenger, i11, strB, dVar);
            return;
        }
        if (cVar.c() == null) {
            sk.d.m(dVar.b(), ProtectedMirApplication.s("ś") + cVar.a() + ProtectedMirApplication.s("Ŝ"));
            v.d(messenger);
            sdkHostService.u(messenger, i11, uc.d.f57566h);
            return;
        }
        BankId bankId = (BankId) sdkHostService.f17917e.get(cVar.c());
        if (bankId == null) {
            sk.d.m(dVar.b(), ProtectedMirApplication.s("ŝ") + cVar.c());
            v.d(messenger);
            sdkHostService.u(messenger, i11, uc.d.f57566h);
            return;
        }
        uc.c.a aVarA2 = cVar.a();
        int i12 = aVarA2 == null ? -1 : d.f17927a[aVarA2.ordinal()];
        if (i12 == 1) {
            v.d(messenger);
            sdkHostService.x(messenger, i11, bankId, dVar);
            return;
        }
        if (i12 == 2) {
            v.d(messenger);
            String strB2 = cVar.b();
            v.f(strB2, strS);
            sdkHostService.w(messenger, i11, bankId, strB2, dVar);
            return;
        }
        if (i12 != 3) {
            v.d(messenger);
            sdkHostService.u(messenger, i11, uc.d.f57565g);
        } else {
            v.d(messenger);
            String strB3 = cVar.b();
            v.f(strB3, strS);
            sdkHostService.v(messenger, i11, bankId, strB3, dVar);
        }
    }

    private final void t(Messenger messenger, int i11, Message message) {
        synchronized (this.f17915c) {
            try {
                message.arg1 = i11;
                messenger.send(message);
            } catch (RemoteException e11) {
                sk.d.d(f17912o, e11);
            }
            l0 l0Var = l0.f39294a;
        }
    }

    private final void u(Messenger messenger, int i11, uc.d dVar) {
        Message messageB = uc.b.b(dVar);
        v.d(messageB);
        t(messenger, i11, messageB);
    }

    private final void v(Messenger messenger, int i11, BankId bankId, String str, g9.d dVar) {
        try {
            vc.a aVarA = wc.h.a(str);
            AddCardGreenCorridorActivity.b bVar = AddCardGreenCorridorActivity.Companion;
            Context applicationContext = getApplicationContext();
            v.f(applicationContext, ProtectedMirApplication.s("Ş"));
            String strA = aVarA.a();
            v.f(strA, ProtectedMirApplication.s("ş"));
            Message messageC = uc.b.c(bVar.a(applicationContext, bankId, strA));
            v.d(messageC);
            t(messenger, i11, messageC);
        } catch (vc.e unused) {
            sk.d.b(dVar.b(), ProtectedMirApplication.s("Š") + str);
            u(messenger, i11, uc.d.f57567i);
        }
    }

    private final void w(Messenger messenger, int i11, BankId bankId, String str, g9.d dVar) {
        try {
            String strA = wc.h.b(str).a();
            v.f(strA, ProtectedMirApplication.s("š"));
            tc.a aVarH = h(bankId, strA, dVar);
            if (aVarH == null) {
                u(messenger, i11, uc.d.f57569k);
                return;
            }
            try {
                Message messageD = uc.b.d(wc.h.d(aVarH));
                v.d(messageD);
                t(messenger, i11, messageD);
            } catch (vc.e unused) {
                sk.d.b(dVar.b(), ProtectedMirApplication.s("Ţ") + str);
                u(messenger, i11, uc.d.f57567i);
            }
        } catch (vc.e unused2) {
            sk.d.b(dVar.b(), ProtectedMirApplication.s("ţ") + str);
            u(messenger, i11, uc.d.f57567i);
        }
    }

    private final void x(Messenger messenger, int i11, BankId bankId, g9.d dVar) {
        List listG = g(bankId, dVar);
        if (listG.isEmpty()) {
            u(messenger, i11, uc.d.f57569k);
            return;
        }
        try {
            Message messageD = uc.b.d(wc.h.e(listG));
            v.d(messageD);
            t(messenger, i11, messageD);
        } catch (vc.e unused) {
            sk.d.b(dVar.b(), ProtectedMirApplication.s("Ť") + listG);
            u(messenger, i11, uc.d.f57567i);
        }
    }

    private final void y(Messenger messenger, int i11, String str, g9.d dVar) {
        sk.d.e(dVar.b(), ProtectedMirApplication.s("ť"));
        try {
            vc.c cVarC = wc.h.c(str);
            sk.d.e(dVar.b(), ProtectedMirApplication.s("Ŧ") + cVarC);
            sk.d.e(dVar.b(), ProtectedMirApplication.s("ŧ"));
            if (1 > cVarC.b()) {
                sk.d.m(dVar.b(), ProtectedMirApplication.s("Ũ"));
                u(messenger, i11, uc.d.f57564f);
                return;
            }
            if (1 < cVarC.b()) {
                sk.d.m(dVar.b(), ProtectedMirApplication.s("ũ"));
                u(messenger, i11, uc.d.f57565g);
                return;
            }
            sk.d.e(dVar.b(), ProtectedMirApplication.s("Ū"));
            if (!k().F0()) {
                sk.d.m(dVar.b(), ProtectedMirApplication.s("ū"));
                u(messenger, i11, uc.d.f57561c);
                return;
            }
            sk.d.e(dVar.b(), ProtectedMirApplication.s("Ŭ"));
            if (p().r1()) {
                sk.d.m(dVar.b(), ProtectedMirApplication.s("ŭ"));
                u(messenger, i11, uc.d.f57562d);
            }
            sk.d.e(dVar.b(), ProtectedMirApplication.s("Ů"));
            DexEnvironmentState dexEnvironmentStateC = l().c(false);
            boolean zB = l().b();
            if (dexEnvironmentStateC.g() || !zB) {
                sk.d.m(dVar.b(), ProtectedMirApplication.s("Ŷ"));
                u(messenger, i11, uc.d.f57563e);
                return;
            }
            String strA = cVarC.a();
            v.f(strA, ProtectedMirApplication.s("ů"));
            BankId bankId = new BankId(strA);
            sk.d.e(dVar.b(), ProtectedMirApplication.s("Ű") + bankId + ProtectedMirApplication.s("ű"));
            if (!e(bankId, dVar)) {
                sk.d.m(dVar.b(), ProtectedMirApplication.s("Ų"));
                u(messenger, i11, uc.d.f57566h);
                return;
            }
            sk.d.e(dVar.b(), ProtectedMirApplication.s("ų"));
            UUID uuidRandomUUID = UUID.randomUUID();
            WalletId walletIdJ = m().J();
            String rawValue = walletIdJ != null ? walletIdJ.getRawValue() : null;
            DeviceId deviceIdG0 = m().G0();
            vc.d dVar2 = new vc.d(uuidRandomUUID.toString(), rawValue, deviceIdG0 != null ? deviceIdG0.getRawValue() : null);
            this.f17917e.put(uuidRandomUUID.toString(), bankId);
            try {
                Message messageD = uc.b.d(wc.h.f(dVar2));
                v.f(messageD, ProtectedMirApplication.s("Ŵ"));
                t(messenger, i11, messageD);
            } catch (vc.e unused) {
                sk.d.b(dVar.b(), ProtectedMirApplication.s("ŵ") + str);
                u(messenger, i11, uc.d.f57567i);
            }
        } catch (vc.e unused2) {
            sk.d.b(dVar.b(), ProtectedMirApplication.s("ŷ") + str);
            u(messenger, i11, uc.d.f57567i);
        }
    }

    private final tc.a z(TokenResponseModel tokenResponseModel) {
        return new tc.a(tokenResponseModel.getTokenReference().getRawValue(), tokenResponseModel.getCard().getSuffix(), f(tokenResponseModel.getCard().getExpiry()), A(tokenResponseModel.getStatus()));
    }

    @Override // android.app.Service
    public IBinder onBind(Intent intent) {
        return this.f17914b.getBinder();
    }
}
