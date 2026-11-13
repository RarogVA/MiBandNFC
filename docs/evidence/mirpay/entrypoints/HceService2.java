package com.ekassir.mirpay.app.services;

import android.app.KeyguardManager;
import android.content.ComponentCallbacks;
import android.content.Intent;
import android.nfc.cardemulation.HostApduService;
import android.os.Bundle;
import com.ekassir.mirpay.app.ProtectedMirApplication;
import com.ekassir.mirpay.app.services.HceService2;
import com.ekassir.mirpay.app.worker.tuk.TukEraserWorker;
import com.ekassir.mirpay.app.worker.tuk.TukReplenishmentDispatcherWorker;
import com.ekassir.mirpay.nfc.cardemulation.applets.AppletAidData;
import com.ekassir.mirpay.ui.activity.PaymentActivity;
import java.security.KeyStoreException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Iterator;
import java.util.List;
import java.util.Locale;
import java.util.concurrent.CancellationException;
import kotlin.jvm.internal.i0;
import kotlin.jvm.internal.r0;
import kotlin.jvm.internal.v0;
import qr.b2;
import qr.e1;
import qr.e2;
import qr.l0;
import qr.o0;
import qr.p0;
import qr.x2;
import ru.nspk.wallet.core.abstractions.error.ScenarioData;
import ru.nspk.wallet.core.abstractions.error.ScenarioId;
import ru.nspk.wallet.core.abstractions.error.SourceId;
import ru.nspk.wallet.core.data.model.access.AccessCardModel;
import ru.nspk.wallet.core.data.model.configuration.NoCvmConfigModel;
import ru.nspk.wallet.core.data.model.configuration.NoCvmLimitsModel;
import ru.nspk.wallet.core.data.model.payment.CurrencyCode;
import ru.nspk.wallet.core.data.model.payment.PaymentCardTokenReference;
import ru.nspk.wallet.core.data.model.payment.ProfileModel;
import ru.nspk.wallet.core.data.model.util.MonetaryAmountModel;
import x60.b1;
import x60.e0;
import x60.n0;

/* JADX INFO: loaded from: <historical-project>/validation/decompiled/phone-native/recovered-1.dex */
public final class HceService2 extends HostApduService {

    /* JADX INFO: renamed from: a, reason: collision with root package name */
    private lb.g f13871a;

    /* JADX INFO: renamed from: b, reason: collision with root package name */
    private g9.d f13872b;

    /* JADX INFO: renamed from: c, reason: collision with root package name */
    private final kq.l f13873c = kq.m.b(new zq.a() { // from class: ea.t
        public final Object invoke() {
            return HceService2.o0(this.f15826a);
        }
    });

    /* JADX INFO: renamed from: d, reason: collision with root package name */
    private final boolean f13874d = h9.a.f17846a.j();

    /* JADX INFO: renamed from: e, reason: collision with root package name */
    private final o0 f13875e = p0.a(x2.b((b2) null, 1, (Object) null).plus(e1.c()).plus(new j(l0.z4)));

    /* JADX INFO: renamed from: f, reason: collision with root package name */
    private final kq.l f13876f;

    /* JADX INFO: renamed from: g, reason: collision with root package name */
    private final kq.l f13877g;

    /* JADX INFO: renamed from: h, reason: collision with root package name */
    private final kq.l f13878h;

    /* JADX INFO: renamed from: i, reason: collision with root package name */
    private final kq.l f13879i;

    /* JADX INFO: renamed from: j, reason: collision with root package name */
    private final kq.l f13880j;

    /* JADX INFO: renamed from: k, reason: collision with root package name */
    private final kq.l f13881k;

    /* JADX INFO: renamed from: l, reason: collision with root package name */
    private final kq.l f13882l;

    /* JADX INFO: renamed from: m, reason: collision with root package name */
    private final kq.l f13883m;

    /* JADX INFO: renamed from: n, reason: collision with root package name */
    private final kq.l f13884n;

    /* JADX INFO: renamed from: o, reason: collision with root package name */
    private final kq.l f13885o;

    /* JADX INFO: renamed from: p, reason: collision with root package name */
    private final kq.l f13886p;

    /* JADX INFO: renamed from: q, reason: collision with root package name */
    private final kq.l f13887q;

    /* JADX INFO: renamed from: r, reason: collision with root package name */
    private final kq.l f13888r;

    /* JADX INFO: renamed from: s, reason: collision with root package name */
    private final kq.l f13889s;

    /* JADX INFO: renamed from: t, reason: collision with root package name */
    private final kq.l f13890t;

    /* JADX INFO: renamed from: u, reason: collision with root package name */
    private final kq.l f13891u;

    /* JADX INFO: renamed from: v, reason: collision with root package name */
    private final kq.l f13892v;

    /* JADX INFO: renamed from: w, reason: collision with root package name */
    private final kq.l f13893w;

    /* JADX INFO: renamed from: x, reason: collision with root package name */
    private final kq.l f13894x;

    /* JADX INFO: renamed from: y, reason: collision with root package name */
    private final kq.l f13895y;

    /* JADX INFO: renamed from: z, reason: collision with root package name */
    private final kq.l f13896z;
    public static final a Companion = new a(null);
    public static final int A = 8;
    private static final String B = HceService2.class.getSimpleName();

    public static final class a {
        private a() {
        }

        public /* synthetic */ a(kotlin.jvm.internal.m mVar) {
            this();
        }
    }

    public static final class a0 implements zq.a {

        /* JADX INFO: renamed from: a, reason: collision with root package name */
        final /* synthetic */ ComponentCallbacks f13897a;

        /* JADX INFO: renamed from: b, reason: collision with root package name */
        final /* synthetic */ kv.a f13898b;

        /* JADX INFO: renamed from: c, reason: collision with root package name */
        final /* synthetic */ zq.a f13899c;

        public a0(ComponentCallbacks componentCallbacks, kv.a aVar, zq.a aVar2) {
            this.f13897a = componentCallbacks;
            this.f13898b = aVar;
            this.f13899c = aVar2;
        }

        public final Object invoke() {
            ComponentCallbacks componentCallbacks = this.f13897a;
            return wu.a.a(componentCallbacks).f(r0.b(x60.b.class), this.f13898b, this.f13899c);
        }
    }

    public /* synthetic */ class b {

        /* JADX INFO: renamed from: a, reason: collision with root package name */
        public static final /* synthetic */ int[] f13900a;

        static {
            int[] iArr = new int[rb.g.values().length];
            try {
                iArr[rb.g.f23124a.ordinal()] = 1;
            } catch (NoSuchFieldError unused) {
            }
            try {
                iArr[rb.g.f23125b.ordinal()] = 2;
            } catch (NoSuchFieldError unused2) {
            }
            try {
                iArr[rb.g.f23126c.ordinal()] = 3;
            } catch (NoSuchFieldError unused3) {
            }
            try {
                iArr[rb.g.f23127d.ordinal()] = 4;
            } catch (NoSuchFieldError unused4) {
            }
            f13900a = iArr;
        }
    }

    public static final class b0 implements zq.a {

        /* JADX INFO: renamed from: a, reason: collision with root package name */
        final /* synthetic */ ComponentCallbacks f13901a;

        /* JADX INFO: renamed from: b, reason: collision with root package name */
        final /* synthetic */ kv.a f13902b;

        /* JADX INFO: renamed from: c, reason: collision with root package name */
        final /* synthetic */ zq.a f13903c;

        public b0(ComponentCallbacks componentCallbacks, kv.a aVar, zq.a aVar2) {
            this.f13901a = componentCallbacks;
            this.f13902b = aVar;
            this.f13903c = aVar2;
        }

        public final Object invoke() {
            ComponentCallbacks componentCallbacks = this.f13901a;
            return wu.a.a(componentCallbacks).f(r0.b(x60.d.class), this.f13902b, this.f13903c);
        }
    }

    static final class c implements zq.a {

        /* JADX INFO: renamed from: a, reason: collision with root package name */
        final /* synthetic */ Throwable f13904a;

        c(Throwable th) {
            this.f13904a = th;
        }

        /* JADX INFO: renamed from: a, reason: merged with bridge method [inline-methods] */
        public final String invoke() {
            return ProtectedMirApplication.s("ö") + this.f13904a;
        }
    }

    public static final class c0 implements zq.a {

        /* JADX INFO: renamed from: a, reason: collision with root package name */
        final /* synthetic */ ComponentCallbacks f13905a;

        /* JADX INFO: renamed from: b, reason: collision with root package name */
        final /* synthetic */ kv.a f13906b;

        /* JADX INFO: renamed from: c, reason: collision with root package name */
        final /* synthetic */ zq.a f13907c;

        public c0(ComponentCallbacks componentCallbacks, kv.a aVar, zq.a aVar2) {
            this.f13905a = componentCallbacks;
            this.f13906b = aVar;
            this.f13907c = aVar2;
        }

        public final Object invoke() {
            ComponentCallbacks componentCallbacks = this.f13905a;
            return wu.a.a(componentCallbacks).f(r0.b(x60.n.class), this.f13906b, this.f13907c);
        }
    }

    static final class d implements lb.e, kotlin.jvm.internal.p {

        /* JADX INFO: renamed from: a, reason: collision with root package name */
        private final /* synthetic */ zq.l f13908a;

        d(zq.l lVar) {
            kotlin.jvm.internal.v.g(lVar, ProtectedMirApplication.s("÷"));
            this.f13908a = lVar;
        }

        public final kq.i c() {
            return this.f13908a;
        }

        public final boolean equals(Object obj) {
            if ((obj instanceof lb.e) && (obj instanceof kotlin.jvm.internal.p)) {
                return kotlin.jvm.internal.v.b(c(), ((kotlin.jvm.internal.p) obj).c());
            }
            return false;
        }

        @Override // lb.e
        public final /* synthetic */ AppletAidData g(PaymentCardTokenReference paymentCardTokenReference) {
            return (AppletAidData) this.f13908a.invoke(paymentCardTokenReference);
        }

        public final int hashCode() {
            return c().hashCode();
        }
    }

    public static final class d0 implements zq.a {

        /* JADX INFO: renamed from: a, reason: collision with root package name */
        final /* synthetic */ ComponentCallbacks f13909a;

        /* JADX INFO: renamed from: b, reason: collision with root package name */
        final /* synthetic */ kv.a f13910b;

        /* JADX INFO: renamed from: c, reason: collision with root package name */
        final /* synthetic */ zq.a f13911c;

        public d0(ComponentCallbacks componentCallbacks, kv.a aVar, zq.a aVar2) {
            this.f13909a = componentCallbacks;
            this.f13910b = aVar;
            this.f13911c = aVar2;
        }

        public final Object invoke() {
            ComponentCallbacks componentCallbacks = this.f13909a;
            return wu.a.a(componentCallbacks).f(r0.b(x60.p.class), this.f13910b, this.f13911c);
        }
    }

    /* synthetic */ class e extends kotlin.jvm.internal.s implements zq.l {
        e(Object obj) {
            super(1, obj, HceService2.class, ProtectedMirApplication.s("ù"), ProtectedMirApplication.s("ø"), 0);
        }

        /* JADX INFO: renamed from: n, reason: merged with bridge method [inline-methods] */
        public final pb.b invoke(PaymentCardTokenReference paymentCardTokenReference) {
            return ((HceService2) ((kotlin.jvm.internal.h) this).receiver).O(paymentCardTokenReference);
        }
    }

    public static final class g implements rb.h {
        g() {
        }

        @Override // rb.h
        public void a(PaymentCardTokenReference paymentCardTokenReference) {
            kotlin.jvm.internal.v.g(paymentCardTokenReference, ProtectedMirApplication.s("ü"));
            HceService2.this.F0();
            HceService2.this.r0(paymentCardTokenReference);
        }

        @Override // rb.h
        public void b(PaymentCardTokenReference paymentCardTokenReference) {
            kotlin.jvm.internal.v.g(paymentCardTokenReference, ProtectedMirApplication.s("ü"));
            HceService2.this.F0();
            HceService2.this.p0(paymentCardTokenReference);
        }
    }

    public static final class h implements rb.j {
        h() {
        }

        /* JADX INFO: Thrown type has an unknown type hierarchy: kq.q */
        @Override // rb.j
        public void a() throws kq.q {
            HceService2.this.w0(HceService2.this.h0().D1(), rb.g.f23124a);
        }

        /* JADX INFO: Thrown type has an unknown type hierarchy: kq.q */
        @Override // rb.j
        public void b() throws kq.q {
            HceService2.this.w0(null, rb.g.f23126c);
        }

        @Override // rb.j
        public void c() {
            HceService2.this.F0();
        }

        @Override // rb.j
        public void d() {
            HceService2.this.q0(HceService2.this.h0().D1());
        }
    }

    public static final class i implements rb.k {
        i() {
        }

        @Override // rb.k
        public void a(PaymentCardTokenReference paymentCardTokenReference, MonetaryAmountModel monetaryAmountModel) {
            kotlin.jvm.internal.v.g(paymentCardTokenReference, ProtectedMirApplication.s("ý"));
            HceService2.this.D0(paymentCardTokenReference, monetaryAmountModel);
        }

        @Override // rb.k
        public void b(PaymentCardTokenReference paymentCardTokenReference, boolean z10) {
            kotlin.jvm.internal.v.g(paymentCardTokenReference, ProtectedMirApplication.s("ý"));
            HceService2.this.B0(paymentCardTokenReference, z10);
        }

        @Override // rb.k
        public void c(PaymentCardTokenReference paymentCardTokenReference, MonetaryAmountModel monetaryAmountModel) {
            kotlin.jvm.internal.v.g(paymentCardTokenReference, ProtectedMirApplication.s("ý"));
            HceService2.this.z0(paymentCardTokenReference, monetaryAmountModel);
        }

        /* JADX INFO: Thrown type has an unknown type hierarchy: kq.q */
        @Override // rb.k
        public void d(PaymentCardTokenReference paymentCardTokenReference, rb.g gVar) throws kq.q {
            kotlin.jvm.internal.v.g(gVar, ProtectedMirApplication.s("þ"));
            HceService2.this.w0(paymentCardTokenReference, gVar);
        }
    }

    public static final class j extends pq.a implements l0 {
        public j(l0.b bVar) {
            super(bVar);
        }

        public void h(pq.i iVar, Throwable th) {
            wb.i iVar2 = wb.i.f25235a;
            String str = HceService2.B;
            kotlin.jvm.internal.v.f(str, ProtectedMirApplication.s("ÿ"));
            iVar2.i(str, new c(th));
        }
    }

    public static final class k implements zq.a {

        /* JADX INFO: renamed from: a, reason: collision with root package name */
        final /* synthetic */ ComponentCallbacks f13915a;

        /* JADX INFO: renamed from: b, reason: collision with root package name */
        final /* synthetic */ kv.a f13916b;

        /* JADX INFO: renamed from: c, reason: collision with root package name */
        final /* synthetic */ zq.a f13917c;

        public k(ComponentCallbacks componentCallbacks, kv.a aVar, zq.a aVar2) {
            this.f13915a = componentCallbacks;
            this.f13916b = aVar;
            this.f13917c = aVar2;
        }

        public final Object invoke() {
            ComponentCallbacks componentCallbacks = this.f13915a;
            return wu.a.a(componentCallbacks).f(r0.b(z30.b.class), this.f13916b, this.f13917c);
        }
    }

    public static final class l implements zq.a {

        /* JADX INFO: renamed from: a, reason: collision with root package name */
        final /* synthetic */ ComponentCallbacks f13918a;

        /* JADX INFO: renamed from: b, reason: collision with root package name */
        final /* synthetic */ kv.a f13919b;

        /* JADX INFO: renamed from: c, reason: collision with root package name */
        final /* synthetic */ zq.a f13920c;

        public l(ComponentCallbacks componentCallbacks, kv.a aVar, zq.a aVar2) {
            this.f13918a = componentCallbacks;
            this.f13919b = aVar;
            this.f13920c = aVar2;
        }

        public final Object invoke() {
            ComponentCallbacks componentCallbacks = this.f13918a;
            return wu.a.a(componentCallbacks).f(r0.b(x60.y.class), this.f13919b, this.f13920c);
        }
    }

    public static final class m implements zq.a {

        /* JADX INFO: renamed from: a, reason: collision with root package name */
        final /* synthetic */ ComponentCallbacks f13921a;

        /* JADX INFO: renamed from: b, reason: collision with root package name */
        final /* synthetic */ kv.a f13922b;

        /* JADX INFO: renamed from: c, reason: collision with root package name */
        final /* synthetic */ zq.a f13923c;

        public m(ComponentCallbacks componentCallbacks, kv.a aVar, zq.a aVar2) {
            this.f13921a = componentCallbacks;
            this.f13922b = aVar;
            this.f13923c = aVar2;
        }

        public final Object invoke() {
            ComponentCallbacks componentCallbacks = this.f13921a;
            return wu.a.a(componentCallbacks).f(r0.b(x60.a0.class), this.f13922b, this.f13923c);
        }
    }

    public static final class n implements zq.a {

        /* JADX INFO: renamed from: a, reason: collision with root package name */
        final /* synthetic */ ComponentCallbacks f13924a;

        /* JADX INFO: renamed from: b, reason: collision with root package name */
        final /* synthetic */ kv.a f13925b;

        /* JADX INFO: renamed from: c, reason: collision with root package name */
        final /* synthetic */ zq.a f13926c;

        public n(ComponentCallbacks componentCallbacks, kv.a aVar, zq.a aVar2) {
            this.f13924a = componentCallbacks;
            this.f13925b = aVar;
            this.f13926c = aVar2;
        }

        public final Object invoke() {
            ComponentCallbacks componentCallbacks = this.f13924a;
            return wu.a.a(componentCallbacks).f(r0.b(e0.class), this.f13925b, this.f13926c);
        }
    }

    public static final class o implements zq.a {

        /* JADX INFO: renamed from: a, reason: collision with root package name */
        final /* synthetic */ ComponentCallbacks f13927a;

        /* JADX INFO: renamed from: b, reason: collision with root package name */
        final /* synthetic */ kv.a f13928b;

        /* JADX INFO: renamed from: c, reason: collision with root package name */
        final /* synthetic */ zq.a f13929c;

        public o(ComponentCallbacks componentCallbacks, kv.a aVar, zq.a aVar2) {
            this.f13927a = componentCallbacks;
            this.f13928b = aVar;
            this.f13929c = aVar2;
        }

        public final Object invoke() {
            ComponentCallbacks componentCallbacks = this.f13927a;
            return wu.a.a(componentCallbacks).f(r0.b(n0.class), this.f13928b, this.f13929c);
        }
    }

    public static final class p implements zq.a {

        /* JADX INFO: renamed from: a, reason: collision with root package name */
        final /* synthetic */ ComponentCallbacks f13930a;

        /* JADX INFO: renamed from: b, reason: collision with root package name */
        final /* synthetic */ kv.a f13931b;

        /* JADX INFO: renamed from: c, reason: collision with root package name */
        final /* synthetic */ zq.a f13932c;

        public p(ComponentCallbacks componentCallbacks, kv.a aVar, zq.a aVar2) {
            this.f13930a = componentCallbacks;
            this.f13931b = aVar;
            this.f13932c = aVar2;
        }

        public final Object invoke() {
            ComponentCallbacks componentCallbacks = this.f13930a;
            return wu.a.a(componentCallbacks).f(r0.b(x60.p0.class), this.f13931b, this.f13932c);
        }
    }

    public static final class q implements zq.a {

        /* JADX INFO: renamed from: a, reason: collision with root package name */
        final /* synthetic */ ComponentCallbacks f13933a;

        /* JADX INFO: renamed from: b, reason: collision with root package name */
        final /* synthetic */ kv.a f13934b;

        /* JADX INFO: renamed from: c, reason: collision with root package name */
        final /* synthetic */ zq.a f13935c;

        public q(ComponentCallbacks componentCallbacks, kv.a aVar, zq.a aVar2) {
            this.f13933a = componentCallbacks;
            this.f13934b = aVar;
            this.f13935c = aVar2;
        }

        public final Object invoke() {
            ComponentCallbacks componentCallbacks = this.f13933a;
            return wu.a.a(componentCallbacks).f(r0.b(d70.c.class), this.f13934b, this.f13935c);
        }
    }

    public static final class r implements zq.a {

        /* JADX INFO: renamed from: a, reason: collision with root package name */
        final /* synthetic */ ComponentCallbacks f13936a;

        /* JADX INFO: renamed from: b, reason: collision with root package name */
        final /* synthetic */ kv.a f13937b;

        /* JADX INFO: renamed from: c, reason: collision with root package name */
        final /* synthetic */ zq.a f13938c;

        public r(ComponentCallbacks componentCallbacks, kv.a aVar, zq.a aVar2) {
            this.f13936a = componentCallbacks;
            this.f13937b = aVar;
            this.f13938c = aVar2;
        }

        public final Object invoke() {
            ComponentCallbacks componentCallbacks = this.f13936a;
            return wu.a.a(componentCallbacks).f(r0.b(b1.class), this.f13937b, this.f13938c);
        }
    }

    public static final class s implements zq.a {

        /* JADX INFO: renamed from: a, reason: collision with root package name */
        final /* synthetic */ ComponentCallbacks f13939a;

        /* JADX INFO: renamed from: b, reason: collision with root package name */
        final /* synthetic */ kv.a f13940b;

        /* JADX INFO: renamed from: c, reason: collision with root package name */
        final /* synthetic */ zq.a f13941c;

        public s(ComponentCallbacks componentCallbacks, kv.a aVar, zq.a aVar2) {
            this.f13939a = componentCallbacks;
            this.f13940b = aVar;
            this.f13941c = aVar2;
        }

        public final Object invoke() {
            ComponentCallbacks componentCallbacks = this.f13939a;
            return wu.a.a(componentCallbacks).f(r0.b(s40.a.class), this.f13940b, this.f13941c);
        }
    }

    public static final class t implements zq.a {

        /* JADX INFO: renamed from: a, reason: collision with root package name */
        final /* synthetic */ ComponentCallbacks f13942a;

        /* JADX INFO: renamed from: b, reason: collision with root package name */
        final /* synthetic */ kv.a f13943b;

        /* JADX INFO: renamed from: c, reason: collision with root package name */
        final /* synthetic */ zq.a f13944c;

        public t(ComponentCallbacks componentCallbacks, kv.a aVar, zq.a aVar2) {
            this.f13942a = componentCallbacks;
            this.f13943b = aVar;
            this.f13944c = aVar2;
        }

        public final Object invoke() {
            ComponentCallbacks componentCallbacks = this.f13942a;
            return wu.a.a(componentCallbacks).f(r0.b(h50.a.class), this.f13943b, this.f13944c);
        }
    }

    public static final class u implements zq.a {

        /* JADX INFO: renamed from: a, reason: collision with root package name */
        final /* synthetic */ ComponentCallbacks f13945a;

        /* JADX INFO: renamed from: b, reason: collision with root package name */
        final /* synthetic */ kv.a f13946b;

        /* JADX INFO: renamed from: c, reason: collision with root package name */
        final /* synthetic */ zq.a f13947c;

        public u(ComponentCallbacks componentCallbacks, kv.a aVar, zq.a aVar2) {
            this.f13945a = componentCallbacks;
            this.f13946b = aVar;
            this.f13947c = aVar2;
        }

        public final Object invoke() {
            ComponentCallbacks componentCallbacks = this.f13945a;
            return wu.a.a(componentCallbacks).f(r0.b(x60.l0.class), this.f13946b, this.f13947c);
        }
    }

    public static final class v implements zq.a {

        /* JADX INFO: renamed from: a, reason: collision with root package name */
        final /* synthetic */ ComponentCallbacks f13948a;

        /* JADX INFO: renamed from: b, reason: collision with root package name */
        final /* synthetic */ kv.a f13949b;

        /* JADX INFO: renamed from: c, reason: collision with root package name */
        final /* synthetic */ zq.a f13950c;

        public v(ComponentCallbacks componentCallbacks, kv.a aVar, zq.a aVar2) {
            this.f13948a = componentCallbacks;
            this.f13949b = aVar;
            this.f13950c = aVar2;
        }

        public final Object invoke() {
            ComponentCallbacks componentCallbacks = this.f13948a;
            return wu.a.a(componentCallbacks).f(r0.b(q9.d.class), this.f13949b, this.f13950c);
        }
    }

    public static final class w implements zq.a {

        /* JADX INFO: renamed from: a, reason: collision with root package name */
        final /* synthetic */ ComponentCallbacks f13951a;

        /* JADX INFO: renamed from: b, reason: collision with root package name */
        final /* synthetic */ kv.a f13952b;

        /* JADX INFO: renamed from: c, reason: collision with root package name */
        final /* synthetic */ zq.a f13953c;

        public w(ComponentCallbacks componentCallbacks, kv.a aVar, zq.a aVar2) {
            this.f13951a = componentCallbacks;
            this.f13952b = aVar;
            this.f13953c = aVar2;
        }

        public final Object invoke() {
            ComponentCallbacks componentCallbacks = this.f13951a;
            return wu.a.a(componentCallbacks).f(r0.b(j60.c.class), this.f13952b, this.f13953c);
        }
    }

    public static final class x implements zq.a {

        /* JADX INFO: renamed from: a, reason: collision with root package name */
        final /* synthetic */ ComponentCallbacks f13954a;

        /* JADX INFO: renamed from: b, reason: collision with root package name */
        final /* synthetic */ kv.a f13955b;

        /* JADX INFO: renamed from: c, reason: collision with root package name */
        final /* synthetic */ zq.a f13956c;

        public x(ComponentCallbacks componentCallbacks, kv.a aVar, zq.a aVar2) {
            this.f13954a = componentCallbacks;
            this.f13955b = aVar;
            this.f13956c = aVar2;
        }

        public final Object invoke() {
            ComponentCallbacks componentCallbacks = this.f13954a;
            return wu.a.a(componentCallbacks).f(r0.b(h00.d.class), this.f13955b, this.f13956c);
        }
    }

    public static final class y implements zq.a {

        /* JADX INFO: renamed from: a, reason: collision with root package name */
        final /* synthetic */ ComponentCallbacks f13957a;

        /* JADX INFO: renamed from: b, reason: collision with root package name */
        final /* synthetic */ kv.a f13958b;

        /* JADX INFO: renamed from: c, reason: collision with root package name */
        final /* synthetic */ zq.a f13959c;

        public y(ComponentCallbacks componentCallbacks, kv.a aVar, zq.a aVar2) {
            this.f13957a = componentCallbacks;
            this.f13958b = aVar;
            this.f13959c = aVar2;
        }

        public final Object invoke() {
            ComponentCallbacks componentCallbacks = this.f13957a;
            return wu.a.a(componentCallbacks).f(r0.b(t9.a.class), this.f13958b, this.f13959c);
        }
    }

    public static final class z implements zq.a {

        /* JADX INFO: renamed from: a, reason: collision with root package name */
        final /* synthetic */ ComponentCallbacks f13960a;

        /* JADX INFO: renamed from: b, reason: collision with root package name */
        final /* synthetic */ kv.a f13961b;

        /* JADX INFO: renamed from: c, reason: collision with root package name */
        final /* synthetic */ zq.a f13962c;

        public z(ComponentCallbacks componentCallbacks, kv.a aVar, zq.a aVar2) {
            this.f13960a = componentCallbacks;
            this.f13961b = aVar;
            this.f13962c = aVar2;
        }

        public final Object invoke() {
            ComponentCallbacks componentCallbacks = this.f13960a;
            return wu.a.a(componentCallbacks).f(r0.b(p40.a.class), this.f13961b, this.f13962c);
        }
    }

    public HceService2() {
        kq.p pVar = kq.p.a;
        this.f13876f = kq.m.a(pVar, new u(this, null, null));
        this.f13877g = kq.m.a(pVar, new w(this, null, null));
        this.f13878h = kq.m.a(pVar, new x(this, null, new zq.a() { // from class: ea.u
            public final Object invoke() {
                return HceService2.w(this.f15827a);
            }
        }));
        this.f13879i = kq.m.a(pVar, new y(this, null, null));
        this.f13880j = kq.m.a(pVar, new z(this, null, null));
        this.f13881k = kq.m.a(pVar, new a0(this, null, null));
        this.f13882l = kq.m.a(pVar, new b0(this, null, null));
        this.f13883m = kq.m.a(pVar, new c0(this, null, null));
        this.f13884n = kq.m.a(pVar, new d0(this, null, null));
        this.f13885o = kq.m.a(pVar, new k(this, null, null));
        this.f13886p = kq.m.a(pVar, new l(this, null, null));
        this.f13887q = kq.m.a(pVar, new m(this, null, null));
        this.f13888r = kq.m.a(pVar, new n(this, null, null));
        this.f13889s = kq.m.a(pVar, new o(this, null, null));
        this.f13890t = kq.m.a(pVar, new p(this, null, null));
        this.f13891u = kq.m.a(pVar, new q(this, null, null));
        this.f13892v = kq.m.a(pVar, new r(this, null, null));
        this.f13893w = kq.m.a(pVar, new s(this, null, null));
        this.f13894x = kq.m.a(pVar, new t(this, null, null));
        this.f13895y = kq.m.a(pVar, new v(this, null, null));
        this.f13896z = kq.m.b(new zq.a() { // from class: ea.v
            public final Object invoke() {
                return HceService2.x();
            }
        });
    }

    /* JADX INFO: Access modifiers changed from: private */
    public static final void A0(HceService2 hceService2, PaymentCardTokenReference paymentCardTokenReference, MonetaryAmountModel monetaryAmountModel) {
        hceService2.startActivity(PaymentActivity.Companion.h(hceService2, paymentCardTokenReference, monetaryAmountModel));
    }

    /* JADX INFO: Access modifiers changed from: private */
    public final void B0(final PaymentCardTokenReference paymentCardTokenReference, final boolean z10) {
        vk.e eVar = vk.e.a;
        String strS = ProtectedMirApplication.s("Ā");
        if (eVar.c(strS)) {
            eVar.e(strS, ProtectedMirApplication.s("ā"));
            eVar.b(strS);
            L(ProtectedMirApplication.s("Ă"));
        }
        ok.f.b(new Runnable() { // from class: ea.n
            @Override // java.lang.Runnable
            public final void run() {
                HceService2.C0(this.f15818a, paymentCardTokenReference, z10);
            }
        });
    }

    /* JADX INFO: Access modifiers changed from: private */
    public static final void C0(HceService2 hceService2, PaymentCardTokenReference paymentCardTokenReference, boolean z10) {
        hceService2.startActivity(PaymentActivity.Companion.i(hceService2, paymentCardTokenReference, z10));
    }

    /* JADX INFO: Access modifiers changed from: private */
    public final void D0(final PaymentCardTokenReference paymentCardTokenReference, final MonetaryAmountModel monetaryAmountModel) {
        vk.e eVar = vk.e.a;
        String strS = ProtectedMirApplication.s("Ā");
        if (eVar.c(strS)) {
            eVar.e(strS, ProtectedMirApplication.s("ă"));
            eVar.b(strS);
            L(ProtectedMirApplication.s("Ą"));
        }
        k0().a(j0());
        ok.f.b(new Runnable() { // from class: ea.o
            @Override // java.lang.Runnable
            public final void run() {
                HceService2.E0(this.f15821a, paymentCardTokenReference, monetaryAmountModel);
            }
        });
    }

    /* JADX INFO: Access modifiers changed from: private */
    public static final void E0(HceService2 hceService2, PaymentCardTokenReference paymentCardTokenReference, MonetaryAmountModel monetaryAmountModel) {
        hceService2.startActivity(PaymentActivity.Companion.j(hceService2, paymentCardTokenReference, monetaryAmountModel));
    }

    /* JADX INFO: Access modifiers changed from: private */
    public final void F0() {
        vk.e eVar = vk.e.a;
        String strS = ProtectedMirApplication.s("Ā");
        if (eVar.c(strS)) {
            eVar.e(strS, ProtectedMirApplication.s("ą"));
            eVar.a(strS);
            M(this, null, 1, null);
        }
    }

    /* JADX INFO: Access modifiers changed from: private */
    public static final String G0() {
        return ProtectedMirApplication.s("Ć");
    }

    /* JADX INFO: Access modifiers changed from: private */
    public static final String H0(byte[] bArr) {
        return ProtectedMirApplication.s("ć") + (bArr != null ? al.a.a(bArr, true) : null);
    }

    /* JADX INFO: Access modifiers changed from: private */
    public static final String I0(byte[] bArr) {
        return ProtectedMirApplication.s("Ĉ") + (bArr != null ? al.a.a(bArr, true) : null);
    }

    private final pb.d J() {
        wb.i iVar = wb.i.f25235a;
        String str = B;
        kotlin.jvm.internal.v.f(str, ProtectedMirApplication.s("ĉ"));
        iVar.i(str, new zq.a() { // from class: ea.i
            public final Object invoke() {
                return HceService2.K();
            }
        });
        if (!Q().isDeviceSecure()) {
            a0().g3(true);
            TukEraserWorker.Companion.a();
            return pb.d.a.f22614a;
        }
        pb.d dVarR0 = R0();
        if (!kotlin.jvm.internal.v.b(dVarR0, pb.d.a.f22614a)) {
            return dVarR0;
        }
        N();
        return R0();
    }

    private final lb.g J0(byte[] bArr) {
        i iVar = new i();
        g gVar = new g();
        String string = getApplicationContext().getString(com.ekassir.mirpay.app.j.r.L4);
        kotlin.jvm.internal.v.f(string, ProtectedMirApplication.s("Ċ"));
        zq.a aVar = new zq.a() { // from class: ea.d
            public final Object invoke() {
                return HceService2.K0(this.f15806a);
            }
        };
        zq.l lVar = new zq.l() { // from class: ea.e
            public final Object invoke(Object obj) {
                return HceService2.L0(this.f15807a, (PaymentCardTokenReference) obj);
            }
        };
        return f0().c(new e(this), new i0(h0()) { // from class: com.ekassir.mirpay.app.services.HceService2.f
            {
                String strS = ProtectedMirApplication.s("ú");
                String strS2 = ProtectedMirApplication.s("û");
            }

            public Object get() {
                return ((x60.p0) ((kotlin.jvm.internal.h) this).receiver).D1();
            }
        }, iVar, new mb.e(gVar, string, aVar, c0(), new d(lVar), new zq.l() { // from class: ea.f
            public final Object invoke(Object obj) {
                return HceService2.M0(this.f15808a, (PaymentCardTokenReference) obj);
            }
        }, new zq.l() { // from class: ea.g
            public final Object invoke(Object obj) {
                return HceService2.N0(this.f15809a, (PaymentCardTokenReference) obj);
            }
        }), bArr, new h());
    }

    /* JADX INFO: Access modifiers changed from: private */
    public static final String K() {
        return ProtectedMirApplication.s("ċ");
    }

    /* JADX INFO: Access modifiers changed from: private */
    public static final PaymentCardTokenReference K0(HceService2 hceService2) {
        return hceService2.h0().D1();
    }

    private final void L(String str) {
        if (this.f13874d) {
            if (str != null) {
                vk.e.a.b(str);
            }
            vk.e eVar = vk.e.a;
            eVar.a(ProtectedMirApplication.s("Č"));
            eVar.a(ProtectedMirApplication.s("č"));
            eVar.a(ProtectedMirApplication.s("Ą"));
            eVar.a(ProtectedMirApplication.s("Ď"));
            eVar.a(ProtectedMirApplication.s("Ă"));
        }
    }

    /* JADX INFO: Access modifiers changed from: private */
    public static final AppletAidData L0(HceService2 hceService2, PaymentCardTokenReference paymentCardTokenReference) {
        kotlin.jvm.internal.v.g(paymentCardTokenReference, ProtectedMirApplication.s("ď"));
        return hceService2.S().g(paymentCardTokenReference);
    }

    static /* synthetic */ void M(HceService2 hceService2, String str, int i10, Object obj) {
        if ((i10 & 1) != 0) {
            str = null;
        }
        hceService2.L(str);
    }

    /* JADX INFO: Access modifiers changed from: private */
    public static final pb.b M0(HceService2 hceService2, PaymentCardTokenReference paymentCardTokenReference) {
        return hceService2.O(paymentCardTokenReference);
    }

    private final void N() {
        try {
            e0().r(Y().h().getUserVerificationLifetimeInSeconds());
        } catch (KeyStoreException unused) {
        }
        try {
            e0().P3(Y().h().getUserVerificationLifetimeInSecondsForLockedDevice());
        } catch (KeyStoreException unused2) {
        }
    }

    /* JADX INFO: Access modifiers changed from: private */
    public static final nb.b N0(HceService2 hceService2, PaymentCardTokenReference paymentCardTokenReference) {
        AccessCardModel accessCardModelK;
        if (paymentCardTokenReference == null || (accessCardModelK = hceService2.T().K(paymentCardTokenReference)) == null) {
            return null;
        }
        return new nb.b(accessCardModelK.getPropKeyVer(), accessCardModelK.getAsn(), accessCardModelK.getPan(), accessCardModelK.getIccMkAc());
    }

    /* JADX INFO: Access modifiers changed from: private */
    public final pb.b O(PaymentCardTokenReference paymentCardTokenReference) {
        ea.a aVar = null;
        if (paymentCardTokenReference == null) {
            return null;
        }
        ProfileModel profileModelN1 = V().n1(paymentCardTokenReference);
        if (profileModelN1 != null) {
            aVar = new ea.a(paymentCardTokenReference, profileModelN1, W(), X(), l0(), new zq.a() { // from class: ea.h
                public final Object invoke() {
                    return HceService2.P(this.f15810a);
                }
            }, g0(), i0(), zb.h.f29202a, U(), b0(), Y().h().getCurrencies(), R(), g9.a.f17269a.e(this));
            if (h9.a.f17846a.i()) {
                return new pb.a(aVar);
            }
        }
        return aVar;
    }

    private final void O0() {
        if (this.f13874d) {
            vk.e eVar = vk.e.a;
            eVar.f(ProtectedMirApplication.s("Č"));
            eVar.f(ProtectedMirApplication.s("č"));
            eVar.f(ProtectedMirApplication.s("Ą"));
            eVar.f(ProtectedMirApplication.s("Ď"));
            eVar.f(ProtectedMirApplication.s("Ă"));
        }
    }

    /* JADX INFO: Access modifiers changed from: private */
    public static final pb.d P(HceService2 hceService2) {
        return hceService2.J();
    }

    private final byte[] P0(byte[] bArr, String str) {
        String upperCase;
        if (bArr == null) {
            return null;
        }
        if (bArr.length > 1) {
            v0 v0Var = v0.a;
            String str2 = String.format(ProtectedMirApplication.s("Đ"), Arrays.copyOf(new Object[]{Byte.valueOf(bArr[0]), Byte.valueOf(bArr[1])}, 2));
            kotlin.jvm.internal.v.f(str2, ProtectedMirApplication.s("đ"));
            upperCase = str2.toUpperCase(Locale.ROOT);
            kotlin.jvm.internal.v.f(upperCase, ProtectedMirApplication.s("Ē"));
        } else {
            upperCase = "";
        }
        lb.g gVar = this.f13871a;
        if (gVar != null) {
            vk.e eVar = vk.e.a;
            StringBuilder sb2 = new StringBuilder();
            String strS = ProtectedMirApplication.s("ē");
            sb2.append(strS);
            sb2.append(upperCase);
            eVar.f(sb2.toString());
            lb.d dVarA = gVar.a(bArr, str);
            eVar.b(strS + upperCase);
            byte[] bArrA = dVarA.a();
            if (bArrA != null) {
                return bArrA;
            }
        }
        wb.i.f25235a.m(str, new zq.a() { // from class: ea.c
            public final Object invoke() {
                return HceService2.Q0();
            }
        });
        return null;
    }

    private final KeyguardManager Q() {
        return (KeyguardManager) this.f13873c.getValue();
    }

    /* JADX INFO: Access modifiers changed from: private */
    public static final String Q0() {
        return ProtectedMirApplication.s("Ĕ");
    }

    private final NoCvmConfigModel R() {
        List limits;
        if (!h9.a.f17846a.v()) {
            return Y().h().getNoCvmConfig();
        }
        Integer numValueOf = Integer.valueOf(getResources().getInteger(com.ekassir.mirpay.app.j.k.f12610l));
        if (numValueOf.intValue() <= 0) {
            numValueOf = null;
        }
        Integer numValueOf2 = Integer.valueOf(getResources().getInteger(com.ekassir.mirpay.app.j.k.f12608k));
        Integer num = numValueOf2.intValue() > 0 ? numValueOf2 : null;
        NoCvmConfigModel noCvmConfig = Y().h().getNoCvmConfig();
        long jIntValue = numValueOf != null ? numValueOf.intValue() : noCvmConfig.getMaxTransactionCount();
        if (num != null) {
            long jIntValue2 = num.intValue();
            List limits2 = noCvmConfig.getLimits();
            limits = new ArrayList(lq.w.y(limits2, 10));
            Iterator it = limits2.iterator();
            while (it.hasNext()) {
                limits.add(NoCvmLimitsModel.copy$default((NoCvmLimitsModel) it.next(), jIntValue2, (CurrencyCode) null, 2, (Object) null));
            }
        } else {
            limits = noCvmConfig.getLimits();
        }
        return NoCvmConfigModel.copy$default(noCvmConfig, false, false, limits, jIntValue, (String) null, false, 51, (Object) null);
    }

    private final pb.d R0() {
        if (!e0().d3()) {
            return pb.d.a.f22614a;
        }
        boolean zIsDeviceLocked = Q().isDeviceLocked();
        try {
            return n0(zIsDeviceLocked) ? pb.d.b.f22615a : new pb.d.c(m0(zIsDeviceLocked));
        } catch (KeyStoreException unused) {
            return pb.d.a.f22614a;
        }
    }

    private final x60.b S() {
        return (x60.b) this.f13881k.getValue();
    }

    private final x60.d T() {
        return (x60.d) this.f13882l.getValue();
    }

    private final t9.a U() {
        return (t9.a) this.f13879i.getValue();
    }

    private final x60.n V() {
        return (x60.n) this.f13883m.getValue();
    }

    private final j60.c W() {
        return (j60.c) this.f13877g.getValue();
    }

    private final x60.p X() {
        return (x60.p) this.f13884n.getValue();
    }

    private final z30.b Y() {
        return (z30.b) this.f13885o.getValue();
    }

    private final s40.a Z() {
        return (s40.a) this.f13893w.getValue();
    }

    private final x60.y a0() {
        return (x60.y) this.f13886p.getValue();
    }

    private final x60.a0 b0() {
        return (x60.a0) this.f13887q.getValue();
    }

    private final e0 c0() {
        return (e0) this.f13888r.getValue();
    }

    private final q9.d d0() {
        return (q9.d) this.f13895y.getValue();
    }

    private final p40.a e0() {
        return (p40.a) this.f13880j.getValue();
    }

    private final h00.d f0() {
        return (h00.d) this.f13878h.getValue();
    }

    private final x60.l0 g0() {
        return (x60.l0) this.f13876f.getValue();
    }

    /* JADX INFO: Access modifiers changed from: private */
    public final x60.p0 h0() {
        return (x60.p0) this.f13890t.getValue();
    }

    private final d70.c i0() {
        return (d70.c) this.f13891u.getValue();
    }

    private final ScenarioData j0() {
        return (ScenarioData) this.f13896z.getValue();
    }

    private final h50.a k0() {
        return (h50.a) this.f13894x.getValue();
    }

    private final b1 l0() {
        return (b1) this.f13892v.getValue();
    }

    private final boolean m0(boolean z10) {
        return z10 ? Z().y1() : Z().p0();
    }

    private final boolean n0(boolean z10) {
        return z10 ? e0().X() : e0().u0();
    }

    /* JADX INFO: Access modifiers changed from: private */
    public static final KeyguardManager o0(HceService2 hceService2) {
        Object systemService = hceService2.getApplication().getSystemService(ProtectedMirApplication.s("ĕ"));
        kotlin.jvm.internal.v.e(systemService, ProtectedMirApplication.s("Ė"));
        return (KeyguardManager) systemService;
    }

    /* JADX INFO: Access modifiers changed from: private */
    public final void p0(PaymentCardTokenReference paymentCardTokenReference) {
        startActivity(PaymentActivity.Companion.a(this, paymentCardTokenReference));
    }

    /* JADX INFO: Access modifiers changed from: private */
    public final void q0(PaymentCardTokenReference paymentCardTokenReference) {
        startActivity(PaymentActivity.Companion.b(this, paymentCardTokenReference));
    }

    /* JADX INFO: Access modifiers changed from: private */
    public final void r0(PaymentCardTokenReference paymentCardTokenReference) {
        startActivity(PaymentActivity.Companion.c(this, paymentCardTokenReference));
    }

    /* JADX INFO: Access modifiers changed from: private */
    public static final String s0() {
        return ProtectedMirApplication.s("ė");
    }

    /* JADX INFO: Access modifiers changed from: private */
    public static final String t0(String str) {
        return ProtectedMirApplication.s("Ę") + str;
    }

    /* JADX INFO: Access modifiers changed from: private */
    public static final String u0(String str) {
        return ProtectedMirApplication.s("ę") + str;
    }

    /* JADX INFO: Access modifiers changed from: private */
    public static final String v0() {
        return ProtectedMirApplication.s("Ě");
    }

    /* JADX INFO: Access modifiers changed from: private */
    public static final jv.a w(HceService2 hceService2) {
        return jv.b.b(new Object[]{hceService2.f13875e});
    }

    /* JADX INFO: Access modifiers changed from: private */
    /* JADX INFO: Thrown type has an unknown type hierarchy: kq.q */
    public final void w0(PaymentCardTokenReference paymentCardTokenReference, final rb.g gVar) throws kq.q {
        String strB;
        final Intent intentF;
        g9.d dVar = this.f13872b;
        if (dVar == null || (strB = dVar.b()) == null) {
            strB = B;
        }
        vk.e eVar = vk.e.a;
        String strS = ProtectedMirApplication.s("Ā");
        if (eVar.c(strS)) {
            eVar.e(strS, ProtectedMirApplication.s("ě"));
            eVar.b(strS);
            L(ProtectedMirApplication.s("Ď"));
            wb.i iVar = wb.i.f25235a;
            kotlin.jvm.internal.v.d(strB);
            iVar.m(strB, new zq.a() { // from class: ea.j
                public final Object invoke() {
                    return HceService2.x0(gVar);
                }
            });
        }
        if (gVar == rb.g.f23127d) {
            TukReplenishmentDispatcherWorker.Companion.b(j0());
        }
        int i10 = b.f13900a[gVar.ordinal()];
        if (i10 == 1) {
            intentF = PaymentActivity.Companion.f(this, paymentCardTokenReference);
        } else if (i10 == 2) {
            intentF = PaymentActivity.Companion.e(this, paymentCardTokenReference);
        } else if (i10 == 3) {
            intentF = PaymentActivity.Companion.g(this);
        } else {
            if (i10 != 4) {
                throw new kq.q();
            }
            intentF = PaymentActivity.Companion.f(this, paymentCardTokenReference);
        }
        ok.f.b(new Runnable() { // from class: ea.k
            @Override // java.lang.Runnable
            public final void run() {
                HceService2.y0(this.f15812a, intentF);
            }
        });
    }

    /* JADX INFO: Access modifiers changed from: private */
    public static final ScenarioData x() {
        return new ScenarioData(SourceId.Service.Hce.INSTANCE, ScenarioId.b.b);
    }

    /* JADX INFO: Access modifiers changed from: private */
    public static final String x0(rb.g gVar) {
        return ProtectedMirApplication.s("Ĝ") + gVar;
    }

    /* JADX INFO: Access modifiers changed from: private */
    public static final void y0(HceService2 hceService2, Intent intent) {
        hceService2.startActivity(intent);
    }

    /* JADX INFO: Access modifiers changed from: private */
    public final void z0(final PaymentCardTokenReference paymentCardTokenReference, final MonetaryAmountModel monetaryAmountModel) {
        vk.e eVar = vk.e.a;
        String strS = ProtectedMirApplication.s("Ā");
        if (eVar.c(strS)) {
            eVar.e(strS, ProtectedMirApplication.s("ĝ"));
            eVar.b(strS);
            L(ProtectedMirApplication.s("č"));
        }
        TukReplenishmentDispatcherWorker.Companion.b(j0());
        k0().a(j0());
        ok.f.b(new Runnable() { // from class: ea.l
            @Override // java.lang.Runnable
            public final void run() {
                HceService2.A0(this.f15814a, paymentCardTokenReference, monetaryAmountModel);
            }
        });
    }

    @Override // android.app.Service
    public void onCreate() {
        super.onCreate();
        wb.i iVar = wb.i.f25235a;
        String str = B;
        kotlin.jvm.internal.v.f(str, ProtectedMirApplication.s("ĉ"));
        iVar.q(str, new zq.a() { // from class: ea.p
            public final Object invoke() {
                return HceService2.s0();
            }
        });
    }

    @Override // android.nfc.cardemulation.HostApduService
    public void onDeactivated(int i10) {
        String strB;
        final String strS;
        g9.d dVar = this.f13872b;
        if (dVar == null || (strB = dVar.b()) == null) {
            strB = B;
        }
        if (i10 != 0) {
            strS = i10 != 1 ? ProtectedMirApplication.s("Ğ") : ProtectedMirApplication.s("ğ");
        } else {
            strS = ProtectedMirApplication.s("Ġ");
        }
        vk.e eVar = vk.e.a;
        String strS2 = ProtectedMirApplication.s("Ā");
        if (eVar.c(strS2)) {
            eVar.e(strS2, ProtectedMirApplication.s("ġ"));
            eVar.b(strS2);
            L(ProtectedMirApplication.s("Č"));
            wb.i iVar = wb.i.f25235a;
            kotlin.jvm.internal.v.d(strB);
            iVar.m(strB, new zq.a() { // from class: ea.b
                public final Object invoke() {
                    return HceService2.t0(strS);
                }
            });
        } else {
            wb.i iVar2 = wb.i.f25235a;
            kotlin.jvm.internal.v.d(strB);
            iVar2.q(strB, new zq.a() { // from class: ea.m
                public final Object invoke() {
                    return HceService2.u0(strS);
                }
            });
        }
        lb.g gVar = this.f13871a;
        if (gVar != null) {
            gVar.disconnect(strB);
        }
        this.f13871a = null;
        this.f13872b = null;
        d0().b();
    }

    @Override // android.app.Service
    public void onDestroy() {
        super.onDestroy();
        wb.i iVar = wb.i.f25235a;
        String str = B;
        kotlin.jvm.internal.v.f(str, ProtectedMirApplication.s("ĉ"));
        iVar.i(str, new zq.a() { // from class: ea.w
            public final Object invoke() {
                return HceService2.v0();
            }
        });
        e2.i(this.f13875e.getCoroutineContext(), (CancellationException) null, 1, (Object) null);
    }

    @Override // android.nfc.cardemulation.HostApduService
    public byte[] processCommandApdu(final byte[] bArr, Bundle bundle) {
        if (bArr == null) {
            return null;
        }
        if (this.f13871a == null) {
            wb.i iVar = wb.i.f25235a;
            String str = B;
            kotlin.jvm.internal.v.f(str, ProtectedMirApplication.s("ĉ"));
            iVar.i(str, new zq.a() { // from class: ea.q
                public final Object invoke() {
                    return HceService2.G0();
                }
            });
            vk.e eVar = vk.e.a;
            String strS = ProtectedMirApplication.s("Ģ");
            eVar.f(strS);
            this.f13871a = J0(bArr);
            eVar.b(strS);
        }
        vk.e eVar2 = vk.e.a;
        String strS2 = ProtectedMirApplication.s("Ā");
        if (!eVar2.c(strS2)) {
            eVar2.f(strS2);
            O0();
            d0().d();
        }
        g9.d dVarE = this.f13872b;
        if (dVarE == null) {
            dVarE = g9.a.f17269a.e(this);
        }
        this.f13872b = dVarE;
        wb.i iVar2 = wb.i.f25235a;
        iVar2.s(dVarE.b(), new zq.a() { // from class: ea.r
            public final Object invoke() {
                return HceService2.H0(bArr);
            }
        });
        final byte[] bArrP0 = P0(bArr, dVarE.b());
        iVar2.s(dVarE.b(), new zq.a() { // from class: ea.s
            public final Object invoke() {
                return HceService2.I0(bArrP0);
            }
        });
        return bArrP0;
    }
}
