/* Freestanding C/EABI memory helpers; avoids dependencies on unknown firmware
 * libc exports. Compile with -ffreestanding -fno-builtin. */
#include <stddef.h>
void *memcpy(void *d,const void *s,size_t n) {
    unsigned char *a=d;const unsigned char *b=s;for(size_t i=0;i<n;i++)a[i]=b[i];return d;
}
void *memset(void *d,int c,size_t n) {
    unsigned char *a=d;for(size_t i=0;i<n;i++)a[i]=(unsigned char)c;return d;
}
void *memmove(void *d,const void *s,size_t n) {
    unsigned char *a=d;const unsigned char *b=s;
    if(a<b)for(size_t i=0;i<n;i++)a[i]=b[i];
    else for(size_t i=n;i;i--)a[i-1]=b[i-1];
    return d;
}
void __aeabi_memcpy(void *d,const void *s,size_t n){(void)memcpy(d,s,n);}
void __aeabi_memcpy4(void *d,const void *s,size_t n){(void)memcpy(d,s,n);}
void __aeabi_memcpy8(void *d,const void *s,size_t n){(void)memcpy(d,s,n);}
void __aeabi_memmove(void *d,const void *s,size_t n){(void)memmove(d,s,n);}
void __aeabi_memset(void *d,size_t n,int c){(void)memset(d,c,n);}
void __aeabi_memset4(void *d,size_t n,int c){(void)memset(d,c,n);}
void __aeabi_memset8(void *d,size_t n,int c){(void)memset(d,c,n);}
void __aeabi_memclr(void *d,size_t n){(void)memset(d,0,n);}
void __aeabi_memclr4(void *d,size_t n){(void)memset(d,0,n);}
void __aeabi_memclr8(void *d,size_t n){(void)memset(d,0,n);}
