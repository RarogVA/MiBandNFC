package dev.mbapdu.probe;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.atomic.AtomicReference;

/** JVM tests for wire boundaries and anti-false-positive evidence, not Android mocks. */
public final class ProtocolTest {
    private static int assertions;
    private static void check(boolean ok, String message) {
        assertions++; if (!ok) throw new AssertionError(message);
    }
    private static void rejects(Runnable r, String name) {
        boolean rejected = false;
        try { r.run(); } catch (IllegalArgumentException expected) { rejected = true; }
        check(rejected, name);
    }
    public static void main(String[] args) throws Exception {
        byte[] challenge = Protocol.hex("000102030405060708090A0B0C0D0E0F");
        byte[] expected = Protocol.hex("AAABA8A9AEAFACADA2A3A0A1A6A7A4A59000");
        byte[] command = Protocol.challengeApdu(challenge);
        check(Protocol.SELECT.length == 14 && Protocol.SELECT[4] == 8, "AID is eight bytes");
        check(Protocol.hex(command).equals("8010000010000102030405060708090A0B0C0D0E0F00"), "challenge CAPDU vector");
        check(Arrays.equals(Protocol.respond(command), expected), "independent reversed XOR vector");
        check(Arrays.equals(Protocol.respond(Protocol.SELECT), Protocol.OK), "SELECT exact whitelist");
        byte[] otherAid = Protocol.SELECT.clone(); otherAid[6] ^= 1;
        check(!Arrays.equals(Protocol.respond(otherAid), Protocol.OK), "other AID is not selected");
        byte[] malformed = command.clone(); malformed[4] = 15;
        check(!Protocol.isChallenge(malformed), "bad Lc");
        check(!Protocol.isChallenge(Arrays.copyOf(command, command.length - 1)), "missing Le");

        Protocol.Frame frame = new Protocol.Frame(Protocol.REQUEST, 0xfedcba98L, command);
        byte[] wire = Protocol.encode(frame);
        check(Protocol.hex(Arrays.copyOf(wire, 10)).equals("4D420101FEDCBA980016"), "wire header independent vector");
        for (int cut = 1; cut < wire.length; cut++) {
            Protocol.Parser parser = new Protocol.Parser(); List<Protocol.Frame> frames = new ArrayList<>();
            parser.feed(Arrays.copyOfRange(wire, 0, cut), frames::add);
            check(frames.isEmpty() && parser.hasPartialFrame(), "split remains partial " + cut);
            parser.feed(Arrays.copyOfRange(wire, cut, wire.length), frames::add);
            check(frames.size() == 1 && frames.get(0).sequence == frame.sequence &&
                    Arrays.equals(frames.get(0).payload, command) && !parser.hasPartialFrame(), "split roundtrip " + cut);
        }
        Protocol.Parser parser = new Protocol.Parser(); List<Protocol.Frame> frames = new ArrayList<>();
        for (byte b : wire) parser.feed(new byte[]{b}, frames::add);
        check(frames.size() == 1, "one-byte fragmentation");
        byte[] two = new byte[wire.length * 2]; System.arraycopy(wire, 0, two, 0, wire.length);
        System.arraycopy(wire, 0, two, wire.length, wire.length);
        frames.clear(); parser.feed(two, frames::add); check(frames.size() == 2, "concatenated frames");
        byte[] bad = Arrays.copyOf(wire, 10); bad[8] = 0x7f; bad[9] = (byte) 0xff;
        rejects(() -> parser.feed(bad, f -> {}), "oversize rejected at header, before allocation");
        check(!parser.hasPartialFrame(), "malformed header resets parser");
        byte[] badMagic = wire.clone(); badMagic[0] = 0;
        rejects(() -> parser.feed(badMagic, f -> {}), "bad magic rejected");
        byte[] badVersion = wire.clone(); badVersion[2] = 2;
        rejects(() -> parser.feed(badVersion, f -> {}), "unknown version rejected");
        rejects(() -> new Protocol.Frame(Protocol.RESPONSE, 1, new byte[259]), "response boundary");
        rejects(() -> new Protocol.Frame(Protocol.RESPONSE, 1, new byte[1]), "response requires status word");
        rejects(() -> new Protocol.Frame(Protocol.REQUEST, 1, new byte[262]), "request boundary");
        rejects(() -> new Protocol.Frame(Protocol.STATUS, 1, new byte[129]), "status boundary");
        rejects(() -> new Protocol.Frame(Protocol.CONTROL, 1, new byte[2]), "control one-byte boundary");
        rejects(() -> new Protocol.Frame(Protocol.CONTROL, 1, new byte[0]), "empty control rejected");
        rejects(() -> new Protocol.Frame(5, 1, new byte[1]), "unknown kind");
        for (int kind = 1; kind <= 4; kind++) {
            int size = kind == 1 ? 261 : kind == 2 ? 258 : kind == 3 ? 128 : 1;
            byte[] encoded = Protocol.encode(new Protocol.Frame(kind, 0xffffffffL, new byte[size]));
            frames.clear();
            for (int i = 0; i < encoded.length; i += 20)
                parser.feed(Arrays.copyOfRange(encoded, i, Math.min(i + 20, encoded.length)), frames::add);
            check(frames.size() == 1 && frames.get(0).kind == kind && frames.get(0).payload.length == size, "ATT20 max kind " + kind);
        }
        parser.feed(Arrays.copyOf(wire, 7), f -> {}); parser.reset();
        frames.clear(); parser.feed(wire, frames::add); check(frames.size() == 1, "reset after disconnect boundary");

        ControlPreflight gate = new ControlPreflight();
        check(!gate.verified() && !gate.pending(), "no preflight evidence initially");
        gate.begin(349066985L);
        check(Protocol.hex(Protocol.encode(new Protocol.Frame(Protocol.CONTROL, 349066985L, new byte[]{0})))
                .equals("4D42010414CE56E9000100"), "QUERY wire uses zero control and exact uint32 sequence");
        gate.status(349066984L, "STOPPED"); gate.writeCompleted(349066984L, true);
        check(!gate.verified(), "wrong sequence cannot verify BLE");
        gate.status(349066985L, "STOPPED");
        check(gate.pending() && !gate.verified(), "STOPPED alone cannot verify BLE");
        gate.writeCompleted(349066985L, true);
        check(gate.verified(), "status before ATT completion verifies after both");
        gate.begin(2); gate.writeCompleted(2, true);
        check(!gate.verified(), "ATT completion alone cannot verify BLE");
        gate.status(2, "STOPPED"); check(gate.verified(), "ATT before status verifies after both");
        gate.begin(3); gate.status(3, "READY"); gate.writeCompleted(3, true);
        check(!gate.verified() && !gate.pending(), "READY invalidates idle preflight");
        gate.begin(4); gate.status(4, "ERROR:recovery"); gate.writeCompleted(4, true);
        check(!gate.verified(), "ERROR cannot enable NFC start");
        gate.begin(5); gate.status(5, "STOPPED"); gate.writeCompleted(5, false);
        check(!gate.verified(), "failed ATT invalidates STOPPED receipt");
        gate.begin(6); gate.status(6, "STOPPED"); gate.begin(7); gate.writeCompleted(6, true);
        check(!gate.verified(), "old write completion cannot verify a replacement query");
        gate.writeCompleted(7, true); gate.reset(); gate.status(7, "STOPPED");
        check(!gate.verified(), "disconnect/timeout reset rejects late status");
        rejects(() -> gate.begin(0x100000000L), "preflight sequence must be uint32");

        InfoRequest info = new InfoRequest();
        check(!info.pending() && !info.verified(), "INFO starts without evidence");
        byte[] infoWire = Protocol.encode(new Protocol.Frame(Protocol.CONTROL, 0xdeadbeefL, new byte[]{3}));
        check(Protocol.hex(infoWire).equals("4D420104DEADBEEF000103"), "read-only INFO wire vector");
        frames.clear(); parser.feed(infoWire, frames::add);
        check(frames.size() == 1 && frames.get(0).payload[0] == 3, "CONTROL03 survives parser");
        info.begin(10);
        info.status(9, "INFO:caps"); info.writeCompleted(9, true);
        check(!info.verified(), "wrong sequence cannot confirm INFO");
        info.status(10, "INFO:v=20");
        check(info.pending() && info.value() == null, "INFO payload alone is not confirmed");
        info.writeCompleted(10, true);
        check(info.verified() && "v=20".equals(info.value()), "INFO before ATT confirms after both");
        info.finish(); info.status(10, "INFO:late"); info.writeCompleted(10, true);
        check(!info.verified() && !info.pending() && info.matches(10), "finished INFO consumes late replies without reopening");
        info.begin(11); info.writeCompleted(11, true);
        check(!info.verified(), "INFO ATT alone is insufficient");
        info.status(11, "INFO:features=00000800");
        check(info.verified(), "ATT before INFO confirms after both");
        for (String invalid : new String[]{"STOPPED", "READY", "ERROR:unsupported", "INFO:", "CAPS:v=20"}) {
            info.begin(12); info.writeCompleted(12, true); info.status(12, invalid);
            check(!info.verified() && !info.pending(), "non-INFO or empty response fails closed: " + invalid);
        }
        info.begin(13); info.status(13, "INFO:caps"); info.writeCompleted(13, false);
        check(!info.verified() && !info.pending(), "failed ATT invalidates INFO");
        info.begin(14); info.writeCompleted(14, true); info.finish(); info.status(14, "INFO:late");
        check(!info.verified(), "timed-out INFO cannot accept late payload");
        info.begin(15); info.writeCompleted(15, true); info.reset(); info.status(15, "INFO:old session");
        check(!info.verified() && !info.matches(15), "disconnect clears INFO session");
        gate.begin(20); gate.status(20, "STOPPED"); gate.writeCompleted(20, true);
        info.begin(21); info.status(21, "INFO:caps"); info.writeCompleted(21, true);
        check(info.verified() && gate.verified(), "INFO leaves existing BLE preflight unchanged");
        gate.reset(); info.finish(); info.begin(22); info.status(22, "INFO:caps"); info.writeCompleted(22, true);
        check(info.verified() && !gate.verified(), "INFO cannot replace QUERY evidence");
        rejects(() -> info.begin(0x100000000L), "INFO sequence must be uint32");

        ProofJournal journal = new ProofJournal(); journal.connected();
        ProofJournal.Run run = journal.begin(challenge);
        check(journal.verify(run, expected, 0) != null, "matching NFC echo WITHOUT Bluetooth cannot pass");
        check(journal.receivedOverBluetooth(1, malformed) == null, "malformed APDU cannot become proof");
        ProofJournal.Receipt receipt = journal.receivedOverBluetooth(2, command);
        check(journal.verify(run, expected, 0) != null, "enqueue/receive alone cannot pass");
        journal.completedBluetoothWrite(receipt, false);
        check(journal.verify(run, expected, 0) != null, "failed ATT cannot pass");
        journal.completedBluetoothWrite(receipt, true);
        check(journal.verify(run, expected, 0) == null, "actual receive and final ATT completion pass");
        byte[] wrong = expected.clone(); wrong[0] ^= 1;
        check(journal.verify(run, wrong, 0) != null, "wrong RAPDU cannot pass even with BT evidence");
        journal.receivedOverBluetooth(3, command);
        check(journal.verify(run, expected, 0) != null, "duplicate challenge cannot pass");
        ProofJournal.Run next = journal.begin(challenge);
        journal.completedBluetoothWrite(receipt, true);
        check(journal.verify(next, expected, 0) != null, "old receipt cannot validate new run");
        receipt = journal.receivedOverBluetooth(4, command); journal.completedBluetoothWrite(receipt, true);
        journal.disconnected(); journal.connected();
        check(journal.verify(next, expected, 0) != null, "disconnect invalidates provenance");
        ProofJournal.Run current = journal.begin(challenge);
        ProofJournal.Receipt currentReceipt = journal.receivedOverBluetooth(5, command);
        AtomicReference<String> verified = new AtomicReference<>("not completed");
        Thread verifier = new Thread(() -> {
            try { verified.set(journal.verify(current, expected, 500)); }
            catch (InterruptedException e) { verified.set("interrupted"); }
        });
        verifier.start(); journal.completedBluetoothWrite(currentReceipt, true); verifier.join(1000);
        check(!verifier.isAlive() && verified.get() == null, "independent write callback releases waiting NFC verifier");
        System.out.println("PASS " + assertions + " assertions: protocol boundaries, fragmentation, whitelist, BLE preflight, read-only INFO, fresh BT proof");
    }
}
