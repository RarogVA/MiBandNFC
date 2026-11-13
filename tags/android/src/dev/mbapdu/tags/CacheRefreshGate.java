package dev.mbapdu.tags;

/** One cache refresh and at most one later discovery per explicit user connection. */
public final class CacheRefreshGate {
    private long attempt;private boolean attempted,live,accepted,explicitDiscovery,setup;
    public void begin(){attempt++;attempted=accepted=explicitDiscovery=setup=false;live=true;}
    public void cancel(){attempt++;attempted=accepted=explicitDiscovery=setup=false;live=false;}
    public boolean claim(boolean expectedMissing,boolean legacyPresent){
        if(!live||attempted||!expectedMissing||!legacyPresent)return false;
        attempted=true;return true;
    }
    public long token(){return attempt;}
    public boolean current(long token){return live&&attempt==token;}
    public void accepted(long token){if(current(token)&&attempted&&!setup)accepted=true;}
    public boolean waitForRediscovery(long token){return current(token)&&accepted&&!explicitDiscovery&&!setup;}
    public boolean claimExplicitDiscovery(long token){
        if(!waitForRediscovery(token))return false;explicitDiscovery=true;return true;
    }
    public boolean claimSetup(){if(!live||setup)return false;setup=true;accepted=false;return true;}
    public boolean setupStarted(){return setup;}
    public boolean attempted(){return attempted;}
}
