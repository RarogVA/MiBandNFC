#include "state.h"
static int equal(const char *a,const char *b) {
    while(*a && *a==*b){a++;b++;}return *a==*b;
}
static int line(char *out,size_t cap,const char **p,const char *end) {
    size_t n=0;
    while(*p<end && **p!='\n') {
        unsigned c=(unsigned char)*(*p)++;
        if(c<32 || c>126 || n+1>=cap)return 0;
        out[n++]=(char)c;
    }
    if(*p==end)return 0;
    (*p)++;out[n]=0;return 1;
}
int tag_ui_parse(tag_ui_state *out,const char *bytes,size_t length) {
    if(!out || !bytes || !length || length>160)return 0;
    const char *p=bytes,*end=bytes+length;tag_ui_state value={0};char magic[5],flag[2],tail[4];
    if(!line(magic,sizeof(magic),&p,end) || !equal(magic,"TAG1") ||
       !line(value.name,sizeof(value.name),&p,end) || !value.name[0] ||
       !line(value.uid,sizeof(value.uid),&p,end) ||
       !line(value.technology,sizeof(value.technology),&p,end) ||
       !line(flag,sizeof(flag),&p,end) || (flag[0]!='0' && flag[0]!='1') ||
       !line(value.state,sizeof(value.state),&p,end) ||
       !line(tail,sizeof(tail),&p,end) || !equal(tail,"END") || p!=end)return 0;
    unsigned n=0;
    while(value.uid[n]) {
        unsigned c=(unsigned char)value.uid[n++];
        if(!((c>='0'&&c<='9') || (c>='A'&&c<='F')))return 0;
    }
    if(n&1)return 0;
    static const char *const states[]={"OFFLINE","SELECTED","STARTING","READY","STOPPING","ERROR"};
    unsigned i=0;for(;i<6;i++)if(equal(value.state,states[i]))break;
    if(i==6)return 0;
    value.supported=(unsigned)(flag[0]-'0');*out=value;return 1;
}
