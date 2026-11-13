/* Catalog variant only. One existing UI-thread timer owns BLE/NFC, disk
 * transactions and local UI requests. No new worker or filesystem poll. */
#include <stddef.h>
#include <stdint.h>
#include "transport.h"
#include "nfc_probe.h"
#include "native_clock.h"
#include "diagnostic.h"
#include "tag_record.h"
#include "storage.h"
#include "provider.h"
typedef void (*timer_callback)(void *);
#ifndef APDU_LAB_HOST_TEST
#define fw_timer_create ((void *(*)(timer_callback,uint32_t,void *))(uintptr_t)0x2c558209u)
#define fw_log ((int (*)(int,const char *,...))(uintptr_t)0x2c1beab9u)
typedef struct {uintptr_t address;uint8_t bytes[24];} module_fingerprint;
#include "../band/module_fingerprints.h"
static int module_matches(void){for(unsigned i=0;i<sizeof(module_fingerprints)/sizeof(module_fingerprints[0]);i++){const volatile uint8_t *p=(const volatile uint8_t *)module_fingerprints[i].address;for(unsigned j=0;j<24;j++)if(p[j]!=module_fingerprints[i].bytes[j])return 0;}return 1;}
#else
#include "module_test_backend.h"
#endif
#define POLL_MS 10u
#define SELECTION_DEADLINE_MS 30000u
static uint32_t now_ms,last_poll_ms,heartbeat,peer_generation,initialization_complete;
static uint32_t apdu_sequence=0x10000000u,select_sequence=0x20000000u,operation_sequence;
static uint32_t request_sequence,request_at,started_at,selection_at,transaction_at,transaction_sequence;
static int initialized,initialization_result=-1,booted,clock_failed,desired,authorized,pending,request_sent;
static int selected=-1,wanted=-1,selection_dirty,selection_sent,stop_requested,phase,error_code;
static int last_ready=-1,last_clean=-1;
static uint8_t request_bytes[261],selection_tuple[57];static uint16_t request_length;
typedef struct{char bytes[128];uint16_t length;uint8_t queued;uint32_t sequence;} message;
static message reply,operation_reply;
static tag_catalog_api api;
static void copy(void *a,const void *b,size_t n){uint8_t *d=a;const uint8_t *s=b;while(n--)*d++=*s++;}
static void append(char *p,size_t *n,const char *s){while(*s&&*n<127)p[(*n)++]=*s++;p[*n]=0;}
static void number(char *p,size_t *n,uint32_t v){char d[10];unsigned k=0;do{d[k++]=(char)('0'+v%10);v/=10;}while(v);while(k&&*n<127)p[(*n)++]=d[--k];p[*n]=0;}
static void status(uint32_t seq,const char *s){size_t n=0;append(reply.bytes,&n,s);reply.length=(uint16_t)n;reply.sequence=seq;reply.queued=1;}
static void status_number(uint32_t seq,const char *s,uint32_t v){status(seq,s);size_t n=reply.length;number(reply.bytes,&n,v);reply.length=(uint16_t)n;}
static void fault(const char *why){desired=authorized=pending=request_sent=selection_sent=selection_dirty=0;phase=TAG_CATALOG_ERROR;error_code=-1;(void)nfc_probe_stop();status(operation_sequence,"ERROR:");size_t n=reply.length;append(reply.bytes,&n,why);reply.length=(uint16_t)n;operation_reply=reply;reply.queued=0;}
static int clock_sample(void){uint32_t t;if(!native_clock_ms(&t)){now_ms=t;return 1;}clock_failed=1;return 0;}
static int clean(void){return !desired&&nfc_probe_can_unload()&&!nfc_probe_error();}
static void invalidate_selection(void){authorized=selection_sent=0;selection_dirty=selected>=0;}
static void peer_reconcile(void){uint32_t g=transport_generation();if(g==peer_generation)return;peer_generation=g;desired=pending=request_sent=0;reply.queued=operation_reply.queued=0;invalidate_selection();catalog_storage_abort();(void)nfc_probe_stop();phase=nfc_probe_can_unload()?TAG_CATALOG_WAIT_PIXEL:TAG_CATALOG_STOPPING;}
static void describe(uint32_t seq){if(nfc_probe_error()){status(seq,"ERROR:");size_t n=reply.length;append(reply.bytes,&n,nfc_probe_status());reply.length=(uint16_t)n;}else status(seq,nfc_probe_ready()?"READY":nfc_probe_can_unload()?"STOPPED":desired?"STARTING":"STOPPING");}
static int api_get(uint32_t revision,uint16_t index,tag_catalog_entry *out){if(!out||revision!=catalog_current.revision||index>=catalog_current.count)return-1;*out=catalog_current.entries[index];return 0;}
static int api_select(uint32_t revision,const uint8_t id[16],uint32_t record_revision){
 if(catalog_storage_active())return-2;int at=catalog_find(&catalog_current,id);
 if(revision!=catalog_current.revision||at<0||catalog_current.entries[at].revision!=record_revision)return-1;
 wanted=at;stop_requested=1;authorized=selection_sent=0;selection_dirty=0;return 0;
}
static int api_start(void){return-1;}
static int api_stop(void){stop_requested=1;authorized=selection_sent=0;selection_dirty=selected>=0;return 0;}
static int api_health(tag_catalog_health *out){
 if(!out)return-1;uint32_t t,age=UINT32_MAX;if(heartbeat&&!native_clock_ms(&t))age=t-last_poll_ms;
 *out=(tag_catalog_health){.heartbeat=heartbeat,.poll_age_ms=age,.catalog_revision=catalog_current.revision,.peer_generation=peer_generation,.count=catalog_current.count,.selected_index=(int16_t)selected,
 .phase=(uint8_t)phase,.ble_ready=(uint8_t)(booted>0&&transport_ready()),.nfc_ready=(uint8_t)nfc_probe_ready(),.clean=(uint8_t)nfc_probe_can_unload(),.authorized=(uint8_t)authorized,.transaction_active=(uint8_t)catalog_storage_active(),.loop_live=(uint8_t)(age<1000&&!clock_failed),.error=error_code?error_code:nfc_probe_error()?-22:0};return 0;
}
static void apdu_received(const uint8_t *bytes,size_t n,void *context){
 (void)context;peer_reconcile();if(!clock_sample()||!desired||!authorized||!transport_ready()||!nfc_probe_ready())return;
 if(!tag_apdu_allowed(bytes,n)){static const uint8_t no[]={0x6d,0};(void)nfc_probe_send(no,2);return;}
 if(pending){fault("overlapping APDU");return;}copy(request_bytes,bytes,n);request_length=(uint16_t)n;if(++apdu_sequence<0x10000000u||apdu_sequence>=0x20000000u)apdu_sequence=0x10000001u;request_sequence=apdu_sequence;request_at=now_ms;request_sent=0;pending=1;
}
static int tuple_index(const uint8_t *p){if(catalog_u32(p+1)!=catalog_current.revision)return-1;int at=catalog_find(&catalog_current,p+5);if(at<0)return-1;const tag_catalog_entry *e=&catalog_current.entries[at];return e->revision==catalog_u32(p+21)&&catalog_equal(e->ndef_sha256,p+25,32)?at:-1;}
static void catalog_frame(uint32_t seq,const uint8_t *p,uint16_t n){
 if(!clean()){status(seq,"ERROR:stop NFC before catalog transaction");return;}
 if(n==1&&p[0]==5){status_number(seq,"CATALOG:",catalog_current.revision);size_t z=reply.length;append(reply.bytes,&z,":");number(reply.bytes,&z,catalog_current.count);append(reply.bytes,&z,":");static const char hex[]="0123456789abcdef";for(unsigned i=0;i<32;i++){reply.bytes[z++]=hex[catalog_current.sha256[i]>>4];reply.bytes[z++]=hex[catalog_current.sha256[i]&15];}reply.bytes[z]=0;reply.length=(uint16_t)z;if(!authorized&&selected>=0){if(selection_sent&&(uint32_t)(now_ms-selection_at)>=SELECTION_DEADLINE_MS)selection_sent=0;if(!selection_sent)selection_dirty=1;}return;}
 if(n==1&&p[0]==4){catalog_storage_abort();status(seq,"CAT_ABORT");selection_dirty=selected>=0;return;}
 int rc=-1;
 if(n==42&&p[0]==1){rc=catalog_storage_begin(catalog_u32(p+1),p[5],catalog_u32(p+6),p+10);if(rc==1){status_number(seq,"CAT_SAME:",catalog_current.revision);return;}if(!rc){authorized=selection_sent=selection_dirty=0;wanted=-1;status(seq,"CAT_BEGIN");}}
 else if(n>=6&&n<=245&&p[0]==2){rc=catalog_storage_data(catalog_u32(p+1),p+5,n-5);if(!rc)status_number(seq,"CAT_DATA:",catalog_storage_offset());}
 else if(n==1&&p[0]==3){uint8_t old[16]={0};if(selected>=0)copy(old,catalog_current.entries[selected].id,16);rc=catalog_storage_commit();if(!rc){selected=catalog_find(&catalog_current,old);wanted=selected;authorized=selection_sent=0;selection_dirty=selected>=0;phase=TAG_CATALOG_IDLE;error_code=0;status_number(seq,"CAT_COMMIT:",catalog_current.revision);}}
 if(rc<0){status(seq,"ERROR:catalog transaction rejected");return;}transaction_at=now_ms;transaction_sequence=seq;
}
static void selection_frame(uint32_t seq,const uint8_t *p,uint16_t n){
 if(n!=57||catalog_storage_active()){status(seq,"ERROR:selection unavailable");return;}int at=tuple_index(p);if(at<0){status(seq,"ERROR:selection tuple mismatch");return;}
 if(p[0]==3){int rc=api_select(catalog_current.revision,p+5,catalog_u32(p+21));status(seq,rc?"ERROR:selection busy":"SELECT_PENDING");return;}
 if(p[0]!=2||at!=selected||seq!=select_sequence||!catalog_equal(p+1,selection_tuple+1,56)||!clean()||(!selection_sent&&!authorized)){status(seq,"ERROR:stale selection acknowledgement");return;}
 if(selection_sent&&(uint32_t)(now_ms-selection_at)>=SELECTION_DEADLINE_MS){selection_sent=authorized=0;phase=TAG_CATALOG_ERROR;status(seq,"ERROR:selection acknowledgement timed out");return;}
 authorized=1;selection_sent=selection_dirty=0;phase=TAG_CATALOG_SELECTED;error_code=0;status(seq,"SELECT_OK");
}
static void frame_received(uint8_t type,uint32_t seq,const uint8_t *p,uint16_t n){
 peer_reconcile();if(transport_frame_generation()!=transport_generation())return;if(!clock_sample())return;
 if(type==TRANSPORT_CATALOG){catalog_frame(seq,p,n);return;}if(type==TRANSPORT_SELECTION){selection_frame(seq,p,n);return;}
 if(type==TRANSPORT_TAG){status(seq,"ERROR:use catalog protocol v1");return;}
 if(type==TRANSPORT_CONTROL&&n==1){
  if(p[0]==4){status(seq,"CATALOG_V1");return;}if(p[0]==0){describe(seq);return;}
  if(p[0]==3){if(!clean()){status(seq,"ERROR:stop NFC before capabilities");return;}status(seq,"INFO:");size_t z=reply.length;append(reply.bytes,&z,nfc_probe_info());reply.length=(uint16_t)z;return;}
  if(p[0]==2){operation_sequence=seq;api_stop();desired=pending=request_sent=0;(void)nfc_probe_stop();last_ready=last_clean=-1;describe(seq);return;}
  if(p[0]!=1){status(seq,"ERROR:unknown control");return;}
  if(!authorized||selected<0||!catalog_current.entries[selected].kind||wanted>=0||selection_dirty||selection_sent||catalog_storage_active()){status(seq,"ERROR:confirm supported catalog selection first");return;}
  if(clock_failed||!clean()||!transport_ready()){status(seq,"ERROR:NFC unavailable or recovering");return;}
  operation_sequence=seq;if(nfc_probe_start(apdu_received,0)){status(seq,"ERROR:NFC start failed");return;}
  desired=1;started_at=now_ms;phase=TAG_CATALOG_STARTING;last_ready=last_clean=-1;describe(seq);return;
 }
 if(type!=TRANSPORT_RESPONSE||!pending||!request_sent||seq!=request_sequence)return;
 if((uint32_t)(now_ms-request_at)>=3000u){fault("late Bluetooth response");return;}
 if(n<2||n>258){fault("invalid NDEF response");return;}if(nfc_probe_send(p,n)){fault("NFC transmit failed");return;}pending=request_sent=0;
}
static void selection_poll(void){
 if(stop_requested){stop_requested=0;desired=pending=request_sent=0;(void)nfc_probe_stop();phase=TAG_CATALOG_STOPPING;last_ready=last_clean=-1;}
 if(nfc_probe_error()){authorized=selection_sent=selection_dirty=0;phase=TAG_CATALOG_ERROR;error_code=-22;return;}
 if(!clean())return;
 if(wanted>=0){int next=wanted;wanted=-1;if(catalog_storage_select(next)){phase=TAG_CATALOG_ERROR;error_code=-20;selection_dirty=0;return;}selected=next;selection_dirty=1;authorized=selection_sent=0;}
 if(selected<0){if(!error_code)phase=TAG_CATALOG_IDLE;return;}
 if(selection_sent&&(uint32_t)(now_ms-selection_at)>=SELECTION_DEADLINE_MS){selection_sent=selection_dirty=authorized=0;phase=TAG_CATALOG_ERROR;error_code=-21;return;}
 if(!selection_dirty||selection_sent||catalog_storage_active())return;phase=TAG_CATALOG_WAIT_PIXEL;
 if(!transport_ready()||reply.queued||operation_reply.queued||pending)return;
 const tag_catalog_entry *e=&catalog_current.entries[selected];selection_tuple[0]=1;catalog_put_u32(selection_tuple+1,catalog_current.revision);copy(selection_tuple+5,e->id,16);catalog_put_u32(selection_tuple+21,e->revision);copy(selection_tuple+25,e->ndef_sha256,32);
 uint32_t next=select_sequence+1;if(next<0x20000000u||next>=0x30000000u)next=0x20000001u;
 if(!transport_notify(TRANSPORT_SELECTION,next,selection_tuple,57)){select_sequence=next;selection_sent=1;selection_at=now_ms;}
}
static void poll(void *timer){
 (void)timer;if(!__atomic_load_n(&initialization_complete,__ATOMIC_ACQUIRE))return;
 if(!clock_sample()){now_ms+=POLL_MS;if(desired)fault("monotonic clock failed");}else{last_poll_ms=now_ms;heartbeat++;}
 if(!booted){int rc=catalog_provider_install(&api);if(rc){error_code=-30;diagnostic_set("CAT_PROVIDER",rc,0);}rc=transport_start(frame_received);booted=rc?-1:1;peer_generation=transport_generation();diagnostic_set("CAT_TRANSPORT",rc,transport_debug_state());}
 if(booted<0)return;peer_reconcile();transport_poll(now_ms);peer_reconcile();nfc_probe_poll(now_ms);
 if(desired&&!transport_ready())fault("Bluetooth disconnected");if(desired&&(uint32_t)(now_ms-started_at)>=120000u)fault("120 second test limit");if(desired&&nfc_probe_error())fault(nfc_probe_status());
 if(pending&&(uint32_t)(now_ms-request_at)>=3000u)fault("Bluetooth APDU timeout");
 if(catalog_storage_active()&&(uint32_t)(now_ms-transaction_at)>=15000u){catalog_storage_abort();status(transaction_sequence,"ERROR:catalog transaction idle timeout");}
 if(pending&&!request_sent){int rc=transport_notify(TRANSPORT_REQUEST,request_sequence,request_bytes,request_length);if(!rc)request_sent=1;else if(rc!=-2)fault("Bluetooth CAPDU send failed");}
 int ready=nfc_probe_ready(),idle=nfc_probe_can_unload();
 if(desired)phase=ready?TAG_CATALOG_READY:TAG_CATALOG_STARTING;
 if(operation_sequence&&(ready!=last_ready||idle!=last_clean)){message saved=reply;describe(operation_sequence);operation_reply=reply;reply=saved;last_ready=ready;last_clean=idle;}
 message *m=operation_reply.queued?&operation_reply:&reply;if(m->queued&&!pending&&transport_ready()&&!transport_notify(TRANSPORT_STATUS,m->sequence,(const uint8_t *)m->bytes,m->length))m->queued=0;
 selection_poll();
}
int module_initialize(void){
 if(initialized)return initialization_result;initialized=1;
 if(!module_matches()||!catalog_storage_verified()||!catalog_provider_verified())return-1;
 if(!clock_sample())return-1;int loaded=catalog_storage_load();if(loaded<0)error_code=-10;selected=catalog_storage_selected();selection_dirty=selected>=0;
 api=(tag_catalog_api){.magic=TAG_CATALOG_API_MAGIC,.version=1,.bytes=sizeof(api),.profile=TAG_CATALOG_API_PROFILE,.health=api_health,.get=api_get,.select=api_select,.start=api_start,.stop=api_stop};
 if(!fw_timer_create(poll,POLL_MS,0))return-1;
 diagnostic_set("CATALOG_LOADED",0,catalog_current.count);fw_log(6,"[TAG_CATALOG_V1] initialized; records=%u; explicit Pixel START required",(unsigned)catalog_current.count);initialization_result=0;__atomic_store_n(&initialization_complete,1u,__ATOMIC_RELEASE);return 0;
}
#ifndef APDU_LAB_HOST_TEST
__attribute__((constructor)) static void module_constructor(void){(void)module_initialize();}
#endif
