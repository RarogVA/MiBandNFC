package dev.mbapdu.tags;

import java.io.ByteArrayOutputStream;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/** Independent physical Type 4 reader. It cannot call the BLE responder. */
public final class NdefReadback {
    public interface Link {byte[] transceive(byte[] command) throws Exception;}
    public final List<byte[]> commands=new ArrayList<>(),responses=new ArrayList<>();
    private byte[] exchange(Link link,String request) throws Exception{return exchange(link,Protocol.hex(request));}
    private byte[] exchange(Link link,byte[] request) throws Exception {
        byte[] response=link.transceive(request);commands.add(request.clone());responses.add(response.clone());
        if(response.length<2 || response[response.length-2]!=(byte)0x90 || response[response.length-1]!=0)throw new IllegalStateException("NFC status "+Protocol.hex(response));
        return Arrays.copyOf(response,response.length-2);
    }
    public byte[] read(Link link) throws Exception {
        commands.clear();responses.clear();
        exchange(link,"00A4040007D276000085010100");exchange(link,"00A4000C02E103");
        byte[] cc=exchange(link,"00B000000F");
        if(cc.length!=15 || cc[0]!=0 || cc[1]!=15 || (cc[2]&0xf0)!=0x20 || cc[7]!=4 || cc[8]!=6 || cc[13]!=0)throw new IllegalStateException("Неподдерживаемый CC");
        int maxRead=Math.min(256,((cc[3]&255)<<8)|(cc[4]&255)),capacity=((cc[11]&255)<<8)|(cc[12]&255);
        if(maxRead<15 || capacity<2)throw new IllegalStateException("Неверные лимиты CC");
        exchange(link,new byte[]{0,(byte)0xA4,0,12,2,cc[9],cc[10]});
        byte[] nlen=exchange(link,"00B0000002");if(nlen.length!=2)throw new IllegalStateException("Неверный NLEN");
        int length=((nlen[0]&255)<<8)|(nlen[1]&255);
        if(length==0 || length>Type4Ndef.MAX_NDEF || length+2>capacity)throw new IllegalStateException("Размер NDEF вне диапазона");
        ByteArrayOutputStream out=new ByteArrayOutputStream(length);
        for(int offset=2;offset<length+2;){int count=Math.min(maxRead,length+2-offset);
            byte[] part=exchange(link,new byte[]{0,(byte)0xB0,(byte)(offset>>>8),(byte)offset,(byte)count});
            if(part.length!=count)throw new IllegalStateException("Неполный ответ READ BINARY");out.write(part);offset+=count;}
        return out.toByteArray();
    }
}
