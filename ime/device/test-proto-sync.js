/* 프로토콜 상수 동기화 검증 — 펌웨어 C 헤더 vs JS 브리지
 *
 * 두 곳에 같은 숫자를 적어두면 언젠가 반드시 어긋난다. 어긋나도
 * 컴파일은 되고 테스트도 통과하며, 실기기에서만 조용히 오작동한다.
 * (예: CMD_PING 값이 다르면 하트비트가 안 통해 3초마다 폴백)
 *
 * 그래서 C 헤더를 파싱해 JS 상수와 기계적으로 대조한다.
 */
'use strict';
const fs = require('fs');
const path = require('path');

const HDR = path.join(__dirname, '..', '..', 'firmware', 'hal', 'cuime_rawhid.h');
const D = require('./bridge.js');

let fails = 0;
function chk(name, got, want) {
  if (got === want) console.log(`✅ ${name} = ${want}`);
  else { console.log(`❌ ${name}  JS=${got} C=${want}`); fails++; }
}

const src = fs.readFileSync(HDR, 'utf8');

/** #define NAME 0x1234 형태를 뽑는다 */
function defineOf(name) {
  const m = src.match(new RegExp('#define\\s+' + name + '\\s+(0x[0-9A-Fa-f]+|\\d+)'));
  return m ? Number(m[1]) : undefined;
}
/** enum 안의 NAME = 0x12 형태를 뽑는다 */
function enumOf(name) {
  const m = src.match(new RegExp(name + '\\s*=\\s*(0x[0-9A-Fa-f]+|\\d+)'));
  return m ? Number(m[1]) : undefined;
}

console.log('── 프로토콜 상수 동기화 (C 헤더 ↔ JS) ──');

chk('USAGE_PAGE',  D.USAGE_PAGE,  defineOf('CUIME_RAW_USAGE_PAGE'));
chk('USAGE_ID',    D.USAGE_ID,    defineOf('CUIME_RAW_USAGE_ID'));
chk('REPORT_SIZE', D.REPORT_SIZE, defineOf('CUIME_RAW_REPORT_SIZE'));
chk('PROTO_VER',   D.PROTO_VER,   defineOf('CUIME_RAW_PROTO_VER'));

chk('EV_KEY',   D.EV_KEY,   enumOf('CUIME_RAW_EV_KEY'));
chk('EV_HELLO', D.EV_HELLO, enumOf('CUIME_RAW_EV_HELLO'));
chk('EV_PONG',  D.EV_PONG,  enumOf('CUIME_RAW_EV_PONG'));

chk('CMD_HELLO', D.CMD_HELLO, enumOf('CUIME_RAW_CMD_HELLO'));
chk('CMD_BYE',   D.CMD_BYE,   enumOf('CUIME_RAW_CMD_BYE'));
chk('CMD_PING',  D.CMD_PING,  enumOf('CUIME_RAW_CMD_PING'));
chk('CMD_LED',   D.CMD_LED,   enumOf('CUIME_RAW_CMD_LED'));

/* 하트비트 주기가 펌웨어 타임아웃보다 충분히 짧아야 한다.
 * 같거나 크면 정상 동작 중에도 폴백이 튄다. */
{
  const timeout = defineOf('CUIME_RAW_HEARTBEAT_MS');
  const src2 = fs.readFileSync(path.join(__dirname, 'bridge.js'), 'utf8');
  const m = src2.match(/PING_INTERVAL_MS\s*=\s*(\d+)/);
  const interval = m ? Number(m[1]) : undefined;
  if (interval && timeout && interval * 3 <= timeout) {
    console.log(`✅ 하트비트 여유 (주기 ${interval}ms x3 <= 타임아웃 ${timeout}ms)`);
  } else {
    console.log(`❌ 하트비트 주기(${interval}ms)가 타임아웃(${timeout}ms) 대비 부족`);
    fails++;
  }
}

/* 키 개수도 대조 — 펌웨어 enum 과 JS 배열 길이 */
{
  const core = fs.readFileSync(
    path.join(__dirname, '..', '..', 'firmware', 'core', 'cuime.h'), 'utf8');
  const body = core.slice(core.indexOf('typedef enum'), core.indexOf('CUIME_KEY__COUNT'));
  const n = (body.match(/CUIME_KEY_[A-Z0-9]+\s*(?:=\s*\d+)?\s*,/g) || []).length;
  chk('키 개수', D.KEY_NAMES.length, n);
}

console.log('\n' + (fails === 0
  ? '✅ 프로토콜 상수 완전 동기화'
  : `❌ 불일치 ${fails}건 — 펌웨어와 IME 가 통신하지 못한다`));
process.exit(fails ? 1 : 0);
