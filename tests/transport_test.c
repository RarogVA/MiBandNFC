#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#define TRANSPORT_HOST_TEST 1
/* POSIX headers on macOS also define uuid_t; isolate the firmware ABI name. */
#define uuid_t transport_test_uuid_t
#include "../band/transport.c"
#undef uuid_t
static int fake_instance,fake_service,fake_adv;
static void **retained_service_output;
static unsigned delivered,notifies,adv_starts,adv_stops;
static unsigned instance_gets,table_adds,table_removes,service_unregisters;
static uint8_t register_error;
static database_t *server_database;
static uint8_t server_write_bridge[4];
static int suppress_notify;
static uint8_t received_type,received[258],sent[300];
static uint32_t received_seq;
static uint16_t received_length,sent_length_total;
static address_t test_peer={{1,2,3,4,5,6}},other_peer={{6,5,4,3,2,1}};
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
    assert(db->attributes[0].uuid.v.u128[12]==1);
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
    assert(ad[4]==7 && ad[17]==1 && scan[2]=='M'); adv_starts++;
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
    /* A failed registration must not publish/create a service or advertiser,
     * or tear down the process-level singleton. Isolate globals in a child. */
    pid_t child=fork(); assert(child>=0);
    if(child==0) {
        fake_instance=0x1234; register_error=1;
        assert(transport_start(received_cb)==-61);
        transport_poll(100); transport_stop();
        assert(instance_gets==1 && instance==&fake_instance && fake_instance==0x1234);
        assert(!service && !registration_slot && !transport_ready());
        assert(transport_error()==-61 && transport_debug_state()==0);
        assert(!table_adds && !table_removes && !service_unregisters);
        assert(!adv_starts && !adv_stops && !notifies && !delivered);
        _exit(0);
    }
    int status=0; assert(waitpid(child,&status,0)==child);
    assert(WIFEXITED(status) && WEXITSTATUS(status)==0);
    assert(transport_start(received_cb)==0); transport_poll(100); assert(adv_starts==1);
    subscribe();
    /* A control arriving immediately after CCC must survive the first poll. */
    const uint8_t control[]={0x4d,0x42,1,4,0,0,0,7,0,1,0};
    feed(control,sizeof(control),1); transport_poll(100);
    assert(delivered==1 && received_type==4 && received_seq==7 && received_length==1);
    uint8_t apdu[261]; for(unsigned i=0;i<sizeof(apdu);i++)apdu[i]=(uint8_t)i;
    assert(transport_notify(1,0x12345678,apdu,261)==0);
    assert(transport_notify(1,9,apdu,261)==-2);
    for(unsigned i=0;i<20;i++)transport_poll(100);
    assert(sent_length_total==271 && notifies==14 && sent[4]==0x12 && sent[9]==5);
    assert(memcmp(sent+10,apdu,261)==0);
    uint8_t response[268]={0x4d,0x42,1,2,0x12,0x34,0x56,0x78,1,2};
    for(unsigned i=10;i<sizeof(response);i++)response[i]=(uint8_t)i;
    feed(response,sizeof(response),20); transport_poll(100);
    assert(delivered==2 && received_type==2 && received_length==258 && received_seq==0x12345678);
    assert(memcmp(received,response+10,258)==0);
    assert(rx_written(service,&other_peer,4,response,20,0)==0);
    assert(rx_written(service,&test_peer,4,response,20,1)==0);
    /* Partial old-session frame must not mix with the new session. */
    feed(response,12,12); uint32_t old=transport_generation();
    disconnected(service,&test_peer); assert(!transport_ready() && transport_generation()!=old);
    subscribe(); feed(control,sizeof(control),20); transport_poll(100);
    assert(delivered==3 && received_type==4 && received_seq==7);
    /* Callback thread changes the generation between bytes of one poll. */
    old=transport_generation();
    for(unsigned i=0;i<5;i++)receive_byte(response[i],old,100);
    assert(incoming_used==5);
    subscribe();
    uint32_t fresh=transport_generation();
    for(unsigned i=0;i<sizeof(control);i++)receive_byte(control[i],fresh,100);
    assert(delivered==4 && received_type==4 && received_seq==7);
    receive_byte(0xff,old,100); /* Late old-session byte cannot invalidate new peer. */
    assert(transport_ready() && incoming_used==0);
    /* Reject oversized input before copying into fixed reassembly storage. */
    uint8_t invalid[]={0x4d,0x42,1,2,0,0,0,1,1,3};
    feed(invalid,sizeof(invalid),20); transport_poll(100); assert(!transport_ready() && transport_error()==-6);
    subscribe(); transport_poll(100);
    suppress_notify=1; sent_length_total=0;
    assert(transport_notify(3,8,(const uint8_t *)"READY",5)==0);
    transport_poll(0xfffffff0u); assert(transport_ready());
    transport_poll(1483u); assert(transport_ready());
    transport_poll(1484u); assert(!transport_ready() && transport_error()==-8);
    subscribe(); transport_poll(1600);
    feed(response,5,5); transport_poll(1600);
    transport_poll(3100); assert(!transport_ready() && transport_error()==-9);
    transport_stop(); assert(adv_stops==1); assert(transport_start(received_cb)==-13);
    assert(instance==&fake_instance); /* Singleton is retained after cleanup. */
    puts("PASS: exact CCC IPC mode1 dispatch, register failure preserves singleton, control race, 261/258-byte framing, ATT23 fragments, peer filter, disconnect epochs, invalid lengths, owned cleanup");
    return 0;
}
