/* 물리 키보드 브리지 검증 — 실기기 없이
 *
 * 가짜 전송 계층으로 펌웨어가 보낼 리포트를 그대로 흉내 내고,
 * IME 셸까지 통과시켜 **실제로 한글이 조합되는지** 확인한다.
 *
 * 여기서 잡아야 하는 것:
 *   - 키 배열이 펌웨어와 어긋남 (엉뚱한 글자가 나옴)
 *   - 프레스/릴리즈 짝이 깨짐 (롱프레스 타이머가 영영 남음)
 *   - 漢/방향키가 잘못된 셸 API 로 감 
 */
'use strict';
const fs = require('fs');
const path = require('path');
const vm = require('vm');

const ROOT = path.join(__dirname, '..');
const ctx = { console, setTimeout, clearTimeout, setInterval, clearInterval, Date };
ctx.window = ctx; ctx.globalThis = ctx; ctx.module = undefined;
vm.createContext(ctx);
/* 로드 순서 주의: shell.js 가 평가되는 시점에 window.HanjaDict 가 이미
 * 있어야 한자 사전이 잡힌다. hanja.js 를 먼저 넣는다.
 * (순서를 틀렸다가 漢 키 후보가 안 뜨는 것으로 나타났다) */
for (const f of ['core/cheonjiin.js', 'core/hanja.js', 'core/shell.js']) {
  vm.runInContext(fs.readFileSync(path.join(ROOT, f), 'utf8'), ctx, { filename: f });
}
vm.runInContext(fs.readFileSync(path.join(__dirname, 'bridge.js'), 'utf8'),
                ctx, { filename: 'bridge.js' });

const C = ctx.Cheonjiin, S = ctx.CheonjiinShell, D = ctx.CheonjiinDevice;

let fails = 0;
function chk(name, got, want) {
  const g = JSON.stringify(got), w = JSON.stringify(want);
  if (g === w) console.log('✅ ' + name);
  else { console.log(`❌ ${name}\n   got =${g}\n   want=${w}`); fails++; }
}

/* ── 가짜 전송 계층 ── */
function fakeTransport() {
  const t = {
    sent: [],
    _cb: null,
    on(cb) { t._cb = cb; return () => { t._cb = null; }; },
    send(buf) { t.sent.push(Array.from(buf)); },
    close() { t.closed = true; },
    /* 펌웨어가 보낼 리포트를 흉내 낸다 */
    devKey(keyIdx, pressed, ts) {
      const b = new Uint8Array(D.REPORT_SIZE);
      b[0] = D.PROTO_VER; b[1] = D.EV_KEY;
      b[2] = keyIdx; b[3] = pressed ? 1 : 0;
      ts = ts || 0;
      b[4] = ts & 0xFF; b[5] = (ts >> 8) & 0xFF;
      b[6] = (ts >> 16) & 0xFF; b[7] = (ts >> 24) & 0xFF;
      t._cb(b);
    },
    devHello(ver, name) {
      const b = new Uint8Array(D.REPORT_SIZE);
      b[0] = D.PROTO_VER; b[1] = D.EV_HELLO;
      b[2] = ver === undefined ? D.PROTO_VER : ver;
      b[3] = 16;
      const n = name || 'cuime-f411';
      for (let i = 0; i < n.length; i++) b[4 + i] = n.charCodeAt(i);
      t._cb(b);
    }
  };
  return t;
}

const KI = {};   // 키 이름 -> 인덱스
D.KEY_NAMES.forEach((n, i) => { KI[n] = i; });

console.log('── 물리 키보드 브리지 검증 ──');

/* ── 1. 키 배열이 펌웨어와 일치하는가 ──
 * cuime.h 의 enum 순서를 직접 읽어 대조한다. 손으로 맞춘 배열은 언젠가
 * 어긋나므로, 소스를 정본으로 삼는다. */
{
  const hdr = fs.readFileSync(
    path.join(ROOT, '..', 'firmware', 'core', 'cuime.h'), 'utf8');
  const body = hdr.slice(hdr.indexOf('typedef enum'), hdr.indexOf('CUIME_KEY__COUNT'));
  const order = [];
  for (const m of body.matchAll(/CUIME_KEY_([A-Z0-9]+)\s*(?:=\s*\d+)?\s*,/g)) {
    order.push(m[1]);
  }
  /* 펌웨어 enum 이름 -> 셸 키 이름.
   * 숫자키는 K 접두, 방향키는 셸이 KLEFT/KRIGHT 로 부른다(펌웨어는 LEFT/RIGHT).
   * 이름은 달라도 **순서**가 같아야 한다 — 인덱스로 통신하기 때문이다. */
  const RENAME = { LEFT: 'KLEFT', RIGHT: 'KRIGHT' };
  const expect = order.map((n) =>
    /^\d$/.test(n) ? 'K' + n : (RENAME[n] || n));
  chk('KEY_NAMES 가 펌웨어 enum 순서와 일치', D.KEY_NAMES, expect);
}

/* ── 2. 디코딩 ── */
{
  const t = fakeTransport();
  let got = null;
  t.on((b) => { got = D.decode(b); });
  t.devKey(KI.K4, true, 0x12345678);
  chk('키 눌림 디코딩', got, { type: 'key', key: 'K4', pressed: true, ts: 0x12345678 });
  t.devKey(KI.SPACE, false, 1000);
  chk('키 뗌 디코딩', got, { type: 'key', key: 'SPACE', pressed: false, ts: 1000 });

  // 잘못된 리포트 방어
  const bad = new Uint8Array(D.REPORT_SIZE); bad[0] = 99; bad[1] = D.EV_KEY;
  chk('버전 불일치 거부', D.decode(bad), null);
  const oor = new Uint8Array(D.REPORT_SIZE);
  oor[0] = D.PROTO_VER; oor[1] = D.EV_KEY; oor[2] = 200;
  chk('범위 밖 키 거부', D.decode(oor), null);
  chk('빈 버퍼 거부', D.decode(new Uint8Array(2)), null);
}

/* ── 3. 접속 절차 ── */
{
  const sh = S.create({});
  const t = fakeTransport();
  const events = [];
  const br = D.attach(sh, t, { autoPing: false, onStatus: (s) => events.push(s.status) });

  chk('접속 시 HELLO 전송', t.sent[0].slice(0, 2), [D.PROTO_VER, D.CMD_HELLO]);
  chk('HELLO 전 미접속', br.connected, false);
  t.devHello();
  chk('HELLO 수신 후 접속', br.connected, true);
  chk('디바이스 이름', br.deviceName, 'cuime-f411');
  chk('상태 이벤트', events, ['connected']);
}

/* ── 4. 버전 불일치는 붙지 않는다 ── */
{
  const sh = S.create({});
  const t = fakeTransport();
  const events = [];
  const br = D.attach(sh, t, { autoPing: false, onStatus: (s) => events.push(s.status) });
  t.devHello(99);
  chk('프로토콜 버전 다르면 미접속', br.connected, false);
  chk('version-mismatch 통지', events, ['version-mismatch']);
}

/* ── 5. 실제 조합 — 이게 핵심 ── */
function typeOn(br, t, seq) {
  for (const k of seq) {
    t.devKey(KI[k], true);
    t.devKey(KI[k], false);
  }
}
{
  const sh = S.create({});
  const t = fakeTransport();
  const br = D.attach(sh, t, { autoPing: false });
  t.devHello();

  typeOn(br, t, ['K4', 'K1', 'K2']);          // ㄱ + ㅣ + ㆍ
  chk('물리키 -> 조합 "가"', sh.view().preedit, '가');

  typeOn(br, t, ['SPACE']);
  chk('스페이스로 확정', sh.view().text, '가 ');
}

/* ── 6. 멀티탭 (같은 키 연타) ── */
{
  const sh = S.create({});
  const t = fakeTransport();
  D.attach(sh, t, { autoPing: false });
  t.devHello();
  typeOn(null, t, ['K8', 'K8']);              // ㅅ -> ㅎ
  chk('멀티탭 ㅅ->ㅎ', sh.view().preedit, 'ㅎ');
  typeOn(null, t, ['K1', 'K2', 'K5']);        // + ㅏ + ㄴ
  chk('멀티탭 조합 "한"', sh.view().preedit, '한');
}

/* ── 7. 백스페이스 역조합 ── */
{
  const sh = S.create({});
  const t = fakeTransport();
  D.attach(sh, t, { autoPing: false });
  t.devHello();
  typeOn(null, t, ['K4', 'K1', 'K2']);        // 가
  typeOn(null, t, ['BACK']);
  chk('⌫ 역조합 가->기', sh.view().preedit, '기');
}

/* ── 8. 漢 키는 별도 API 로 가야 한다 ── */
{
  const sh = S.create({});
  const t = fakeTransport();
  D.attach(sh, t, { autoPing: false });
  t.devHello();
  typeOn(null, t, ['K4', 'K3', 'K4']);        // 극? -> '국' 만들기
  // '국' = ㄱ(K4) ㅜ(K3+K2?) ... 대신 확실한 '국'을 코어로 만든다
  sh.reset();
  const seq = C.encode('국', false);          // 참조 구현이 알려주는 키 순서
  for (const k of seq) { t.devKey(KI[k], true); t.devKey(KI[k], false); }
  chk('"국" 조합', sh.view().preedit, '국');

  t.devKey(KI.HANJA, true);
  t.devKey(KI.HANJA, false);
  const v = sh.view();
  chk('漢 키로 후보 열림', Array.isArray(v.candidates) && v.candidates.length > 0, true);
  chk('첫 후보 國', v.candidates && v.candidates[0], '國');
}

/* ── 9. 패킷 유실 방어 — 뗌 없이 또 눌림 ── */
{
  const sh = S.create({});
  const t = fakeTransport();
  const br = D.attach(sh, t, { autoPing: false });
  t.devHello();
  t.devKey(KI.K4, true);
  t.devKey(KI.K4, true);            // 뗌 리포트 유실
  chk('중복 눌림 후 down 집합 정상', br._state.down.size, 1);
  t.devKey(KI.K4, false);
  chk('뗌 후 down 비워짐', br._state.down.size, 0);

  // 짝 없는 뗌은 무시되어야 한다
  const before = sh.view().text + '|' + sh.view().preedit;
  t.devKey(KI.K9, false);
  chk('짝 없는 뗌 무시', sh.view().text + '|' + sh.view().preedit, before);
}

/* ── 10. detach 정리 ── */
{
  const sh = S.create({});
  const t = fakeTransport();
  const br = D.attach(sh, t, { autoPing: false });
  t.devHello();
  t.devKey(KI.K4, true);            // 누른 채로 종료
  br.detach();
  chk('detach 시 BYE 전송',
      t.sent[t.sent.length - 1].slice(0, 2), [D.PROTO_VER, D.CMD_BYE]);
  chk('detach 후 미접속', br.connected, false);
  chk('detach 시 눌린 키 정리', br._state.down.size, 0);
  chk('전송 계층 닫힘', t.closed, true);
}

/* ── 11. LED 명령 ── */
{
  const sh = S.create({});
  const t = fakeTransport();
  const br = D.attach(sh, t, { autoPing: false });
  t.devHello();
  br.setLed(1);
  const last = t.sent[t.sent.length - 1];
  chk('LED 명령 형식', last.slice(0, 3), [D.PROTO_VER, D.CMD_LED, 1]);
}

console.log('\n' + (fails === 0
  ? '✅ 물리 키보드 브리지 전 항목 통과'
  : `❌ 실패 ${fails}건`));
process.exit(fails ? 1 : 0);
