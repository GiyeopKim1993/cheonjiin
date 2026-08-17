/* RP2040 진입점 */
#include "../../app/cuime_app.h"
#ifndef CUIME_PORT_STUB
#include "pico/stdlib.h"
#include "tusb.h"
#endif

static cuime_app_t g_app;

int main(void)
{
    hal_board_init();
    cuime_app_init(&g_app);
    for (;;) {
#ifndef CUIME_PORT_STUB
        tud_task();               /* TinyUSB 처리 */
#endif
        cuime_app_poll(&g_app);
        hal_delay_ms(5);
    }
    return 0;
}
