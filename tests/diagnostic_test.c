#include "../band/diagnostic.h"
#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static unsigned checks,opens,writes,closes;
static int valid=1,open_fail,close_fail,short_write;
static char content[128];static size_t content_length;
static unsigned token;
int diagnostic_hw_verified(void) {checks++;return valid;}
void *diagnostic_hw_open(const char *path,const char *mode) {
    assert(strcmp(path,"/tmp/apdu_lab_state")==0);assert(strcmp(mode,"w")==0);
    opens++;return open_fail?0:&token;
}
size_t diagnostic_hw_write(const void *data,size_t size,size_t count,void *file) {
    assert(file==&token && size==1 && count<sizeof(content));writes++;
    content_length=short_write==2?0:short_write?count/2:count;
    memcpy(content,data,content_length);content[content_length]=0;
    return content_length;
}
int diagnostic_hw_close(void *file) {assert(file==&token);closes++;return close_fail?-1:0;}
static void reset(void) {
    diagnostic_test_reset();checks=opens=writes=closes=0;
    valid=1;open_fail=close_fail=short_write=0;content[0]=0;content_length=0;
}
int main(void) {
    reset();diagnostic_set("entry",0,0);
    assert(strcmp(content,"stage=entry\nrc=0\nstate=0\n")==0);
    assert(checks==1 && opens==1 && writes==1 && closes==1);
    diagnostic_set("entry",0,0);assert(opens==1);
    diagnostic_set("entry",INT_MIN,UINT32_MAX);
    assert(strcmp(content,"stage=entry\nrc=-2147483648\nstate=4294967295\n")==0);
    diagnostic_set("entry",INT_MAX,1);
    assert(strcmp(content,"stage=entry\nrc=2147483647\nstate=1\n")==0);
    assert(checks==1 && opens==3 && closes==3);
    diagnostic_set("a\n\r\x7f\xff",-1,10);
    assert(strcmp(content,"stage=a____\nrc=-1\nstate=10\n")==0);
    diagnostic_set(0,0,0);assert(strcmp(content,"stage=unknown\nrc=0\nstate=0\n")==0);
    diagnostic_set("",0,0);assert(strcmp(content,"stage=\nrc=0\nstate=0\n")==0);
    char long_stage[200];memset(long_stage,'x',sizeof(long_stage));long_stage[199]=0;
    diagnostic_set(long_stage,INT_MIN,UINT32_MAX);
    assert(content_length==87 && content[54]=='\n');
    unsigned attempts=opens;long_stage[100]='y';diagnostic_set(long_stage,INT_MIN,UINT32_MAX);
    assert(opens==attempts);
    reset();valid=0;diagnostic_set("one",0,0);diagnostic_set("two",0,0);
    assert(checks==1 && !opens && !writes && !closes);
    reset();open_fail=1;diagnostic_set("open",0,0);diagnostic_set("open",0,0);
    assert(opens==1 && !writes && !closes);
    open_fail=0;diagnostic_set("open",1,0);assert(opens==2 && writes==1 && closes==1);
    reset();short_write=1;diagnostic_set("short",-5,2);diagnostic_set("short",-5,2);
    assert(opens==1 && writes==1 && closes==1);
    reset();short_write=2;diagnostic_set("writefail",-5,2);
    assert(opens==1 && writes==1 && closes==1 && content_length==0);
    reset();close_fail=1;diagnostic_set("close",-5,2);diagnostic_set("close",-5,2);
    assert(opens==1 && writes==1 && closes==1);
    puts("diagnostic: format, int32/uint32 bounds, ASCII cap, dedup and I/O failures passed");
}
