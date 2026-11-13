#include "nfc_probe.h"
#include "route_table.h"
#include <stdint.h>
#include <stddef.h>

/* Experimental exact-firmware backend. No payment AIDs or SE commands.
 * The only hook is the RAM HAL receive callback. All management packets and
 * unrelated RF data are passed unchanged to the original callback. The module
 * remains resident: do not rmmod it, even after a successful stop. */

typedef void (*hal_rx_fn)(uint16_t, uint8_t *);
typedef void (*vsc_fn)(uint8_t, uint16_t, uint8_t *);
static void raw_rx(uint16_t length, uint8_t *packet);
static void command_complete(uint8_t event,uint16_t length,uint8_t *packet);
static void bytes_copy(uint8_t *d, const uint8_t *s, size_t n) {
    for (size_t i=0;i<n;i++) d[i]=s[i];
}
static int bytes_same(const uint8_t *a, const uint8_t *b, size_t n) {
    for (size_t i=0;i<n;i++) if (a[i]!=b[i]) return 0;
    return 1;
}
#define LOAD(p) __atomic_load_n((p),__ATOMIC_ACQUIRE)
#define STORE(p,v) __atomic_store_n((p),(v),__ATOMIC_RELEASE)

#ifdef NFC_PROBE_HOST_TEST
extern int nfc_hw_preflight(void);
extern int nfc_hw_install(hal_rx_fn hook, hal_rx_fn *old);
extern int nfc_hw_restore(hal_rx_fn hook, hal_rx_fn old);
extern int nfc_hw_stop(void);
extern int nfc_hw_start(void);
extern uint8_t nfc_hw_disc_state(void);
extern uint8_t nfc_hw_version(void);
extern void nfc_hw_info(uint32_t *,uint16_t *,uint8_t *,uint8_t *);
extern int nfc_hw_command(const uint8_t *, size_t, vsc_fn);
extern int nfc_hw_response(const uint8_t *, size_t);
#else
#include "nfc_fingerprints.h"
#ifndef NFC_PROBE_ENABLE_EXPERIMENTAL_ROUTING
#define NFC_PROBE_ENABLE_EXPERIMENTAL_ROUTING 0
#endif
static int nfc_hw_preflight(void) {
#if !NFC_PROBE_ENABLE_EXPERIMENTAL_ROUTING
    return -95;
#else
#if !defined(__arm__) || !defined(__thumb__)
    return -95;
#else
    for (size_t i=0;i<sizeof(nfc_fingerprints)/sizeof(nfc_fingerprints[0]);i++) {
        const nfc_fingerprint *f=&nfc_fingerprints[i];
        const volatile uint8_t *p=(const volatile uint8_t *)f->address;
        for (size_t j=0;j<sizeof(f->bytes);j++) if (p[j]!=f->bytes[j]) return -84;
    }
    uintptr_t rx=LOAD((uintptr_t *)0x200eaab0u);
    uintptr_t open=LOAD((uintptr_t *)0x200eaac0u);
    if ((rx!=0x0c65af1du && rx!=0x2c65af1du) ||
        (open!=0x0c64fb25u && open!=0x2c64fb25u)) return -16;
    if ((LOAD((uint32_t *)0x200eabdcu)&1u)==0) return -19;
    return 0;
#endif
#endif
}
static int nfc_hw_install(hal_rx_fn hook, hal_rx_fn *old) {
    uintptr_t *slot=(uintptr_t *)0x200eaab0u;
    uintptr_t expected=LOAD(slot);
    if (expected!=0x0c65af1du && expected!=0x2c65af1du) return -16;
    *old=(hal_rx_fn)expected;
    return __atomic_compare_exchange_n(slot,&expected,(uintptr_t)hook,0,
                                      __ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE)?0:-16;
}
static int nfc_hw_restore(hal_rx_fn hook, hal_rx_fn old) {
    uintptr_t expected=(uintptr_t)hook;
    return __atomic_compare_exchange_n((uintptr_t *)0x200eaab0u,&expected,
                (uintptr_t)old,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE)?0:-16;
}
static uint8_t nfc_hw_disc_state(void) {
    return *(volatile uint8_t *)0x200eac28u;
}
static uint8_t nfc_hw_version(void) {
    /* Read only after preflight. The fingerprinted firmware getter at
     * 0x2c65b200 reads precisely nfc_cb + 0x1f8; no controller command. */
    return *(volatile uint8_t *)0x200eb7ecu;
}
static void nfc_hw_info(uint32_t *features,uint16_t *lmrt,uint8_t *ctrl,uint8_t *chip) {
    /* Cached CORE_INIT fields in exact .026, only after preflight + NCI2 gate.
     * Reading these does not send CORE_INIT, reset or any controller command. */
    *features=*(volatile uint32_t *)0x200eb6b4u;
    *lmrt=*(volatile uint16_t *)0x200eb6b8u;
    *ctrl=*(volatile uint8_t *)0x200eb6bbu;
    *chip=*(volatile uint8_t *)0x200b04b5u;
}
static int nfc_hw_stop(void) {
    return ((uint8_t (*)(void))0x0c650e45u)();
}
static int nfc_hw_start(void) {
    return ((uint8_t (*)(void))0x0c650e25u)();
}
static int nfc_hw_command(const uint8_t *p,size_t n,vsc_fn cb) {
    if (n<3 || n>255) return -22;
    return ((uint8_t (*)(uint8_t,const uint8_t *,vsc_fn))0x0c650ed9u)
        ((uint8_t)n,p,cb);
}
static int nfc_hw_response(const uint8_t *p,size_t n) {
    return ((uint8_t (*)(const uint8_t *,uint16_t,uint16_t))0x0c650dcdu)
        (p,(uint16_t)n,0);
}
#endif

enum phase {
    IDLE, STOP_START, GET_ADD, SET_ADD, VERIFY_ADD, START_TEST, READY,
    STOP_END, GET_REMOVE, SET_REMOVE, VERIFY_REMOVE, START_END, ERROR, RECOVERY
};
#define CQ_SIZE 16u
#define NCI_PACKET_MAX 258u
#define TIMEOUT_MS 5000u
#define MAX_FRAGMENTS 8u
static struct {
    enum phase phase;
    const char *message;
    nfc_probe_apdu_cb callback;
    void *context;
    hal_rx_fn original;
    uint32_t now,deadline;
    uint8_t hooked,dirty,rf_stopped,stop_pending,ending,stop_requested,last_chunk;
    uint8_t wait_opcode,rsp_seen,rsp_status,command_pending,get_ntf_done;
    volatile unsigned completion;
    /* One callback per outstanding NFA command. Published by completion's
     * release store and consumed in poll, before another command can begin. */
    uint16_t callback_length;
    uint8_t callback_event,callback_size,callback_prefix[8],nci_version;
    /* Independent HAL witness, never used to accept/complete a command. */
    volatile unsigned raw_response;
    char diagnostic[122]; /* ERROR: + this text fits the 127-byte BLE status. */
    uint8_t had_failure;
    size_t cursor;
    route_table snapshot,target,baseline,installed;
    volatile unsigned queue_head,queue_tail,queue_overflow;
    struct { uint16_t length; uint8_t bytes[NCI_PACKET_MAX]; } queue[CQ_SIZE];
    volatile unsigned allow_rf,listen_iso,selected,epoch;
    volatile unsigned capdu_ready;
    uint8_t capdu[NFC_PROBE_MAX_CAPDU];
    uint16_t capdu_length;
    unsigned capdu_epoch;
    unsigned awaiting,await_epoch;
    /* HAL-thread-only reassembly, including original packets for a reselect. */
    uint8_t assembly[NFC_PROBE_MAX_CAPDU];
    uint16_t assembly_length;
    uint8_t fragment_count;
    uint16_t fragment_length[MAX_FRAGMENTS];
    uint8_t fragments[MAX_FRAGMENTS][NCI_PACKET_MAX];
} g;

static void phase_set(enum phase p,const char *s) {
    g.phase=p; g.message=s; g.deadline=g.now+TIMEOUT_MS;
}
static void diagnostic_text(char *out,size_t *at,const char *s) {
    while (*s && *at<sizeof(g.diagnostic)-1) out[(*at)++]=*s++;
}
static void diagnostic_hex(char *out,size_t *at,unsigned value,unsigned digits) {
    static const char hex[]="0123456789ABCDEF";
    while (digits) {
        unsigned shift=4u*--digits;
        if (*at<sizeof(g.diagnostic)-1)
            out[(*at)++]=hex[(value>>shift)&15u];
    }
}
static void diagnostic_decimal(char *out,size_t *at,uint16_t value) {
    char digits[5]; unsigned n=0;
    do {digits[n++]=(char)('0'+value%10u);value=(uint16_t)(value/10u);} while(value);
    while(n && *at<sizeof(g.diagnostic)-1)out[(*at)++]=digits[--n];
}
static void record_response_error(int valid) {
    size_t at=0;
    diagnostic_text(g.diagnostic,&at,valid?"NFC_REJECT op=":"NFC_BAD_RSP op=");
    diagnostic_hex(g.diagnostic,&at,g.wait_opcode,2);
    diagnostic_text(g.diagnostic,&at," st=");
    if(valid)diagnostic_hex(g.diagnostic,&at,g.rsp_status,2);else diagnostic_text(g.diagnostic,&at,"--");
    diagnostic_text(g.diagnostic,&at," ev=");diagnostic_hex(g.diagnostic,&at,g.callback_event,2);
    diagnostic_text(g.diagnostic,&at," n=");diagnostic_decimal(g.diagnostic,&at,g.callback_length);
    diagnostic_text(g.diagnostic,&at," cb=");
    if(!g.callback_size)diagnostic_text(g.diagnostic,&at,"-");
    for(unsigned i=0;i<g.callback_size;i++)diagnostic_hex(g.diagnostic,&at,g.callback_prefix[i],2);
    diagnostic_text(g.diagnostic,&at," raw=");
    unsigned raw=LOAD(&g.raw_response);
    if(raw)diagnostic_hex(g.diagnostic,&at,raw,8);else diagnostic_text(g.diagnostic,&at,"-");
    diagnostic_text(g.diagnostic,&at," v=");diagnostic_hex(g.diagnostic,&at,g.nci_version,2);
    diagnostic_text(g.diagnostic,&at," d=");diagnostic_hex(g.diagnostic,&at,g.dirty?1u:0u,1);
    g.diagnostic[at]=0;
}
const char *nfc_probe_info(void) {
    /* Separate stable buffer: INFO must not invalidate a retained ERROR. */
    static char info[sizeof(g.diagnostic)];
    size_t at=0;
    int rc=nfc_hw_preflight();
    if (rc) {
        diagnostic_text(info,&at,"unavailable gate=");
        if(rc<0)diagnostic_text(info,&at,"-");
        diagnostic_decimal(info,&at,(uint16_t)(rc<0?0u-(unsigned)rc:(unsigned)rc));
    } else {
        uint8_t version=nfc_hw_version();
        diagnostic_text(info,&at,"cached v=");diagnostic_hex(info,&at,version,2);
        if(version!=0x20) {
            diagnostic_text(info,&at," unavailable: unsupported NCI layout");
        } else {
            uint32_t features;uint16_t lmrt;uint8_t ctrl,chip;
            nfc_hw_info(&features,&lmrt,&ctrl,&chip);
            diagnostic_text(info,&at," chip=");diagnostic_hex(info,&at,chip,2);
            diagnostic_text(info,&at," feat=");diagnostic_hex(info,&at,features,8);
            diagnostic_text(info,&at," aid=");diagnostic_hex(info,&at,(features>>11)&1u,1);
            diagnostic_text(info,&at," lmrt=");diagnostic_decimal(info,&at,lmrt);
            diagnostic_text(info,&at," ctrl=");diagnostic_decimal(info,&at,ctrl);
        }
    }
    info[at]=0;
    return info;
}
static int waiting_snapshot(void) {
    return g.phase==GET_ADD || g.phase==GET_REMOVE ||
           g.phase==VERIFY_ADD || g.phase==VERIFY_REMOVE;
}
static void queue_control(uint16_t n,const uint8_t *p) {
    unsigned h=LOAD(&g.queue_head),t=LOAD(&g.queue_tail);
    if (h-t>=CQ_SIZE) {STORE(&g.queue_overflow,1); return;}
    unsigned slot=h&(CQ_SIZE-1);
    g.queue[slot].length=n;
    bytes_copy(g.queue[slot].bytes,p,n);
    STORE(&g.queue_head,h+1);
}
static int is_select(const uint8_t *p,size_t n) {
    return n>=5 && p[0]==0 && p[1]==0xa4 && p[2]==4;
}
static int is_probe_select(const uint8_t *p,size_t n) {
#ifdef TRANSPORT_TAG_PROFILE
    return (n==12 || (n==13 && p[12]==0)) && p[0]==0 && p[1]==0xa4 &&
        p[2]==4 && (p[3]==0 || p[3]==0x0c) && p[4]==7 &&
        bytes_same(p+5,rt_probe_aid,7);
#else
    return (n==13 || (n==14 && p[13]==0)) && p[0]==0 && p[1]==0xa4 &&
        p[2]==4 && (p[3]==0 || p[3]==0x0c) && p[4]==8 &&
        bytes_same(p+5,rt_probe_aid,8);
#endif
}
static void forward(uint16_t n,uint8_t *p) {
    hal_rx_fn fn=g.original;
    if (fn) fn(n,p);
}
static void reset_rf(void) {
    STORE(&g.listen_iso,0); STORE(&g.selected,0);
    __atomic_add_fetch(&g.epoch,1,__ATOMIC_ACQ_REL);
    g.assembly_length=0; g.fragment_count=0;
}
static int intercept_data(uint16_t n,uint8_t *p) {
    if (!LOAD(&g.allow_rf) || !LOAD(&g.listen_iso)) return 0;
    size_t payload=n-3;
    if (!LOAD(&g.selected)) {
        /* Initial SELECT must fit one NCI packet. All other traffic is passed. */
        if ((p[0]&0x10) || !is_probe_select(p+3,payload)) return 0;
        STORE(&g.selected,1);
    }
    if (g.fragment_count>=MAX_FRAGMENTS ||
        payload>NFC_PROBE_MAX_CAPDU-g.assembly_length) {
        STORE(&g.queue_overflow,1);
        g.assembly_length=0; g.fragment_count=0;
        return 1;
    }
    unsigned f=g.fragment_count++;
    g.fragment_length[f]=n;
    bytes_copy(g.fragments[f],p,n);
    bytes_copy(g.assembly+g.assembly_length,p+3,payload);
    g.assembly_length+=(uint16_t)payload;
    if (p[0]&0x10) return 1;
    if (is_select(g.assembly,g.assembly_length) &&
        !is_probe_select(g.assembly,g.assembly_length)) {
        /* Return an unrelated reselect unchanged to the stock stack. */
        STORE(&g.selected,0);
        for (unsigned i=0;i<g.fragment_count;i++)
            forward(g.fragment_length[i],g.fragments[i]);
    } else if (LOAD(&g.capdu_ready)) {
        STORE(&g.queue_overflow,1);
    } else {
        bytes_copy(g.capdu,g.assembly,g.assembly_length);
        g.capdu_length=g.assembly_length;
        g.capdu_epoch=LOAD(&g.epoch);
        STORE(&g.capdu_ready,1);
    }
    g.assembly_length=0; g.fragment_count=0;
    return 1;
}
static void raw_rx(uint16_t n,uint8_t *p) {
    if (p && n>=3 && n<=NCI_PACKET_MAX && (size_t)p[2]+3==n) {
        unsigned mt=p[0]&0xe0u, gid=p[0]&0x0fu, oid=p[1]&0x3fu;
        if (gid==1 && mt==0x40 && (oid==1 || oid==2) && n>=4) {
            /* Four-byte prefix only, including status when the response has
             * the standard shape. Stock/concurrent responses are witnesses. */
            unsigned prefix=((unsigned)p[0]<<24)|((unsigned)p[1]<<16)|
                            ((unsigned)p[2]<<8)|p[3];
            STORE(&g.raw_response,prefix);
        }
        if (mt==0x60 && gid==1 && oid==5) {
            reset_rf();
            /* NCI activation: interface=ISO-DEP(2), protocol=4, listen mode. */
            if (p[2]>=6 && p[4]==2 && p[5]==4 && (p[6]&0x80))
                STORE(&g.listen_iso,1);
        } else if (mt==0x60 && gid==1 && oid==6) {
            reset_rf();
        }
        if (gid==1 && mt==0x60 && oid==2)
            queue_control(n,p);
        if (mt==0 && gid==0 && intercept_data(n,p)) return;
    }
    forward(n,p);
}

static void disable_capdu(void) {
    STORE(&g.allow_rf,0); STORE(&g.capdu_ready,0);
    g.awaiting=0;
}
static int release_hook(void) {
    if (!g.hooked) return 0;
    if (nfc_hw_restore(raw_rx,g.original)) return -16;
    g.hooked=0;
    /* Keep original callable pointer: a preempted HAL frame may still be inside
     * our resident hook. The owner never unloads this module. */
    return 0;
}
static void fail(const char *why) {
    disable_capdu(); g.had_failure=1;
    if (!g.command_pending) g.wait_opcode=0;
    if (g.dirty) {
        phase_set(RECOVERY,why); /* explicit STOP retries current-table cleanup */
    } else if (g.command_pending) {
        phase_set(RECOVERY,"RECOVERY: accepted NFC command unresolved; retry STOP");
    } else if (g.stop_pending) {
        phase_set(RECOVERY,"RECOVERY: accepted RF stop unresolved; retry STOP");
    } else if (g.rf_stopped) {
        g.ending=1;
        if (nfc_hw_start()==0) phase_set(START_END,why);
        else phase_set(RECOVERY,"RECOVERY: RF restart rejected; retry STOP");
    } else {
        if (release_hook()) phase_set(RECOVERY,"RECOVERY: callback changed; resident module retained");
        else phase_set(ERROR,why);
    }
}
static void command_complete(uint8_t event,uint16_t n,uint8_t *p) {
    /* This callback belongs to the queued NFA request, unlike an arbitrary raw
     * same-opcode response seen on HAL. Firmware nci_proc_prop_nxp_rsp supplies
     * the complete NCI packet. It can also dispatch an unexpected notification;
     * reject such completions instead of treating them as a success. */
    unsigned result=0x20000u;
    g.callback_event=event;g.callback_length=n;
    g.callback_size=p?(uint8_t)(n<8?n:8):0;
    if(g.callback_size)bytes_copy(g.callback_prefix,p,g.callback_size);
    if (p && n==4 && p[0]==0x41 && p[2]==1 &&
        (p[1]==1 || p[1]==2) && event==(unsigned)(0x40|p[1]))
        result=0x10000u|((unsigned)p[1]<<8)|p[3];
    STORE(&g.completion,result);
}
static int command(uint8_t oid,const uint8_t *payload,size_t n) {
    uint8_t packet[255];
    if (n>252) return -22;
    if (g.command_pending) return -16;
    packet[0]=0x21; packet[1]=oid; packet[2]=(uint8_t)n;
    if (n) bytes_copy(packet+3,payload,n);
    g.wait_opcode=oid; g.rsp_seen=0; g.rsp_status=0xff;
    STORE(&g.raw_response,0);
    g.command_pending=1;g.get_ntf_done=0;STORE(&g.completion,0);
    g.deadline=g.now+TIMEOUT_MS;
    int rc=nfc_hw_command(packet,n+3,command_complete);
    if (rc) {g.wait_opcode=0;g.command_pending=0;}
    return rc;
}
static int get_table(enum phase p) {
    rt_init(&g.snapshot);
    phase_set(p,p==GET_ADD?"Reading current NFC routes":
                p==GET_REMOVE?"Removing test AID: reading current routes":
                "Verifying NFC routes");
    return command(2,0,0);
}
static int write_chunk(void) {
    uint8_t payload[252];
    int n=rt_set_chunk(&g.target,&g.cursor,payload,sizeof(payload));
    if (n<=0) return -22;
    g.last_chunk=payload[0]==0;
    /* Mark before submission: an asynchronous failure must not claim clean. */
    g.dirty=1;
    return command(1,payload,(size_t)n);
}
/* Compare TLV multisets, including duplicate entries, without reinterpreting
 * unknown vendor routes. Both inputs have already passed snapshot validation. */
static int contains_routes(const route_table *big,const route_table *small) {
    uint8_t used[RT_MAX_ENTRIES]={0};
    for (size_t p=0;p<small->length;p+=(size_t)small->bytes[p+1]+2) {
        size_t n=(size_t)small->bytes[p+1]+2; unsigned i=0,found=0;
        for (size_t q=0;q<big->length;q+=(size_t)big->bytes[q+1]+2,i++) {
            size_t m=(size_t)big->bytes[q+1]+2;
            if (!used[i] && n==m && bytes_same(small->bytes+p,big->bytes+q,n)) {
                used[i]=1;found=1;break;
            }
        }
        if (!found) return 0;
    }
    return 1;
}
static int write_table(enum phase p) {
    /* Validate EVERY chunk before the first controller mutation. A GET packet
     * can contain a 253-byte TLV which our uint8-length API cannot retransmit. */
    size_t cursor=0; uint8_t payload[252]; int n;
    while ((n=rt_set_chunk(&g.target,&cursor,payload,sizeof(payload)))>0) {}
    if (n<0) return n;
    g.cursor=0;
    phase_set(p,p==SET_ADD?"Installing isolated test AID route":
                             "Removing only this test AID route");
    return write_chunk();
}
static void drain_control(void) {
    unsigned t=LOAD(&g.queue_tail),h=LOAD(&g.queue_head);
    while (t!=h) {
        unsigned slot=t&(CQ_SIZE-1);
        const uint8_t *p=g.queue[slot].bytes;
        size_t n=g.queue[slot].length;
        unsigned mt=p[0]&0xe0u,oid=p[1]&0x3fu;
        if (mt==0x60 && oid==2 && g.command_pending && g.wait_opcode==2) {
            if (n>=5 && p[3]==0) g.get_ntf_done=1;
            if ((p[0]&0x10) || rt_snapshot_fragment(&g.snapshot,p+3,n-3)!=RT_OK)
                g.snapshot.invalid=1;
        }
        STORE(&g.queue_tail,++t);
        h=LOAD(&g.queue_head);
    }
    unsigned completion=__atomic_exchange_n(&g.completion,0,__ATOMIC_ACQ_REL);
    if (completion && g.command_pending) {
        int valid=(completion&0x10000u) &&
                  ((completion>>8)&0xffu)==g.wait_opcode;
        g.rsp_seen=1;
        g.rsp_status=valid?(uint8_t)completion:0xff;
        if(g.rsp_status!=0)record_response_error(valid);
    }
    if (g.command_pending && g.rsp_seen &&
        (g.rsp_status!=0 || g.wait_opcode==1 || g.get_ntf_done))
        g.command_pending=0;
}
static int begin_end(void) {
    if (g.command_pending) return -16;
    disable_capdu();
    uint8_t state=nfc_hw_disc_state();
    if ((state==4 || state==5 || state==6) && !LOAD(&g.selected)) return -16;
    g.ending=1; g.stop_requested=0; g.wait_opcode=0;
    if (state==0) {
        g.rf_stopped=1;g.stop_pending=0;
        if (g.dirty) return get_table(GET_REMOVE);
        if (nfc_hw_start()) return -5;
        phase_set(START_END,"Restoring RF discovery"); return 0;
    }
    phase_set(STOP_END,"Stopping test RF session");
    if (g.stop_pending) return 0; /* Reuse the accepted stop; never queue a second. */
    g.stop_pending=1;
    int rc=nfc_hw_stop();
    if (rc) g.stop_pending=0;
    return rc;
}
int nfc_probe_start(nfc_probe_apdu_cb cb,void *ctx) {
    if (!cb) return -22;
    if (g.hooked || g.dirty || (g.phase!=IDLE && g.phase!=ERROR)) return -16;
    int rc=nfc_hw_preflight();
    if (rc) {phase_set(ERROR,rc==-84?"Firmware fingerprint mismatch":
                          rc==-95?"Experimental NFC backend disabled":
                                  "NFC stack is not ready or callback is owned");return rc;}
    /* Existing discovery is required. Never initialize/reset the controller. */
    if (nfc_hw_disc_state()!=1) {
        phase_set(ERROR,"NFC must be discovering, with no active RF transaction");
        return -16;
    }
    g.diagnostic[0]=0;g.nci_version=nfc_hw_version();
    g.callback=cb; g.context=ctx; g.ending=0; g.had_failure=0;
    g.stop_requested=0; g.rf_stopped=0; g.stop_pending=0; g.wait_opcode=0;
    g.command_pending=0;STORE(&g.completion,0);
    rt_init(&g.baseline);rt_init(&g.installed);
    STORE(&g.queue_head,0);STORE(&g.queue_tail,0);STORE(&g.queue_overflow,0);
    disable_capdu();reset_rf();
    if ((rc=nfc_hw_install(raw_rx,&g.original))!=0) {
        phase_set(ERROR,"HAL callback installation conflict");return rc;
    }
    g.hooked=1;
    phase_set(STOP_START,"Stopping discovery before test route update");
    g.stop_pending=1;
    if (nfc_hw_stop()) {g.stop_pending=0;fail("NFC stop request rejected");return -5;}
    return 0;
}
int nfc_probe_stop(void) {
    if (!g.hooked && !g.dirty && !g.command_pending && !g.stop_pending) {phase_set(IDLE,"Stopped");return 0;}
    if (g.phase==READY || g.phase==RECOVERY) {
        if (g.command_pending) {
            phase_set(RECOVERY,"RECOVERY: waiting for accepted NFC command completion");return -16;
        }
        int rc=begin_end();
        if (rc) {phase_set(RECOVERY,"RECOVERY: active unrelated RF; retry STOP when clear");return rc;}
        return 0;
    }
    g.stop_requested=1;
    disable_capdu();
    return 0;
}
int nfc_probe_send(const uint8_t *p,size_t n) {
    if (!p || n<2 || n>NFC_PROBE_MAX_RAPDU) return -22;
    if (g.phase!=READY || !g.awaiting || !LOAD(&g.selected) ||
        !LOAD(&g.listen_iso) || g.await_epoch!=LOAD(&g.epoch)) return -32;
    /* Queued NFA call owns a copy; no pointers escape the caller. */
    int rc=nfc_hw_response(p,n);
    if (rc) return -5;
    g.awaiting=0; return 0;
}
void nfc_probe_poll(uint32_t now_ms) {
    g.now=now_ms;
    if (!g.hooked) return;
    drain_control();
    if (LOAD(&g.queue_overflow)) {
        STORE(&g.queue_overflow,0);fail("RECOVERY: NFC receive overflow; retry STOP");return;
    }
    if (g.phase==RECOVERY || g.phase==ERROR || g.phase==IDLE) return;
    if (g.wait_opcode && g.rsp_seen && g.rsp_status!=0) {
        fail(g.diagnostic);return;
    }
    if (g.phase!=READY && (int32_t)(now_ms-g.deadline)>=0) {
        fail("RECOVERY: NFC transition timed out; retry STOP");return;
    }
    if (g.phase==STOP_START || g.phase==STOP_END) {
        if (nfc_hw_disc_state()!=0) return;
        g.rf_stopped=1;g.stop_pending=0;
        if (g.phase==STOP_START && !g.stop_requested) {
            if (get_table(GET_ADD)) fail("GET_ROUTING request rejected");
        } else if (g.dirty) {
            if (get_table(GET_REMOVE)) fail("RECOVERY: route read request rejected");
        } else {
            g.ending=1;
            if (nfc_hw_start()) fail("RECOVERY: RF restart rejected");
            else phase_set(START_END,"Restoring RF discovery");
        }
        return;
    }
    if (waiting_snapshot()) {
        if (g.snapshot.invalid) {fail("RECOVERY: invalid routing snapshot");return;}
        if (!g.rsp_seen || !g.snapshot.complete) return;
        g.wait_opcode=0;
        if (g.phase==VERIFY_ADD || g.phase==VERIFY_REMOVE) {
            int adding=g.phase==VERIFY_ADD;
            if (!rt_equal(&g.snapshot,&g.target)) {
                fail("RECOVERY: routing verification differed; retry STOP");return;
            }
            if (!adding) {
                if (!contains_routes(&g.snapshot,&g.baseline)) {
                    fail("RECOVERY: original routes missing after cleanup");return;
                }
                g.dirty=0;
            }
            if (adding && g.stop_requested) {
                if (get_table(GET_REMOVE)) fail("RECOVERY: cleanup read rejected");
                return;
            }
            STORE(&g.allow_rf,adding?1u:0u);
            if (nfc_hw_start()) {fail("RECOVERY: discovery restart rejected");return;}
            phase_set(adding?START_TEST:START_END,
                      adding?"Starting isolated test listener":"Restoring discovery");
            return;
        }
        g.target=g.snapshot;
        if (g.phase==GET_ADD) {
            if (g.stop_requested) {
                g.ending=1;
                if (nfc_hw_start()) fail("RF restart rejected");
                else phase_set(START_END,"Restoring discovery");
                return;
            }
            g.baseline=g.snapshot;
            if (rt_add_probe(&g.target)) {fail("Test AID already routed or routing table full");return;}
            g.installed=g.target;
            if (write_table(SET_ADD)) fail("RECOVERY: test route write rejected");
        } else {
            if (rt_remove_probe(&g.target)) {fail("RECOVERY: test AID changed by another owner");return;}
            if (!contains_routes(&g.snapshot,&g.baseline)) {
                /* A failed multi-packet SET can leave only a subset. Restore
                 * baseline ONLY if every current entry belongs to our intended
                 * table. Unknown concurrent changes make repair ambiguous. */
                if (!contains_routes(&g.installed,&g.snapshot)) {
                    fail("RECOVERY: original routes missing with concurrent changes");return;
                }
                g.target=g.baseline;
            }
            if (rt_equal(&g.target,&g.snapshot)) {
                g.dirty=0;
                if (nfc_hw_start()) fail("RF restart rejected");
                else phase_set(START_END,"Restoring discovery");
            } else if (write_table(SET_REMOVE)) fail("RECOVERY: route removal rejected");
        }
        return;
    }
    if (g.phase==SET_ADD || g.phase==SET_REMOVE) {
        if (!g.rsp_seen) return;
        g.wait_opcode=0;
        if (g.last_chunk) {
            if (get_table(g.phase==SET_ADD?VERIFY_ADD:VERIFY_REMOVE))
                fail("RECOVERY: verification request rejected");
        } else if (write_chunk()) fail("RECOVERY: routing continuation rejected");
        return;
    }
    if (g.phase==START_TEST || g.phase==START_END) {
        uint8_t state=nfc_hw_disc_state();
        if (state==0) return;
        /* DISCOVERY or a promptly activated listening session. */
        if (state!=1 && state!=5) {fail("RECOVERY: unexpected RF state");return;}
        g.rf_stopped=0;
        if (g.phase==START_TEST) {
            phase_set(READY,"READY: synthetic test AID only");
            if (g.stop_requested) {
                if (begin_end()) fail("RECOVERY: cleanup could not stop RF");
            }
        } else {
            disable_capdu();
            if (release_hook()) phase_set(RECOVERY,"RECOVERY: callback ownership conflict");
            else phase_set(g.had_failure?ERROR:IDLE,
                g.had_failure?(g.diagnostic[0]?g.diagnostic:
                    "Stopped after NFC error; routes cleaned"):"Stopped; test AID removed");
        }
        return;
    }
    if (g.phase==READY && LOAD(&g.capdu_ready)) {
        uint8_t copy[NFC_PROBE_MAX_CAPDU];
        size_t n=g.capdu_length;
        unsigned epoch=g.capdu_epoch;
        bytes_copy(copy,g.capdu,n);
        STORE(&g.capdu_ready,0);
        if (epoch!=LOAD(&g.epoch) || !LOAD(&g.selected)) return;
        if (g.awaiting && g.await_epoch==epoch) {
            fail("RECOVERY: overlapping synthetic APDUs");return;
        }
        g.awaiting=1;g.await_epoch=epoch;
        g.callback(copy,n,g.context);
    }
}
int nfc_probe_ready(void) {return g.phase==READY;}
int nfc_probe_can_unload(void) {return !g.hooked && !g.dirty && !g.stop_pending && !g.command_pending;}
int nfc_probe_error(void) {return (g.phase==ERROR || g.phase==RECOVERY)?-5:0;}
const char *nfc_probe_status(void) {return g.message?g.message:"Stopped";}
const char *nfc_probe_diagnostic(void) {return g.diagnostic;}
#ifdef NFC_PROBE_HOST_TEST
void nfc_probe_test_rx(uint16_t n,uint8_t *p) {raw_rx(n,p);}
#endif
