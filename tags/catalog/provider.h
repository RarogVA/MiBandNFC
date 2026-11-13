#ifndef CATALOG_PROVIDER_H
#define CATALOG_PROVIDER_H
#include "api.h"
int catalog_provider_verified(void);
int catalog_provider_install(const tag_catalog_api *api);
#endif
