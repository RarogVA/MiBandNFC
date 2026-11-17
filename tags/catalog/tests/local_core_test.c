#include <assert.h>
#include <stdio.h>
#include <string.h>
#define APDU_LAB_HOST_TEST 1
#define TRANSPORT_TAG_PROFILE 1
#define TAG_CATALOG_LOCAL_PROFILE 1
#include "../core_local.c"
#include "sha256.h"
#include "mock_fs.h"

static uint32_t mock_time,mock_gen=1,mock_frame_gen=1;
static int mock_ready,mock_nfc_ready,mock_clean=1,mock_nfc_error,mock_clock_fail;
static unsigned stop_calls,start_calls,send_calls,provider_calls;
static uint8_t sent[258];static size_t sent_length;
static transport_frame_cb receiver;
static nfc_probe_apdu_cb apdu;

int catalog_provider_verified(void){return 1;}
int catalog_provider_install(const tag_catalog_api *a){
 assert(a&&a->magic==TAG_CATALOG_API_MAGIC&&a->select&&a->health&&a->start&&a->stop);
 provider_calls++;return 0;
}
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
int transport_notify(uint8_t type,uint32_t seq,const uint8_t *p,uint16_t n){
 (void)type;(void)seq;(void)p;(void)n;return 0;
}
int nfc_probe_start(nfc_probe_apdu_cb cb,void *ctx){assert(ctx==0);apdu=cb;start_calls++;mock_clean=0;return 0;}
int nfc_probe_stop(void){stop_calls++;mock_nfc_ready=0;mock_clean=1;return 0;}
void nfc_probe_poll(uint32_t now){(void)now;}
int nfc_probe_ready(void){return mock_nfc_ready;}
int nfc_probe_can_unload(void){return mock_clean;}
int nfc_probe_error(void){return mock_nfc_error;}
const char *nfc_probe_status(void){return mock_nfc_ready?"ready":mock_clean?"idle":"route work";}
const char *nfc_probe_info(void){return "local mock";}
int nfc_probe_send(const uint8_t *p,size_t n){
 assert(p&&n>=2&&n<=258);send_calls++;memcpy(sent,p,n);sent_length=n;return 0;
}
static void tick(void){mock_time+=10;test_timer(0);}
static void apdu_call(const uint8_t *p,size_t n){apdu(p,n,0);}
static void expect(const uint8_t *p,size_t n,const char *what){
 if(sent_length!=n||memcmp(sent,p,n)){fprintf(stderr,"%s: got len=%zu",what,sent_length);for(size_t i=0;i<sent_length;i++)fprintf(stderr," %02x",sent[i]);fprintf(stderr,"\n");}
 assert(sent_length==n);assert(!memcmp(sent,p,n));printf("  %s: PASS\n",what);
}
static void upload(void){
 uint8_t blob[100],hash[32],begin[42]={1},data[105]={2};
 size_t n=unhex(blob,golden_hex);unhex(hash,golden_sha);
 catalog_put_u32(begin+1,7);begin[5]=1;catalog_put_u32(begin+6,(uint32_t)n);memcpy(begin+10,hash,32);
 receiver(6,1,begin,42);tick();assert(!strcmp(reply.bytes,"CAT_BEGIN"));
 catalog_put_u32(data+1,0);memcpy(data+5,blob,n);receiver(6,2,data,(uint16_t)n+5);tick();
 assert(!strncmp(reply.bytes,"CAT_DATA:75",11));
 uint8_t commit=3;receiver(6,3,&commit,1);tick();
 assert(!strncmp(reply.bytes,"CAT_COMMIT:7",12));
}
int main(void){
 assert(!module_initialize()&&!provider_calls);tick();assert(provider_calls==1&&receiver);
 tag_catalog_health health;assert(!api.health(&health)&&health.loop_live&&health.count==0);
 upload();
 tag_catalog_entry entry;assert(!api.get(7,0,&entry)&&!strcmp(entry.name,"NFC-Test"));
 assert(!api.select(7,entry.id,entry.revision));tick();
 api.health(&health);assert(selected==0&&health.selected_index==0&&health.phase==TAG_CATALOG_SELECTED);
 /* No BLE at all: local start must not depend on transport_ready(). */
 mock_ready=0;assert(!api.start());tick();assert(start_calls==1&&local_active&&!mock_nfc_ready);
 mock_nfc_ready=1;tick();api.health(&health);assert(health.nfc_ready&&health.phase==TAG_CATALOG_READY);
 const uint8_t select_aid[]={0,0xa4,4,0,7,0xd2,0x76,0,0,0x85,1,1,0};
 const uint8_t sw_ok[]={0x90,0};
 apdu_call(select_aid,sizeof(select_aid));expect(sw_ok,2,"SELECT NDEF AID");
 const uint8_t select_cc[]={0,0xa4,0,0x0c,2,0xe1,3};
 apdu_call(select_cc,sizeof(select_cc));expect(sw_ok,2,"SELECT CC");
 const uint8_t read_cc[]={0,0xb0,0,0,2};
 const uint8_t cc_head[]={0,0x0f,0x90,0};
 apdu_call(read_cc,sizeof(read_cc));expect(cc_head,4,"READ CC");
 const uint8_t select_ndef[]={0,0xa4,0,0x0c,2,0xe1,4};
 apdu_call(select_ndef,sizeof(select_ndef));expect(sw_ok,2,"SELECT NDEF");
 const uint8_t read_nlen[]={0,0xb0,0,0,2};
 const uint8_t nlen[]={0,0x17,0x90,0};
 apdu_call(read_nlen,sizeof(read_nlen));expect(nlen,4,"READ NLEN");
 const uint8_t read_full[]={0,0xb0,0,0,0};
 apdu_call(read_full,sizeof(read_full));
 assert(sent_length==27&&sent[0]==0&&sent[1]==0x17); /* NLEN + 23 bytes, then 6282. */
 assert(!memcmp(sent+2,ndef_file+2,23)&&sent[25]==0x62&&sent[26]==0x82);
 printf("  READ full NDEF with 6282: PASS\n");
 const uint8_t read_exact[]={0,0xb0,0,2,0x17};
 apdu_call(read_exact,sizeof(read_exact));
 assert(sent_length==25&&!memcmp(sent,ndef_file+2,23)&&sent[23]==0x90&&sent[24]==0);
 printf("  READ exact NDEF: PASS\n");
 const uint8_t bad_aid[]={0,0xa4,4,0,7,0xd2,0x76,0,0,0x85,1,2,0};
 apdu_call(bad_aid,sizeof(bad_aid));expect((const uint8_t[]){0x6a,0x82},2,"bad AID");
 const uint8_t bad_ins[]={0,0xca,0,0,0};
 apdu_call(bad_ins,sizeof(bad_ins));expect((const uint8_t[]){0x6d,0},2,"bad INS");
 const uint8_t write_ins[]={0,0xd6,0,0,0};
 apdu_call(write_ins,sizeof(write_ins));expect((const uint8_t[]){0x69,0x86},2,"write refused");
 /* Maximum payload: 1024 NDEF bytes must be readable in 256-byte chunks. */
 ndef_length=1024;ndef_file[0]=4;ndef_file[1]=0;memset(ndef_file+2,0x41,1024);
 app_selected=1;file_selected=0xe104;mock_nfc_ready=1;
 const uint8_t read_256[]={0,0xb0,0,0,0};
 apdu_call(read_256,sizeof(read_256));
 assert(sent_length==258&&sent[0]==4&&sent[1]==0&&sent[2]==0x41&&sent[256]==0x90&&sent[257]==0);
 const uint8_t read_770[]={0,0xb0,3,2,0};
 apdu_call(read_770,sizeof(read_770));
 assert(sent_length==258&&sent[0]==0x41&&sent[255]==0x41&&sent[256]==0x90&&sent[257]==0);
 const uint8_t read_tail[]={0,0xb0,3,0xfe,0};
 apdu_call(read_tail,sizeof(read_tail));
 assert(sent_length==6&&sent[4]==0x62&&sent[5]==0x82);
 printf("1024-byte chunked READ BINARY: PASS\n");
 /* Session timeout stops NFC without a phone. */
 mock_time+=60000;tick();
 assert(!local_active&&stop_calls&&phase==TAG_CATALOG_SELECTED&&mock_clean);
 printf("session timeout cleanup: PASS\n");
 /* Selecting another record stops an active session first. */
 mock_nfc_ready=1;assert(!api.start());tick();assert(local_active);
 assert(!api.select(7,entry.id,entry.revision));tick();assert(!local_active&&stop_calls>=2);
 /* A catalog transaction is refused while NFC is active. */
 mock_nfc_ready=1;assert(!api.start());tick();
 uint8_t query=5;receiver(6,9,&query,1);tick();
 assert(!strncmp(reply.bytes,"ERROR:stop NFC",14));
 printf("catalog transaction refused during NFC: PASS\n");
 api.stop();tick();assert(!local_active);
 /* Clock failure must not leave a live session. */
 mock_nfc_ready=1;assert(!api.start());tick();
 mock_clock_fail=1;tick();assert(!local_active);
 puts("local NDEF core: phone-free start, Type 4 APDUs, 60 s timeout, selection stop and clean refusal: PASS");
 return 0;
}
