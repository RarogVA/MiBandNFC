package dev.mbapdu.probe;

import java.util.Arrays;

/** A matching RAPDU alone is not proof. Requires a live BT request and completed ATT writes. */
public final class ProofJournal {
    public static final class Run {
        public final long id, session;
        public final byte[] challenge;
        private Run(long id, long session, byte[] challenge) {
            this.id = id; this.session = session; this.challenge = challenge.clone();
        }
    }
    public static final class Receipt {
        private final Run run;
        public final long sequence;
        private boolean written;
        private Receipt(Run run, long sequence) { this.run = run; this.sequence = sequence; }
    }
    private long session, nextRun;
    private boolean connected;
    private Run current;
    private Receipt receipt;
    private int received;

    public synchronized void connected() { session++; connected = true; invalidateRun(); }
    public synchronized void disconnected() { session++; connected = false; invalidateRun(); }
    public synchronized void invalidateRun() { current = null; receipt = null; received = 0; notifyAll(); }
    public synchronized Run begin(byte[] challenge) {
        if (!connected) throw new IllegalStateException("BT не готов");
        if (challenge.length != 16) throw new IllegalArgumentException("challenge length");
        current = new Run(++nextRun, session, challenge); receipt = null; received = 0;
        return current;
    }
    /** Call exclusively from transport notification ingress, never from the NFC worker. */
    public synchronized Receipt receivedOverBluetooth(long sequence, byte[] capdu) {
        if (!connected || current == null || !Protocol.isChallenge(capdu) ||
                !Arrays.equals(Protocol.challengeApdu(current.challenge), capdu)) return null;
        received++;
        Receipt r = new Receipt(current, sequence);
        if (received == 1) receipt = r;
        notifyAll();
        return r;
    }
    /** Call only after successful onCharacteristicWrite for the LAST frame fragment. */
    public synchronized void completedBluetoothWrite(Receipt r, boolean success) {
        if (r != null && r == receipt && r.run == current && success && connected && r.run.session == session)
            r.written = true;
        notifyAll();
    }
    public synchronized String verify(Run run, byte[] rapdu, long waitMillis) throws InterruptedException {
        if (!Arrays.equals(Protocol.expectedResponse(run.challenge), rapdu)) return "RAPDU: ответ не совпал с challenge";
        long deadline = System.nanoTime() + Math.max(0, waitMillis) * 1_000_000L;
        while (run == current && connected && run.session == session && received == 1 &&
                receipt != null && !receipt.written && System.nanoTime() < deadline) {
            long remaining = deadline - System.nanoTime();
            if (remaining > 0) wait(Math.max(1, remaining / 1_000_000L));
        }
        if (!connected || run != current || run.session != session) return "PROOF: сеанс сменился или тест остановлен";
        if (received != 1 || receipt == null) return "PROOF: требуется ровно один свежий challenge через Bluetooth (получено " + received + ")";
        if (!receipt.written) return "PROOF: нет подтверждения последнего ATT write";
        return null;
    }
    public synchronized long sequence(Run run) { return run == current && receipt != null ? receipt.sequence : -1; }
}
