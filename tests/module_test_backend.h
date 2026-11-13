static void (*test_timer)(void *);
static void *fw_timer_create(timer_callback cb,uint32_t period,void *context) {
    (void)period;(void)context;test_timer=cb;cb(0);return (void *)1;
}
static int fw_log(int level,const char *fmt,...) {(void)level;(void)fmt;return 0;}
static int module_matches(void) {return 1;}
