package dev.mbapdu.tags;

import java.io.ByteArrayOutputStream;
import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.util.Arrays;
import android.util.Log;
import javax.crypto.Cipher;
import javax.crypto.spec.IvParameterSpec;
import javax.crypto.spec.SecretKeySpec;
import org.json.JSONArray;
import org.json.JSONObject;

/** Minimal local miIO client used by the Mi Home test bridge. No cloud. */
final class MiioClient {
    private static final byte[] HELLO = hex(
        "21310020ffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
    private final String host;
    private final byte[] token;
    private final byte[] key;
    private final byte[] iv;
    private final String deviceId;
    private final int timeoutMs;
    private byte[] did = new byte[4];
    private int ts;

    MiioClient(String host, String tokenHex, String deviceId) {
        this(host, tokenHex, deviceId, 10000);
    }

    MiioClient(String host, String tokenHex, String deviceId, int timeoutMs) {
        this.host = host;
        this.token = hex(tokenHex);
        this.deviceId = deviceId;
        this.timeoutMs = timeoutMs;
        if (token.length != 16) throw new IllegalArgumentException("token must be 16 bytes");
        try {
            MessageDigest md5 = MessageDigest.getInstance("MD5");
            this.key = md5.digest(token);
            md5.reset();
            md5.update(key);
            md5.update(token);
            this.iv = md5.digest();
        } catch (Exception e) {
            throw new IllegalStateException(e);
        }
    }

    private static byte[] hex(String value) {
        if ((value.length() & 1) != 0) throw new IllegalArgumentException("hex length");
        byte[] out = new byte[value.length() / 2];
        for (int i = 0; i < out.length; i++) {
            out[i] = (byte) Integer.parseInt(value.substring(i * 2, i * 2 + 2), 16);
        }
        return out;
    }

    private byte[] crypt(byte[] data, int mode) throws Exception {
        Cipher cipher = Cipher.getInstance("AES/CBC/PKCS5Padding");
        cipher.init(mode, new SecretKeySpec(key, "AES"), new IvParameterSpec(iv));
        return cipher.doFinal(data);
    }

    private static byte[] md5(byte[]... parts) throws Exception {
        MessageDigest md5 = MessageDigest.getInstance("MD5");
        for (byte[] part : parts) md5.update(part);
        return md5.digest();
    }

    private DatagramSocket socket() throws Exception {
        DatagramSocket socket = new DatagramSocket();
        socket.setSoTimeout(timeoutMs);
        return socket;
    }

    private byte[] transact(byte[] request) throws Exception {
        DatagramSocket socket = socket();
        try {
            InetAddress address = InetAddress.getByName(host);
            socket.send(new DatagramPacket(request, request.length, address, 54321));
            byte[] buffer = new byte[4096];
            DatagramPacket response = new DatagramPacket(buffer, buffer.length);
            socket.receive(response);
            return Arrays.copyOf(buffer, response.getLength());
        } finally {
            socket.close();
        }
    }

    void hello() throws Exception {
        Log.d("MiioClient", "hello " + host);
        byte[] response = transact(HELLO);
        if (response.length < 32) throw new IllegalStateException("short hello");
        did = Arrays.copyOfRange(response, 8, 12);
        ts = ByteBuffer.wrap(response, 12, 4).order(ByteOrder.BIG_ENDIAN).getInt();
    }

    JSONObject send(String method, Object params) throws Exception {
        Log.d("MiioClient", "send " + method + " " + deviceId);
        if (did.length != 4 || did[0] == 0 && did[1] == 0 && did[2] == 0 && did[3] == 0) hello();
        addDid(method, params);
        JSONObject request = new JSONObject();
        request.put("id", 1);
        request.put("method", method);
        request.put("params", params);
        ByteArrayOutputStream plain = new ByteArrayOutputStream();
        plain.write(request.toString().getBytes(StandardCharsets.UTF_8));
        plain.write(0);
        byte[] encrypted = crypt(plain.toByteArray(), Cipher.ENCRYPT_MODE);
        ByteBuffer header = ByteBuffer.allocate(16).order(ByteOrder.BIG_ENDIAN);
        header.putShort((short) 0x2131);
        header.putShort((short) (32 + encrypted.length));
        header.putInt(0);
        header.put(did);
        header.putInt(ts + 1);
        byte[] headerBytes = header.array();
        byte[] checksum = md5(headerBytes, token, encrypted);
        byte[] packet = new byte[32 + encrypted.length];
        System.arraycopy(headerBytes, 0, packet, 0, 16);
        System.arraycopy(checksum, 0, packet, 16, 16);
        System.arraycopy(encrypted, 0, packet, 32, encrypted.length);
        byte[] response = transact(packet);
        if (response.length < 32) throw new IllegalStateException("short response");
        byte[] body = Arrays.copyOfRange(response, 32, response.length);
        byte[] decrypted = crypt(body, Cipher.DECRYPT_MODE);
        int end = decrypted.length;
        while (end > 0 && decrypted[end - 1] == 0) end--;
        return new JSONObject(new String(decrypted, 0, end, StandardCharsets.UTF_8));
    }

    private void addDid(String method, Object params) {
        try {
            if (deviceId == null || deviceId.isEmpty()) return;
            if ("action".equals(method) && params instanceof JSONObject) {
                JSONObject object = (JSONObject) params;
                if (!object.has("did")) object.put("did", deviceId);
            } else if ("set_properties".equals(method) && params instanceof JSONArray) {
                JSONArray array = (JSONArray) params;
                for (int i = 0; i < array.length(); i++) {
                    JSONObject object = array.getJSONObject(i);
                    if (!object.has("did")) object.put("did", deviceId);
                }
            }
        } catch (Exception ignored) {
        }
    }

    String deviceIdHex() {
        StringBuilder out = new StringBuilder();
        for (byte b : did) out.append(String.format("%02x", b & 0xff));
        return out.toString();
    }
}
