/* 천지인 원시 키 이벤트 프로토콜 (커스텀 HID)
 *
 * 디바이스 ──(키 눌림/뗌)──> 호스트 IME ──(조합 결과)──> 애플리케이션
 *
 * 디바이스는 조합에 관여하지 않는다. "몇 번 키가 눌렸다/떨어졌다"만 보낸다.
 * 조합·멀티탭 타이머·롱프레스 판정·프리에딧 표시·한자 후보는 전부 호스트 몫이다.
 *
 * ── 왜 커스텀 HID 인가 (CDC 를 쓰지 않는 이유) ──
 *   CDC-ACM 은 COM 포트로 뜬다. 포트 번호가 꽂는 순서마다 바뀌고, 다른
 *   프로그램이 선점하면 IME 가 열지 못한다. 키보드가 시리얼 포트를
 *   노출하는 것도 이상하다. 커스텀 HID 는 VID/PID/UsagePage 로 고정
 *   식별되고, 3개 OS 모두 드라이버 없이 붙는다.
 *
 * ── 식별자 ──
 *   UsagePage 0xFF60 / Usage 0x61 — QMK Raw HID 와 동일한 관례값.
 *   실사용으로 검증된 조합이라 그대로 쓴다.
 *
 *   ⚠️ Usage 를 0 으로 두거나 Collection 을 Application(0x01) 이 아닌
 *      벤더 타입으로 두면 **macOS 가 인터페이스를 아예 노출하지 않는다**
 *      (hidapi #266 실제 사례). 반드시 Usage(0x61) + Collection(Application).
 *
 * ── 리포트 형식 (32바이트 고정) ──
 *   [0]    프로토콜 버전 (CUIME_RAW_PROTO_VER)
 *   [1]    메시지 종류 (cuime_raw_msg_t)
 *   [2..]  페이로드 (종류별)
 *
 *   고정 길이인 이유: HID 는 가변 길이가 없다. 짧은 메시지는 0 으로 채운다.
 */
#ifndef CUIME_RAWHID_H
#define CUIME_RAWHID_H

#include <stdint.h>

#define CUIME_RAW_USAGE_PAGE  0xFF60u
#define CUIME_RAW_USAGE_ID    0x61u
#define CUIME_RAW_REPORT_SIZE 32
#define CUIME_RAW_PROTO_VER   1

/* 디바이스 -> 호스트 */
typedef enum {
    CUIME_RAW_EV_KEY   = 0x01,  /* [2]=키번호 [3]=1눌림/0뗌 [4..7]=타임스탬프(ms, LE) */
    CUIME_RAW_EV_HELLO = 0x02,  /* [2]=프로토콜버전 [3]=키수 [4..]=펌웨어명 */
    CUIME_RAW_EV_PONG  = 0x03   /* 하트비트 응답. [2..5]=호스트가 보낸 nonce 반향 */
} cuime_raw_dev_msg_t;

/* 호스트 -> 디바이스 */
typedef enum {
    CUIME_RAW_CMD_HELLO   = 0x81,  /* IME 접속. 이후 디바이스는 RAW 모드로 전환 */
    CUIME_RAW_CMD_BYE     = 0x82,  /* IME 정상 종료. 즉시 두벌식 폴백 */
    CUIME_RAW_CMD_PING    = 0x83,  /* 하트비트. [2..5]=nonce */
    CUIME_RAW_CMD_LED     = 0x84   /* [2]=LED 상태(0 소등 / 1 조합중 / 2 후보열림) */
} cuime_raw_host_msg_t;

/* 하트비트가 이 시간 동안 없으면 IME 가 죽은 것으로 보고 두벌식 폴백.
 *
 * 왜 하트비트인가: 프로세스가 강제 종료되면 BYE 를 못 보낸다. 그 상태로
 * RAW 모드에 머물면 키를 눌러도 아무 반응이 없는 "먹통 키보드"가 된다.
 * 호스트가 살아있음을 주기적으로 증명하게 하는 편이 안전하다.
 *
 * 3초: 1초 주기 PING 을 3번 놓치면 끊긴 것으로 본다. 너무 짧으면
 * 호스트가 잠깐 바쁠 때 오탐하고, 너무 길면 먹통 시간이 길어진다. */
#define CUIME_RAW_HEARTBEAT_MS 3000

/* HID 리포트 디스크립터 — 위 주의사항을 반영한 정본.
 * 이 배열을 그대로 쓰면 3개 OS 에서 인식된다. */
#define CUIME_RAW_HID_REPORT_DESC()                                  \
    0x06, 0x60, 0xFF,  /* Usage Page (Vendor Defined 0xFF60)      */ \
    0x09, 0x61,        /* Usage (0x61) — 0 이면 macOS 가 무시한다 */ \
    0xA1, 0x01,        /* Collection (Application) — 필수         */ \
    0x09, 0x62,        /*   Usage (0x62) — 디바이스->호스트       */ \
    0x15, 0x00,        /*   Logical Minimum (0)                   */ \
    0x26, 0xFF, 0x00,  /*   Logical Maximum (255)                 */ \
    0x75, 0x08,        /*   Report Size (8 bits)                  */ \
    0x95, 0x20,        /*   Report Count (32 bytes)               */ \
    0x81, 0x02,        /*   Input (Data, Var, Abs)                */ \
    0x09, 0x63,        /*   Usage (0x63) — 호스트->디바이스       */ \
    0x15, 0x00,                                                      \
    0x26, 0xFF, 0x00,                                                \
    0x75, 0x08,                                                      \
    0x95, 0x20,                                                      \
    0x91, 0x02,        /*   Output (Data, Var, Abs)               */ \
    0xC0               /* End Collection                          */

#endif /* CUIME_RAWHID_H */
