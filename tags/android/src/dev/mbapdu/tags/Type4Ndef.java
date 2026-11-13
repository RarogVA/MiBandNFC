package dev.mbapdu.tags;

import java.util.Arrays;

/** Read-only NFC Forum Type 4 NDEF application. No access credentials or UID emulation. */
public final class Type4Ndef {
    public static final int MAX_NDEF=1024;
    private static final byte[] AID=Protocol.hex("D2760000850101");
    // CCLEN15, mapping2.0, MLe256, MLc255, NDEF E104 with capacity1026, read00/writeFF.
    private static final byte[] CC=Protocol.hex("000F20010000FF0406E104040200FF");
    private final byte[] file;private boolean application;private int selected;
    public Type4Ndef(byte[] message) {
        if(message==null || message.length==0 || message.length>MAX_NDEF)throw new IllegalArgumentException("NDEF 1..1024 bytes");
        file=new byte[message.length+2];file[0]=(byte)(message.length>>>8);file[1]=(byte)message.length;
        System.arraycopy(message,0,file,2,message.length);
    }
    public synchronized void reset(){application=false;selected=0;}
    private static byte[] status(String sw){return Protocol.hex(sw);}
    public synchronized byte[] respond(byte[] command) {
        if(command==null || command.length<4)return status("6700");
        if(command[0]!=0)return status("6E00");
        int ins=command[1]&255,p1=command[2]&255,p2=command[3]&255;
        if(ins==0xA4) {
            if(command.length<5)return status("6700");
            int lc=command[4]&255;
            if(command.length!=5+lc && command.length!=6+lc)return status("6700");
            if(command.length==6+lc && command[command.length-1]!=0)return status("6A86");
            if(p1==4) {
                // A new application selection always drops the previous file selection.
                application=false;selected=0;
                if(p2!=0 && p2!=12)return status("6A86");
                if(!Arrays.equals(Arrays.copyOfRange(command,5,5+lc),AID))return status("6A82");
                application=true;return status("9000");
            }
            if(p1!=0 || (p2!=0 && p2!=12))return status("6A86");
            if(!application)return status("6985");
            if(lc!=2)return status("6700");
            int id=((command[5]&255)<<8)|(command[6]&255);
            if(id!=0xE103 && id!=0xE104)return status("6A82");
            selected=id;return status("9000");
        }
        if(ins==0xB0) {
            if(command.length!=5)return status("6700");
            if((p1&0x80)!=0)return status("6A86");
            if(!application || selected==0)return status("6985");
            int offset=(p1<<8)|p2,requested=command[4]&255;if(requested==0)requested=256;
            byte[] source=selected==0xE103?CC:file;
            if(offset>source.length)return status("6B00");
            int count=Math.min(requested,source.length-offset);
            byte[] out=new byte[count+2];System.arraycopy(source,offset,out,0,count);
            if(count<requested){out[count]=0x62;out[count+1]=(byte)0x82;}else out[count]=(byte)0x90;
            return out;
        }
        if(ins==0xD6 || ins==0xD0)return status("6986");
        return status("6D00");
    }
}
