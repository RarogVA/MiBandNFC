#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "tag_record.h"
int main(void) {
    tag_record out={0};
    uint8_t p[]={1,4,4,4,1,'T','e','s','t','A','B','1','2','N','D','E','F'};
    assert(tag_record_parse(&out,p,sizeof(p))==0);
    assert(!strcmp(out.name,"Test") && !strcmp(out.uid,"AB12") && !strcmp(out.technology,"NDEF") && out.supported==1);
    tag_record before=out;
    for(size_t n=0;n<sizeof(p);n++)assert(tag_record_parse(&out,p,n)<0);
    assert(!memcmp(&out,&before,sizeof(out)));
    p[5]='\n';assert(tag_record_parse(&out,p,sizeof(p))<0);p[5]='T';
    p[9]='G';assert(tag_record_parse(&out,p,sizeof(p))<0);p[9]='A';
    p[2]=3;assert(tag_record_parse(&out,p,sizeof(p))<0);p[2]=4;
    p[4]=2;assert(tag_record_parse(&out,p,sizeof(p))<0);p[4]=0;
    assert(tag_record_parse(&out,p,sizeof(p))==0 && out.supported==0);
    uint8_t max[105]={1,32,20,48,1};
    memset(max+5,'A',100);assert(tag_record_parse(&out,max,sizeof(max))==0);
    max[1]=33;assert(tag_record_parse(&out,max,sizeof(max))<0);
    uint8_t empty_uid[]={1,1,0,1,1,'A','T'};
    assert(tag_record_parse(&out,empty_uid,sizeof(empty_uid))==0 && !out.uid[0]);
    const uint8_t select[]={0,0xa4,4,0,7,0xd2,0x76,0,0,0x85,1,1,0};
    assert(tag_apdu_allowed(select,sizeof(select)) && tag_apdu_allowed(select,sizeof(select)-1));
    for(size_t n=0;n<12;n++)assert(!tag_apdu_allowed(select,n));
    uint8_t other[13];memcpy(other,select,sizeof(other));other[6]=0x77;assert(!tag_apdu_allowed(other,13));
    const uint8_t cc[]={0,0xa4,0,0x0c,2,0xe1,3}, ndef[]={0,0xa4,0,0,2,0xe1,4,0};
    assert(tag_apdu_allowed(cc,sizeof(cc)) && tag_apdu_allowed(ndef,sizeof(ndef)));
    const uint8_t read[]={0,0xb0,0,0,0}, write[]={0,0xd6,0,0,1,0};
    assert(tag_apdu_allowed(read,5));assert(!tag_apdu_allowed(write,6));
    uint8_t sfiread[]={0,0xb0,0x80,0,1};assert(!tag_apdu_allowed(sfiread,5));
    const uint8_t payment[]={0,0xa4,4,0,7,0xa0,0,0,0,4,0x10,0x10,0};assert(!tag_apdu_allowed(payment,sizeof(payment)));
    puts("tags record: atomic bounded metadata and read-only Type4 APDU scope PASS");
}
