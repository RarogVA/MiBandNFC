#include "tag_record.h"
static int text(char *dst,const uint8_t *src,size_t length,int hexadecimal) {
    for(size_t i=0;i<length;i++) {
        uint8_t c=src[i];
        if(c<32 || c>126)return -1;
        if(hexadecimal && !((c>='0'&&c<='9')||(c>='A'&&c<='F')||(c>='a'&&c<='f')))return -1;
        dst[i]=(char)c;
    }
    dst[length]=0;return 0;
}
int tag_record_parse(tag_record *out,const uint8_t *p,size_t n) {
    if(!out || !p || n<5 || p[0]!=1 || p[1]<1 || p[1]>32 || p[2]>20 || (p[2]&1) ||
       p[3]<1 || p[3]>48 || p[4]>1 || n!=5u+p[1]+p[2]+p[3])return -1;
    tag_record next={0};
    if(text(next.name,p+5,p[1],0) || text(next.uid,p+5+p[1],p[2],1) ||
       text(next.technology,p+5+p[1]+p[2],p[3],0))return -1;
    next.supported=p[4];*out=next;return 0;
}
int tag_apdu_allowed(const uint8_t *p,size_t n) {
    static const uint8_t aid[7]={0xd2,0x76,0,0,0x85,1,1};
    if(!p || n<4 || p[0]!=0)return 0;
    if(p[1]==0xb0)return n==5 && !(p[2]&0x80);
    if(p[1]!=0xa4 || (p[3]!=0 && p[3]!=0x0c) || n<5)return 0;
    if(p[2]==4 && p[4]==7 && (n==12 || (n==13 && p[12]==0))) {
        for(unsigned i=0;i<7;i++)if(p[5+i]!=aid[i])return 0;
        return 1;
    }
    return p[2]==0 && p[4]==2 && (n==7 || (n==8 && p[7]==0)) &&
        p[5]==0xe1 && (p[6]==3 || p[6]==4);
}
