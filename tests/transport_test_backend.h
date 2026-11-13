#ifndef TRANSPORT_TEST_BACKEND_H
#define TRANSPORT_TEST_BACKEND_H
static void *current_instance(void);
static uint8_t register_service(void *,void **,callbacks_t *);
static uint8_t unregister_service(void *);
static uint8_t add_table(void *,database_t *);
static uint8_t remove_table(void *,uint16_t);
static uint8_t notify_value(void *,address_t *,uint16_t,const uint8_t *,uint16_t);
static void *start_advertising(void *,adv_params_t *,uint8_t *,uint16_t,uint8_t *,uint16_t,adv_callbacks_t *);
static void stop_advertising(void *,void *);
#endif
