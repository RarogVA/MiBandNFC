package dev.mbapdu.probe;

/** Main-thread gate: a fresh QUERY must receive STOPPED and complete its ATT write. */
public final class ControlPreflight {
    private long sequence = -1;
    private boolean written, stopped;

    public void begin(long value) {
        if (value < 0 || value > 0xffffffffL) throw new IllegalArgumentException("sequence uint32");
        reset(); sequence = value;
    }
    public void reset() { sequence = -1; written = stopped = false; }
    public boolean matches(long value) { return sequence >= 0 && sequence == value; }
    public boolean verified() { return sequence >= 0 && written && stopped; }
    public boolean pending() { return sequence >= 0 && !verified(); }
    public void writeCompleted(long value, boolean successful) {
        if (!matches(value)) return;
        if (successful) written = true; else reset();
    }
    public void status(long value, String status) {
        if (!matches(value)) return;
        if ("STOPPED".equals(status)) stopped = true; else reset();
    }
}
