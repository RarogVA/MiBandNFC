/* Derived from the shared NFC lifecycle suite for the 7-byte NDEF profile. */
#include "nfc_probe.h"
#include "route_table.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

typedef void (*rx_fn)(uint16_t,uint8_t *);
static rx_fn hook;
static unsigned chain_count,apdu_count;
static unsigned state=1;
static route_table routes,incoming;
static uint8_t apdu[261],response[258];
static size_t apdu_len,response_len;
static uint32_t now;
static int reject_next_set,drop_first_route,delay_stop,commit_partial;
static unsigned sets,reject_set_number;
static int defer_completion;
static int get_callback_case,preflight_error;
static unsigned version_reads,info_reads,hardware_writes;
static uint8_t mock_version=0x20,mock_ctrl=255,mock_chip;
static uint16_t mock_lmrt=1024;
static uint32_t mock_features=0x00000e00u;
static void (*saved_completion)(uint8_t,uint16_t,uint8_t *);
static uint8_t saved_rsp[4];
static void stock(uint16_t n,uint8_t *p) {(void)n;(void)p;chain_count++;}
int nfc_hw_preflight(void) {return preflight_error;}
int nfc_hw_install(rx_fn f,rx_fn *old) {hardware_writes++;*old=stock;hook=f;return 0;}
int nfc_hw_restore(rx_fn f,rx_fn old) {hardware_writes++;assert(hook==f);assert(old==stock);hook=0;return 0;}
int nfc_hw_stop(void) {hardware_writes++;if(!delay_stop)state=0;return 0;}
int nfc_hw_start(void) {hardware_writes++;state=1;return 0;}
uint8_t nfc_hw_disc_state(void) {return (uint8_t)state;}
uint8_t nfc_hw_version(void) {version_reads++;return mock_version;}
void nfc_hw_info(uint32_t *features,uint16_t *lmrt,uint8_t *ctrl,uint8_t *chip) {
    info_reads++;*features=mock_features;*lmrt=mock_lmrt;*ctrl=mock_ctrl;*chip=mock_chip;
}
static void rx(uint8_t *p,size_t n) {assert(hook);hook((uint16_t)n,p);}
int nfc_hw_command(const uint8_t *p,size_t n,void (*cb)(uint8_t,uint16_t,uint8_t *)) {
    hardware_writes++;
    assert(n>=3 && p[0]==0x21 && n==(size_t)p[2]+3);
    uint8_t rsp[4]={0x41,p[1],1,0};
    if(p[1]==2 && get_callback_case) {
        switch(get_callback_case) {
        case 1: rsp[3]=3;rx(rsp,4);cb(0x42,4,rsp);break;
        case 2: rx(rsp,4);cb(0x42,1,rsp+3);break;
        case 3: {
            uint8_t extended[]={0x41,2,6,0,0xaa,0xbb,0xcc,0xdd,0xee};
            rx(extended,sizeof(extended));cb(0x42,sizeof(extended),extended);break;
        }
        case 4: cb(0xff,65535,NULL);break;
        case 5: rx(rsp,4);cb(0x41,4,rsp);break;
        case 6: rsp[1]=1;rx(rsp,4);cb(0x41,4,rsp);break;
        case 7: {uint8_t ntf[]={0x61,2,2,0,0};rx(rsp,4);cb(0x42,sizeof(ntf),ntf);break;}
        case 8: cb(0x42,0,rsp);break;
        default: assert(0);
        }
        return 0;
    }
    if(p[1]==1)sets++;
    if(p[1]==1 && (reject_next_set || (reject_set_number && sets==reject_set_number))) {
        reject_next_set=0;incoming.complete=1;rsp[3]=3;rx(rsp,4);cb((uint8_t)(0x40|p[1]),4,rsp);return 0;
    }
    rx(rsp,4);
    if(defer_completion) {saved_completion=cb;memcpy(saved_rsp,rsp,4);}
    else cb((uint8_t)(0x40|p[1]),4,rsp);
    if(p[1]==2) {
        size_t cursor=0;uint8_t ntf[258]={0x61,2,0};int size;
        while((size=rt_set_chunk(&routes,&cursor,ntf+3,255))>0) {
            ntf[2]=(uint8_t)size;rx(ntf,(size_t)size+3);
        }
        assert(size==0);
    } else {
        assert(p[1]==1);
        if(incoming.complete) rt_init(&incoming);
        assert(rt_snapshot_fragment(&incoming,p+3,n-3)==0);
        if(incoming.complete || commit_partial) {routes=incoming;routes.complete=1;}
        if(incoming.complete && drop_first_route) {
            /* The synthetic AID now leads the table. Corrupt an original
             * route, so the restoration tests still exercise baseline loss. */
            size_t at=routes.length>=11 && routes.bytes[0]==2 &&
                routes.bytes[1]==9 && routes.bytes[2]==0 && routes.bytes[3]==1 &&
                memcmp(routes.bytes+4,rt_probe_aid,7)==0 ? 11u:0u;
            assert(routes.length>=at+2);
            size_t lost=(size_t)routes.bytes[at+1]+2;
            memmove(routes.bytes+at,routes.bytes+at+lost,routes.length-at-lost);
            routes.length-=(uint16_t)lost;routes.entries--;drop_first_route=0;
        }
    }
    return 0;
}
int nfc_hw_response(const uint8_t *p,size_t n) {hardware_writes++;assert(n<=sizeof(response));memcpy(response,p,n);response_len=n;return 0;}
static void got(const uint8_t *p,size_t n,void *ctx) {
    assert(ctx==(void*)1);apdu_count++;apdu_len=n;memcpy(apdu,p,n);
}
static void tick(void) {now+=10;nfc_probe_poll(now);}
static void until_ready(void) {for(unsigned i=0;i<100 && !nfc_probe_ready();i++)tick();assert(nfc_probe_ready());}
static void until_clean(void) {for(unsigned i=0;i<100 && !nfc_probe_can_unload();i++)tick();if(!nfc_probe_can_unload())fprintf(stderr,"unclean: %s sets=%u state=%u\n",nfc_probe_status(),sets,state);assert(nfc_probe_can_unload());}
static void activate(void) {uint8_t p[]={0x61,5,6,1,2,4,0x80,0xff,1};state=5;rx(p,sizeof(p));}
static void deactivate(void) {uint8_t p[]={0x61,6,2,0,0};state=1;rx(p,sizeof(p));}
static void select_probe(void) {
    uint8_t p[]={0,0,13,0,0xa4,4,0,7,0xd2,0x76,0,0,0x85,1,1,0};
    unsigned c=chain_count;rx(p,sizeof(p));assert(chain_count==c);tick();
}
int main(void) {
    rt_init(&routes);uint8_t initial[]={0,2,0,3,0xc0,1,0,0x87,3,0xaa,0xbb,0xcc};
    assert(rt_snapshot_fragment(&routes,initial,sizeof(initial))==0);
    route_table original=routes;
    assert(nfc_probe_start(got,(void*)1)==0);until_ready();
    assert(routes.length==original.length+11);assert(!nfc_probe_can_unload());
    /* Ordinary data before exact SELECT is never intercepted. */
    activate();uint8_t other[]={0,0,5,0,0xa4,4,0,0};
    unsigned c=chain_count;rx(other,sizeof(other));assert(chain_count==c+1);
    uint8_t wrong_aid[]={0,0,13,0,0xa4,4,0,7,0xd2,0x76,0,0,0x85,1,2,0};
    c=chain_count;rx(wrong_aid,sizeof(wrong_aid));tick();assert(chain_count==c+1 && !apdu_count);
    wrong_aid[14]=1;wrong_aid[7]=8; /* Own bytes but malformed Lc. */
    c=chain_count;rx(wrong_aid,sizeof(wrong_aid));tick();assert(chain_count==c+1 && !apdu_count);
    wrong_aid[7]=7;wrong_aid[0]=0x10; /* First SELECT fragmentation unsupported. */
    c=chain_count;rx(wrong_aid,sizeof(wrong_aid));tick();assert(chain_count==c+1 && !apdu_count);
    select_probe();assert(apdu_count==1 && apdu_len==13);
    uint8_t ok[]={0x90,0};assert(nfc_probe_send(ok,2)==0);assert(response_len==2);
    /* Own APDU reassembly spans NCI packets; callbacks occur only in poll. */
    uint8_t f1[]={0x10,0,3,0x80,0x10,0};uint8_t f2[]={0,0,3,0,1,0x42};
    c=chain_count;rx(f1,sizeof(f1));rx(f2,sizeof(f2));assert(chain_count==c);
    tick();assert(apdu_count==2 && apdu_len==6 && apdu[5]==0x42);
    assert(nfc_probe_send(ok,2)==0);
    /* Unrelated reselect is replayed byte-for-byte to stock; later data too. */
    c=chain_count;rx(other,sizeof(other));assert(chain_count==c+1);
    rx(f2,sizeof(f2));assert(chain_count==c+2);
    deactivate();
    activate();uint8_t no_le[]={0,0,12,0,0xa4,4,0x0c,7,0xd2,0x76,0,0,0x85,1,1};
    c=chain_count;rx(no_le,sizeof(no_le));tick();assert(chain_count==c && apdu_len==12 && apdu_count==3);
    assert(nfc_probe_send(ok,2)==0);deactivate();
    /* A concurrent owner's unrelated route must survive removal. */
    routes.bytes[routes.length++]=0x88;routes.bytes[routes.length++]=1;
    routes.bytes[routes.length++]=0x55;routes.entries++;
    assert(nfc_probe_stop()==0);until_clean();
    assert(routes.length==original.length+3);
    assert(memcmp(routes.bytes,original.bytes,original.length)==0);
    assert(routes.bytes[routes.length-1]==0x55);
    /* Rejected SET remains explicit recovery; STOP can clean without reset. */
    reject_next_set=1;assert(nfc_probe_start(got,(void*)1)==0);
    for(unsigned i=0;i<30 && !nfc_probe_error();i++)tick();
    assert(nfc_probe_error()<0 && !nfc_probe_can_unload());
    assert(nfc_probe_stop()==0);until_clean();
    /* Stale response from a deactivated test session cannot be sent. */
    assert(nfc_probe_start(got,(void*)1)==0);until_ready();activate();select_probe();
    deactivate();assert(nfc_probe_send(ok,2)<0);
    assert(nfc_probe_stop()==0);until_clean();
    /* A corrupted acknowledged SET must retain baseline and restore it. */
    route_table before=routes;drop_first_route=1;
    assert(nfc_probe_start(got,(void*)1)==0);
    for(unsigned i=0;i<100 && !nfc_probe_error();i++)tick();
    assert(nfc_probe_error()<0 && !nfc_probe_can_unload());
    assert(!rt_equal(&before,&routes));
    assert(nfc_probe_stop()==0);until_clean();assert(rt_equal(&before,&routes));
    /* Partial multi-command SET commits followed by rejection restore baseline. */
    routes=original;
    for(unsigned k=0;k<8;k++) {
        routes.bytes[routes.length++]=(uint8_t)(0x80+k);routes.bytes[routes.length++]=48;
        memset(routes.bytes+routes.length,(int)k,48);routes.length+=48;routes.entries++;
    }
    before=routes;commit_partial=1;reject_set_number=sets+2;
    assert(nfc_probe_start(got,(void*)1)==0);
    for(unsigned i=0;i<100 && !nfc_probe_error();i++)tick();
    assert(nfc_probe_error()<0 && !nfc_probe_can_unload());
    assert(!rt_equal(&before,&routes));
    reject_set_number=0;assert(nfc_probe_stop()==0);until_clean();
    assert(rt_equal(&before,&routes));commit_partial=0;
    /* Unknown concurrent entries plus missing baseline cannot claim clean. */
    drop_first_route=1;assert(nfc_probe_start(got,(void*)1)==0);
    for(unsigned i=0;i<100 && !nfc_probe_error();i++)tick();
    routes.bytes[routes.length++]=0x99;routes.bytes[routes.length++]=1;
    routes.bytes[routes.length++]=0x42;routes.entries++;
    unsigned old_sets=sets;
    assert(nfc_probe_stop()==0);
    for(unsigned i=0;i<20;i++)tick();
    assert(nfc_probe_error()<0 && !nfc_probe_can_unload());assert(sets==old_sets);
    routes=before;assert(nfc_probe_stop()==0);until_clean();
    /* A late unsendable TLV is rejected before ANY SET chunk is transmitted. */
    routes=original;routes.bytes[routes.length++]=0x98;routes.bytes[routes.length++]=251;
    memset(routes.bytes+routes.length,0xaa,251);routes.length+=251;routes.entries++;
    before=routes;old_sets=sets;
    assert(nfc_probe_start(got,(void*)1)==0);until_clean();
    assert(nfc_probe_error()<0 && sets==old_sets && rt_equal(&before,&routes));
    /* An accepted STOP that times out stays owned until late completion and
     * compensating restart; it must never release the hook while RF can stop. */
    routes=original;delay_stop=1;assert(nfc_probe_start(got,(void*)1)==0);
    now+=6000;tick();assert(nfc_probe_error()<0 && !nfc_probe_can_unload() && hook);
    state=0;delay_stop=0;assert(nfc_probe_stop()==0);until_clean();assert(state==1);
    /* A raw same-opcode RSP is insufficient: only our queued callback can
     * finish the accepted command. Timeout retains ownership until it arrives. */
    old_sets=sets;defer_completion=1;assert(nfc_probe_start(got,(void*)1)==0);
    for(unsigned i=0;i<20;i++)tick();assert(!nfc_probe_ready() && sets==old_sets);
    now+=6000;tick();assert(nfc_probe_error()<0 && !nfc_probe_can_unload());
    assert(nfc_probe_stop()<0);assert(hook);
    defer_completion=0;saved_completion((uint8_t)(0x40|saved_rsp[1]),4,saved_rsp);
    tick();assert(nfc_probe_stop()==0);until_clean();assert(state==1 && sets==old_sets);
    /* Controller errors and malformed callbacks have distinct, bounded,
     * sticky diagnostics. Neither permits a SET without a complete snapshot. */
    const char *expected[]={
        "NFC_REJECT op=02 st=03 ev=42 n=4 cb=41020103 raw=41020103 v=20 d=0",
        "NFC_BAD_RSP op=02 st=-- ev=42 n=1 cb=00 raw=41020100 v=20 d=0",
        "NFC_BAD_RSP op=02 st=-- ev=42 n=9 cb=41020600AABBCCDD raw=41020600 v=20 d=0",
        "NFC_BAD_RSP op=02 st=-- ev=FF n=65535 cb=- raw=- v=20 d=0",
        "NFC_BAD_RSP op=02 st=-- ev=41 n=4 cb=41020100 raw=41020100 v=20 d=0",
        "NFC_BAD_RSP op=02 st=-- ev=41 n=4 cb=41010100 raw=41010100 v=20 d=0",
        "NFC_BAD_RSP op=02 st=-- ev=42 n=5 cb=6102020000 raw=41020100 v=20 d=0",
        "NFC_BAD_RSP op=02 st=-- ev=42 n=0 cb=- raw=- v=20 d=0"
    };
    for(unsigned i=0;i<sizeof(expected)/sizeof(expected[0]);i++) {
        before=routes;old_sets=sets;get_callback_case=(int)i+1;
        assert(nfc_probe_start(got,(void*)1)==0);
        assert(nfc_probe_diagnostic()[0]==0);
        until_clean();
        assert(nfc_probe_error()<0 && sets==old_sets && rt_equal(&routes,&before));
        assert(strcmp(nfc_probe_diagnostic(),expected[i])==0);
        assert(strcmp(nfc_probe_status(),expected[i])==0);
        assert(strlen(nfc_probe_diagnostic())+strlen("ERROR:")<=127);
        assert(nfc_probe_stop()==0);
        assert(strcmp(nfc_probe_diagnostic(),expected[i])==0);
        assert(strcmp(nfc_probe_status(),"Stopped")==0);
    }
    /* No version-memory read until preflight and discovery-state gates pass;
     * rejected START attempts preserve the previous diagnostic. */
    unsigned prior_reads=version_reads;
    preflight_error=-84;assert(nfc_probe_start(got,(void*)1)==-84);
    assert(version_reads==prior_reads && strcmp(nfc_probe_diagnostic(),expected[7])==0);
    preflight_error=0;state=5;assert(nfc_probe_start(got,(void*)1)==-16);
    assert(version_reads==prior_reads);state=1;
    /* INFO is read-only and its separate buffer cannot erase the ERROR that
     * a caller still needs to send after STOP. Cache reads are gated. */
    unsigned writes_before=hardware_writes,reads_before=info_reads;
    const char *status_before=nfc_probe_status();
    const int gates[]={-84,-19,-16,-95};
    for(unsigned i=0;i<sizeof(gates)/sizeof(gates[0]);i++) {
        preflight_error=gates[i];assert(strncmp(nfc_probe_info(),"unavailable gate=-",18)==0);
        assert(version_reads==prior_reads && info_reads==reads_before);
    }
    preflight_error=0;mock_version=0x10;
    assert(strcmp(nfc_probe_info(),"cached v=10 unavailable: unsupported NCI layout")==0);
    assert(version_reads==prior_reads+1 && info_reads==reads_before);
    mock_version=0x20;
    assert(strcmp(nfc_probe_info(),"cached v=20 chip=00 feat=00000E00 aid=1 lmrt=1024 ctrl=255")==0);
    assert(info_reads==reads_before+1);
    mock_features=UINT32_MAX;mock_lmrt=UINT16_MAX;mock_ctrl=UINT8_MAX;mock_chip=UINT8_MAX;
    const char *info=nfc_probe_info();
    assert(strcmp(info,"cached v=20 chip=FF feat=FFFFFFFF aid=1 lmrt=65535 ctrl=255")==0);
    assert(strlen(info)+strlen("INFO:")<=127);
    mock_features=0;mock_lmrt=0;mock_ctrl=0;mock_chip=1;
    assert(strcmp(nfc_probe_info(),"cached v=20 chip=01 feat=00000000 aid=0 lmrt=0 ctrl=0")==0);
    assert(hardware_writes==writes_before && !hook && state==1 && nfc_probe_can_unload());
    assert(strcmp(nfc_probe_diagnostic(),expected[7])==0);
    assert(strcmp(nfc_probe_status(),status_before)==0);
    prior_reads=version_reads;
    get_callback_case=0;assert(nfc_probe_start(got,(void*)1)==0);
    assert(version_reads==prior_reads+1 && nfc_probe_diagnostic()[0]==0);
    until_ready();assert(nfc_probe_stop()==0);until_clean();
    puts("tags nfc_probe: NDEF SELECT7 with/without Le, malformed/foreign pass-through, lifecycle and cleanup PASS");
}
