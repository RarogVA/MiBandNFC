#include <assert.h>
#include <stdio.h>
#include <string.h>
#define APDU_LAB_HOST_TEST 1
#include "../band/module.c"

static uint32_t mock_time,mock_gen=1,mock_frame_gen=1;
static int mock_clock_fail,mock_transport_ready=1,mock_nfc_ready,mock_clean=1;
static int mock_nfc_error,mock_stop_calls,mock_nfc_polls,mock_send_calls,mock_busy;
static int inject_epoch_response,display_fail;
static unsigned diagnostic_calls,transport_start_calls,info_calls,display_calls;
static transport_frame_cb test_receiver;
static uint8_t last_type,last_payload[261];static uint32_t last_seq;static uint16_t last_len;
static nfc_probe_apdu_cb test_apdu;
int native_clock_ms(uint32_t *p){if(mock_clock_fail)return -1;*p=mock_time;return 0;}
int transport_start(transport_frame_cb cb){transport_start_calls++;test_receiver=cb;return 0;}
int transport_ready(void){return mock_transport_ready;}
int transport_error(void){return 0;}
uint32_t transport_debug_state(void){return mock_transport_ready?0x110011u:0u;}
void diagnostic_set(const char *s,int c,uint32_t d){(void)s;(void)c;(void)d;diagnostic_calls++;}
int tag_display_write(const tag_record *r,const char *s){(void)r;(void)s;display_calls++;return display_fail?-1:0;}
uint32_t transport_generation(void){return mock_gen;}
uint32_t transport_frame_generation(void){return mock_frame_gen;}
void transport_stop(void){}
void transport_poll(uint32_t now){
    (void)now;
    if(inject_epoch_response){inject_epoch_response=0;mock_gen++;mock_frame_gen=mock_gen;
        const uint8_t ok[]={0x90,0};test_receiver(2,request_sequence,ok,2);}
}
int transport_notify(uint8_t t,uint32_t s,const uint8_t *p,uint16_t n){
    if(mock_busy)return -2;last_type=t;last_seq=s;last_len=n;memcpy(last_payload,p,n);return 0;
}
int nfc_probe_start(nfc_probe_apdu_cb cb,void *c){(void)c;test_apdu=cb;mock_clean=0;return 0;}
int nfc_probe_stop(void){mock_stop_calls++;mock_nfc_ready=0;return 0;}
void nfc_probe_poll(uint32_t n){(void)n;mock_nfc_polls++;}
int nfc_probe_ready(void){return mock_nfc_ready;}
int nfc_probe_can_unload(void){return mock_clean;}
int nfc_probe_error(void){return mock_nfc_error;}
const char *nfc_probe_status(void){return mock_nfc_ready?"ready":mock_clean?"idle":"route work";}
const char *nfc_probe_info(void){info_calls++;return "cached v=20 aid=1";}
int nfc_probe_send(const uint8_t *p,size_t n){(void)p;(void)n;mock_send_calls++;return 0;}
static void reset(void){
    now_ms=0;peer_generation=1;sequence=0x10000000u;operation_sequence=0;started_at=0;
    booted=0;desired=0;last_ready=last_clean=0;pending=request_sent=0;
    operation=stopped_by_fault=clock_failed=0;request_sequence=request_at=request_length=0;
    initialization_attempted=0;initialization_result=-1;initialization_complete=0;
    diagnostic_seen=0;diagnostic_calls=transport_start_calls=info_calls=display_calls=0;
    selection_valid=0;last_display_state=-1;memset(&selected_tag,0,sizeof(selected_tag));
    memset(&operation_status,0,sizeof(operation_status));memset(&reply_status,0,sizeof(reply_status));
    memset(last_status,0,sizeof(last_status));mock_time=0;mock_gen=mock_frame_gen=1;
    mock_clock_fail=mock_nfc_ready=mock_nfc_error=mock_stop_calls=mock_nfc_polls=mock_send_calls=mock_busy=0;
    mock_transport_ready=mock_clean=1;inject_epoch_response=display_fail=0;last_type=0;last_len=0;
    assert(module_initialize()==0 && transport_start_calls==0);
    test_timer(0);assert(test_receiver && transport_start_calls==1);
}
static void control(uint32_t seq,uint8_t op){test_receiver(4,seq,&op,1);}
static void sync_tag(int supported){
    uint8_t p[]={1,4,4,4,0,'T','e','s','t','A','B','1','2','N','D','E','F'};p[4]=(uint8_t)supported;
    test_receiver(5,10,p,sizeof(p));test_timer(0);
    assert(last_type==3 && last_seq==10 && last_len==6 && !memcmp(last_payload,"TAG_OK",6));
}
static void ready(void){sync_tag(1);control(7,1);mock_nfc_ready=1;test_timer(0);
    assert(last_type==3 && last_seq==7 && last_len==5 && !memcmp(last_payload,"READY",5));}
int main(void){
    const uint8_t select[]={0,0xa4,4,0,7,0xd2,0x76,0,0,0x85,1,1,0},ok[]={0x90,0};
    reset();control(1,1);test_timer(0);assert(!desired && !selection_valid && !mock_stop_calls);
    reset();sync_tag(0);control(2,1);test_timer(0);assert(!desired);
    reset();sync_tag(1);display_fail=1;
    uint8_t failed_update[]={1,1,0,1,1,'X','T'};test_receiver(5,11,failed_update,sizeof(failed_update));test_timer(0);
    assert(!selection_valid && !strcmp(selected_tag.name,"Test") && last_seq==11 &&
        last_len==29 && !memcmp(last_payload,"ERROR:cannot save tag display",29));
    control(12,1);test_timer(0);assert(!desired);
    reset();ready();test_apdu(select,sizeof(select),0);test_timer(0);
    assert(last_type==1 && last_len==sizeof(select));uint32_t seq=last_seq;
    test_receiver(2,seq-1,ok,2);assert(!mock_send_calls);
    test_receiver(2,seq,ok,2);assert(mock_send_calls==1 && !pending);
    test_receiver(2,seq,ok,2);assert(mock_send_calls==1);
    uint8_t update[]={1,1,0,1,1,'X','T'};test_receiver(5,12,update,sizeof(update));
    assert(!strcmp(selected_tag.name,"Test") && reply_status.queued);
    test_apdu(select,sizeof(select),0);test_timer(0);inject_epoch_response=1;test_timer(0);
    assert(!selection_valid && !desired && !pending && mock_stop_calls && mock_send_calls==1);
    mock_clean=1;control(13,1);assert(!desired);
    reset();ready();const uint8_t write[]={0,0xd6,0,0,1,0};test_apdu(write,sizeof(write),0);
    assert(!pending && mock_send_calls==1);
    reset();ready();test_apdu(select,sizeof(select),0);test_timer(0);mock_time=3000;test_timer(0);
    assert(!desired && !pending && mock_stop_calls && !mock_send_calls);
    reset();ready();mock_time=120000;test_timer(0);assert(!desired && mock_stop_calls);
    reset();ready();mock_clock_fail=1;int polls=mock_nfc_polls;test_timer(0);test_timer(0);
    assert(clock_failed && !desired && mock_nfc_polls>=polls+2 && mock_stop_calls);
    reset();sync_tag(1);unsigned displays=display_calls;test_timer(0);test_timer(0);
    assert(display_calls==displays);
    puts("tags module: sync-before-start, read-only NDEF relay, epoch/timeout/cleanup and display dedup PASS");
}
