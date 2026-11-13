/* Synthetic APDU laboratory transport. Exact p67glt / 3.201.026 only.
 * OpenVela GATT ABI independently matched to the stock bttool call sites.
 * See transport-evidence.txt. No Xiaomi pairing or payment service is used. */
#include "transport.h"
#include <stddef.h>

typedef struct { uint8_t b[6]; } address_t;
typedef struct { uint8_t type, pad[3]; union { uint16_t u16; uint32_t u32; uint8_t u128[16]; } v; } uuid_t;
typedef uint16_t (*read_cb_t)(void *, address_t *, uint16_t, uint32_t);
typedef uint16_t (*write_cb_t)(void *, address_t *, uint16_t, const uint8_t *, uint16_t, uint16_t);
typedef struct {
    uint16_t handle; uuid_t uuid; uint32_t type, properties, permissions, response;
    read_cb_t read; write_cb_t write; uint32_t length; uint8_t *value;
} attr_t;
typedef struct { int32_t count; attr_t *attributes; } database_t;
typedef struct {
    uint32_t size;
    void (*connected)(void *, address_t *);
    void (*disconnected)(void *, address_t *);
    void (*added)(void *, uint32_t, uint16_t);
    void (*removed)(void *, uint32_t, uint16_t);
    void (*notified)(void *, address_t *, uint32_t, uint16_t);
    void (*mtu)(void *, address_t *, uint32_t);
    void (*phy_read)(void *, address_t *, uint32_t, uint32_t);
    void (*phy_updated)(void *, address_t *, uint32_t, uint32_t, uint32_t);
    void (*conn_params)(void *, address_t *, uint16_t, uint16_t, uint16_t);
} callbacks_t;
typedef struct {
    address_t peer; uint8_t type, peer_type; address_t own;
    uint8_t own_type; int8_t power; uint32_t interval, duration;
    uint8_t channel, filter;
} adv_params_t;
typedef struct {
    uint32_t size;
    void (*started)(void *, uint8_t, uint8_t);
    void (*stopped)(void *, uint8_t);
} adv_callbacks_t;

#ifndef TRANSPORT_HOST_TEST
_Static_assert(sizeof(attr_t) == 56, "p67glt GATT attribute ABI");
_Static_assert(offsetof(attr_t, uuid) == 4, "p67glt UUID ABI");
_Static_assert(offsetof(attr_t, write) == 44, "p67glt callback ABI");
_Static_assert(sizeof(callbacks_t) == 40, "p67glt callbacks ABI");
_Static_assert(sizeof(adv_params_t) == 28, "p67glt advertising ABI");
#define FN(addr, type) ((type)(uintptr_t)((addr) | 1u))
#define current_instance FN(0x2c35603cu, void *(*)(void))
#define register_service FN(0x2c357594u, uint8_t (*)(void *, void **, callbacks_t *))
#define unregister_service FN(0x2c357674u, uint8_t (*)(void *))
#define add_table FN(0x2c357764u, uint8_t (*)(void *, database_t *))
#define remove_table FN(0x2c35783cu, uint8_t (*)(void *, uint16_t))
#define notify_value FN(0x2c357974u, uint8_t (*)(void *, address_t *, uint16_t, const uint8_t *, uint16_t))
#define start_advertising FN(0x2c357f68u, void *(*)(void *, adv_params_t *, uint8_t *, uint16_t, uint8_t *, uint16_t, adv_callbacks_t *))
#define stop_advertising FN(0x2c358018u, void (*)(void *, void *))
#else
#include "transport_test_backend.h"
#endif

#define RING_SIZE 1024u
#define FRAME_SIZE (10u + TRANSPORT_MAX_REQUEST)
#define LOAD(x) __atomic_load_n(&(x), __ATOMIC_ACQUIRE)
#define STORE(x,v) __atomic_store_n(&(x), (v), __ATOMIC_RELEASE)
/* Process-level client selected/created for this control thread's task group.
 * It may be shared. Retain until reboot; never delete it from this module. */
static void *instance, *service, *advertiser;
/* OpenVela retains the caller's output-pointer address and clears it during
 * unregister. It must have module lifetime, never be a stack temporary. */
static void *registration_slot;
static callbacks_t callbacks;
static adv_callbacks_t adv_callbacks;
static attr_t attributes[4];
static database_t database;
static adv_params_t adv_parameters;
static transport_frame_cb receiver;
static address_t peer;
static uint32_t running, subscribed, generation, table_state, adv_state, retired;
static int last_error;
static uint8_t ring[RING_SIZE], incoming[FRAME_SIZE], outgoing[FRAME_SIZE];
static uint32_t ring_generation[RING_SIZE];
static uint32_t ring_head, ring_tail, tx_completion;
static uint16_t incoming_used, incoming_target, outgoing_length, outgoing_offset, sent_length;
static uint32_t observed_generation;
static uint32_t delivered_generation;
static uint32_t tx_started_ms, incoming_started_ms;
static uint8_t tx_waiting, adv_attempted;

static void copy_bytes(void *dst, const void *src, uint32_t size) {
    uint8_t *d=dst; const uint8_t *s=src;
    while (size--) *d++=*s++;
}
static void clear_bytes(void *dst, uint32_t size) {
    uint8_t *d=dst; while (size--) *d++=0;
}
static int peer_equal(const address_t *address) {
    for (unsigned i=0;i<6;i++) if (address->b[i]!=LOAD(peer.b[i])) return 0;
    return 1;
}
static void publish_peer(const address_t *address) {
    for (unsigned i=0;i<6;i++) STORE(peer.b[i],address->b[i]);
}
static void snapshot_peer(address_t *address) {
    for (unsigned i=0;i<6;i++) address->b[i]=LOAD(peer.b[i]);
}
static void invalidate(int error) {
    STORE(subscribed,0); STORE(last_error,error);
    __atomic_add_fetch(&generation,1,__ATOMIC_ACQ_REL);
}

/* All callbacks only publish state. Calling the synchronous socket API from
 * its own callback thread can deadlock; all firmware calls are made by poll. */
static void connected(void *handle, address_t *address) { (void)handle; (void)address; }
static void disconnected(void *handle, address_t *address) {
    if (handle==LOAD(service) && LOAD(subscribed) && peer_equal(address)) invalidate(-4);
}
static void table_added(void *handle, uint32_t status, uint16_t id) {
    if (handle!=LOAD(service) || id!=1 || !LOAD(running)) return;
    STORE(table_state,status ? 2u : 1u);
    if (status) STORE(last_error,-20-(int)status);
}
static void table_removed(void *handle, uint32_t status, uint16_t id) {
    (void)handle; (void)status; (void)id;
}
static void notified(void *handle, address_t *address, uint32_t status, uint16_t id) {
    if (handle==LOAD(service) && id==2 && LOAD(subscribed) && peer_equal(address))
        STORE(tx_completion,status+1u);
}
static void mtu_changed(void *handle, address_t *address, uint32_t mtu) {
    (void)handle; (void)address; (void)mtu; /* Fixed 20-byte chunks work at MTU 23. */
}
static uint16_t cccd_written(void *handle, address_t *address, uint16_t id,
                             const uint8_t *value, uint16_t length, uint16_t offset) {
    if (!LOAD(running) || handle!=LOAD(service) || id!=3 || offset || length!=2) return 0;
    if (value[0]==1 && value[1]==0) {
        if (LOAD(subscribed) && !peer_equal(address)) return 0;
        if (!LOAD(subscribed)) publish_peer(address);
        __atomic_add_fetch(&generation,1,__ATOMIC_ACQ_REL);
        STORE(last_error,0); STORE(subscribed,1);
    } else if (LOAD(subscribed) && peer_equal(address)) invalidate(-4);
    return length;
}
static uint16_t rx_written(void *handle, address_t *address, uint16_t id,
                           const uint8_t *value, uint16_t length, uint16_t offset) {
    if (!LOAD(running) || !LOAD(subscribed) || handle!=LOAD(service) || id!=4 || offset ||
        !peer_equal(address)) return 0;
    uint32_t head=LOAD(ring_head), tail=LOAD(ring_tail);
    if (length>RING_SIZE-(head-tail)) { invalidate(-5); return 0; }
    uint32_t epoch=LOAD(generation);
    for (uint16_t i=0;i<length;i++) {
        ring[(head+i)&(RING_SIZE-1u)]=value[i];
        ring_generation[(head+i)&(RING_SIZE-1u)]=epoch;
    }
    STORE(ring_head,head+length);
    return length;
}
static void advertising_started(void *handle, uint8_t id, uint8_t status) {
    (void)handle; (void)id;
    STORE(adv_state,status ? 3u : 1u);
    if (status) STORE(last_error,-40-(int)status);
}
static void advertising_stopped(void *handle, uint8_t id) {
    (void)handle; (void)id; STORE(adv_state,2u);
}

static void uuid128(uuid_t *uuid, uint8_t component) {
    /* OpenVela UUID storage is little-endian, like the BLE AD value. */
    const uint8_t bytes[16]={0x31,0x0e,0x8c,0xbb,0x40,0x9d,0xcb,0xa9,
                             0x89,0x4f,0x51,0x8e,0x01,0x00,0xbb,0x7d};
    uuid->type=16; copy_bytes(uuid->v.u128,bytes,16); uuid->v.u128[12]=component;
#ifdef TRANSPORT_TAG_PROFILE
    uuid->v.u128[12]=(uint8_t)(component+0x10u);
#endif
}

#ifndef TRANSPORT_HOST_TEST
/* Generated from the independently extracted firmware, first 24 bytes each.
 * This verifies bindings before any firmware call. No data/ROM is modified. */
typedef struct { uintptr_t address; uint8_t bytes[24]; } fingerprint_t;
#include "transport_fingerprints.h"
static int firmware_matches(void) {
    for (unsigned i=0;i<sizeof(transport_fingerprints)/sizeof(transport_fingerprints[0]);i++) {
        const volatile uint8_t *actual=(const volatile uint8_t *)transport_fingerprints[i].address;
        for (unsigned j=0;j<24;j++) if (actual[j]!=transport_fingerprints[i].bytes[j]) return 0;
    }
    return 1;
}
#else
static int firmware_matches(void) { return 1; }
#endif

int transport_start(transport_frame_cb callback) {
    if (LOAD(running)) return -2;
    if (retired) return -13; /* No reuse while old asynchronous callbacks exist. */
    if (!firmware_matches()) return -10;
    instance=current_instance();
    if (!instance) return -11;
    receiver=callback;
    clear_bytes(&callbacks,sizeof(callbacks));
    callbacks.size=sizeof(callbacks); callbacks.connected=connected;
    callbacks.disconnected=disconnected; callbacks.added=table_added;
    callbacks.removed=table_removed; callbacks.notified=notified; callbacks.mtu=mtu_changed;
    clear_bytes(attributes,sizeof(attributes));
    attributes[0].handle=1; uuid128(&attributes[0].uuid,1); attributes[0].permissions=1;
    attributes[1].handle=2; uuid128(&attributes[1].uuid,3);
    attributes[1].type=3; attributes[1].properties=0x10;
    attributes[2].handle=3; attributes[2].uuid.type=2; attributes[2].uuid.v.u16=0x2902;
    /* .026 socket server forwards callbacks only for response mode1.
     * Our experimental client writes CCC directly; reads are not supported. */
    attributes[2].type=4; attributes[2].permissions=2; attributes[2].response=1;
    attributes[2].write=cccd_written;
    attributes[3].handle=4; uuid128(&attributes[3].uuid,2);
    attributes[3].type=3; attributes[3].properties=8; attributes[3].permissions=2;
    attributes[3].response=1; attributes[3].write=rx_written;
    database.count=4; database.attributes=attributes;
    clear_bytes(&adv_parameters,sizeof(adv_parameters));
    adv_parameters.type=0; adv_parameters.own_type=0xff;
    adv_parameters.interval=160; /* 100 ms, 0.625 ms units. */
    adv_callbacks.size=sizeof(adv_callbacks);
    adv_callbacks.started=advertising_started; adv_callbacks.stopped=advertising_stopped;
    STORE(subscribed,0); STORE(table_state,0); STORE(adv_state,0);
    STORE(last_error,0); STORE(ring_head,0); STORE(ring_tail,0); STORE(tx_completion,0);
    outgoing_length=outgoing_offset=incoming_used=incoming_target=sent_length=0;
    tx_waiting=adv_attempted=0; advertiser=0;
    __atomic_add_fetch(&generation,1,__ATOMIC_ACQ_REL); observed_generation=LOAD(generation);
    STORE(running,1);
    registration_slot=0;
    int error=register_service(instance,&registration_slot,&callbacks);
    if (error) { STORE(running,0); STORE(last_error,-60-error); return -60-error; }
    STORE(service,registration_slot);
    error=add_table(service,&database);
    if (error) { transport_stop(); STORE(last_error,-80-error); return -80-error; }
    return 0;
}

int transport_ready(void) { return LOAD(running) && LOAD(table_state)==1 && LOAD(subscribed); }
int transport_error(void) { return LOAD(last_error); }
uint32_t transport_debug_state(void) {
    return (LOAD(running)&1u) | ((LOAD(table_state)&3u)<<4) |
           ((LOAD(adv_state)&3u)<<8) | ((uint32_t)(adv_attempted!=0)<<12) |
           ((uint32_t)(LOAD(service)!=0)<<16) | ((LOAD(subscribed)&1u)<<20);
}
uint32_t transport_generation(void) { return LOAD(generation); }
uint32_t transport_frame_generation(void) { return delivered_generation; }

int transport_notify(uint8_t type,uint32_t sequence,const uint8_t *payload,uint16_t length) {
    if (!transport_ready()) return -1;
    if (outgoing_length) return -2;
    if (LOAD(generation)!=observed_generation) return -7;
    uint16_t limit=type==1 ? TRANSPORT_MAX_REQUEST : type==2 ? TRANSPORT_MAX_RESPONSE : type==3 ? 128u : 0u;
    if (!payload || length==0 || length>limit) return -3;
    outgoing[0]=0x4d; outgoing[1]=0x42; outgoing[2]=1; outgoing[3]=type;
    outgoing[4]=(uint8_t)(sequence>>24); outgoing[5]=(uint8_t)(sequence>>16);
    outgoing[6]=(uint8_t)(sequence>>8); outgoing[7]=(uint8_t)sequence;
    outgoing[8]=(uint8_t)(length>>8); outgoing[9]=(uint8_t)length;
    copy_bytes(outgoing+10,payload,length); outgoing_offset=0; outgoing_length=length+10;
    return 0;
}

static uint32_t synchronize_generation(void) {
    uint32_t current=LOAD(generation);
    if (current!=observed_generation) {
        observed_generation=current;
        incoming_used=incoming_target=outgoing_length=outgoing_offset=sent_length=0;
        tx_waiting=0; STORE(tx_completion,0);
    }
    return current;
}

static void receive_byte(uint8_t byte,uint32_t epoch,uint32_t now) {
    /* CCC/disconnect callbacks may run during this poll, including halfway
     * through a frame. Never combine bytes from different peer sessions. */
    if (epoch!=synchronize_generation()) return;
    if (incoming_used>=FRAME_SIZE) { invalidate(-6); incoming_used=0; return; }
    if (!incoming_used) incoming_started_ms=now;
    incoming[incoming_used++]=byte;
    if ((incoming_used==1 && byte!=0x4d) || (incoming_used==2 && byte!=0x42) ||
        (incoming_used==3 && byte!=1) ||
        (incoming_used==4 && byte!=TRANSPORT_RESPONSE && byte!=TRANSPORT_CONTROL
#ifdef TRANSPORT_TAG_PROFILE
         && byte!=TRANSPORT_TAG
#endif
        )) {
        invalidate(-6); incoming_used=0; return;
    }
    if (incoming_used==10) {
        uint16_t length=((uint16_t)incoming[8]<<8)|incoming[9];
        if ((incoming[3]==TRANSPORT_RESPONSE && (length<2 || length>TRANSPORT_MAX_RESPONSE)) ||
            (incoming[3]==TRANSPORT_CONTROL && length!=1)
#ifdef TRANSPORT_TAG_PROFILE
            || (incoming[3]==TRANSPORT_TAG && (length<1 || length>TRANSPORT_MAX_TAG))
#endif
            ) { invalidate(-6); incoming_used=0; return; }
        incoming_target=length+10;
    }
    if (incoming_target && incoming_used==incoming_target) {
        uint32_t sequence=((uint32_t)incoming[4]<<24)|((uint32_t)incoming[5]<<16)|
                          ((uint32_t)incoming[6]<<8)|incoming[7];
        if (receiver && epoch==LOAD(generation)) {
            delivered_generation=epoch;
            receiver(incoming[3],sequence,incoming+10,incoming_target-10);
        }
        incoming_used=incoming_target=0;
    }
}

void transport_poll(uint32_t now) {
    if (!LOAD(running)) return;
    (void)synchronize_generation();
    uint32_t adv=LOAD(adv_state);
    if (adv==2 || adv==3) {
        advertiser=0; STORE(adv_state,0);
        if (adv==2) adv_attempted=0;
    }
    if (LOAD(table_state)==1 && !adv_attempted && !transport_ready()) {
        /* A separate advertising instance; stop never uses an arbitrary ID. */
        uint8_t ad[21]={2,1,6,17,7};
#ifdef TRANSPORT_TAG_PROFILE
        uint8_t scan[9]={8,9,'M','B','-','T','A','G','S'};
#else
        uint8_t scan[9]={8,9,'M','B','-','A','P','D','U'};
#endif
        copy_bytes(ad+5,attributes[0].uuid.v.u128,16);
        adv_attempted=1;
        advertiser=start_advertising(instance,&adv_parameters,ad,sizeof(ad),scan,sizeof(scan),&adv_callbacks);
        if (!advertiser) STORE(last_error,-12);
    }
    if (!transport_ready()) return;
    if (incoming_used && (uint32_t)(now-incoming_started_ms)>=1500u) {
        invalidate(-9); return;
    }
    uint32_t tail=LOAD(ring_tail), head=LOAD(ring_head);
    while (tail!=head && transport_ready()) {
        uint32_t slot=tail&(RING_SIZE-1u);
        uint8_t byte=ring[slot]; uint32_t epoch=ring_generation[slot]; tail++;
        STORE(ring_tail,tail);
        receive_byte(byte,epoch,now);
    }
    (void)synchronize_generation();
    if (!transport_ready() || !outgoing_length) return;
    if (tx_waiting) {
        uint32_t complete=LOAD(tx_completion);
        if (!complete) {
            if ((uint32_t)(now-tx_started_ms)>=1500u) invalidate(-8);
            return;
        }
        STORE(tx_completion,0); tx_waiting=0;
        if (complete!=1) { invalidate(-100-(int)complete); return; }
        outgoing_offset+=sent_length;
        if (outgoing_offset==outgoing_length) { outgoing_length=0; return; }
    }
    uint16_t remaining=outgoing_length-outgoing_offset;
    sent_length=remaining>20 ? 20 : remaining;
    address_t destination;
    uint32_t send_generation=LOAD(generation);
    snapshot_peer(&destination);
    if (send_generation!=LOAD(generation) || send_generation!=observed_generation || !transport_ready()) return;
    tx_waiting=1; tx_started_ms=now; STORE(tx_completion,0);
    int error=notify_value(service,&destination,2,outgoing+outgoing_offset,sent_length);
    if (error) { tx_waiting=0; invalidate(-120-error); }
}

void transport_stop(void) {
    if (!LOAD(running)) return;
    STORE(running,0); retired=1; invalidate(0);
    /* Callbacks are static and the module must remain resident until reboot.
     * Service removal is asynchronous; never rmmod this code while callbacks
     * could still be queued. */
    if (advertiser && LOAD(adv_state)!=2 && LOAD(adv_state)!=3) stop_advertising(instance,advertiser);
    advertiser=0;
    if (service) {
        if (LOAD(table_state)==1) (void)remove_table(service,1);
        (void)unregister_service(service); STORE(service,0);
    }
    outgoing_length=incoming_used=incoming_target=0;
}
