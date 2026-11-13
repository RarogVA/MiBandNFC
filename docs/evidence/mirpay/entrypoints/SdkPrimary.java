package ea;

import android.os.Message;
import android.os.RemoteException;
import com.ekassir.mirpay.app.ProtectedMirApplication;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;
import java.util.concurrent.locks.ReentrantLock;
import kotlin.jvm.internal.r0;
import kq.l0;
import qr.o0;
import qr.p0;
import qr.q0;
import ru.nspk.wallet.core.data.model.configuration.AppStoreModel;
import ru.nspk.wallet.ui.screen.EnterCardScenario;
import s90.e1;
import x60.d1;
import x60.f0;

/* JADX INFO: loaded from: <historical-project>/validation/decompiled/phone-native/recovered-1.dex */
public final class b0 implements bv.a {
    public static final a Companion = new a(null);

    /* JADX INFO: renamed from: p, reason: collision with root package name */
    public static final int f15741p = 8;

    /* JADX INFO: renamed from: q, reason: collision with root package name */
    private static final String f15742q = b0.class.getSimpleName();

    /* JADX INFO: renamed from: a, reason: collision with root package name */
    private final kq.l f15743a;

    /* JADX INFO: renamed from: b, reason: collision with root package name */
    private final kq.l f15744b;

    /* JADX INFO: renamed from: c, reason: collision with root package name */
    private final kq.l f15745c;

    /* JADX INFO: renamed from: d, reason: collision with root package name */
    private final kq.l f15746d;

    /* JADX INFO: renamed from: e, reason: collision with root package name */
    private final kq.l f15747e;

    /* JADX INFO: renamed from: f, reason: collision with root package name */
    private final kq.l f15748f;

    /* JADX INFO: renamed from: g, reason: collision with root package name */
    private final kq.l f15749g;

    /* JADX INFO: renamed from: h, reason: collision with root package name */
    private final kq.l f15750h;

    /* JADX INFO: renamed from: i, reason: collision with root package name */
    private final kq.l f15751i;

    /* JADX INFO: renamed from: j, reason: collision with root package name */
    private final kq.l f15752j;

    /* JADX INFO: renamed from: k, reason: collision with root package name */
    private final kq.l f15753k;

    /* JADX INFO: renamed from: l, reason: collision with root package name */
    private final kq.l f15754l;

    /* JADX INFO: renamed from: m, reason: collision with root package name */
    private final o0 f15755m;

    /* JADX INFO: renamed from: n, reason: collision with root package name */
    private final List f15756n;

    /* JADX INFO: renamed from: o, reason: collision with root package name */
    private final ReentrantLock f15757o;

    public static final class a {
        private a() {
        }

        public /* synthetic */ a(kotlin.jvm.internal.m mVar) {
            this();
        }
    }

    public /* synthetic */ class b {

        /* JADX INFO: renamed from: a, reason: collision with root package name */
        public static final /* synthetic */ int[] f15758a;

        static {
            int[] iArr = new int[uc.c.a.values().length];
            try {
                iArr[uc.c.a.b.ordinal()] = 1;
            } catch (NoSuchFieldError unused) {
            }
            try {
                iArr[uc.c.a.d.ordinal()] = 2;
            } catch (NoSuchFieldError unused2) {
            }
            try {
                iArr[uc.c.a.g.ordinal()] = 3;
            } catch (NoSuchFieldError unused3) {
            }
            try {
                iArr[uc.c.a.h.ordinal()] = 4;
            } catch (NoSuchFieldError unused4) {
            }
            f15758a = iArr;
        }
    }

    static final class c extends kotlin.coroutines.jvm.internal.l implements zq.p {

        /* JADX INFO: renamed from: a, reason: collision with root package name */
        int f15759a;

        /* JADX INFO: renamed from: c, reason: collision with root package name */
        final /* synthetic */ Message f15761c;

        /* JADX WARN: 'super' call moved to the top of the method (can break code semantics) */
        c(Message message, pq.e eVar) {
            super(2, eVar);
            this.f15761c = message;
        }

        /* JADX INFO: renamed from: a, reason: merged with bridge method [inline-methods] */
        public final Object invoke(o0 o0Var, pq.e eVar) {
            return create(o0Var, eVar).invokeSuspend(l0.a);
        }

        public final pq.e create(Object obj, pq.e eVar) {
            return b0.this.new c(this.f15761c, eVar);
        }

        public final Object invokeSuspend(Object obj) {
            qq.b.f();
            if (this.f15759a != 0) {
                throw new IllegalStateException(ProtectedMirApplication.s("Ͻ"));
            }
            kq.v.b(obj);
            b0 b0Var = b0.this;
            Message message = this.f15761c;
            kotlin.jvm.internal.v.d(message);
            b0Var.x(message);
            return l0.a;
        }
    }

    static final class d extends kotlin.coroutines.jvm.internal.l implements zq.p {

        /* JADX INFO: renamed from: a, reason: collision with root package name */
        int f15762a;

        /* JADX INFO: renamed from: c, reason: collision with root package name */
        final /* synthetic */ Message f15764c;

        /* JADX INFO: renamed from: d, reason: collision with root package name */
        final /* synthetic */ uc.c f15765d;

        /* JADX WARN: 'super' call moved to the top of the method (can break code semantics) */
        d(Message message, uc.c cVar, pq.e eVar) {
            super(2, eVar);
            this.f15764c = message;
            this.f15765d = cVar;
        }

        /* JADX INFO: renamed from: a, reason: merged with bridge method [inline-methods] */
        public final Object invoke(o0 o0Var, pq.e eVar) {
            return create(o0Var, eVar).invokeSuspend(l0.a);
        }

        public final pq.e create(Object obj, pq.e eVar) {
            return b0.this.new d(this.f15764c, this.f15765d, eVar);
        }

        public final Object invokeSuspend(Object obj) {
            qq.b.f();
            if (this.f15762a != 0) {
                throw new IllegalStateException(ProtectedMirApplication.s("Ͼ"));
            }
            kq.v.b(obj);
            b0 b0Var = b0.this;
            Message message = this.f15764c;
            kotlin.jvm.internal.v.d(message);
            uc.c cVar = this.f15765d;
            kotlin.jvm.internal.v.d(cVar);
            b0Var.A(message, cVar);
            return l0.a;
        }
    }

    static final class e extends kotlin.coroutines.jvm.internal.l implements zq.p {

        /* JADX INFO: renamed from: a, reason: collision with root package name */
        int f15766a;

        /* JADX INFO: renamed from: c, reason: collision with root package name */
        final /* synthetic */ Message f15768c;

        /* JADX WARN: 'super' call moved to the top of the method (can break code semantics) */
        e(Message message, pq.e eVar) {
            super(2, eVar);
            this.f15768c = message;
        }

        /* JADX INFO: renamed from: a, reason: merged with bridge method [inline-methods] */
        public final Object invoke(o0 o0Var, pq.e eVar) {
            return create(o0Var, eVar).invokeSuspend(l0.a);
        }

        public final pq.e create(Object obj, pq.e eVar) {
            return b0.this.new e(this.f15768c, eVar);
        }

        public final Object invokeSuspend(Object obj) {
            qq.b.f();
            if (this.f15766a != 0) {
                throw new IllegalStateException(ProtectedMirApplication.s("Ͽ"));
            }
            kq.v.b(obj);
            b0 b0Var = b0.this;
            Message message = this.f15768c;
            kotlin.jvm.internal.v.d(message);
            b0Var.y(message);
            return l0.a;
        }
    }

    static final class f extends kotlin.coroutines.jvm.internal.l implements zq.p {

        /* JADX INFO: renamed from: a, reason: collision with root package name */
        int f15769a;

        /* JADX INFO: renamed from: c, reason: collision with root package name */
        final /* synthetic */ uc.c f15771c;

        /* JADX INFO: renamed from: d, reason: collision with root package name */
        final /* synthetic */ Message f15772d;

        /* JADX WARN: 'super' call moved to the top of the method (can break code semantics) */
        f(uc.c cVar, Message message, pq.e eVar) {
            super(2, eVar);
            this.f15771c = cVar;
            this.f15772d = message;
        }

        /* JADX INFO: renamed from: a, reason: merged with bridge method [inline-methods] */
        public final Object invoke(o0 o0Var, pq.e eVar) {
            return create(o0Var, eVar).invokeSuspend(l0.a);
        }

        public final pq.e create(Object obj, pq.e eVar) {
            return b0.this.new f(this.f15771c, this.f15772d, eVar);
        }

        public final Object invokeSuspend(Object obj) {
            qq.b.f();
            if (this.f15769a != 0) {
                throw new IllegalStateException(ProtectedMirApplication.s("Ѐ"));
            }
            kq.v.b(obj);
            b0 b0Var = b0.this;
            uc.c cVar = this.f15771c;
            kotlin.jvm.internal.v.d(cVar);
            if (b0Var.v(cVar)) {
                b0 b0Var2 = b0.this;
                Message message = this.f15772d;
                kotlin.jvm.internal.v.d(message);
                uc.c cVar2 = this.f15771c;
                kotlin.jvm.internal.v.d(cVar2);
                b0Var2.z(message, cVar2);
            } else {
                b0 b0Var3 = b0.this;
                Message message2 = this.f15772d;
                kotlin.jvm.internal.v.d(message2);
                b0Var3.C(message2);
            }
            return l0.a;
        }
    }

    public static final class g implements zq.a {

        /* JADX INFO: renamed from: a, reason: collision with root package name */
        final /* synthetic */ bv.a f15773a;

        /* JADX INFO: renamed from: b, reason: collision with root package name */
        final /* synthetic */ kv.a f15774b;

        /* JADX INFO: renamed from: c, reason: collision with root package name */
        final /* synthetic */ zq.a f15775c;

        public g(bv.a aVar, kv.a aVar2, zq.a aVar3) {
            this.f15773a = aVar;
            this.f15774b = aVar2;
            this.f15775c = aVar3;
        }

        public final Object invoke() {
            bv.a aVar = this.f15773a;
            return aVar.getKoin().g().d().f(r0.b(e1.class), this.f15774b, this.f15775c);
        }
    }

    public static final class h implements zq.a {

        /* JADX INFO: renamed from: a, reason: collision with root package name */
        final /* synthetic */ bv.a f15776a;

        /* JADX INFO: renamed from: b, reason: collision with root package name */
        final /* synthetic */ kv.a f15777b;

        /* JADX INFO: renamed from: c, reason: collision with root package name */
        final /* synthetic */ zq.a f15778c;

        public h(bv.a aVar, kv.a aVar2, zq.a aVar3) {
            this.f15776a = aVar;
            this.f15777b = aVar2;
            this.f15778c = aVar3;
        }

        public final Object invoke() {
            bv.a aVar = this.f15776a;
            return aVar.getKoin().g().d().f(r0.b(c20.b.class), this.f15777b, this.f15778c);
        }
    }

    public static final class i implements zq.a {

        /* JADX INFO: renamed from: a, reason: collision with root package name */
        final /* synthetic */ bv.a f15779a;

        /* JADX INFO: renamed from: b, reason: collision with root package name */
        final /* synthetic */ kv.a f15780b;

        /* JADX INFO: renamed from: c, reason: collision with root package name */
        final /* synthetic */ zq.a f15781c;

        public i(bv.a aVar, kv.a aVar2, zq.a aVar3) {
            this.f15779a = aVar;
            this.f15780b = aVar2;
            this.f15781c = aVar3;
        }

        public final Object invoke() {
            bv.a aVar = this.f15779a;
            return aVar.getKoin().g().d().f(r0.b(cb0.a.class), this.f15780b, this.f15781c);
        }
    }

    public static final class j implements zq.a {

        /* JADX INFO: renamed from: a, reason: collision with root package name */
        final /* synthetic */ bv.a f15782a;

        /* JADX INFO: renamed from: b, reason: collision with root package name */
        final /* synthetic */ kv.a f15783b;

        /* JADX INFO: renamed from: c, reason: collision with root package name */
        final /* synthetic */ zq.a f15784c;

        public j(bv.a aVar, kv.a aVar2, zq.a aVar3) {
            this.f15782a = aVar;
            this.f15783b = aVar2;
            this.f15784c = aVar3;
        }

        public final Object invoke() {
            bv.a aVar = this.f15782a;
            return aVar.getKoin().g().d().f(r0.b(cb0.c.class), this.f15783b, this.f15784c);
        }
    }

    public static final class k implements zq.a {

        /* JADX INFO: renamed from: a, reason: collision with root package name */
        final /* synthetic */ bv.a f15785a;

        /* JADX INFO: renamed from: b, reason: collision with root package name */
        final /* synthetic */ kv.a f15786b;

        /* JADX INFO: renamed from: c, reason: collision with root package name */
        final /* synthetic */ zq.a f15787c;

        public k(bv.a aVar, kv.a aVar2, zq.a aVar3) {
            this.f15785a = aVar;
            this.f15786b = aVar2;
            this.f15787c = aVar3;
        }

        public final Object invoke() {
            bv.a aVar = this.f15785a;
            return aVar.getKoin().g().d().f(r0.b(pa.c.class), this.f15786b, this.f15787c);
        }
    }

    public static final class l implements zq.a {

        /* JADX INFO: renamed from: a, reason: collision with root package name */
        final /* synthetic */ bv.a f15788a;

        /* JADX INFO: renamed from: b, reason: collision with root package name */
        final /* synthetic */ kv.a f15789b;

        /* JADX INFO: renamed from: c, reason: collision with root package name */
        final /* synthetic */ zq.a f15790c;

        public l(bv.a aVar, kv.a aVar2, zq.a aVar3) {
            this.f15788a = aVar;
            this.f15789b = aVar2;
            this.f15790c = aVar3;
        }

        public final Object invoke() {
            bv.a aVar = this.f15788a;
            return aVar.getKoin().g().d().f(r0.b(m40.g.class), this.f15789b, this.f15790c);
        }
    }

    public static final class m implements zq.a {

        /* JADX INFO: renamed from: a, reason: collision with root package name */
        final /* synthetic */ bv.a f15791a;

        /* JADX INFO: renamed from: b, reason: collision with root package name */
        final /* synthetic */ kv.a f15792b;

        /* JADX INFO: renamed from: c, reason: collision with root package name */
        final /* synthetic */ zq.a f15793c;

        public m(bv.a aVar, kv.a aVar2, zq.a aVar3) {
            this.f15791a = aVar;
            this.f15792b = aVar2;
            this.f15793c = aVar3;
        }

        public final Object invoke() {
            bv.a aVar = this.f15791a;
            return aVar.getKoin().g().d().f(r0.b(x60.p.class), this.f15792b, this.f15793c);
        }
    }

    public static final class n implements zq.a {

        /* JADX INFO: renamed from: a, reason: collision with root package name */
        final /* synthetic */ bv.a f15794a;

        /* JADX INFO: renamed from: b, reason: collision with root package name */
        final /* synthetic */ kv.a f15795b;

        /* JADX INFO: renamed from: c, reason: collision with root package name */
        final /* synthetic */ zq.a f15796c;

        public n(bv.a aVar, kv.a aVar2, zq.a aVar3) {
            this.f15794a = aVar;
            this.f15795b = aVar2;
            this.f15796c = aVar3;
        }

        public final Object invoke() {
            bv.a aVar = this.f15794a;
            return aVar.getKoin().g().d().f(r0.b(z30.b.class), this.f15795b, this.f15796c);
        }
    }

    public static final class o implements zq.a {

        /* JADX INFO: renamed from: a, reason: collision with root package name */
        final /* synthetic */ bv.a f15797a;

        /* JADX INFO: renamed from: b, reason: collision with root package name */
        final /* synthetic */ kv.a f15798b;

        /* JADX INFO: renamed from: c, reason: collision with root package name */
        final /* synthetic */ zq.a f15799c;

        public o(bv.a aVar, kv.a aVar2, zq.a aVar3) {
            this.f15797a = aVar;
            this.f15798b = aVar2;
            this.f15799c = aVar3;
        }

        public final Object invoke() {
            bv.a aVar = this.f15797a;
            return aVar.getKoin().g().d().f(r0.b(a40.b.class), this.f15798b, this.f15799c);
        }
    }

    public static final class p implements zq.a {

        /* JADX INFO: renamed from: a, reason: collision with root package name */
        final /* synthetic */ bv.a f15800a;

        /* JADX INFO: renamed from: b, reason: collision with root package name */
        final /* synthetic */ kv.a f15801b;

        /* JADX INFO: renamed from: c, reason: collision with root package name */
        final /* synthetic */ zq.a f15802c;

        public p(bv.a aVar, kv.a aVar2, zq.a aVar3) {
            this.f15800a = aVar;
            this.f15801b = aVar2;
            this.f15802c = aVar3;
        }

        public final Object invoke() {
            bv.a aVar = this.f15800a;
            return aVar.getKoin().g().d().f(r0.b(f0.class), this.f15801b, this.f15802c);
        }
    }

    public static final class q implements zq.a {

        /* JADX INFO: renamed from: a, reason: collision with root package name */
        final /* synthetic */ bv.a f15803a;

        /* JADX INFO: renamed from: b, reason: collision with root package name */
        final /* synthetic */ kv.a f15804b;

        /* JADX INFO: renamed from: c, reason: collision with root package name */
        final /* synthetic */ zq.a f15805c;

        public q(bv.a aVar, kv.a aVar2, zq.a aVar3) {
            this.f15803a = aVar;
            this.f15804b = aVar2;
            this.f15805c = aVar3;
        }

        public final Object invoke() {
            bv.a aVar = this.f15803a;
            return aVar.getKoin().g().d().f(r0.b(d1.class), this.f15804b, this.f15805c);
        }
    }

    public b0() {
        rv.a aVar = rv.a.a;
        this.f15743a = kq.m.a(aVar.b(), new i(this, null, null));
        this.f15744b = kq.m.a(aVar.b(), new j(this, null, null));
        this.f15745c = kq.m.a(aVar.b(), new k(this, null, null));
        this.f15746d = kq.m.a(aVar.b(), new l(this, null, null));
        this.f15747e = kq.m.a(aVar.b(), new m(this, null, null));
        this.f15748f = kq.m.a(aVar.b(), new n(this, null, null));
        this.f15749g = kq.m.a(aVar.b(), new o(this, null, null));
        this.f15750h = kq.m.a(aVar.b(), new p(this, null, null));
        this.f15751i = kq.m.a(aVar.b(), new q(this, null, null));
        this.f15752j = kq.m.a(aVar.b(), new g(this, null, null));
        this.f15753k = kq.m.a(aVar.b(), new h(this, null, null));
        this.f15754l = kq.m.b(new zq.a() { // from class: ea.a0
            public final Object invoke() {
                return b0.b(this.f15739a);
            }
        });
        this.f15755m = p0.a(qr.e1.b());
        this.f15756n = new ArrayList();
        this.f15757o = new ReentrantLock();
    }

    /* JADX INFO: Access modifiers changed from: private */
    public final void A(Message message, uc.c cVar) {
        Message messageB;
        w50.a aVarA = o().a(cVar);
        if (aVarA.b() == uc.d.a) {
            vc.d dVarA = aVarA.a();
            if (dVarA == null) {
                throw new IllegalStateException(ProtectedMirApplication.s("Ѓ"));
            }
            String strB = dVarA.b();
            kotlin.jvm.internal.v.f(strB, ProtectedMirApplication.s("Ё"));
            w(d00.a.b(strB));
            messageB = uc.b.d(wc.h.f(dVarA));
            kotlin.jvm.internal.v.f(messageB, ProtectedMirApplication.s("Ђ"));
        } else {
            messageB = uc.b.b(aVarA.b());
            kotlin.jvm.internal.v.f(messageB, ProtectedMirApplication.s("Є"));
        }
        B(message, messageB);
    }

    private final void B(Message message, Message message2) {
        ReentrantLock reentrantLock = this.f15757o;
        reentrantLock.lock();
        try {
            try {
                message2.arg1 = message.arg1;
                message.replyTo.send(message2);
            } catch (RemoteException e10) {
                sk.d.o(f15742q, e10);
            }
            l0 l0Var = l0.a;
        } finally {
            reentrantLock.unlock();
        }
    }

    /* JADX INFO: Access modifiers changed from: private */
    public final void C(Message message) {
        Message messageB = uc.b.b(uc.d.i);
        kotlin.jvm.internal.v.d(messageB);
        B(message, messageB);
    }

    /* JADX INFO: Access modifiers changed from: private */
    public static final w50.b b(b0 b0Var) {
        return new w50.b(b0Var.m(), b0Var.t(), b0Var.j(), b0Var.k(), b0Var.l(), b0Var.p(), b0Var.s(), b0Var.q(), b0Var.r());
    }

    private final cb0.a i() {
        return (cb0.a) this.f15743a.getValue();
    }

    private final x60.p j() {
        return (x60.p) this.f15747e.getValue();
    }

    private final z30.b k() {
        return (z30.b) this.f15748f.getValue();
    }

    private final a40.b l() {
        return (a40.b) this.f15749g.getValue();
    }

    private final pa.c m() {
        return (pa.c) this.f15745c.getValue();
    }

    private final cb0.c n() {
        return (cb0.c) this.f15744b.getValue();
    }

    private final w50.b o() {
        return (w50.b) this.f15754l.getValue();
    }

    private final f0 p() {
        return (f0) this.f15750h.getValue();
    }

    private final e1 q() {
        return (e1) this.f15752j.getValue();
    }

    private final c20.b r() {
        return (c20.b) this.f15753k.getValue();
    }

    private final d1 s() {
        return (d1) this.f15751i.getValue();
    }

    private final m40.g t() {
        return (m40.g) this.f15746d.getValue();
    }

    /* JADX INFO: Access modifiers changed from: private */
    public final boolean v(uc.c cVar) {
        String strC = cVar.c();
        if (strC == null || ir.q.p0(strC)) {
            return false;
        }
        String strB = d00.a.b(strC);
        ReentrantLock reentrantLock = this.f15757o;
        reentrantLock.lock();
        try {
            return this.f15756n.contains(d00.a.a(strB));
        } finally {
            reentrantLock.unlock();
        }
    }

    private final void w(String str) {
        ReentrantLock reentrantLock = this.f15757o;
        reentrantLock.lock();
        try {
            this.f15756n.add(d00.a.a(str));
        } finally {
            reentrantLock.unlock();
        }
    }

    /* JADX INFO: Access modifiers changed from: private */
    public final void x(Message message) {
        String strA;
        Message messageB;
        List appStores = k().h().getAppStores();
        if (appStores == null) {
            appStores = lq.w.n();
        }
        List list = appStores;
        ArrayList arrayList = new ArrayList(lq.w.y(list, 10));
        Iterator it = list.iterator();
        while (true) {
            strA = null;
            if (it.hasNext()) {
                AppStoreModel appStoreModel = (AppStoreModel) it.next();
                arrayList.add(new a00.c(a00.a.a(appStoreModel.getAppStoreId-ju-TO2s()), appStoreModel.getAppStoreName(), appStoreModel.getLink(), (kotlin.jvm.internal.m) null));
            } else {
                try {
                    break;
                } catch (vc.e e10) {
                    sk.d.o(f15742q, e10);
                }
            }
        }
        strA = a00.b.a.a(arrayList);
        if (strA != null) {
            messageB = uc.b.d(strA);
            kotlin.jvm.internal.v.f(messageB, ProtectedMirApplication.s("Ђ"));
        } else {
            messageB = uc.b.b(uc.d.i);
            kotlin.jvm.internal.v.f(messageB, ProtectedMirApplication.s("Є"));
        }
        B(message, messageB);
    }

    /* JADX INFO: Access modifiers changed from: private */
    public final void y(Message message) {
        Message messageC = uc.b.c(i().a(EnterCardScenario.Sdk.a));
        kotlin.jvm.internal.v.d(messageC);
        B(message, messageC);
    }

    /* JADX INFO: Access modifiers changed from: private */
    public final void z(Message message, uc.c cVar) {
        e00.a aVarA;
        Message messageB;
        String strB = cVar.b();
        try {
            e00.b bVar = e00.b.a;
            kotlin.jvm.internal.v.d(strB);
            aVarA = bVar.a(strB);
        } catch (vc.e e10) {
            sk.d.o(f15742q, e10);
            aVarA = null;
        }
        if (aVarA != null) {
            messageB = uc.b.c(n().a(aVarA.a()));
            kotlin.jvm.internal.v.f(messageB, ProtectedMirApplication.s("Ѕ"));
        } else {
            messageB = uc.b.b(uc.d.i);
            kotlin.jvm.internal.v.f(messageB, ProtectedMirApplication.s("Є"));
        }
        B(message, messageB);
    }

    public av.a getKoin() {
        return bv.a.a.a(this);
    }

    public final void u(Message message) {
        kotlin.jvm.internal.v.g(message, ProtectedMirApplication.s("І"));
        Message messageObtain = Message.obtain(message);
        uc.c cVarB = uc.a.b(messageObtain);
        uc.c.a aVarA = cVarB.a();
        int i10 = aVarA == null ? -1 : b.f15758a[aVarA.ordinal()];
        if (i10 == 1) {
            qr.i.d(this.f15755m, (pq.i) null, (q0) null, new c(messageObtain, null), 3, (Object) null);
            return;
        }
        if (i10 == 2) {
            qr.i.d(this.f15755m, (pq.i) null, (q0) null, new d(messageObtain, cVarB, null), 3, (Object) null);
            return;
        }
        if (i10 == 3) {
            qr.i.d(this.f15755m, (pq.i) null, (q0) null, new e(messageObtain, null), 3, (Object) null);
        } else {
            if (i10 == 4) {
                qr.i.d(this.f15755m, (pq.i) null, (q0) null, new f(cVarB, messageObtain, null), 3, (Object) null);
                return;
            }
            throw new UnsupportedOperationException(ProtectedMirApplication.s("Ї") + aVarA);
        }
    }
}
