Mir Pay ordinary-app HCE access check — 0.1

Package: dev.mbapdu.compat
Activity: dev.mbapdu.compat/.MainActivity
Build from the project root: python3 diagnostics/mirpay-access/build.py
APK: diagnostics/mirpay-access/build/mirpay-access-check-debug.apk
Preserved tested APK: releases/mirpay-access-check-0.1.apk
Recorded result: docs/evidence/mirpay/pixel-mirpay-access-report.json

The APK declares no permissions and names only ru.nspk.mirpay in package
visibility queries. It reads public package/service/permission metadata,
resolves the target HOST_APDU_SERVICE, and attempts one ordinary explicit
bindService call. It does not call or inspect the returned IBinder, send
APDUs, read cards, connect Bluetooth, or issue payment requests. No modified
Mir Pay APK, privilege grant or device setup change is involved.

If binding is accepted, the first service callback or a three-second timeout
ends the check and unbinds. A SecurityException is recorded without retry.
Missing metadata/query results, ambiguous endpoints, rejected binding,
null binding, timeout and successful connection are distinct outcomes.
probe_stage identifies the operation that failed. The permission definition
includes its protection level/base/flags; caller permission and exact HCE
service requirement are recorded independently.

Runtime report, written only by the APK when it runs:
  adb shell run-as dev.mbapdu.compat cat files/report.json
Use your configured ADB server port if it differs from the default. The
diagnostic must first be installed and opened manually; the saved report
in this project does not require access to a phone.
Own logcat tag: MBAPDU_COMPAT

The build script never installs or runs the APK. It verifies SDK compilation,
APK signature, alignment, package name and the absence of permissions. It
reuses .local/signing/lab-debug.keystore, creating a new local debug identity
only when that key is absent. The key is never bundled. Its original copy
must be retained to update an already installed APK with the same signature.
build/build-result.json records the new local build and source hashes; it
does not overwrite or confer hardware-tested status on the frozen release.

Interpretation limits:
- A denied bind tests this ordinary app and the currently installed target.
- A successful bind alone would not establish an APDU API or payment ability.
- A package/query failure is not evidence of a permission denial.
- No real payment or card operation is performed by this diagnostic.
