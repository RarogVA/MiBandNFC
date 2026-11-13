#include "route_table.h"

const uint8_t rt_probe_aid[RT_PROBE_AID_LENGTH] =
#ifdef TRANSPORT_TAG_PROFILE
    {0xd2,0x76,0x00,0x00,0x85,0x01,0x01};
static const uint8_t probe_entry[] =
    {0x02,0x09,0x00,0x01,0xd2,0x76,0x00,0x00,0x85,0x01,0x01};
#else
    {0xf0,0x4d,0x42,0x41,0x50,0x44,0x55,0x01};
/* NCI AID route: tag, length, DH=0, switched-on power=1, AID. */
static const uint8_t probe_entry[] =
    {0x02,0x0a,0x00,0x01,0xf0,0x4d,0x42,0x41,0x50,0x44,0x55,0x01};
#endif

static int same(const uint8_t *a, const uint8_t *b, size_t n) {
    for (size_t i=0;i<n;i++) if (a[i]!=b[i]) return 0;
    return 1;
}
static void copy(uint8_t *d,const uint8_t *s,size_t n) {
    for (size_t i=0;i<n;i++) d[i]=s[i];
}
void rt_init(route_table *t) {
    t->length=0; t->entries=0; t->complete=0; t->invalid=0;
}
static int valid(const route_table *t) {
    if (!t || t->invalid || t->length>RT_CAPACITY || t->entries>RT_MAX_ENTRIES)
        return RT_BAD;
    if (!t->complete) return RT_INCOMPLETE;
    size_t p=0,n=0;
    while (p<t->length) {
        if (t->length-p<2) return RT_BAD;
        size_t size=(size_t)t->bytes[p+1]+2;
        if (size>t->length-p) return RT_BAD;
        p+=size; n++;
    }
    return n==t->entries ? RT_OK : RT_BAD;
}
int rt_snapshot_fragment(route_table *t,const uint8_t *p,size_t n) {
    if (!t || t->invalid) return RT_BAD;
    if (t->complete || !p || n<2 || n>RT_NCI_PAYLOAD_MAX || p[0]>1) {
        t->invalid=1; return RT_BAD;
    }
    size_t at=2;
    for (unsigned i=0;i<p[1];i++) {
        if (n-at<2 || (size_t)p[at+1]+2>n-at) {
            t->invalid=1; return RT_BAD;
        }
        at+=(size_t)p[at+1]+2;
    }
    if (at!=n) {t->invalid=1; return RT_BAD;}
    if (n-2>RT_CAPACITY-t->length || p[1]>RT_MAX_ENTRIES-t->entries) {
        t->invalid=1; return RT_FULL;
    }
    copy(t->bytes+t->length,p+2,n-2);
    t->length+=(uint16_t)(n-2); t->entries+=p[1]; t->complete=p[0]==0;
    return RT_OK;
}
static int is_probe_aid(const uint8_t *entry) {
    return (entry[0]&0x0f)==2 && entry[1]==2+RT_PROBE_AID_LENGTH
        && same(entry+4,rt_probe_aid,RT_PROBE_AID_LENGTH);
}
int rt_add_probe(route_table *t) {
    int v=valid(t); if (v) return v;
    for (size_t p=0;p<t->length;p+=(size_t)t->bytes[p+1]+2)
        if (is_probe_aid(t->bytes+p)) return RT_CONFLICT;
    if (sizeof(probe_entry)>RT_CAPACITY-t->length || t->entries==RT_MAX_ENTRIES)
        return RT_FULL;
    /* NCI 2.0 builders place AID routes before protocol/technology routes.
     * Put our exact AID first, also ahead of any existing prefix/empty AID.
     * Preserve every old TLV byte and their relative order; never sort an
     * opaque vendor entry or modify an existing route. */
    for(size_t i=t->length;i>0;i--)
        t->bytes[i-1+sizeof(probe_entry)]=t->bytes[i-1];
    copy(t->bytes,probe_entry,sizeof(probe_entry));
    t->length+=sizeof(probe_entry); t->entries++; return RT_OK;
}
int rt_remove_probe(route_table *t) {
    int v=valid(t); if (v) return v;
    size_t found=RT_CAPACITY; unsigned count=0;
    for (size_t p=0;p<t->length;p+=(size_t)t->bytes[p+1]+2) {
        if (!is_probe_aid(t->bytes+p)) continue;
        if (!same(t->bytes+p,probe_entry,sizeof(probe_entry))) return RT_CONFLICT;
        found=p; count++;
    }
    if (count>1) return RT_CONFLICT;
    if (!count) return RT_OK;
    for (size_t i=found;i+sizeof(probe_entry)<t->length;i++)
        t->bytes[i]=t->bytes[i+sizeof(probe_entry)];
    t->length-=sizeof(probe_entry); t->entries--; return RT_OK;
}
int rt_set_chunk(const route_table *t,size_t *cursor,uint8_t *out,size_t cap) {
    int v=valid(t); if (v) return v;
    if (!cursor || !out || cap<2) return RT_BAD;
    if (cap>RT_NCI_PAYLOAD_MAX) cap=RT_NCI_PAYLOAD_MAX;
    if (!t->length) {
        if (*cursor==1) return 0;
        if (*cursor!=0) return RT_BAD;
        out[0]=out[1]=0; *cursor=1; return 2;
    }
    if (*cursor==t->length) return 0;
    if (*cursor>t->length) return RT_BAD;
    /* Reject cursors in the middle of a TLV. */
    size_t p=0;
    while (p<*cursor) p+=(size_t)t->bytes[p+1]+2;
    if (p!=*cursor) return RT_BAD;
    size_t end=p,used=2; unsigned count=0;
    while (end<t->length && count<255) {
        size_t size=(size_t)t->bytes[end+1]+2;
        if (size>cap-used) break;
        used+=size; end+=size; count++;
    }
    if (!count) return RT_FULL;
    out[0]=end<t->length; out[1]=(uint8_t)count;
    copy(out+2,t->bytes+p,used-2); *cursor=end; return (int)used;
}
int rt_equal(const route_table *a,const route_table *b) {
    return valid(a)==0 && valid(b)==0 && a->length==b->length
        && a->entries==b->entries && same(a->bytes,b->bytes,a->length);
}
