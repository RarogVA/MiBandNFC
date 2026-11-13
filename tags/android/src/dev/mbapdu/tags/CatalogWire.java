package dev.mbapdu.tags;

import java.io.ByteArrayOutputStream;
import java.nio.ByteBuffer;
import java.nio.charset.CharacterCodingException;
import java.nio.charset.CodingErrorAction;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashSet;
import java.util.List;
import java.util.UUID;

/** Versioned, bounded catalogue bytes shared with the native catalogue profile. */
public final class CatalogWire {
    public static final int MAX_RECORDS=32,MAX_BYTES=65536,CHUNK_BYTES=240;
    public static final int BEGIN=1,DATA=2,COMMIT=3,ABORT=4,QUERY=5;
    public static final int SELECT_REQUEST=1,SELECT_ACK=2,SELECT_PROPOSE=3;
    public static final class Entry {
        public final String id,name,uid,technology;
        public final long revision;
        private final byte[] ndef;
        public Entry(String id,long revision,String name,String uid,String technology,byte[] ndef) {
            uuid(id);positiveRevision(revision);
            byte[] named=name.getBytes(StandardCharsets.UTF_8);
            if(named.length<1 || named.length>96 || !utf8(named) || controls(name))throw new IllegalArgumentException("Название метки не помещается в каталог браслета");
            if(uid==null || uid.length()>20 || (uid.length()&1)!=0 || !uid.matches("[0-9A-F]*"))throw new IllegalArgumentException("Неверный UID каталога");
            if(technology==null || technology.length()>48 || !technology.matches("[ -~]*"))throw new IllegalArgumentException("Неверная технология каталога");
            if(ndef!=null && (ndef.length==0 || ndef.length>1024))throw new IllegalArgumentException("NDEF должен содержать 1–1024 байта");
            this.id=id;this.revision=revision;this.name=name;this.uid=uid;this.technology=technology;this.ndef=ndef==null?new byte[0]:ndef.clone();
        }
        public boolean supported(){return ndef.length>0;}
        public byte[] ndef(){return ndef.clone();}
        public byte[] ndefHash(){return sha256(ndef);}
        public Selection selection(long catalogRevision){return new Selection(catalogRevision,id,revision,ndefHash());}
    }
    public static final class Snapshot {
        public final long revision;
        private final Entry[] entries;
        private final byte[] bytes,hash;
        public Snapshot(long revision,List<Entry> source) {
            positiveRevision(revision);if(source==null || source.size()>MAX_RECORDS)throw new IllegalArgumentException("Лимит каталога — 32 записи");
            this.revision=revision;entries=source.toArray(new Entry[0]);HashSet<String> ids=new HashSet<>();
            ByteArrayOutputStream out=new ByteArrayOutputStream();write(out,new byte[]{'T','C','A','T'});u16(out,1);u16(out,entries.length);u32(out,revision);
            for(Entry entry:entries) {
                if(entry==null || !ids.add(entry.id))throw new IllegalArgumentException("Повторяющийся ID метки");
                byte[] name=entry.name.getBytes(StandardCharsets.UTF_8),uid=entry.uid.getBytes(StandardCharsets.US_ASCII),tech=entry.technology.getBytes(StandardCharsets.US_ASCII);
                u16(out,26+name.length+uid.length+tech.length+entry.ndef.length);
                write(out,uuid(entry.id));u32(out,entry.revision);out.write(entry.supported()?1:0);
                out.write(name.length);out.write(uid.length);out.write(tech.length);u16(out,entry.ndef.length);
                write(out,name);write(out,uid);write(out,tech);write(out,entry.ndef);
            }
            bytes=out.toByteArray();if(bytes.length>MAX_BYTES)throw new IllegalArgumentException("Каталог превышает 65536 байт");hash=sha256(bytes);
        }
        public int count(){return entries.length;}
        public int length(){return bytes.length;}
        public byte[] bytes(){return bytes.clone();}
        public byte[] hash(){return hash.clone();}
        public String hashHex(){return Protocol.hex(hash);}
        public Entry find(String id){for(Entry entry:entries)if(entry.id.equals(id))return entry;return null;}
        public byte[] begin(){ByteArrayOutputStream out=new ByteArrayOutputStream();out.write(BEGIN);u32(out,revision);out.write(entries.length);u32(out,bytes.length);write(out,hash);return out.toByteArray();}
        public byte[] chunk(int offset){if(offset<0 || offset>=bytes.length)throw new IllegalArgumentException("Неверное смещение каталога");
            ByteArrayOutputStream out=new ByteArrayOutputStream();out.write(DATA);u32(out,offset);out.write(bytes,offset,Math.min(CHUNK_BYTES,bytes.length-offset));return out.toByteArray();}
    }
    public static final class Selection {
        public final long catalogRevision,recordRevision;
        public final String id;
        private final byte[] hash;
        public Selection(long catalogRevision,String id,long recordRevision,byte[] hash) {
            positiveRevision(catalogRevision);positiveRevision(recordRevision);uuid(id);
            if(hash==null || hash.length!=32)throw new IllegalArgumentException("Неверный хеш метки");
            this.catalogRevision=catalogRevision;this.id=id;this.recordRevision=recordRevision;this.hash=hash.clone();
        }
        public byte[] payload(int opcode){if(opcode<1 || opcode>3)throw new IllegalArgumentException("Неверная команда выбора");
            ByteArrayOutputStream out=new ByteArrayOutputStream();out.write(opcode);u32(out,catalogRevision);write(out,uuid(id));u32(out,recordRevision);write(out,hash);return out.toByteArray();}
        public boolean matches(Snapshot snapshot){if(snapshot==null || snapshot.revision!=catalogRevision)return false;Entry entry=snapshot.find(id);return entry!=null && entry.revision==recordRevision && Arrays.equals(hash,entry.ndefHash());}
        public boolean same(Selection other){return other!=null && catalogRevision==other.catalogRevision && recordRevision==other.recordRevision && id.equals(other.id) && Arrays.equals(hash,other.hash);}
        public static Selection parse(byte[] payload,int opcode){if(payload==null || payload.length!=57 || (payload[0]&255)!=opcode)throw new IllegalArgumentException("Неверный запрос выбора");
            ByteBuffer in=ByteBuffer.wrap(payload);in.get();long cat=Integer.toUnsignedLong(in.getInt());String id=new UUID(in.getLong(),in.getLong()).toString();long rev=Integer.toUnsignedLong(in.getInt());byte[] hash=new byte[32];in.get(hash);return new Selection(cat,id,rev,hash);}
    }
    public static final class Summary {
        public final long revision;public final int count;public final String hash;
        private Summary(long revision,int count,String hash){this.revision=revision;this.count=count;this.hash=hash;}
        public static Summary parse(String status){String[] parts=status.split(":",-1);
            if(parts.length!=4 || !parts[0].equals("CATALOG") || !parts[1].matches("[0-9]{1,10}") || !parts[2].matches("[0-9]{1,2}") || !parts[3].matches("[0-9A-Fa-f]{64}"))throw new IllegalArgumentException("Неверное состояние каталога браслета");
            long revision=Long.parseLong(parts[1]);int count=Integer.parseInt(parts[2]);if(revision>0xffffffffL || count>MAX_RECORDS || (revision==0&&count!=0))throw new IllegalArgumentException("Неверная ревизия каталога браслета");
            return new Summary(revision,count,parts[3].toUpperCase(java.util.Locale.ROOT));}
        public boolean matches(Snapshot snapshot){return snapshot!=null && revision==snapshot.revision && count==snapshot.count() && hash.equals(snapshot.hashHex());}
    }
    /** Truncate UTF-8 only at a code-point boundary; the full name remains in Pixel JSON. */
    public static String displayName(String text){if(text==null)throw new IllegalArgumentException("Пустое имя");StringBuilder out=new StringBuilder();int bytes=0;
        for(int at=0;at<text.length();){int cp=text.codePointAt(at);at+=Character.charCount(cp);if(Character.isISOControl(cp))cp=' ';
            String c=new String(Character.toChars(cp));int n=c.getBytes(StandardCharsets.UTF_8).length;if(bytes+n>96)break;out.append(c);bytes+=n;}
        String result=out.toString().trim();return result.isEmpty()?"Tag":result;}
    public static byte[] sha256(byte[] bytes){try{return MessageDigest.getInstance("SHA-256").digest(bytes);}catch(NoSuchAlgorithmException impossible){throw new AssertionError(impossible);}}
    private static byte[] uuid(String id){if(id==null)throw new IllegalArgumentException("Пустой ID");UUID value=UUID.fromString(id);if(!value.toString().equals(id) || (value.getMostSignificantBits()==0&&value.getLeastSignificantBits()==0))throw new IllegalArgumentException("Неверный стабильный ID");return ByteBuffer.allocate(16).putLong(value.getMostSignificantBits()).putLong(value.getLeastSignificantBits()).array();}
    private static boolean controls(String text){for(int i=0;i<text.length();i++)if(Character.isISOControl(text.charAt(i)))return true;return false;}
    private static boolean utf8(byte[] bytes){try{StandardCharsets.UTF_8.newDecoder().onMalformedInput(CodingErrorAction.REPORT).onUnmappableCharacter(CodingErrorAction.REPORT).decode(ByteBuffer.wrap(bytes));return true;}catch(CharacterCodingException e){return false;}}
    private static void positiveRevision(long value){if(value<1 || value>0xffffffffL)throw new IllegalArgumentException("Неверная ревизия");}
    private static void u16(ByteArrayOutputStream out,int value){out.write(value>>>8);out.write(value);}
    private static void u32(ByteArrayOutputStream out,long value){for(int i=3;i>=0;i--)out.write((int)(value>>>(8*i)));}
    private static void write(ByteArrayOutputStream out,byte[] bytes){out.write(bytes,0,bytes.length);}
}
