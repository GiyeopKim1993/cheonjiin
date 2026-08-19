#!/usr/bin/env node
/*!
 * 천지인 물리 키보드 데몬
 *
 * USB 로 연결된 천지인 키보드에서 원시 키 이벤트를 받아 조합하고,
 * 결과를 표준 출력에 보여준다. (OS 텍스트 주입은 플랫폼 IME 가 담당)
 *
 * 사용법:
 *   node cuime-device.js            # 장치 연결 대기 후 조합 표시
 *   node cuime-device.js --list     # 연결된 HID 장치 목록
 *   node cuime-device.js --demo     # 장치 없이 동작 시연 (가짜 입력)
 *
 * node-hid 가 필요하다:  npm i node-hid
 * (--demo 는 node-hid 없이도 동작한다)
 */
'use strict';
const path = require('path');

function loadShell() {
  const Cheonjiin = require('../core/cheonjiin.js');
  const HanjaDict = require('../core/hanja.js');
  // shell.js 는 require('./hanja.js') 를 스스로 한다
  const Shell = require('../core/shell.js');
  return { Cheonjiin, HanjaDict, Shell };
}

function render(v) {
  /* view().text 는 committed + preedit 을 합친 값이다.
   * text 와 preedit 을 둘 다 찍으면 조합 중 글자가 두 번 나온다.
   * 확정분(committed)만 그대로 쓰고 조합 중인 부분만 대괄호로 감싼다. */
  let line = v.committed || '';
  if (v.preedit) line += `[${v.preedit}]`;
  if (v.candidates && v.candidates.length) {
    const list = v.candidates
      .map((c, i) => (i === v.candIndex ? `<${c}>` : c)).join(' ');
    line += `   후보: ${list}`;
  }
  if (v.paletteOpen) line += `   기호: ${v.symbols.slice(0, 8).join(' ')}…`;
  process.stdout.write('\r\x1b[2K' + line);
}

function main() {
  const args = process.argv.slice(2);

  if (args.includes('--list')) {
    let HID;
    try { HID = require('node-hid'); }
    catch (e) { console.error('node-hid 가 없다:  npm i node-hid'); process.exit(1); }
    const T = require('./transport-hid.js');
    console.log('연결된 HID 장치:');
    for (const d of HID.devices()) {
      const mark = (d.vendorId === T.DEFAULT_VID && d.productId === T.DEFAULT_PID
                    && d.usagePage === T.USAGE_PAGE) ? '  ← 천지인 커스텀 HID' : '';
      console.log(`  VID=0x${(d.vendorId||0).toString(16).padStart(4,'0')}` +
                  ` PID=0x${(d.productId||0).toString(16).padStart(4,'0')}` +
                  ` usagePage=0x${(d.usagePage||0).toString(16)}` +
                  ` ${d.product || ''}${mark}`);
    }
    return;
  }

  const { Shell } = loadShell();
  const Device = require('./bridge.js');
  const shell = Shell.create({});
  shell.on(render);

  if (args.includes('--demo')) {
    /* 장치 없이 동작을 보여준다. 가짜 전송 계층으로 '한글' 을 입력한다. */
    console.log('데모: 가짜 장치로 "한글" 입력 (실제 USB 불필요)\n');
    const listeners = [];
    const t = {
      on(cb) { listeners.push(cb); return () => {}; },
      send() {}, close() {}
    };
    const br = Device.attach(shell, t, { autoPing: false });
    const hello = new Uint8Array(Device.REPORT_SIZE);
    hello[0] = Device.PROTO_VER; hello[1] = Device.EV_HELLO;
    hello[2] = Device.PROTO_VER; hello[3] = 16;
    'demo'.split('').forEach((c, i) => { hello[4 + i] = c.charCodeAt(0); });
    listeners.forEach((f) => f(hello));

    const KI = {}; Device.KEY_NAMES.forEach((n, i) => { KI[n] = i; });
    const Cheonjiin = require('../core/cheonjiin.js');
    const seq = [];
    for (const ch of '한글') {
      if (seq.length) seq.push(null);              // 글자 사이 간격
      for (const k of Cheonjiin.encode(ch, false)) seq.push(k);
    }
    let i = 0;
    const tick = () => {
      if (i >= seq.length) {
        process.stdout.write('\n\n완료. 실제 장치로 쓰려면 npm i node-hid 후 인자 없이 실행.\n');
        br.detach();
        return;
      }
      const k = seq[i++];
      if (k === null) { setTimeout(tick, 900); return; }   // 멀티탭 타임아웃 유도
      const push = (pressed) => {
        const b = new Uint8Array(Device.REPORT_SIZE);
        b[0] = Device.PROTO_VER; b[1] = Device.EV_KEY;
        b[2] = KI[k]; b[3] = pressed ? 1 : 0;
        listeners.forEach((f) => f(b));
      };
      push(true); push(false);
      setTimeout(tick, 180);
    };
    tick();
    return;
  }

  // ── 실제 장치 ──
  let T;
  try { T = require('./transport-hid.js'); require('node-hid'); }
  catch (e) {
    console.error('node-hid 가 필요하다:  npm i node-hid');
    console.error('장치 없이 확인하려면:  node cuime-device.js --demo');
    process.exit(1);
  }

  let transport;
  try {
    transport = T.open({ onError: (e) => {
      process.stdout.write(`\n장치 오류: ${e.message}\n`);
      process.exit(1);
    }});
  } catch (e) {
    console.error(e.message);
    process.exit(1);
  }

  const br = Device.attach(shell, transport, {
    onStatus: (s) => {
      if (s.status === 'connected') {
        console.log(`연결됨: ${s.name} (키 ${s.keyCount}개)`);
        console.log('키를 누르면 조합 결과가 보인다. Ctrl+C 로 종료.\n');
      } else if (s.status === 'version-mismatch') {
        console.error(`프로토콜 버전 불일치: 장치 v${s.deviceVer} / 호스트 v${s.hostVer}`);
        console.error('펌웨어나 IME 를 맞춰 업데이트해라.');
        process.exit(1);
      }
    }
  });

  const bye = () => {
    br.detach();                 // BYE 를 보내 장치를 두벌식 폴백으로 되돌린다
    process.stdout.write('\n종료. 장치는 두벌식 모드로 복귀했다.\n');
    process.exit(0);
  };
  process.on('SIGINT', bye);
  process.on('SIGTERM', bye);
}

main();
