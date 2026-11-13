#include "provider.h"
#include "../launcher/abi.h"
#include "provider_fingerprints.h"
static tag_catalog_provider provider;
static ui_app app;
static const char *get_name(void){return provider.label;}
int catalog_provider_verified(void){
 for(unsigned i=0;i<sizeof(provider_checks)/sizeof(provider_checks[0]);i++){
  const volatile uint8_t *p=(const volatile uint8_t *)provider_checks[i].address;
  for(unsigned j=0;j<24;j++)if(p[j]!=provider_checks[i].bytes[j])return 0;
 }
 uintptr_t p=*(volatile uintptr_t *)(uintptr_t)0x200b72e0u;return p==0x0c7abb69u||p==0x2c7abb69u;
}
int catalog_provider_install(const tag_catalog_api *api){
 ui_app *(*lookup)(uint16_t)=(ui_app *(*)(uint16_t))(uintptr_t)0x2c7ab19du;
 if(!api||lookup(TAG_CATALOG_PROVIDER_ID))return-1;
 const char *label=TAG_CATALOG_PROVIDER_LABEL;unsigned i=0;while(label[i]){provider.label[i]=label[i];i++;}
 provider.api=*api;app.name=TAG_CATALOG_PROVIDER_NAME;app.app_id=TAG_CATALOG_PROVIDER_ID;app.get_name=get_name;
 /* Exact .026 install checks count at0x2c7abc42 and skips the registration
  * loop when zero. No launcher entry or dummy screen is needed for this own
  * native registry provider. Its name is a valid NUL-terminated label. */
 int (*install)(ui_app *,ui_page **,int)=*(int (**)(ui_app *,ui_page **,int))(uintptr_t)0x200b72e0u;
 int rc=install(&app,0,0);ui_app *registered=lookup(TAG_CATALOG_PROVIDER_ID);
 return !rc&&registered&&registered->get_name==get_name?0:-1;
}
