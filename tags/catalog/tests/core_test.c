#include <stdio.h>
#include <assert.h>
#include <string.h>
#define APDU_LAB_HOST_TEST 1
#include "../core.c"
#include "sha256.h"
#include "mock_fs.h"
static uint32_t mock_time,mock_gen=1,mock_frame_gen=1;
static int mock_ready=1,mock_nfc_ready,mock_clean=1,mock_nfc_error,mock_clock_fail;
static unsigned stop_calls,start_calls,send_calls,provider_calls;
static uint8_t last_type,last_payload[262];static uint16_t last_length;static uint32_t last_seq;
static uint8_t last_selection[57];static uint32_t last_selection_seq;
static transport_frame_cb receiver;
static nfc_probe_apdu_cb apdu;
int catalog_provider_verified(void){return 1;}
int catalog_provider_install(const tag_catalog_api *a){assert(a&&a->magic==TAG_CATALOG_API_MAGIC&&a->select&&a->health);provider_calls++;return 0;}
int native_clock_ms(uint32_t *t){if(mock_clock_fail)return-1;*t=mock_time;return 0;}
void diagnostic_set(const char *s,int rc,uint32_t detail){(void)s;(void)rc;(void)detail;}
int transport_start(transport_frame_cb cb){receiver=cb;return 0;}
int transport_ready(void){return mock_ready;}
int transport_error(void){return 0;}
uint32_t transport_generation(void){return mock_gen;}
uint32_t transport_frame_generation(void){return mock_frame_gen;}
uint32_t transport_debug_state(void){return 0x111111;}
void transport_stop(void){}
void transport_poll(uint32_t now){(void)now;}
int transport_notify(uint8_t type,uint32_t seq,const uint8_t *p,uint16_t n){last_type=type;last_seq=seq;last_length=n;memcpy(last_payload,p,n);last_payload[n]=0;if(type==7){assert(n==57);memcpy(last_selection,p,57);last_selection_seq=seq;}return 0;}
int nfc_probe_start(nfc_probe_apdu_cb cb,void *ctx){(void)ctx;apdu=cb;start_calls++;mock_clean=0;return 0;}
int nfc_probe_stop(void){stop_calls++;mock_nfc_ready=0;return 0;}
void nfc_probe_poll(uint32_t now){(void)now;}
int nfc_probe_ready(void){return mock_nfc_ready;}
int nfc_probe_can_unload(void){return mock_clean;}
int nfc_probe_error(void){return mock_nfc_error;}
const char *nfc_probe_status(void){return mock_nfc_ready?"ready":mock_clean?"idle":"route work";}
const char *nfc_probe_info(void){return "catalog mock";}
int nfc_probe_send(const uint8_t *p,size_t n){assert(n>=2&&p);send_calls++;return 0;}
static void tick(void){mock_time+=10;test_timer(0);}
static void control(uint8_t op,uint32_t seq){receiver(4,seq,&op,1);tick();}
static void handshake(void){
 for(unsigned i=0;i<4&&!selection_sent;i++)tick();assert(selection_sent&&last_selection_seq==select_sequence);
 uint8_t ack[57];memcpy(ack,last_selection,57);ack[0]=2;receiver(7,last_selection_seq,ack,57);tick();
 for(unsigned i=0;i<4&&(last_type!=3||last_seq!=select_sequence||strcmp((char *)last_payload,"SELECT_OK"));i++)tick();
 if(!authorized)fprintf(stderr,"handshake error: phase=%d selected=%d clean=%d seq=%u status=%s\n",phase,selected,clean(),select_sequence,last_payload);
 assert(authorized&&phase==TAG_CATALOG_SELECTED&&last_type==3&&!strcmp((char *)last_payload,"SELECT_OK"));
 receiver(7,select_sequence,ack,57);tick();assert(authorized&&!strcmp((char *)last_payload,"SELECT_OK"));
}
int main(void){
 assert(!module_initialize()&&!provider_calls);tick();assert(provider_calls==1&&receiver);assert(!module_initialize());
 tag_catalog_health health;assert(!api.health(&health)&&health.loop_live&&health.count==0);uint32_t h=health.heartbeat;mock_time+=1100;api.health(&health);assert(!health.loop_live&&health.heartbeat==h);tick();api.health(&health);assert(health.loop_live);
 control(4,1);assert(!strcmp((char *)last_payload,"CATALOG_V1"));control(1,2);assert(!start_calls);
 uint8_t blob[100],hash[32],begin[42]={1},data[105]={2};size_t n=unhex(blob,golden_hex);unhex(hash,golden_sha);catalog_put_u32(begin+1,7);begin[5]=1;catalog_put_u32(begin+6,(uint32_t)n);memcpy(begin+10,hash,32);
 receiver(6,3,begin,42);tick();assert(!strcmp((char *)last_payload,"CAT_BEGIN"));catalog_put_u32(data+1,0);memcpy(data+5,blob,n);receiver(6,4,data,(uint16_t)n+5);tick();assert(!strcmp((char *)last_payload,"CAT_DATA:75"));uint8_t commit=3;receiver(6,5,&commit,1);tick();assert(!strcmp((char *)last_payload,"CAT_COMMIT:7"));
 tag_catalog_entry entry;assert(!api.get(7,0,&entry)&&!strcmp(entry.name,"NFC-Test"));assert(api.get(6,0,&entry)<0);
 /* Offline selection persists without pretending Pixel has acknowledged it. */
 mock_ready=0;mock_gen++;tick();assert(!api.select(7,entry.id,9));tick();assert(selected==0&&!authorized&&catalog_storage_selected()==0);api.health(&health);assert(health.count==1&&health.selected_index==0&&!health.ble_ready);
 mock_ready=1;mock_gen++;mock_frame_gen=mock_gen;tick();handshake();assert(!start_calls&&api.start()<0);
 /* Selection tuple must match in full, including session and digest. */
 uint8_t proposal[57];memcpy(proposal,selection_tuple,57);proposal[0]=3;proposal[56]^=1;receiver(7,9,proposal,57);tick();assert(strstr((char *)last_payload,"ERROR:"));proposal[56]^=1;
 control(1,10);assert(start_calls==1&&desired);mock_nfc_ready=1;tick();assert(phase==TAG_CATALOG_READY);
 const uint8_t select[]={0,0xa4,4,0,7,0xd2,0x76,0,0,0x85,1,1,0},ok[]={0x90,0};apdu(select,sizeof(select),0);tick();assert(last_type==1);uint32_t capdu=last_seq;receiver(2,capdu,ok,2);assert(send_calls==1&&!pending);
 /* Changing selection cannot send REQUEST or authorize START until cleanup. */
 receiver(7,11,proposal,57);tick();assert(!desired&&!authorized&&wanted==0&&!selection_sent&&!mock_clean);
 unsigned starts=start_calls;control(1,12);assert(start_calls==starts);mock_clean=1;tick();handshake();assert(start_calls==starts);
 /* New epoch invalidates old ACK and requests the same persisted band choice. */
 uint8_t old[57];memcpy(old,selection_tuple,57);old[0]=2;uint32_t oldseq=select_sequence;mock_gen++;mock_frame_gen=mock_gen;tick();receiver(7,oldseq,old,57);tick();assert(!authorized);handshake();
 /* Thirty-second handshake timeout is distinct from the APDU deadline. */
 api.stop();tick();for(unsigned i=0;i<4&&!selection_sent;i++)tick();assert(selection_sent);mock_time+=30000;tick();assert(!authorized&&!selection_sent&&phase==TAG_CATALOG_ERROR);
 uint8_t query=5;receiver(6,20,&query,1);tick();handshake();
 control(1,21);mock_nfc_ready=1;tick();apdu(select,sizeof(select),0);tick();mock_time+=3000;tick();assert(!desired&&!pending&&stop_calls);mock_clean=1;tick();
 /* Re-authorize after explicit local selection, then enforce session limit. */
 assert(!api.select(7,entry.id,9));tick();handshake();control(1,22);mock_nfc_ready=1;tick();mock_time+=120000;tick();assert(!desired&&phase==TAG_CATALOG_ERROR);
 mock_clean=1;mock_nfc_error=1;api.stop();tick();api.health(&health);assert(phase==TAG_CATALOG_ERROR&&health.error&&!health.authorized);
 mock_nfc_error=0;assert(!api.select(7,entry.id,9));tick();handshake();control(1,23);mock_nfc_ready=1;tick();apdu_sequence=0x1fffffffu;apdu(select,sizeof(select),0);tick();assert(request_sequence==0x10000001u&&last_type==1);
 mock_clock_fail=1;tick();tick();api.health(&health);assert(!desired&&!pending&&!health.loop_live&&clock_failed);
 /* MiHome: a Pixel query consumes the band action file; status is written back. */
 mock_clock_fail=0;uint8_t mh_get=1;receiver(8,30,&mh_get,1);tick();
 assert(last_type==8&&last_seq==30&&last_length==2&&last_payload[0]==1&&last_payload[1]==0xff);
 mock_files[4].exists=1;mock_files[4].length=2;memcpy(mock_files[4].bytes,"3\n",2);
 receiver(8,31,&mh_get,1);tick();assert(last_type==8&&last_payload[0]==1&&last_payload[1]==3);
 receiver(8,32,&mh_get,1);tick();assert(last_payload[1]==0xff);
 uint8_t mh_status[2]={2,0};receiver(8,33,mh_status,2);tick();
 assert(last_type==8&&last_payload[0]==2&&!last_payload[1]);
 assert(mock_files[5].exists&&mock_files[5].length==3&&!memcmp(mock_files[5].bytes,"OK\n",3));
 puts("catalog core: capability, transactional sync, offline API, STOP-before-select, exact handshake, epochs, health, NFC deadlines and MiHome mailbox: PASS");return 0;
}
