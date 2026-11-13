package dev.mbapdu.tags;

/** Identity authorization is scoped to one catalog snapshot and BLE connection. */
public final class SelectionGate {
    public static final class Request {
        public final long sequence;
        public final CatalogWire.Selection selection;
        private Request(long sequence,CatalogWire.Selection selection){this.sequence=sequence;this.selection=selection;}
    }
    private CatalogWire.Snapshot local;
    private CatalogWire.Summary remote;
    private Request pending,confirmed;
    public void local(CatalogWire.Snapshot snapshot){
        boolean changed=local==null || snapshot==null || local.revision!=snapshot.revision || !local.hashHex().equals(snapshot.hashHex());
        local=snapshot;if(changed)invalidate();
    }
    public void observed(CatalogWire.Summary summary){remote=summary;if(!catalogMatches())invalidate();}
    public boolean catalogMatches(){return remote!=null && remote.matches(local);}
    public void invalidate(){pending=confirmed=null;}
    public void disconnected(){remote=null;invalidate();}
    public Request receive(long sequence,byte[] payload){
        invalidate();
        if(sequence<0 || sequence>0xffffffffL || !catalogMatches())throw new IllegalArgumentException("Каталоги Pixel и браслета не совпадают");
        CatalogWire.Selection selection=CatalogWire.Selection.parse(payload,CatalogWire.SELECT_REQUEST);
        if(!selection.matches(local))throw new IllegalArgumentException("Браслет запросил другую ревизию или содержимое метки");
        pending=new Request(sequence,selection);return pending;
    }
    public boolean confirm(Request request){
        if(request==null || request!=pending || !catalogMatches() || !request.selection.matches(local))return false;
        confirmed=request;pending=null;return true;
    }
    public CatalogWire.Entry authorized(String id){
        if(confirmed==null || !catalogMatches() || !confirmed.selection.id.equals(id) || !confirmed.selection.matches(local))return null;
        return local.find(id);
    }
}
