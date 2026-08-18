/* STM32F411 데모 진입점
 *
 * 메인 루프는 이게 전부다. 조합 로직은 코어(cuime.c)와 앱(cuime_app.c)에
 * 있고, 이 파일은 보드를 깨우고 5ms 주기로 폴링만 한다.
 */
#include "../../app/cuime_app.h"

static cuime_app_t g_app;

int main(void)
{
    hal_board_init();
    cuime_app_init(&g_app);

    /* 부팅 알림: UART 배너 (UTF-8 모드일 때만 의미 있음) */
    if (hal_out_mode() == HAL_OUT_UNICODE_TEXT) {
        const char *banner = "\r\n천지인 IME (STM32F411)\r\n";
        for (const char *p = banner; *p; p++) hal_out_ascii(*p, false);
    }

    for (;;) {
        cuime_app_poll(&g_app);   /* 스캔 + 에지검출 + 타이머 + 조합 + 전송 */
        hal_delay_ms(5);          /* 200Hz — WFI 로 대기하므로 저전력 */
    }
}
