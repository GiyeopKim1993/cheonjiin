/* 포트 정합성 검사용 최소 main.
 * CUIME_PORT_STUB 로 각 보드 HAL 을 호스트에서 컴파일/링크해
 * HAL 함수 5종이 빠짐없이 구현됐는지(=포팅 완결성) 확인한다. */
#include "../app/cuime_app.h"
static cuime_app_t app;
int main(void)
{
    hal_board_init();
    cuime_app_init(&app);
    for (int i = 0; i < 3; i++) cuime_app_poll(&app);
    return 0;
}
