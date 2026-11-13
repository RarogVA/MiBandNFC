#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../band/route_table.h"

int main(void) {
    route_table original,table,roundtrip;
    const uint8_t existing[]={0,3,0,3,0xc0,1,1,1,3,0xc0,1,4,0x7f,4,7,8,9,10};
    rt_init(&original);
    assert(rt_snapshot_fragment(&original,existing,sizeof(existing))==0);
    table=original;
    assert(rt_add_probe(&table)==0);
    const uint8_t probe[]={2,10,0,1,0xf0,0x4d,0x42,0x41,0x50,0x44,0x55,1};
    assert(table.entries==original.entries+1 && table.length==original.length+sizeof(probe));
    assert(memcmp(table.bytes,probe,sizeof(probe))==0);
    assert(memcmp(table.bytes+sizeof(probe),original.bytes,original.length)==0);
    assert(rt_add_probe(&table)==RT_CONFLICT);
    rt_init(&roundtrip);
    size_t cursor=0; uint8_t out[255]; int n;
    while ((n=rt_set_chunk(&table,&cursor,out,16))>0)
        assert(rt_snapshot_fragment(&roundtrip,out,n)==0);
    assert(n==0 && rt_equal(&table,&roundtrip));
    assert(rt_remove_probe(&roundtrip)==0 && rt_equal(&original,&roundtrip));
    assert(rt_remove_probe(&roundtrip)==0);
    table.bytes[2]=0xc0;
    assert(rt_remove_probe(&table)==RT_CONFLICT);
    /* Exact test route precedes a prefix and empty/default AID, preserving
     * the original AID ordering and later protocol/technology TLVs. */
    const uint8_t mixed[]={0,4,0x12,4,0xc0,1,0xf0,0x4d,2,2,0xc0,1,
                           1,3,0xc0,1,4,0,3,0xc0,1,0};
    route_table aid_baseline;
    rt_init(&aid_baseline);
    assert(rt_snapshot_fragment(&aid_baseline,mixed,sizeof(mixed))==0);
    table=aid_baseline;
    assert(rt_add_probe(&table)==0);
    assert(memcmp(table.bytes,probe,sizeof(probe))==0);
    assert(memcmp(table.bytes+sizeof(probe),aid_baseline.bytes,aid_baseline.length)==0);
    assert(rt_remove_probe(&table)==0 && rt_equal(&table,&aid_baseline));
    rt_init(&table);
    const uint8_t bad[]={0,1,2,10,0};
    assert(rt_snapshot_fragment(&table,bad,sizeof(bad))==RT_BAD);
    assert(rt_add_probe(&table)==RT_BAD);
    rt_init(&table); const uint8_t unfinished[]={1,0};
    assert(rt_snapshot_fragment(&table,unfinished,2)==0);
    assert(rt_add_probe(&table)==RT_INCOMPLETE);
    const uint8_t empty[]={0,0};
    assert(rt_snapshot_fragment(&table,empty,2)==0);
    cursor=0; assert(rt_set_chunk(&table,&cursor,out,255)==2);
    assert(out[0]==0 && out[1]==0 && rt_set_chunk(&table,&cursor,out,255)==0);
    assert(rt_snapshot_fragment(&table,empty,2)==RT_BAD);
    table=original; cursor=1;
    assert(rt_set_chunk(&table,&cursor,out,255)==RT_BAD);
    /* Every truncated form must fail or remain incomplete, never be usable. */
    for (size_t i=0;i<sizeof(existing);i++) {
        rt_init(&table); assert(rt_snapshot_fragment(&table,existing,i)!=0);
    }
    /* Preserve opaque vendor entries across a full multi-packet snapshot. */
    rt_init(&table);
    for (int k=0;k<16;k++) {
        uint8_t vendor[252]={1,1,0x7f,248};
        for (int i=4;i<252;i++) vendor[i]=(uint8_t)(i+k);
        if(k==15)vendor[0]=0;
        assert(rt_snapshot_fragment(&table,vendor,sizeof(vendor))==0);
    }
    assert(table.length==4000 && rt_add_probe(&table)==0);
    rt_init(&roundtrip); cursor=0;
    while ((n=rt_set_chunk(&table,&cursor,out,sizeof(out)))>0)
        assert(rt_snapshot_fragment(&roundtrip,out,n)==0);
    assert(n==0 && rt_equal(&table,&roundtrip));
    puts("route table: preservation, multi-packet round trip, conflicts and malformed input PASS");
    return 0;
}
