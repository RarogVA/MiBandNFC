package w50;

import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;
import java.util.UUID;
import kb.i;
import kotlin.jvm.internal.m;
import kotlin.jvm.internal.v;
import lq.w;
import m40.g;
import pa.c;
import ru.nspk.wallet.core.data.model.registration.DeviceId;
import ru.nspk.wallet.core.data.model.registration.WalletId;
import ru.nspk.wallet.core.data.model.registration.token.TokenResponseModel;
import ru.nspk.wallet.core.security.cryptography.JwsToken;
import ru.nspk.wallet.data.model.integrity.DexEnvironmentState;
import s90.e1;
import uc.d;
import vc.e;
import x60.d1;
import x60.f0;
import x60.p;

/* JADX INFO: loaded from: <historical-project>/validation/decompiled/phone-native/recovered-3.dex */
public final class b {
    public static final a Companion = new a(null);

    /* JADX INFO: renamed from: h, reason: collision with root package name */
    public static final int f60186h = 8;

    /* JADX INFO: renamed from: i, reason: collision with root package name */
    private static final String f60187i = b.class.getSimpleName();

    /* JADX INFO: renamed from: a, reason: collision with root package name */
    private final c f60188a;

    /* JADX INFO: renamed from: b, reason: collision with root package name */
    private final p f60189b;

    /* JADX INFO: renamed from: c, reason: collision with root package name */
    private final a40.b f60190c;

    /* JADX INFO: renamed from: d, reason: collision with root package name */
    private final f0 f60191d;

    /* JADX INFO: renamed from: e, reason: collision with root package name */
    private final d1 f60192e;

    /* JADX INFO: renamed from: f, reason: collision with root package name */
    private final e1 f60193f;

    /* JADX INFO: renamed from: g, reason: collision with root package name */
    private final y50.c f60194g;

    public static final class a {
        private a() {
        }

        public /* synthetic */ a(m mVar) {
            this();
        }
    }

    /* JADX INFO: renamed from: w50.b$b, reason: collision with other inner class name */
    public /* synthetic */ class C0962b {

        /* JADX INFO: renamed from: a, reason: collision with root package name */
        public static final /* synthetic */ int[] f60195a;

        static {
            int[] iArr = new int[uc.c.a.values().length];
            try {
                iArr[uc.c.a.f57553d.ordinal()] = 1;
            } catch (NoSuchFieldError unused) {
            }
            f60195a = iArr;
        }
    }

    public b(c environmentSecurityManager, g x509CertificateProvider, p cardTokensRepository, z30.b configurationRepository, a40.b deviceRegisteredStateRepository, f0 installationInfoRepository, d1 updateRequiredFlagRepository, e1 paymentCardViewModelFactory, c20.b tokenValidatorDevMode) {
        v.g(environmentSecurityManager, "environmentSecurityManager");
        v.g(x509CertificateProvider, "x509CertificateProvider");
        v.g(cardTokensRepository, "cardTokensRepository");
        v.g(configurationRepository, "configurationRepository");
        v.g(deviceRegisteredStateRepository, "deviceRegisteredStateRepository");
        v.g(installationInfoRepository, "installationInfoRepository");
        v.g(updateRequiredFlagRepository, "updateRequiredFlagRepository");
        v.g(paymentCardViewModelFactory, "paymentCardViewModelFactory");
        v.g(tokenValidatorDevMode, "tokenValidatorDevMode");
        this.f60188a = environmentSecurityManager;
        this.f60189b = cardTokensRepository;
        this.f60190c = deviceRegisteredStateRepository;
        this.f60191d = installationInfoRepository;
        this.f60192e = updateRequiredFlagRepository;
        this.f60193f = paymentCardViewModelFactory;
        this.f60194g = new y50.c(x509CertificateProvider, configurationRepository, tokenValidatorDevMode);
    }

    private final w50.a b(uc.c cVar) {
        e00.c cVarA;
        d dVar;
        String strB = cVar.b();
        vc.d dVar2 = null;
        try {
            e00.d dVar3 = e00.d.f29423a;
            v.d(strB);
            cVarA = dVar3.a(strB);
        } catch (e e11) {
            sk.d.o(f60187i, e11);
            cVarA = null;
        }
        if (cVarA == null) {
            dVar = d.f57567i;
        } else if (f(cVarA)) {
            dVar = d.f57564f;
        } else if (g(cVarA)) {
            dVar = d.f57565g;
        } else if (this.f60190c.S2()) {
            dVar = d.f57561c;
        } else if (this.f60192e.r1()) {
            dVar = d.f57562d;
        } else if (c()) {
            dVar = d.f57563e;
        } else if (d(cVarA)) {
            dVar = d.f57568j;
        } else {
            dVar = e() ? d.f57569k : d.f57559a;
        }
        if (dVar == d.f57559a) {
            String string = UUID.randomUUID().toString();
            v.f(string, "toString(...)");
            WalletId walletIdJ = this.f60191d.J();
            String string2 = walletIdJ != null ? walletIdJ.toString() : null;
            DeviceId deviceIdG0 = this.f60191d.G0();
            dVar2 = new vc.d(string, string2, deviceIdG0 != null ? deviceIdG0.toString() : null);
        }
        return new w50.a(dVar, dVar2);
    }

    private final boolean c() {
        boolean z11 = false;
        DexEnvironmentState dexEnvironmentStateC = this.f60188a.c(false);
        boolean zB = this.f60188a.b();
        if (!dexEnvironmentStateC.g() && zB) {
            z11 = true;
        }
        return !z11;
    }

    private final boolean d(e00.c cVar) {
        return !this.f60194g.isValid(JwsToken.a(JwsToken.b(cVar.a())));
    }

    private final boolean e() {
        List listX1 = this.f60189b.X1();
        ArrayList arrayList = new ArrayList(w.y(listX1, 10));
        Iterator it = listX1.iterator();
        while (it.hasNext()) {
            arrayList.add(this.f60193f.a((TokenResponseModel) it.next()));
        }
        ArrayList arrayList2 = new ArrayList();
        int size = arrayList.size();
        int i11 = 0;
        while (i11 < size) {
            Object obj = arrayList.get(i11);
            i11++;
            if (((i) obj).a() == j60.b.ACTIVE) {
                arrayList2.add(obj);
            }
        }
        return arrayList2.isEmpty();
    }

    private final boolean f(e00.c cVar) {
        return cVar.b() < 1;
    }

    private final boolean g(e00.c cVar) {
        return cVar.b() > 1;
    }

    public final w50.a a(uc.c serviceCall) {
        v.g(serviceCall, "serviceCall");
        uc.c.a aVarA = serviceCall.a();
        if ((aVarA == null ? -1 : C0962b.f60195a[aVarA.ordinal()]) == 1) {
            return b(serviceCall);
        }
        throw new IllegalArgumentException("Unsupported call type: " + aVarA);
    }
}
