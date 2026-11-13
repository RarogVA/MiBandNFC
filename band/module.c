/* Single control loop for the synthetic APDU experiment. No payment AIDs.
 * This module stays resident until reboot: asynchronous firmware callbacks
 * may outlive a user action, so there is deliberately no rmmod/destructor. */
#include <stddef.h>
#include <stdint.h>
#include "transport.h"
#include "nfc_probe.h"
#include "native_clock.h"
#include "diagnostic.h"

#define RESPONSE_DEADLINE_MS 3000u
#define EXPERIMENT_LIMIT_MS 120000u
#define POLL_MS 10u

typedef void (*timer_callback)(void *);
#ifndef APDU_LAB_HOST_TEST
#define fw_timer_create ((void *(*)(timer_callback,uint32_t,void *))(uintptr_t)0x2c558209u)
#define fw_log ((int (*)(int,const char *,...))(uintptr_t)0x2c1beab9u)
typedef struct { uintptr_t address; uint8_t bytes[24]; } module_fingerprint;
#include "module_fingerprints.h"
static int module_matches(void) {
    for(unsigned i=0;i<sizeof(module_fingerprints)/sizeof(module_fingerprints[0]);i++) {
        const volatile uint8_t *p=(const volatile uint8_t *)module_fingerprints[i].address;
        for(unsigned j=0;j<24;j++)if(p[j]!=module_fingerprints[i].bytes[j])return 0;
    }
    return 1;
}
#else
#include "module_test_backend.h"
#endif

static uint32_t now_ms, peer_generation, sequence=0x10000000u;
static uint32_t operation_sequence, started_at;
static int booted, desired, last_ready, last_clean, pending, request_sent;
static int operation, stopped_by_fault, clock_failed;
static uint32_t request_sequence, request_at;
static uint16_t request_length;
static uint8_t request_bytes[261];
static char last_status[128];
static int initialization_attempted, initialization_result=-1;
static uint32_t initialization_complete;
static int diagnostic_error;
static uint32_t diagnostic_state;
static int diagnostic_seen;
typedef struct { char bytes[128]; uint32_t sequence; uint16_t length; uint8_t queued; } status_message;
static status_message operation_status, reply_status;

static size_t length(const char *s,size_t limit) {
    size_t n=0; while(n<limit && s[n])n++; return n;
}
static void copy(void *d,const void *s,size_t n) {
    uint8_t *to=d; const uint8_t *from=s; for(size_t i=0;i<n;i++)to[i]=from[i];
}
static int equal(const void *a,const void *b,size_t n) {
    const uint8_t *x=a,*y=b;for(size_t i=0;i<n;i++)if(x[i]!=y[i])return 0;return 1;
}
static void queue_status(uint32_t seq,const char *prefix,const char *detail) {
    status_message *message=operation && seq==operation_sequence ? &operation_status : &reply_status;
    size_t n=length(prefix,sizeof(message->bytes)-1);
    copy(message->bytes,prefix,n);
    if(detail) {
        size_t d=length(detail,sizeof(message->bytes)-1-n);
        copy(message->bytes+n,detail,d);n+=d;
    }
    message->bytes[n]=0;message->length=(uint16_t)n;message->sequence=seq;message->queued=1;
}
static int allowed_request(const uint8_t *p,size_t n) {
    static const uint8_t select[]={0,0xa4,4,0,8,0xf0,0x4d,0x42,0x41,0x50,0x44,0x55,1,0};
    if(n==sizeof(select) && equal(p,select,n))return 1;
    return n==22 && p[0]==0x80 && p[1]==0x10 && p[2]==0 && p[3]==0 && p[4]==16 && p[21]==0;
}
static int expected_response(const uint8_t *p,size_t n) {
    if(request_length==14)return n==2 && p[0]==0x90 && p[1]==0;
    if(n!=18 || p[16]!=0x90 || p[17]!=0)return 0;
    for(unsigned i=0;i<16;i++)if(p[i]!=(uint8_t)(request_bytes[20-i]^0xa5))return 0;
    return 1;
}
static void stop_after_fault(const char *reason) {
    desired=0;pending=0;request_sent=0;stopped_by_fault=1;
    (void)nfc_probe_stop();
    queue_status(operation_sequence,"ERROR:",reason);
    fw_log(4,"[APDU_LAB] %s",reason);
}
static int sample_clock(void) {
    uint32_t measured;
    if(native_clock_ms(&measured)==0) {now_ms=measured;return 1;}
    if(!clock_failed) {
        clock_failed=1;
        if(booted>0)stop_after_fault("monotonic clock failed; test disabled");
    }
    return 0;
}
static void reconcile_peer(void) {
    uint32_t generation=transport_generation();
    if(generation==peer_generation)return;
    peer_generation=generation;pending=0;request_sent=0;
    operation_status.queued=reply_status.queued=0;
    if(desired || !nfc_probe_can_unload()) {
        desired=0;operation=2;stopped_by_fault=1;
        (void)nfc_probe_stop();
    }
}
static void apdu_received(const uint8_t *bytes,size_t size,void *context) {
    (void)context;
    reconcile_peer();
    if(!sample_clock())return;
    if(!desired || !transport_ready() || !nfc_probe_ready())return;
    if(!allowed_request(bytes,size)) {
        static const uint8_t unsupported[]={0x6d,0};
        (void)nfc_probe_send(unsupported,sizeof(unsupported));return;
    }
    if(pending) {stop_after_fault("overlapping APDU");return;}
    copy(request_bytes,bytes,size);request_length=(uint16_t)size;
    request_sequence=++sequence;
    if(!request_sequence)request_sequence=++sequence;
    request_at=now_ms;request_sent=0;pending=1;
    fw_log(6,"[APDU_LAB] RF CAPDU seq=%lu len=%u",(unsigned long)request_sequence,(unsigned)size);
}
static void describe(uint32_t seq) {
    if(nfc_probe_error())queue_status(seq,"ERROR:",nfc_probe_status());
    else if(nfc_probe_ready())queue_status(seq,"READY",0);
    else if(nfc_probe_can_unload())queue_status(seq,"STOPPED",0);
    else queue_status(seq,desired?"STARTING:":"STOPPING:",nfc_probe_status());
}
static void frame_received(uint8_t type,uint32_t seq,const uint8_t *p,uint16_t n) {
    /* A CCC/disconnect callback can race the start of this poll iteration. */
    reconcile_peer();
    if(transport_frame_generation()!=transport_generation())return;
    if(type==TRANSPORT_CONTROL && n==1) {
        if(p[0]==0) {describe(seq);return;}
        if(p[0]==3) {
            if(desired || !nfc_probe_can_unload()) {
                queue_status(seq,"ERROR:stop NFC before reading capabilities",0);return;
            }
            queue_status(seq,"INFO:",nfc_probe_info());return;
        }
        if(p[0]>2) {queue_status(seq,"ERROR:unknown control",0);return;}
        if(p[0]==1) {
            if(clock_failed) {queue_status(seq,"ERROR:clock failed; restart band",0);return;}
            if(!transport_ready())return;
            if(desired && operation_sequence==seq) {describe(seq);return;}
            if(desired || !nfc_probe_can_unload()) {
                queue_status(seq,"ERROR:NFC already active or recovering",0);return;
            }
            operation_sequence=seq;operation=1;stopped_by_fault=0;
            if(nfc_probe_start(apdu_received,0)!=0) {
                queue_status(seq,"ERROR:",nfc_probe_status());return;
            }
            desired=1;started_at=now_ms;last_status[0]=0;last_ready=last_clean=-1;
            queue_status(seq,"STARTING:",nfc_probe_status());
        } else {
            operation_sequence=seq;operation=2;desired=0;pending=0;request_sent=0;
            stopped_by_fault=0;last_status[0]=0;last_ready=last_clean=-1;
            if(nfc_probe_stop()!=0)queue_status(seq,"ERROR:",nfc_probe_status());
            else describe(seq);
        }
        return;
    }
    if(type!=TRANSPORT_RESPONSE || !pending || !request_sent || seq!=request_sequence)return;
    if(!sample_clock())return;
    if((uint32_t)(now_ms-request_at)>=RESPONSE_DEADLINE_MS) {stop_after_fault("late Bluetooth response");return;}
    if(!expected_response(p,n)) {stop_after_fault("invalid synthetic response");return;}
    if(nfc_probe_send(p,n)!=0) {stop_after_fault("NFC transmit failed");return;}
    fw_log(6,"[APDU_LAB] BT RAPDU -> RF seq=%lu len=%u",(unsigned long)seq,(unsigned)n);
    pending=0;request_sent=0;
}

static void poll(void *timer) {
    (void)timer;
    /* LVGL may schedule the newly created timer before the NSH constructor
     * returns. Publish completion only after all initializer diagnostics. */
    if(!__atomic_load_n(&initialization_complete,__ATOMIC_ACQUIRE))return;
    if(!sample_clock()) {
        /* Permanently disarm experiments, but keep pumping async restoration.
         * This fallback is a cleanup poll budget, not an APDU timing clock. */
        now_ms+=POLL_MS;
        if(!booted)return;
    }
    if(!booted) {
        int rc=transport_start(frame_received);
        booted=rc==0 ? 1 : -1;
        peer_generation=transport_generation();
        diagnostic_set("TRANSPORT_START",rc,transport_debug_state());
        fw_log(rc ? 3 : 6,"[APDU_LAB] transport start %d",rc);
    }
    if(booted<0)return;
    /* Notice disconnect/re-subscription before delivering any queued response. */
    reconcile_peer();
    transport_poll(now_ms);
    uint32_t state=transport_debug_state();
    int error=transport_error();
    if(!diagnostic_seen || state!=diagnostic_state || error!=diagnostic_error) {
        diagnostic_seen=1;diagnostic_state=state;diagnostic_error=error;
        diagnostic_set("BLE",error,state);
        fw_log(error ? 4 : 6,"[APDU_LAB] BLE rc=%d state=%lu",error,(unsigned long)state);
    }
    reconcile_peer();
    (void)sample_clock();
    nfc_probe_poll(now_ms);
    if(desired && !transport_ready())stop_after_fault("Bluetooth disconnected");
    if(desired && (uint32_t)(now_ms-started_at)>=EXPERIMENT_LIMIT_MS)
        stop_after_fault("120 second test limit");
    if(desired && nfc_probe_error())stop_after_fault(nfc_probe_status());
    if(pending && (uint32_t)(now_ms-request_at)>=RESPONSE_DEADLINE_MS)
        stop_after_fault("Bluetooth APDU timeout");
    if(pending && !request_sent) {
        int rc=transport_notify(TRANSPORT_REQUEST,request_sequence,request_bytes,request_length);
        if(rc==0)request_sent=1;
        else if(rc!=-2)stop_after_fault("Bluetooth CAPDU send failed");
    }
    const char *stage=nfc_probe_status();
    size_t size=length(stage,sizeof(last_status)-1);
    int ready=nfc_probe_ready(),clean=nfc_probe_can_unload();
    if(operation && !stopped_by_fault &&
       (ready!=last_ready || clean!=last_clean ||
        length(last_status,sizeof(last_status))!=size || !equal(stage,last_status,size))) {
        describe(operation_sequence);
        copy(last_status,stage,size);last_status[size]=0;last_ready=ready;last_clean=clean;
    }
    /* Never let diagnostic messages displace an in-flight APDU. */
    status_message *message=operation_status.queued ? &operation_status : &reply_status;
    if(message->queued && !pending && transport_ready()) {
        if(transport_notify(TRANSPORT_STATUS,message->sequence,(const uint8_t *)message->bytes,message->length)==0)
            message->queued=0;
    }
}

int module_initialize(void) {
    if(initialization_attempted)return initialization_result;
    initialization_attempted=1;
    diagnostic_set("INIT",0,0);
    if(!module_matches()) {diagnostic_set("CORE_MISMATCH",-1,0);return -1;}
    uint32_t time;
    if(native_clock_ms(&time)!=0) {diagnostic_set("CLOCK_FAILED",-1,0);return -1;}
    if(!fw_timer_create(poll,POLL_MS,0)) {diagnostic_set("TIMER_FAILED",-1,0);return -1;}
    diagnostic_set("TIMER_CREATED",0,0);
    fw_log(6,"[APDU_LAB] loaded; BLE only until explicit START; do not rmmod");
    initialization_result=0;
    __atomic_store_n(&initialization_complete,1u,__ATOMIC_RELEASE);
    return initialization_result;
}

/* Exact NuttX insmod invokes constructors, not ELF e_entry. Keep e_entry only
 * as metadata; this is the actual runtime initialization path. */
#ifndef APDU_LAB_HOST_TEST
__attribute__((constructor)) static void module_constructor(void) {
    (void)module_initialize();
}
#endif
