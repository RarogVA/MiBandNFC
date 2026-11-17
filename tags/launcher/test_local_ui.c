/* Local NFC UI5 host checks, reusing the UI4 fault-injection harness. */
#define LOCAL_UI_TEST 1
#define TAG_UI_LOCAL_NFC 1
#include "test_catalog_ui.c"

static void local_test(void) {
    /* Opening the catalog page starts the local session without BLE. */
    reset();provider_present=1;start_rc=0;
    health_value.phase=TAG_CATALOG_SELECTED;health_value.nfc_ready=0;
    open_page();
    CHECK(start_count==1);
    CHECK(live_timers()==1);
    CHECK(!strcmp(badge->text,"NDEF"));
    health_value.nfc_ready=1;health_value.phase=TAG_CATALOG_READY;
    tick(500);
    CHECK(!strcmp(badge->text,"NFC"));
    destroy(0,&parent,0);
    CHECK(stop_count==1);
    CHECK(!live_timers());
    CHECK(!title&&!badge);

    /* Leaving the page stops; resuming starts again. */
    reset();provider_present=1;start_rc=0;
    health_value.phase=TAG_CATALOG_SELECTED;
    open_page();
    CHECK(start_count==1&&live_timers()==1);
    pause_page(0,&parent,0);
    CHECK(stop_count==1&&!live_timers()&&!page_active);
    resume_page(0,&parent,0);
    CHECK(start_count==2&&live_timers()==1);
    destroy(0,&parent,0);
    CHECK(stop_count==2&&!live_timers());

    /* No selected NDEF: no start and no session timer. */
    reset();provider_present=1;start_rc=0;
    health_value.selected_index=-1;health_value.phase=TAG_CATALOG_IDLE;
    open_page();
    CHECK(!start_count&&!live_timers());
    destroy(0,&parent,0);
    CHECK(!stop_count);

    /* Start refusal is not retried and leaves the page usable. */
    reset();provider_present=1;start_rc=-1;
    health_value.phase=TAG_CATALOG_SELECTED;
    open_page();
    CHECK(start_count==1&&!live_timers());
    refreshed(0);
    CHECK(start_count==2&&!live_timers());
    destroy(0,&parent,0);

    /* Selecting a record restarts the local session after the refresh window. */
    reset();provider_present=1;start_rc=0;
    health_value.phase=TAG_CATALOG_SELECTED;
    open_page();
    unsigned starts=start_count;
    go_next(0);
    CHECK(select_count==1);
    health_value.selected_index=2;
    tick(500);
    CHECK(start_count==starts+1);
    destroy(0,&parent,0);
}

int main(void) {
    local_test();
    if (failures) {
        fprintf(stderr, "local NFC UI: %u of %u checks failed\n", failures, checks);
        return 1;
    }
    printf("local NFC UI5 start/stop, badge, pause/resume and selection restart: %u checks PASS\n", checks);
    return 0;
}
