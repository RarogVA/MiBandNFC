package dev.mbapdu.tags;

import java.nio.charset.StandardCharsets;
import java.util.Arrays;

/** MB/v1 framing with a separate tag-profile GATT service. */
public final class Protocol {
    public static final int REQUEST=1, RESPONSE=2, STATUS=3, CONTROL=4, TAG=5, CATALOG=6, SELECTION=7, HEADER=10;
    public static final int MAX_REQUEST=261, MAX_RESPONSE=258;
    public static final String SERVICE="7dbb0011-8e51-4f89-a9cb-9d40bb8c0e31";
    public static final String RX="7dbb0012-8e51-4f89-a9cb-9d40bb8c0e31";
    public static final String TX="7dbb0013-8e51-4f89-a9cb-9d40bb8c0e31";
    private Protocol() {}
    public static final class Frame {
        public final int kind; public final long sequence; public final byte[] payload;
        public Frame(int kind,long sequence,byte[] payload) {
            if(sequence<0 || sequence>0xffffffffL || payload==null ||
                payload.length<(kind==RESPONSE?2:1) || payload.length>maximum(kind))
                throw new IllegalArgumentException("Неверный кадр MB/v1");
            this.kind=kind;this.sequence=sequence;this.payload=payload.clone();
        }
    }
    private static int maximum(int kind) {
        switch(kind) {case REQUEST:return 261;case RESPONSE:return 258;
            case STATUS:case TAG:return 128;case CATALOG:case SELECTION:return 261;case CONTROL:return 1;default:return 0;}
    }
    public static byte[] encode(Frame frame) {
        byte[] out=new byte[HEADER+frame.payload.length];out[0]=0x4d;out[1]=0x42;out[2]=1;out[3]=(byte)frame.kind;
        for(int i=0;i<4;i++)out[4+i]=(byte)(frame.sequence>>>(24-i*8));
        out[8]=(byte)(frame.payload.length>>>8);out[9]=(byte)frame.payload.length;
        System.arraycopy(frame.payload,0,out,HEADER,frame.payload.length);return out;
    }
    public interface Sink {void accept(Frame frame);}
    public static final class Parser {
        private final byte[] bytes=new byte[HEADER+MAX_REQUEST];private int used,wanted=HEADER;
        public boolean hasPartialFrame(){return used!=0;}
        public void reset(){used=0;wanted=HEADER;}
        public void feed(byte[] part,Sink sink) {
            if(part==null || part.length==0)throw new IllegalArgumentException("Пустой фрагмент");
            for(byte b:part) {
                bytes[used++]=b;
                if(used==HEADER && wanted==HEADER) {
                    int kind=bytes[3]&255,length=((bytes[8]&255)<<8)|(bytes[9]&255);
                    if(bytes[0]!=0x4d || bytes[1]!=0x42 || bytes[2]!=1 || length<(kind==RESPONSE?2:1) || length>maximum(kind)) {
                        reset();throw new IllegalArgumentException("Неверный заголовок MB/v1");
                    }
                    wanted+=length;
                }
                if(used==wanted && wanted>HEADER) {
                    long seq=0;for(int i=4;i<8;i++)seq=(seq<<8)|(bytes[i]&255);
                    Frame frame=new Frame(bytes[3]&255,seq,Arrays.copyOfRange(bytes,HEADER,wanted));
                    reset();sink.accept(frame);
                }
            }
        }
    }
    /** The band font profile is ASCII; the full Unicode name stays in the phone catalogue. */
    public static String ascii(String value,int max) {
        String cyr="АБВГДЕЁЖЗИЙКЛМНОПРСТУФХЦЧШЩЪЫЬЭЮЯабвгдеёжзийклмнопрстуфхцчшщъыьэюя";
        String[] lat={"A","B","V","G","D","E","Yo","Zh","Z","I","Y","K","L","M","N","O","P","R","S","T","U","F","Kh","Ts","Ch","Sh","Sch","","Y","","E","Yu","Ya",
            "a","b","v","g","d","e","yo","zh","z","i","y","k","l","m","n","o","p","r","s","t","u","f","kh","ts","ch","sh","sch","","y","","e","yu","ya"};
        StringBuilder out=new StringBuilder();
        for(int i=0;i<value.length();i++) {
            char c=value.charAt(i);int at=cyr.indexOf(c);
            String add=at>=0?lat[at]:(c>=32 && c<=126?String.valueOf(c):"?");
            if(out.length()+add.length()>max)break;out.append(add);
        }
        String result=out.toString().trim();return result.isEmpty()?"Tag":result;
    }
    public static byte[] metadata(String name,String uid,String tech,boolean supported) {
        byte[] n=ascii(name,32).getBytes(StandardCharsets.US_ASCII);
        byte[] t=ascii(tech,48).getBytes(StandardCharsets.US_ASCII);
        if(uid==null || uid.length()>20 || (uid.length()&1)!=0 || !uid.matches("[0-9A-Fa-f]*"))
            throw new IllegalArgumentException("UID не помещается в формат браслета");
        byte[] u=uid.toUpperCase(java.util.Locale.ROOT).getBytes(StandardCharsets.US_ASCII);
        byte[] out=new byte[5+n.length+u.length+t.length];out[0]=1;out[1]=(byte)n.length;
        out[2]=(byte)u.length;out[3]=(byte)t.length;out[4]=(byte)(supported?1:0);
        int offset=5;for(byte[] field:new byte[][]{n,u,t}) {System.arraycopy(field,0,out,offset,field.length);offset+=field.length;}
        return out;
    }
    public static byte[] hex(String text) {
        if((text.length()&1)!=0)throw new IllegalArgumentException("hex length");
        byte[] out=new byte[text.length()/2];for(int i=0;i<out.length;i++)out[i]=(byte)Integer.parseInt(text.substring(i*2,i*2+2),16);return out;
    }
    public static String hex(byte[] bytes) {
        if(bytes==null)return "";StringBuilder out=new StringBuilder();
        for(byte b:bytes)out.append(String.format(java.util.Locale.ROOT,"%02X",b&255));return out.toString();
    }
}
