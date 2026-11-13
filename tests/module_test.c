#include <assert.h>
#include <stdio.h>
#include <string.h>
#define APDU_LAB_HOST_TEST 1
#include "../band/module.c"

static uint32_t mock_time, mock_gen=1, mock_frame_gen=1;
static int mock_clock_fail, mock_transport_ready=1, mock_nfc_ready, mock_clean=1;
static int mock_nfc_error, mock_stop_calls, mock_nfc_polls, mock_send_calls, mock_busy;
static int inject_epoch_response;
static unsigned diagnostic_calls;
static unsigned transport_start_calls;
static unsigned info_calls;
static transport_frame_cb test_receiver;
static uint8_t last_type,last_payload[261];static uint32_t last_seq;static uint16_t last_len;
static nfc_probe_apdu_cb test_apdu;
int native_clock_ms(uint32_t *p){if(mock_clock_fail)return -1;*p=mock_time;return 0;}
int transport_start(transport_frame_cb cb){transport_start_calls++;test_receiver=cb;return 0;}
int transport_ready(void){return mock_transport_ready;}
int transport_error(void){return 0;}
uint32_t transport_debug_state(void){return mock_transport_ready ? 0x110011u : 0u;}
void diagnostic_set(const char *stage,int code,uint32_t detail){(void)stage;(void)code;(void)detail;diagnostic_calls++;}
uint32_t transport_generation(void){return mock_gen;}
uint32_t transport_frame_generation(void){return mock_frame_gen;}
void transport_stop(void){}
void transport_poll(uint32_t now) {
    (void)now;
    if(inject_epoch_response) {
        inject_epoch_response=0;mock_gen++;mock_frame_gen=mock_gen;
        const uint8_t rapdu[]={0x90,0};
        test_receiver(2,request_sequence,rapdu,2);
    }
}
int transport_notify(uint8_t t,uint32_t s,const uint8_t *p,uint16_t n){
    if(mock_busy)return -2;
    last_type=t;last_seq=s;last_len=n;memcpy(last_payload,p,n);return 0;
}
int nfc_probe_start(nfc_probe_apdu_cb cb,void *context){(void)context;test_apdu=cb;mock_clean=0;return 0;}
int nfc_probe_stop(void){mock_stop_calls++;mock_nfc_ready=0;return 0;}
void nfc_probe_poll(uint32_t now){(void)now;mock_nfc_polls++;}
int nfc_probe_ready(void){return mock_nfc_ready;}
int nfc_probe_can_unload(void){return mock_clean;}
int nfc_probe_error(void){return mock_nfc_error;}
const char *nfc_probe_status(void){return mock_nfc_ready?"ready":mock_clean?"idle":"route work";}
const char *nfc_probe_info(void){info_calls++;return "v=20 chip=00 features=001E0000 lmrt=0800 ctrl=FF";}
int nfc_probe_send(const uint8_t *p,size_t n){(void)p;(void)n;mock_send_calls++;return 0;}

static void reset(void) {
    now_ms=0;peer_generation=1;sequence=0x10000000u;operation_sequence=0;started_at=0;
    booted=0;desired=0;last_ready=last_clean=0;pending=request_sent=0;
    operation=stopped_by_fault=clock_failed=0;request_sequence=request_at=request_length=0;
    initialization_attempted=0;initialization_result=-1;initialization_complete=0;
    diagnostic_seen=0;diagnostic_calls=0;transport_start_calls=0;info_calls=0;
    memset(&operation_status,0,sizeof(operation_status));memset(&reply_status,0,sizeof(reply_status));
    memset(last_status,0,sizeof(last_status));mock_time=0;mock_gen=mock_frame_gen=1;
    mock_clock_fail=mock_nfc_ready=mock_nfc_error=mock_stop_calls=mock_nfc_polls=mock_send_calls=mock_busy=0;
    mock_transport_ready=mock_clean=1;inject_epoch_response=0;last_type=0;last_len=0;
    assert(module_initialize()==0 && transport_start_calls==0);
    test_timer(0);assert(test_receiver && transport_start_calls==1);
    unsigned calls=diagnostic_calls;
    assert(module_initialize()==0 && diagnostic_calls==calls);
}
static void control(uint32_t seq,uint8_t command){test_receiver(4,seq,&command,1);}
static void ready(void){control(7,1);mock_nfc_ready=1;test_timer(0);assert(last_type==3 && last_seq==7 && last_len==5 && !memcmp(last_payload,"READY",5));}
int main(void) {
    const uint8_t sel[]={0,0xa4,4,0,8,0xf0,0x4d,0x42,0x41,0x50,0x44,0x55,1,0};
    const uint8_t ok[]={0x90,0};
    reset();control(31,3);test_timer(0);
    assert(last_type==3 && last_seq==31 && last_len>5 && !memcmp(last_payload,"INFO:",5));
    assert(info_calls==1 && !desired && mock_clean && mock_stop_calls==0 && !pending);
    ready();control(32,3);test_timer(0);
    assert(last_seq==32 && last_len>6 && !memcmp(last_payload,"ERROR:",6));
    assert(info_calls==1 && desired && mock_stop_calls==0);
    reset();ready();test_apdu(sel,sizeof(sel),0);test_timer(0);
    assert(last_type==1 && last_len==sizeof(sel));uint32_t seq=last_seq;
    test_receiver(2,seq-1,ok,2);assert(mock_send_calls==0);
    test_receiver(2,seq,ok,2);assert(mock_send_calls==1 && !pending);
    test_receiver(2,seq,ok,2);assert(mock_send_calls==1);
    /* Reconnect during transport_poll must cancel a previously pending APDU. */
    test_apdu(sel,sizeof(sel),0);test_timer(0);inject_epoch_response=1;test_timer(0);
    assert(mock_send_calls==1 && !desired && !pending && mock_stop_calls>0);
    reset();ready();test_apdu(sel,sizeof(sel),0);test_timer(0);
    mock_frame_gen=mock_gen-1;test_receiver(2,request_sequence,ok,2);assert(mock_send_calls==0);
    /* Queries cannot overwrite the READY for the operation's original seq. */
    reset();control(42,1);mock_nfc_ready=1;mock_busy=1;test_timer(0);
    control(43,0);mock_busy=0;test_timer(0);assert(last_seq==42 && last_len==5 && !memcmp(last_payload,"READY",5));
    /* Monotonic failure disables START and keeps pumping async restoration. */
    reset();ready();mock_clock_fail=1;int polls=mock_nfc_polls;test_timer(0);test_timer(0);
    assert(clock_failed && !desired && mock_nfc_polls>=polls+2 && mock_stop_calls>0);
    mock_clean=1;control(70,1);assert(!desired && reply_status.queued);
    reset();ready();test_apdu(sel,sizeof(sel),0);test_timer(0);mock_time=3000;test_timer(0);
    assert(!desired && !pending && mock_send_calls==0);
    reset();ready();mock_time=120000;test_timer(0);assert(!desired);
    puts("module: complete SELECT exchange, stale/duplicate responses, epoch race, status priority, clock failure, deadlines PASS");
}
