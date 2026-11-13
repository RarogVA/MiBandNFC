package dev.mbapdu.tags;

/** One command is complete only after its matching final STATUS and successful ATT write. */
public final class CommandGate {
    private long sequence=-1;private String[] expected;private String result;private boolean prefix,written,replied,failed;
    public void begin(long value,String status) {
        begin(value,new String[]{status},false);
    }
    public void begin(long value,String[] statuses,boolean prefix) {
        if(value<0 || value>0xffffffffL || statuses==null || statuses.length==0)throw new IllegalArgumentException();
        for(String s:statuses)if(s==null || s.isEmpty())throw new IllegalArgumentException();
        sequence=value;expected=statuses.clone();this.prefix=prefix;result=null;written=replied=failed=false;
    }
    public boolean matches(long value){return sequence>=0 && sequence==value;}
    public boolean complete(){return sequence>=0 && written && replied && !failed;}
    public boolean failed(){return failed;}
    public String result(){return complete()?result:null;}
    public void reset(){sequence=-1;expected=null;result=null;prefix=false;written=replied=failed=false;}
    public void write(long seq,boolean success){if(matches(seq)){if(success)written=true;else failed=true;}}
    public void status(long seq,String value) {
        if(!matches(seq))return;
        boolean accepted=false;for(String s:expected)if(prefix?value.startsWith(s):value.equals(s))accepted=true;
        if(accepted){if(replied && !value.equals(result)){failed=true;return;}result=value;replied=true;}
        else if(!value.startsWith("STARTING:") && !value.startsWith("STOPPING:"))failed=true;
    }
}
