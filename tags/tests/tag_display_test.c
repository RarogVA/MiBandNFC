#include "tag_display.h"
#include "diagnostic.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned checks,opens,writes,closes,token;
static int valid=1,open_fail,close_fail,short_write;
static char content[192];static size_t content_length;
static const char *expected_path="/tmp/tag_catalog_state";
int diagnostic_hw_verified(void){checks++;return valid;}
void *diagnostic_hw_open(const char *path,const char *mode){
    assert(!strcmp(path,expected_path) && !strcmp(mode,"w"));
    opens++;return open_fail?0:&token;
}
size_t diagnostic_hw_write(const void *data,size_t size,size_t count,void *file){
    assert(file==&token && size==1 && count<sizeof(content));writes++;
    content_length=short_write?count/2:count;memcpy(content,data,content_length);content[content_length]=0;
    return content_length;
}
int diagnostic_hw_close(void *file){assert(file==&token);closes++;return close_fail?-1:0;}
static void reset(void){
    tag_display_test_reset();checks=opens=writes=closes=0;
    valid=1;open_fail=close_fail=short_write=0;content[0]=0;content_length=0;
}
int main(void){
    tag_record r={"Tag","AB12","NDEF",1};reset();
    assert(tag_display_write(&r,"SELECTED")==0);
    assert(!strcmp(content,"TAG1\nTag\nAB12\nNDEF\n1\nSELECTED\nEND\n"));
    assert(checks==1 && opens==1 && writes==1 && closes==1);
    assert(tag_display_write(0,"OFFLINE")==0 && checks==1);
    assert(!strcmp(content,"TAG1\nNo tag selected\n\n\n0\nOFFLINE\nEND\n"));
    reset();valid=0;assert(tag_display_write(&r,"READY")<0);assert(tag_display_write(&r,"READY")<0);
    assert(checks==1 && !opens && !writes && !closes);
    reset();open_fail=1;assert(tag_display_write(&r,"READY")<0);assert(opens==1 && !writes && !closes);
    reset();short_write=1;assert(tag_display_write(&r,"READY")<0);assert(opens==1 && writes==1 && closes==1);
    reset();close_fail=1;assert(tag_display_write(&r,"READY")<0);assert(opens==1 && writes==1 && closes==1);
    reset();memset(r.name,'N',32);r.name[32]=0;memset(r.uid,'A',20);r.uid[20]=0;
    memset(r.technology,'T',48);r.technology[48]=0;assert(tag_display_write(&r,"STOPPING")==0);
    assert(content_length<160 && strstr(content,"\nEND\n"));
    reset();diagnostic_test_reset();
#ifdef TAG_DIAGNOSTIC_V3
    expected_path="/tmp/tag_catalog_diag";
    diagnostic_set("TAG_BLE",0,69905);
    assert(!strcmp(content,"stage=TAG_BLE\nrc=0\nstate=69905\n"));
#else
    expected_path="/tmp/apdu_lab_state";
    diagnostic_set("BLE",0,69905);
    assert(!strcmp(content,"stage=BLE\nrc=0\nstate=69905\n"));
#endif
    assert(opens==1 && writes==1 && closes==1);
    puts("tags display: full-write/close, framing, I/O failures and profile-specific diagnostic path PASS");
}
