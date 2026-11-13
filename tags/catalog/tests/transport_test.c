/* Reuses the established exact-firmware transport mock scaffolding. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#define TRANSPORT_HOST_TEST 1
/* POSIX headers on macOS also define uuid_t; isolate the firmware ABI name. */
#define uuid_t transport_test_uuid_t
#include "../../../band/transport.c"
#undef uuid_t
static int fake_instance,fake_service,fake_adv;
static void **retained_service_output;
static unsigned delivered,notifies,adv_starts,adv_stops;
static unsigned instance_gets,table_adds,table_removes,service_unregisters;
static uint8_t register_error;
static database_t *server_database;
static uint8_t server_write_bridge[4];
static int suppress_notify;
static uint8_t received_type,received[261],sent[300];
static uint32_t received_seq;
static uint16_t received_length,sent_length_total;
static address_t test_peer={{1,2,3,4,5,6}};
static void *current_instance(void) { instance_gets++; return &fake_instance; }
static uint8_t register_service(void *i,void **s,callbacks_t *cb) {
    assert(i==&fake_instance); assert(cb->added && cb->notified);
    if(register_error)return register_error;
    retained_service_output=s; *s=&fake_service; return 0;
}
static uint8_t unregister_service(void *s) {
    service_unregisters++;
    assert(s==&fake_service); *retained_service_output=0; return 0;
}
static uint8_t add_table(void *s,database_t *db) {
    table_adds++;
    assert(db->count==4 && db->attributes[3].properties==8);
    assert(db->attributes[0].uuid.v.u128[12]==0x11);
    assert(db->attributes[2].permissions==2 && db->attributes[2].properties==0);
    assert(db->attributes[2].uuid.v.u16==0x2902 && db->attributes[2].write);
    server_database=db;
    /* Exact .026 ADD_ATTR_TABLE at0x2c354626 tests serialized rsp byte==1.
     * Mode2 does NOT install the IPC bridge, unlike the newer public SDK. */
    for(int i=0;i<db->count;i++)server_write_bridge[i]=(uint8_t)db->attributes[i].response==1;
    callbacks.added(s,0,1); return 0;
}
static uint8_t remove_table(void *s,uint16_t h) {
    table_removes++; assert(s==&fake_service && h==1); return 0;
}
static uint8_t notify_value(void *s,address_t *a,uint16_t h,const uint8_t *v,uint16_t n) {
    assert(s==&fake_service && h==2 && n<=20 && memcmp(a,&test_peer,sizeof(test_peer))==0);
    memcpy(sent+sent_length_total,v,n); sent_length_total+=n; notifies++;
    if(!suppress_notify) callbacks.notified(s,a,0,h); return 0;
}
static void *start_advertising(void *i,adv_params_t *p,uint8_t *ad,uint16_t n,
                                uint8_t *scan,uint16_t sn,adv_callbacks_t *cb) {
    assert(i==&fake_instance && p->interval==160 && n==21 && sn==9);
    assert(ad[4]==7 && ad[17]==0x11 && !memcmp(scan+2,"MB-TAGS",7)); adv_starts++;
    cb->started(&fake_adv,7,0); return &fake_adv;
}
static void stop_advertising(void *i,void *a) {
    assert(i==&fake_instance && a==&fake_adv); adv_stops++;
    adv_callbacks.stopped(a,7);
}
static void received_cb(uint8_t t,uint32_t s,const uint8_t *p,uint16_t n) {
    delivered++; received_type=t; received_seq=s; received_length=n; memcpy(received,p,n);
}
static uint16_t firmware_att_write(address_t *address,uint16_t id,
                                    const uint8_t *value,uint16_t length) {
    assert(server_database);
    for(int i=0;i<server_database->count;i++) {
        attr_t *attr=&server_database->attributes[i];
        if(attr->handle!=id)continue;
        assert(attr->permissions&2u);
        if(server_write_bridge[i] && attr->write)
            (void)attr->write(service,address,id,value,length,0);
        /* Exact service ACKs ATT before dispatch; ACK alone proves no callback. */
        return length;
    }
    assert(0); return 0;
}
static void subscribe(void) {
    const uint8_t v[]={1,0};
    assert(firmware_att_write(&test_peer,3,v,2)==2);
    assert(transport_ready()); /* Fails with the old CCC response2 despite ATT ACK. */
}
static void feed(const uint8_t *p,unsigned n,unsigned chunk) {
    while(n) { unsigned k=n<chunk?n:chunk; assert(firmware_att_write(&test_peer,4,p,k)==k); p+=k;n-=k; }
}
int main(void) {
    assert(transport_start(received_cb)==0);transport_poll(100);subscribe();
    uint8_t frame[271]={0x4d,0x42,1,6,0,0,0,9,1,5};
    for(unsigned i=10;i<sizeof(frame);i++)frame[i]=(uint8_t)i;
    for(unsigned chunk=1;chunk<=20;chunk++) {
        unsigned before=delivered;feed(frame,sizeof(frame),chunk);transport_poll(100);
        assert(delivered==before+1&&received_type==6&&received_length==261&&!memcmp(received,frame+10,261));
    }
    frame[3]=7;frame[8]=0;frame[9]=57;
    for(unsigned chunk=1;chunk<=20;chunk++){unsigned before=delivered;feed(frame,67,chunk);transport_poll(100);assert(delivered==before+1&&received_type==7&&received_length==57);}
    frame[9]=56;feed(frame,10,10);transport_poll(100);assert(!transport_ready());subscribe();transport_poll(100);
    frame[9]=58;feed(frame,10,10);transport_poll(100);assert(!transport_ready());subscribe();transport_poll(100);
    frame[3]=6;frame[8]=1;frame[9]=6;feed(frame,10,10);transport_poll(100);assert(!transport_ready());subscribe();transport_poll(100);
    frame[8]=0;frame[9]=0;feed(frame,10,10);transport_poll(100);assert(!transport_ready());subscribe();transport_poll(100);
    uint8_t request[57]={1};assert(transport_notify(7,0x20000001u,request,57)==0);
    for(unsigned i=0;i<12;i++)transport_poll(100+i*10);
    assert(sent_length_total==67&&sent[3]==7&&sent[4]==0x20&&sent[9]==57);
    assert(transport_notify(7,2,request,58)==-3&&transport_notify(6,2,request,1)==-3);
    frame[3]=7;frame[9]=57;feed(frame,12,12);disconnected(service,&test_peer);subscribe();transport_poll(200);
    unsigned before=delivered;feed(frame,67,20);transport_poll(200);assert(delivered==before+1);
    transport_stop();puts("catalog transport: type6<=261/type7exact57 fragments, ATT send, malformed bounds and epochs: PASS");
}
