/* ESP32 진입점 — 메인 루프는 이게 전부다. */
#include "../../app/cuime_app.h"
#ifndef CUIME_PORT_STUB
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#endif

static cuime_app_t g_app;

void app_main(void)
{
    hal_board_init();
    cuime_app_init(&g_app);
    for (;;) {
        cuime_app_poll(&g_app);   /* 스캔 + 에지검출 + 타이머 + 조합 + 전송 */
        hal_delay_ms(5);          /* 5ms 주기 = 200Hz 스캔 */
    }
}
