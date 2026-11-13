package dev.mbapdu.tags;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/** Evidence recorded exclusively by the BLE handler, independently of the NFC reader. */
public final class ReadbackProof {
    public static final class Receipt {
        final long epoch;final byte[] request,response;boolean completed,ok;
        Receipt(long epoch,byte[] request,byte[] response){this.epoch=epoch;this.request=request.clone();this.response=response.clone();}
    }
    private long epoch;private boolean collecting;private final List<Receipt> receipts=new ArrayList<>();
    public synchronized long begin(){epoch++;collecting=true;receipts.clear();return epoch;}
    public synchronized void reset(){epoch++;collecting=false;receipts.clear();}
    public synchronized Receipt received(byte[] request,byte[] response){if(!collecting)return null;if(receipts.size()>=64){collecting=false;return null;}Receipt receipt=new Receipt(epoch,request,response);receipts.add(receipt);return receipt;}
    public synchronized void written(Receipt receipt,boolean ok){if(receipt!=null&&collecting&&receipt.epoch==epoch){receipt.completed=true;receipt.ok=ok;}}
    public synchronized boolean matches(long token,List<byte[]> commands,List<byte[]> responses){
        if(!collecting||token!=epoch||commands.isEmpty()||commands.size()!=responses.size()||receipts.size()!=commands.size())return false;
        for(int i=0;i<receipts.size();i++){Receipt r=receipts.get(i);if(!r.completed||!r.ok||!Arrays.equals(r.request,commands.get(i))||!Arrays.equals(r.response,responses.get(i)))return false;}
        return true;
    }
}
