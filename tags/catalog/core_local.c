/* Local NDEF Type 4 profile: the band answers the reader itself.
 * BLE is used only to synchronize the catalog; the NFC APDU path never waits
 * for a phone.  Exact p67glt / 3.201.026 only. */
#include <stddef.h>
#include <stdint.h>
#include "transport.h"
#include "nfc_probe.h"
#include "native_clock.h"
#include "diagnostic.h"
#include "storage.h"
#include "provider.h"
typedef void (*timer_callback)(void *);
#ifndef APDU_LAB_HOST_TEST
#define fw_timer_create ((void *(*)(timer_callback,uint32_t,void *))(uintptr_t)0x2c558209u)
#define fw_log ((int (*)(int,const char *,...))(uintptr_t)0x2c1beab9u)
typedef struct {uintptr_t address;uint8_t bytes[24];} module_fingerprint;
#include "../band/module_fingerprints.h"
static int module_matches(void){
 for(unsigned i=0;i<sizeof(module_fingerprints)/sizeof(module_fingerprints[0]);i++){
  const volatile uint8_t *p=(const volatile uint8_t *)module_fingerprints[i].address;
  for(unsigned j=0;j<24;j++)if(p[j]!=module_fingerprints[i].bytes[j])return 0;
 }
 return 1;
}
#else
#include "module_test_backend.h"
#endif
#define POLL_MS 10u
#define SESSION_MS 60000u
#define LOCAL_MAX_NDEF 1024u
#define LOCAL_CC_LEN 15u
static const uint8_t local_aid[7]={0xd2,0x76,0,0,0x85,1,1};
static const uint8_t local_cc[LOCAL_CC_LEN]={0x00,0x0f,0x20,0x01,0x00,0x00,0xff,0x04,
    0x06,0xe1,0x04,0x04,0x02,0x00,0xff};
static uint32_t now_ms,last_poll_ms,heartbeat,initialization_complete;
static int initialized,initialization_result=-1,booted,clock_failed;
static int selected=-1,wanted=-1,stop_requested,start_pending,local_active,error_code,phase;
static uint32_t started_at;
static uint8_t ndef_file[LOCAL_MAX_NDEF+2];
static uint16_t ndef_length;
static uint16_t file_selected;
static int app_selected;
static uint8_t response[258];
static tag_catalog_api api;
typedef struct {char bytes[128];uint16_t length;uint8_t queued;uint32_t sequence;} message;
static message reply;

static void copy(void *a,const void *b,size_t n){uint8_t *d=a;const uint8_t *s=b;while(n--)*d++=*s++;}
static void append(char *p,size_t *n,const char *s){while(*s&&*n<127)p[(*n)++]=*s++;p[*n]=0;}
static void number(char *p,size_t *n,uint32_t v){char d[10];unsigned k=0;do{d[k++]=(char)('0'+v%10);v/=10;}while(v);while(k&&*n<127)p[(*n)++]=d[--k];p[*n]=0;}
static void status(uint32_t seq,const char *s){size_t n=0;append(reply.bytes,&n,s);reply.length=(uint16_t)n;reply.sequence=seq;reply.queued=1;}
static void status_number(uint32_t seq,const char *s,uint32_t v){status(seq,s);size_t n=reply.length;number(reply.bytes,&n,v);reply.length=(uint16_t)n;}
static int clock_sample(void){uint32_t t;if(native_clock_ms(&t)){clock_failed=1;return 0;}now_ms=t;return 1;}
static int clean(void){return !local_active&&!start_pending&&nfc_probe_can_unload()&&!nfc_probe_error();}
static int can_start(void){return !local_active&&nfc_probe_can_unload()&&!nfc_probe_error();}

static void local_fault(const char *why){
 local_active=0;start_pending=0;phase=TAG_CATALOG_ERROR;error_code=-22;
 (void)nfc_probe_stop();diagnostic_set("LOCAL_NFC_FAULT",-1,0);
 fw_log(4,"[TAG_CATALOG_LOCAL] %s",why);
}
static void send_status(uint16_t sw){
 response[0]=(uint8_t)(sw>>8);response[1]=(uint8_t)sw;
 if(nfc_probe_send(response,2))local_fault("NFC status send failed");
}
static void send_data(const uint8_t *src,unsigned count,uint16_t sw){
 for(unsigned i=0;i<count;i++)response[i]=src[i];
 response[count]=(uint8_t)(sw>>8);response[count+1]=(uint8_t)sw;
 if(nfc_probe_send(response,count+2))local_fault("NFC data send failed");
}
/* Read-only NFC Forum Type 4 NDEF application.  No UID, keys or payment AIDs. */
static void local_apdu(const uint8_t *p,size_t n,void *context){
 (void)context;
 if(!local_active||!p||n<4){send_status(0x6700);return;}
 if(p[0]!=0){send_status(0x6e00);return;}
 unsigned ins=p[1],p1=p[2],p2=p[3];
 if(ins==0xa4){
  if(n<5){send_status(0x6700);return;}
  unsigned lc=p[4];
  if(n!=5+lc&&n!=6+lc){send_status(0x6700);return;}
  if(n==6+lc&&p[5+lc]!=0){send_status(0x6a86);return;}
  if(p1==4){
   if(p2!=0&&p2!=0x0c){send_status(0x6a86);return;}
   if(lc!=7){send_status(0x6a82);return;}
   for(unsigned i=0;i<7;i++)if(p[5+i]!=local_aid[i]){send_status(0x6a82);return;}
   app_selected=1;file_selected=0;send_status(0x9000);return;
  }
  if(p1==0){
   if(p2!=0&&p2!=0x0c){send_status(0x6a86);return;}
   if(!app_selected){send_status(0x6985);return;}
   if(lc!=2){send_status(0x6700);return;}
   unsigned id=(unsigned)p[5]<<8|p[6];
   if(id!=0xe103&&id!=0xe104){send_status(0x6a82);return;}
   file_selected=(uint16_t)id;send_status(0x9000);return;
  }
  send_status(0x6a86);return;
 }
 if(ins==0xb0){
  if(n!=5){send_status(0x6700);return;}
  if(p1&0x80){send_status(0x6a86);return;}
  if(!app_selected||!file_selected){send_status(0x6985);return;}
  unsigned offset=((unsigned)p1<<8)|p2,le=p[4]?p[4]:256u;
  const uint8_t *src;unsigned length;
  if(file_selected==0xe103){src=local_cc;length=LOCAL_CC_LEN;}
  else {src=ndef_file;length=(unsigned)ndef_length+2u;}
  if(offset>length){send_status(0x6b00);return;}
  unsigned count=length-offset;if(count>le)count=le;
  send_data(src+offset,count,count<le?0x6282:0x9000);return;
 }
 if(ins==0xd6||ins==0xd0){send_status(0x6986);return;}
 send_status(0x6d00);
}
static int load_ndef(void){
 uint16_t length=0;
 if(catalog_storage_ndef(selected,ndef_file+2,LOCAL_MAX_NDEF,&length))return -1;
 ndef_file[0]=(uint8_t)(length>>8);ndef_file[1]=(uint8_t)length;ndef_length=length;return 0;
}
static int start_local(void){
 if(local_active)return 0;
 if(!can_start()||selected<0||!catalog_current.entries[selected].kind)return -1;
 if(catalog_storage_active())return -1;
 if(load_ndef()){error_code=-23;phase=TAG_CATALOG_ERROR;return -2;}
 app_selected=0;file_selected=0;
 if(nfc_probe_start(local_apdu,0)){
  error_code=-24;phase=TAG_CATALOG_ERROR;return -3;
 }
 local_active=1;started_at=now_ms;phase=TAG_CATALOG_STARTING;error_code=0;
 return 0;
}
static int api_health(tag_catalog_health *out){
 if(!out)return -1;
 uint32_t age=UINT32_MAX;
 if(heartbeat)age=now_ms-last_poll_ms;
 *out=(tag_catalog_health){.heartbeat=heartbeat,.poll_age_ms=age,
  .catalog_revision=catalog_current.revision,.peer_generation=transport_generation(),
  .count=catalog_current.count,.selected_index=(int16_t)selected,.phase=(uint8_t)phase,
  .ble_ready=(uint8_t)(booted&&transport_ready()),.nfc_ready=(uint8_t)nfc_probe_ready(),
  .clean=(uint8_t)nfc_probe_can_unload(),
  .authorized=(uint8_t)(selected>=0&&catalog_current.entries[selected].kind),
  .transaction_active=(uint8_t)catalog_storage_active(),
  .loop_live=(uint8_t)(age<1000&&!clock_failed),
  .error=error_code?error_code:nfc_probe_error()?-22:0};
 return 0;
}
static int api_get(uint32_t revision,uint16_t index,tag_catalog_entry *out){
 if(!out||revision!=catalog_current.revision||index>=catalog_current.count)return -1;
 *out=catalog_current.entries[index];return 0;
}
static int api_select(uint32_t revision,const uint8_t id[16],uint32_t record_revision){
 if(catalog_storage_active())return -2;
 int at=catalog_find(&catalog_current,id);
 if(revision!=catalog_current.revision||at<0||catalog_current.entries[at].revision!=record_revision)return -1;
 wanted=at;stop_requested=1;start_pending=0;return 0;
}
static int api_start(void){
 if(local_active)return 0;
 if(selected<0||!catalog_current.entries[selected].kind)return -1;
 if(nfc_probe_error())return -2;
 start_pending=1;return 0;
}
static int api_stop(void){start_pending=0;stop_requested=1;return 0;}

static void selection_poll(void){
 if(stop_requested){
  stop_requested=0;start_pending=0;
  if(local_active||!nfc_probe_can_unload()){
   local_active=0;phase=TAG_CATALOG_STOPPING;
   int rc=nfc_probe_stop();
   if(rc){phase=TAG_CATALOG_ERROR;error_code=rc;}
  }
 }
 if(wanted>=0&&clean()&&!catalog_storage_active()){
  int next=wanted;wanted=-1;
  if(catalog_storage_select(next)){phase=TAG_CATALOG_ERROR;error_code=-20;}
  else {selected=next;phase=TAG_CATALOG_SELECTED;}
 }
 if(!local_active&&!start_pending&&!nfc_probe_error()&&nfc_probe_can_unload()&&
    phase==TAG_CATALOG_STOPPING)phase=selected>=0?TAG_CATALOG_SELECTED:TAG_CATALOG_IDLE;
}
static int tuple_index(const uint8_t *p){
 if(catalog_u32(p+1)!=catalog_current.revision)return -1;
 int at=catalog_find(&catalog_current,p+5);if(at<0)return -1;
 const tag_catalog_entry *e=&catalog_current.entries[at];
 return e->revision==catalog_u32(p+21)&&catalog_equal(e->ndef_sha256,p+25,32)?at:-1;
}
static void catalog_frame(uint32_t seq,const uint8_t *p,uint16_t n){
 if(!clean()){status(seq,"ERROR:stop NFC before catalog transaction");return;}
 if(n==1&&p[0]==5){
  status_number(seq,"CATALOG:",catalog_current.revision);
  size_t z=reply.length;append(reply.bytes,&z,":");number(reply.bytes,&z,catalog_current.count);
  append(reply.bytes,&z,":");static const char hex[]="0123456789abcdef";
  for(unsigned i=0;i<32;i++){reply.bytes[z++]=hex[catalog_current.sha256[i]>>4];reply.bytes[z++]=hex[catalog_current.sha256[i]&15];}
  reply.bytes[z]=0;reply.length=(uint16_t)z;return;
 }
 if(n==1&&p[0]==4){catalog_storage_abort();status(seq,"CAT_ABORT");return;}
 int rc=-1;
 if(n==42&&p[0]==1){
  rc=catalog_storage_begin(catalog_u32(p+1),p[5],catalog_u32(p+6),p+10);
  if(rc==1){status_number(seq,"CAT_SAME:",catalog_current.revision);return;}
  if(!rc){wanted=-1;start_pending=0;status(seq,"CAT_BEGIN");}
 } else if(n>=6&&n<=245&&p[0]==2){
  rc=catalog_storage_data(catalog_u32(p+1),p+5,n-5);
  if(!rc)status_number(seq,"CAT_DATA:",catalog_storage_offset());
 } else if(n==1&&p[0]==3){
  uint8_t old[16]={0};if(selected>=0)copy(old,catalog_current.entries[selected].id,16);
  rc=catalog_storage_commit();
  if(!rc){selected=catalog_find(&catalog_current,old);wanted=-1;start_pending=0;
   phase=selected>=0?TAG_CATALOG_SELECTED:TAG_CATALOG_IDLE;error_code=0;
   status_number(seq,"CAT_COMMIT:",catalog_current.revision);}
 }
 if(rc<0)status(seq,"ERROR:catalog transaction rejected");
}
static void selection_frame(uint32_t seq,const uint8_t *p,uint16_t n){
 if(n!=57||catalog_storage_active()){status(seq,"ERROR:selection unavailable");return;}
 int at=tuple_index(p);if(at<0){status(seq,"ERROR:selection tuple mismatch");return;}
 if(p[0]==3){
  wanted=at;stop_requested=1;start_pending=0;status(seq,"SELECT_PENDING");return;
 }
 if(p[0]==2&&at==selected){status(seq,"SELECT_OK");return;}
 status(seq,"ERROR:stale selection acknowledgement");
}
static void describe(uint32_t seq){
 if(nfc_probe_error()){status(seq,"ERROR:");size_t n=reply.length;append(reply.bytes,&n,nfc_probe_status());reply.length=(uint16_t)n;}
 else status(seq,nfc_probe_ready()?"READY":nfc_probe_can_unload()?"STOPPED":local_active?"STARTING":"STOPPING");
}
static void frame_received(uint8_t type,uint32_t seq,const uint8_t *p,uint16_t n){
 if(transport_frame_generation()!=transport_generation())return;
 if(!clock_sample())return;
 if(type==TRANSPORT_CATALOG){catalog_frame(seq,p,n);return;}
 if(type==TRANSPORT_SELECTION){selection_frame(seq,p,n);return;}
 if(type==TRANSPORT_TAG){status(seq,"ERROR:use catalog protocol v1");return;}
 if(type==TRANSPORT_MIHOME){return;}
 if(type==TRANSPORT_CONTROL&&n==1){
  if(p[0]==4){status(seq,"CATALOG_V1");return;}
  if(p[0]==0){describe(seq);return;}
  if(p[0]==3){if(!clean()){status(seq,"ERROR:stop NFC before capabilities");return;}
   status(seq,"INFO:");size_t z=reply.length;append(reply.bytes,&z,nfc_probe_info());reply.length=(uint16_t)z;return;}
  if(p[0]==2){api_stop();status(seq,"STOPPING");return;}
  if(p[0]==1){status(seq,"ERROR:start NFC on the band page");return;}
  status(seq,"ERROR:unknown control");return;
 }
 if(type==TRANSPORT_RESPONSE)return;
 status(seq,"ERROR:unsupported frame");
}
static void poll(void *timer){
 (void)timer;
 if(!__atomic_load_n(&initialization_complete,__ATOMIC_ACQUIRE))return;
 if(!clock_sample()){now_ms+=POLL_MS;if(local_active)local_fault("monotonic clock failed");}
 last_poll_ms=now_ms;heartbeat++;
 if(!booted){
  int rc=catalog_provider_install(&api);
  if(rc){error_code=-30;diagnostic_set("LOCAL_PROVIDER",rc,0);}
  rc=transport_start(frame_received);booted=1;
  diagnostic_set("LOCAL_TRANSPORT",rc,transport_debug_state());
 }
 transport_poll(now_ms);
 nfc_probe_poll(now_ms);
 selection_poll();
 if(start_pending){
  if(nfc_probe_error()){start_pending=0;phase=TAG_CATALOG_ERROR;error_code=-22;}
  else if(can_start()){
   int rc=start_local();start_pending=0;
   if(rc){phase=TAG_CATALOG_ERROR;error_code=rc;}
  }
 }
 if(local_active){
  if(nfc_probe_error())local_fault("NFC error");
  else if((uint32_t)(now_ms-started_at)>=SESSION_MS){
   local_active=0;phase=TAG_CATALOG_STOPPING;
   int rc=nfc_probe_stop();if(rc){phase=TAG_CATALOG_ERROR;error_code=rc;}
  } else phase=nfc_probe_ready()?TAG_CATALOG_READY:TAG_CATALOG_STARTING;
 }
 if(!local_active&&!start_pending&&!nfc_probe_error()&&nfc_probe_can_unload()&&
    phase==TAG_CATALOG_STOPPING)phase=selected>=0?TAG_CATALOG_SELECTED:TAG_CATALOG_IDLE;
 if(reply.queued&&transport_ready()&&!transport_notify(TRANSPORT_STATUS,reply.sequence,
    (const uint8_t *)reply.bytes,reply.length))reply.queued=0;
}
int module_initialize(void){
 if(initialized)return initialization_result;initialized=1;
 if(!module_matches()||!catalog_storage_verified()||!catalog_provider_verified())return -1;
 if(!clock_sample())return -1;
 if(catalog_storage_load()<0)error_code=-10;
 selected=catalog_storage_selected();
 phase=selected>=0?TAG_CATALOG_SELECTED:TAG_CATALOG_IDLE;
 api=(tag_catalog_api){.magic=TAG_CATALOG_API_MAGIC,.version=TAG_CATALOG_API_VERSION,
  .bytes=sizeof(api),.profile=TAG_CATALOG_API_PROFILE,.health=api_health,.get=api_get,
  .select=api_select,.start=api_start,.stop=api_stop};
 if(!fw_timer_create(poll,POLL_MS,0))return -1;
 diagnostic_set("LOCAL_NDEF_LOADED",0,catalog_current.count);
 fw_log(6,"[TAG_CATALOG_LOCAL] initialized; records=%u",(unsigned)catalog_current.count);
 initialization_result=0;__atomic_store_n(&initialization_complete,1u,__ATOMIC_RELEASE);
 return 0;
}
#ifndef APDU_LAB_HOST_TEST
__attribute__((constructor))static void module_constructor(void){(void)module_initialize();}
#endif
