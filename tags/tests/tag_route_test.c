#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "route_table.h"
int main(void) {
    assert(RT_PROBE_AID_LENGTH==7);
    const uint8_t aid[]={0xd2,0x76,0,0,0x85,1,1};assert(!memcmp(rt_probe_aid,aid,7));
    const uint8_t baseline[]={0,1,1,3,0,1,4};
    route_table t,original;rt_init(&t);assert(rt_snapshot_fragment(&t,baseline,sizeof(baseline))==0);
    original=t;
    assert(rt_add_probe(&t)==0 && t.entries==2 && t.length==original.length+11);
    const uint8_t entry[]={2,9,0,1,0xd2,0x76,0,0,0x85,1,1};
    assert(!memcmp(t.bytes,entry,sizeof(entry)));
    assert(!memcmp(t.bytes+sizeof(entry),original.bytes,original.length));
    assert(rt_add_probe(&t)==RT_CONFLICT);
    uint8_t chunk[255];size_t cursor=0;int n=rt_set_chunk(&t,&cursor,chunk,sizeof(chunk));
    assert(n==18 && chunk[0]==0 && chunk[1]==2 && !memcmp(chunk+2,entry,sizeof(entry)));
    assert(rt_remove_probe(&t)==0 && rt_equal(&t,&original));
    assert(rt_add_probe(&t)==0);t.bytes[2]=1;assert(rt_remove_probe(&t)==RT_CONFLICT);
    puts("tags route: exact NDEF AID, prepend, original preservation, conflict and cleanup PASS");
}
