package dev.mbapdu.probe;

/** Independent read-only request; it cannot provide NFC-start/preflight evidence. */
public final class InfoRequest {
    private long sequence = -1;
    private boolean active, written;
    private String value;

    public void begin(long next) {
        if (next < 0 || next > 0xffffffffL) throw new IllegalArgumentException("sequence uint32");
        reset(); sequence = next; active = true;
    }
    public void reset() { sequence = -1; active = written = false; value = null; }
    // Retain the last sequence on completion/timeout to consume its late replies.
    public void finish() { active = false; }
    public boolean matches(long next) { return sequence >= 0 && sequence == next; }
    public boolean pending() { return active; }
    public boolean verified() { return active && written && value != null; }
    public String value() { return verified() ? value : null; }
    public void writeCompleted(long next, boolean successful) {
        if (!matches(next) || !active) return;
        if (successful) written = true; else finish();
    }
    public void status(long next, String text) {
        if (!matches(next) || !active) return;
        if (text != null && text.startsWith("INFO:") && text.length() > 5) value = text.substring(5);
        else finish();
    }
}
