package dev.mbapdu.tags;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

public final class TagTest {
    private static int checks;
    private static void require(boolean value,String message){checks++;if(!value)throw new AssertionError(message);}
    private static void equal(byte[] actual,String expected,String message){require(Protocol.hex(actual).equals(expected),message+" got "+Protocol.hex(actual));}
    private static void rejected(Runnable call,String message){try{call.run();throw new AssertionError(message);}catch(IllegalArgumentException expected){checks++;}}
    public static void main(String[] args) throws Exception {
        byte[] message=Protocol.hex("D1010C55016578616D706C652E636F6D");
        Type4Ndef tag=new Type4Ndef(message);
        equal(tag.respond(Protocol.hex("00B0000002")),"6985","read before select");
        equal(tag.respond(Protocol.hex("00A4000C02E104")),"6985","file before application");
        equal(tag.respond(Protocol.hex("00A4040007D276000085010100")),"9000","select app with Le");
        equal(tag.respond(Protocol.hex("00A4000C02E103")),"9000","select CC");
        equal(tag.respond(Protocol.hex("00B000000F")),"000F20010000FF0406E104040200FF9000","exact CC");
        equal(tag.respond(Protocol.hex("00B0000002")),"000F9000","CC split header");
        equal(tag.respond(Protocol.hex("00B000020D")),"20010000FF0406E104040200FF9000","CC remainder");
        equal(tag.respond(Protocol.hex("00A4000002E10400")),"9000","select NDEF with Le");
        equal(tag.respond(Protocol.hex("00B0000002")),"00109000","NLEN");
        equal(tag.respond(Protocol.hex("00B0000210")),Protocol.hex(message)+"9000","NDEF payload");
        equal(tag.respond(Protocol.hex("00B0000200")),Protocol.hex(message)+"6282","EOF returns short data and warning");
        equal(tag.respond(Protocol.hex("00B0001201")),"6282","exact EOF");
        equal(tag.respond(Protocol.hex("00B0001301")),"6B00","beyond EOF");
        equal(tag.respond(Protocol.hex("00D600000100")),"6986","read only UPDATE");
        equal(tag.respond(Protocol.hex("00D000000100")),"6986","read only WRITE");
        equal(tag.respond(Protocol.hex("80B0000002")),"6E00","CLA");
        equal(tag.respond(Protocol.hex("00B0800002")),"6A86","no SFI addressing");
        equal(tag.respond(Protocol.hex("00B00000000100")),"6700","no extended READ");
        equal(tag.respond(Protocol.hex("00A4040007D276000085010101")),"6A86","unsupported select Le");
        equal(tag.respond(Protocol.hex("00A4040006D2760000850101")),"6A86","inconsistent select Lc/Le");
        equal(tag.respond(Protocol.hex("00A4040007D2760000850102")),"6A82","wrong application");
        equal(tag.respond(Protocol.hex("00B0000002")),"6985","wrong application clears file");
        equal(tag.respond(Protocol.hex("00A4040C07D2760000850101")),"9000","select no FCI no Le");
        equal(tag.respond(Protocol.hex("00A4000C02E105")),"6A82","unknown file");
        equal(tag.respond(Protocol.hex("00B0000002")),"6985","unknown file no selection");
        equal(tag.respond(Protocol.hex("00A4000C02E104")),"9000","select file again");tag.reset();
        equal(tag.respond(Protocol.hex("00B0000002")),"6985","reset drops session");
        equal(tag.respond(null),"6700","null request");
        equal(tag.respond(new byte[3]),"6700","short APDU");
        equal(tag.respond(Protocol.hex("00100000")),"6D00","unimplemented instruction");
        rejected(()->new Type4Ndef(new byte[0]),"empty NDEF");rejected(()->new Type4Ndef(new byte[1025]),"oversize NDEF");
        byte[] large=new byte[1024];for(int i=0;i<large.length;i++)large[i]=(byte)i;
        tag=new Type4Ndef(large);large[0]=99;
        tag.respond(Protocol.hex("00A4040007D2760000850101"));tag.respond(Protocol.hex("00A4000C02E104"));
        byte[] chunk=tag.respond(Protocol.hex("00B0000200"));require(chunk.length==258,"Le00 is 256 bytes");
        require(chunk[0]==0 && chunk[255]==(byte)255 && chunk[256]==(byte)0x90,"defensive snapshot and boundary bytes");
        equal(tag.respond(Protocol.hex("00B0040002")),"FEFF9000","last two file bytes");
        equal(tag.respond(Protocol.hex("00B0040102")),"FF6282","partial last byte");

        byte[] metadata=Protocol.metadata("Подъезд №1","04AABBCC","NfcA / Ndef",true);
        require(metadata[0]==1 && metadata[4]==1,"tag metadata opcode/flag");
        require((metadata[1]&255)+(metadata[2]&255)+(metadata[3]&255)+5==metadata.length,"exact metadata sizes");
        require(new String(metadata,5,metadata[1]&255,java.nio.charset.StandardCharsets.US_ASCII).startsWith("Podezd"),"transliteration");
        require(Protocol.metadata(new String(new char[100]).replace('\0','A'),"00112233445566778899",new String(new char[100]).replace('\0','T'),false).length==105,"metadata max105");
        rejected(()->Protocol.metadata("name","ABC","Ndef",true),"odd UID rejected");
        rejected(()->Protocol.metadata("name","GG","Ndef",true),"nonhex UID rejected");
        rejected(()->Protocol.metadata("name","0011223344556677889900","Ndef",true),"long UID rejected");
        Protocol.Frame frame=new Protocol.Frame(Protocol.TAG,0xffffffffL,metadata);byte[] wire=Protocol.encode(frame);
        for(int split=1;split<wire.length;split++){
            Protocol.Parser parser=new Protocol.Parser();List<Protocol.Frame> got=new ArrayList<>();
            parser.feed(Arrays.copyOfRange(wire,0,split),got::add);require(got.isEmpty(),"no premature frame");
            parser.feed(Arrays.copyOfRange(wire,split,wire.length),got::add);
            require(got.size()==1 && got.get(0).sequence==0xffffffffL && Arrays.equals(metadata,got.get(0).payload),"every fragment split");
        }
        Protocol.Parser parser=new Protocol.Parser();List<Protocol.Frame> got=new ArrayList<>();
        for(int i=0;i<wire.length;i+=20)parser.feed(Arrays.copyOfRange(wire,i,Math.min(i+20,wire.length)),got::add);
        require(got.size()==1&&!parser.hasPartialFrame(),"ATT20 fragment roundtrip");
        rejected(()->new Protocol.Frame(Protocol.CONTROL,1,new byte[2]),"control length");
        rejected(()->new Protocol.Frame(Protocol.TAG,1,new byte[129]),"metadata cap");
        rejected(()->new Protocol.Frame(Protocol.RESPONSE,1,new byte[1]),"response SW required");
        rejected(()->new Protocol.Frame(9,1,new byte[1]),"unknown kind");
        require(new Protocol.Frame(Protocol.MIHOME,1,new byte[]{1}).payload.length==1,"mihome frame");
        rejected(()->new Protocol.Frame(Protocol.MIHOME,1,new byte[9]),"mihome cap");
        byte[] bad=wire.clone();bad[8]=1;bad[9]=1;rejected(()->new Protocol.Parser().feed(bad,f->{}),"oversized header");

        CommandGate gate=new CommandGate();gate.begin(9,"TAG_OK");gate.status(8,"TAG_OK");gate.write(9,true);require(!gate.complete(),"wrong seq never completes");
        gate.status(9,"TAG_OK");require(gate.complete(),"ATT then status");
        gate.begin(10,"READY");gate.status(10,"STARTING:route");gate.status(10,"READY");require(!gate.complete(),"status requires ATT");gate.write(10,true);require(gate.complete(),"status then ATT");
        gate.begin(11,"STOPPED");gate.write(11,false);gate.status(11,"STOPPED");require(gate.failed()&&!gate.complete(),"failed ATT");
        gate.begin(12,"TAG_OK");gate.write(12,true);gate.status(12,"READY");require(gate.failed()&&!gate.complete(),"wrong final status");
        gate.reset();gate.status(12,"TAG_OK");gate.write(12,true);require(!gate.complete(),"late callbacks after reset");
        final Type4Ndef simulated=new Type4Ndef(message);NdefReadback readback=new NdefReadback();
        require(Arrays.equals(readback.read(simulated::respond),message),"independent Type4 reader gets complete message");
        require(readback.commands.size()==6,"small NDEF uses six APDUs");
        final Type4Ndef full=new Type4Ndef(new byte[1024]);NdefReadback largeReadback=new NdefReadback();
        require(largeReadback.read(full::respond).length==1024&&largeReadback.commands.size()==9,"large file read in four chunks");
        ReadbackProof proof=new ReadbackProof();long token=proof.begin();
        require(!proof.matches(token,readback.commands,readback.responses),"NFC bytes alone are not BLE proof");
        ArrayList<ReadbackProof.Receipt> receipts=new ArrayList<>();
        for(int i=0;i<readback.commands.size();i++)receipts.add(proof.received(readback.commands.get(i),readback.responses.get(i)));
        require(!proof.matches(token,readback.commands,readback.responses),"BLE receive alone needs ATT complete");
        for(ReadbackProof.Receipt receipt:receipts)proof.written(receipt,true);
        require(proof.matches(token,readback.commands,readback.responses),"complete independent BT evidence");
        proof.written(receipts.get(0),false);require(!proof.matches(token,readback.commands,readback.responses),"failed ATT invalidates proof");
        proof.reset();for(ReadbackProof.Receipt receipt:receipts)proof.written(receipt,true);
        require(!proof.matches(token,readback.commands,readback.responses),"stale receipts after reset");
        token=proof.begin();for(int i=0;i<readback.commands.size();i++){ReadbackProof.Receipt receipt=proof.received(readback.commands.get(i),readback.responses.get(i));proof.written(receipt,true);}
        proof.written(proof.received(readback.commands.get(0),readback.responses.get(0)),true);
        require(!proof.matches(token,readback.commands,readback.responses),"duplicate exchange not accepted");
        CacheRefreshGate refresh=new CacheRefreshGate();
        require(!refresh.claim(true,true),"no refresh before explicit connect");refresh.begin();
        require(!refresh.claim(false,true),"present expected service never triggers refresh");
        require(!refresh.claim(true,false),"unrelated device never triggers refresh");
        require(refresh.claim(true,true),"legacy-only discovery can claim one refresh");long retry=refresh.token();
        require(!refresh.waitForRediscovery(retry),"refresh call is not accepted until API returned true");
        refresh.accepted(retry);require(refresh.waitForRediscovery(retry),"accepted refresh waits with connection open");
        require(!refresh.claim(true,true),"automatic old callback cannot trigger another refresh");
        require(refresh.claimExplicitDiscovery(retry),"one delayed explicit discovery allowed");
        require(!refresh.waitForRediscovery(retry),"missing result after explicit discovery must fail closed");
        require(!refresh.claimExplicitDiscovery(retry),"no repeated explicit discovery");
        require(refresh.claimSetup(),"new expected service starts CCCD setup");
        require(refresh.setupStarted()&&!refresh.claimSetup(),"auto and explicit callbacks cannot duplicate CCCD setup");
        require(!refresh.claimExplicitDiscovery(retry),"CCCD setup cancels delayed rediscovery");
        refresh.cancel();require(!refresh.current(retry),"disconnect cancels delayed work");
        require(!refresh.claimSetup(),"setup after cancellation rejected");
        refresh.begin();require(!refresh.current(retry),"new explicit attempt rejects old callbacks");
        require(refresh.claim(true,true),"new explicit attempt has fresh one-shot budget");retry=refresh.token();
        refresh.accepted(retry);require(refresh.claimSetup(),"automatic fresh callback before delay starts setup");
        require(!refresh.waitForRediscovery(retry)&&!refresh.claimExplicitDiscovery(retry),"automatic success suppresses delayed explicit discover");
        refresh.begin();require(!refresh.current(retry),"second explicit connect cancels previous work");
        require(refresh.claimSetup()&&!refresh.attempted(),"normal service discovery needs no cache refresh");
        checks+=CatalogTest.run();
        System.out.println("PASS "+checks+" assertions: Type4 NDEF, catalog vectors/bounds, selection identity/reconnect races, STATUS+ATT gates, independent readback and bounded cache refresh");
    }
}
