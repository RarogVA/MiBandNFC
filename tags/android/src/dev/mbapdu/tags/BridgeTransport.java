package dev.mbapdu.tags;

public interface BridgeTransport {
    interface Listener {
        void state(boolean ready, String message);
        void received(Protocol.Frame frame);
        void log(String message);
    }
    interface Completion { void done(boolean successful); }
    void connect(String address);
    void send(Protocol.Frame response, Completion completion);
    void disconnect();
    void shutdown();
}
