package dev.mbapdu.probe;

import java.util.Arrays;

/** Transport v1; no Android dependencies, shared wire contract with the native module. */
public final class Protocol {
    public static final int REQUEST = 1, RESPONSE = 2, STATUS = 3, CONTROL = 4, HEADER = 10;
    public static final int MAX_REQUEST = 261, MAX_RESPONSE = 258;
    public static final String SERVICE = "7dbb0001-8e51-4f89-a9cb-9d40bb8c0e31";
    public static final String RX = "7dbb0002-8e51-4f89-a9cb-9d40bb8c0e31";
    public static final String TX = "7dbb0003-8e51-4f89-a9cb-9d40bb8c0e31";
    public static final byte[] SELECT = hex("00A4040008F04D42415044550100");
    public static final byte[] OK = hex("9000");
    private Protocol() {}

    public static final class Frame {
        public final int kind;
        public final long sequence;
        public final byte[] payload;
        public Frame(int kind, long sequence, byte[] payload) {
            if (kind < REQUEST || kind > CONTROL) throw new IllegalArgumentException("тип кадра");
            if (sequence < 0 || sequence > 0xffffffffL) throw new IllegalArgumentException("sequence uint32");
            int max = maxLength(kind);
            if (payload == null || payload.length < (kind == RESPONSE ? 2 : 1) || payload.length > max)
                throw new IllegalArgumentException("длина кадра");
            this.kind = kind; this.sequence = sequence; this.payload = payload.clone();
        }
    }

    public static byte[] encode(Frame f) {
        byte[] b = new byte[HEADER + f.payload.length];
        b[0] = 0x4d; b[1] = 0x42; b[2] = 1; b[3] = (byte) f.kind;
        for (int i = 0; i < 4; i++) b[4 + i] = (byte) (f.sequence >>> (24 - 8 * i));
        b[8] = (byte) (f.payload.length >>> 8); b[9] = (byte) f.payload.length;
        System.arraycopy(f.payload, 0, b, HEADER, f.payload.length);
        return b;
    }

    public interface Sink { void accept(Frame f); }
    /** Bounded stream reassembly. Bad headers fail closed; no silent resynchronization. */
    public static final class Parser {
        private final byte[] buffer = new byte[HEADER + MAX_REQUEST];
        private int used, wanted = HEADER;
        public boolean hasPartialFrame() { return used != 0; }
        public void reset() { used = 0; wanted = HEADER; }
        public void feed(byte[] chunk, Sink sink) {
            if (chunk == null || chunk.length == 0) throw new IllegalArgumentException("пустой фрагмент");
            for (byte value : chunk) {
                buffer[used++] = value;
                if (used == HEADER && wanted == HEADER) {
                    int kind = buffer[3] & 255;
                    int length = ((buffer[8] & 255) << 8) | (buffer[9] & 255);
                    int max = maxLength(kind);
                    if (buffer[0] != 0x4d || buffer[1] != 0x42 || buffer[2] != 1 ||
                            (kind < REQUEST || kind > CONTROL) || length < (kind == RESPONSE ? 2 : 1) || length > max) {
                        reset(); throw new IllegalArgumentException("неверный заголовок MB/v1/длина");
                    }
                    wanted += length;
                }
                if (used == wanted && wanted > HEADER) {
                    long sequence = 0;
                    for (int i = 4; i < 8; i++) sequence = (sequence << 8) | (buffer[i] & 255);
                    Frame frame = new Frame(buffer[3] & 255, sequence,
                            Arrays.copyOfRange(buffer, HEADER, wanted));
                    reset(); sink.accept(frame);
                }
            }
        }
    }

    private static int maxLength(int kind) {
        if (kind == REQUEST) return MAX_REQUEST;
        if (kind == RESPONSE) return MAX_RESPONSE;
        if (kind == STATUS) return 128;
        if (kind == CONTROL) return 1;
        return 0;
    }

    public static byte[] challengeApdu(byte[] challenge) {
        if (challenge.length != 16) throw new IllegalArgumentException("challenge должен быть 16 байт");
        byte[] apdu = new byte[22];
        apdu[0] = (byte) 0x80; apdu[1] = 0x10; apdu[4] = 0x10;
        System.arraycopy(challenge, 0, apdu, 5, 16);
        return apdu;
    }
    public static boolean isChallenge(byte[] apdu) {
        return apdu != null && apdu.length == 22 && apdu[0] == (byte) 0x80 && apdu[1] == 0x10 &&
                apdu[2] == 0 && apdu[3] == 0 && apdu[4] == 0x10 && apdu[21] == 0;
    }
    public static byte[] expectedResponse(byte[] challenge) {
        if (challenge.length != 16) throw new IllegalArgumentException("challenge length");
        byte[] out = new byte[18];
        for (int i = 0; i < 16; i++) out[i] = (byte) (challenge[15 - i] ^ 0xa5);
        out[16] = (byte) 0x90;
        return out;
    }
    /** Whitelist only: no arbitrary APDU forwarding to another application or secure element. */
    public static byte[] respond(byte[] apdu) {
        if (Arrays.equals(apdu, SELECT)) return OK.clone();
        if (isChallenge(apdu)) return expectedResponse(Arrays.copyOfRange(apdu, 5, 21));
        return hex("6D00");
    }
    public static byte[] hex(String s) {
        if ((s.length() & 1) != 0) throw new IllegalArgumentException("hex length");
        byte[] b = new byte[s.length() / 2];
        for (int i = 0; i < b.length; i++) b[i] = (byte) Integer.parseInt(s.substring(i * 2, i * 2 + 2), 16);
        return b;
    }
    public static String hex(byte[] b) {
        StringBuilder out = new StringBuilder();
        for (byte value : b) out.append(String.format(java.util.Locale.ROOT, "%02X", value & 255));
        return out.toString();
    }
}
