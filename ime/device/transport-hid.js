/*!
 * USB HID 전송 계층 — node-hid 기반
 *
 * bridge.js 가 요구하는 { on, send, close } 를 구현한다.
 * 브리지는 이 파일을 몰라도 되고, 이 파일은 조합 로직을 모른다.
 *
 * ── 장치 찾기 ──
 * VID/PID 만으로 열면 안 된다. 복합장치라 부트 키보드와 커스텀 HID 가
 * 같은 VID/PID 로 **둘 다** 잡힌다(Windows 는 MI_00/MI_01 로 분리).
 * 부트 키보드를 잘못 열면 키 이벤트가 안 온다.
 * 반드시 usagePage/usage 까지 걸러야 한다.
 *
 * ── 플랫폼 주의 ──
 * macOS: Catalina 부터 HID 접근에 "입력 모니터링" 권한이 필요하다.
 *        권한이 없으면 open 이 실패한다 — 그 경우 폴백을 안내한다.
 * Linux: hidraw 노드 권한이 필요하다. udev 규칙 예시는 README 참조.
 */
'use strict';

const DEFAULT_VID = 0x1209;   // pid.codes (오픈소스 공용)
const DEFAULT_PID = 0xCE01;
const USAGE_PAGE  = 0xFF60;
const USAGE_ID    = 0x61;

/** 커스텀 HID 인터페이스만 골라낸다. */
function findDevice(HID, opts) {
  opts = opts || {};
  const vid = opts.vendorId  || DEFAULT_VID;
  const pid = opts.productId || DEFAULT_PID;

  const all = HID.devices();
  const matches = all.filter((d) =>
    d.vendorId === vid && d.productId === pid &&
    d.usagePage === USAGE_PAGE && d.usage === USAGE_ID);

  if (matches.length) return { device: matches[0], all };

  /* 못 찾은 이유를 구분해 알려준다 — "안 됨" 만으로는 고칠 수 없다. */
  const sameId = all.filter((d) => d.vendorId === vid && d.productId === pid);
  if (sameId.length) {
    const err = new Error(
      `장치는 있으나 커스텀 HID 인터페이스가 없다 ` +
      `(usagePage 0x${USAGE_PAGE.toString(16)}/usage 0x${USAGE_ID.toString(16)}).\n` +
      `  - RAWHID=1 로 빌드한 펌웨어인지 확인\n` +
      `  - macOS 라면 '입력 모니터링' 권한 확인\n` +
      `  발견된 인터페이스: ` +
      sameId.map((d) => `usagePage=0x${(d.usagePage || 0).toString(16)}`).join(', '));
    err.code = 'NO_RAW_INTERFACE';
    throw err;
  }
  const err = new Error(
    `천지인 키보드를 찾지 못했다 (VID 0x${vid.toString(16)} / PID 0x${pid.toString(16)}).`);
  err.code = 'NOT_FOUND';
  throw err;
}

/**
 * 전송 계층 생성.
 * @param opts { vendorId, productId, HID }  HID 는 테스트 주입용
 */
function open(opts) {
  opts = opts || {};
  const HID = opts.HID || require('node-hid');
  const { device } = findDevice(HID, opts);

  let dev;
  try {
    dev = new HID.HID(device.path);
  } catch (e) {
    /* macOS 권한 거부가 여기로 온다. 원인을 명시해 폴백을 유도한다. */
    const err = new Error(
      `장치를 열지 못했다: ${e.message}\n` +
      `  macOS: 시스템 설정 > 개인정보 보호 > 입력 모니터링 에서 허용\n` +
      `  Linux: hidraw 권한 (udev 규칙) 확인\n` +
      `  허용 전까지는 디바이스가 두벌식 폴백으로 동작한다.`);
    err.code = 'OPEN_FAILED';
    err.cause = e;
    throw err;
  }

  const listeners = [];
  dev.on('data', (buf) => { listeners.forEach((f) => f(buf)); });
  dev.on('error', (e) => {
    /* 케이블이 빠지면 여기로 온다. 조용히 죽지 않도록 통지한다. */
    if (opts.onError) opts.onError(e);
  });

  return {
    devicePath: device.path,
    on(cb) {
      listeners.push(cb);
      return () => listeners.splice(listeners.indexOf(cb), 1);
    },
    send(buf) {
      /* node-hid 의 write 는 첫 바이트를 리포트 ID 로 본다.
       * 우리는 리포트 ID 를 쓰지 않으므로 0 을 앞에 붙인다.
       * (이걸 빼면 첫 바이트가 잘려 프로토콜 버전이 깨진다) */
      const out = Buffer.alloc(buf.length + 1);
      out[0] = 0;
      Buffer.from(buf).copy(out, 1);
      dev.write(Array.from(out));
    },
    close() {
      try { dev.close(); } catch (e) { /* 이미 닫힘 */ }
    }
  };
}

module.exports = { open, findDevice, DEFAULT_VID, DEFAULT_PID, USAGE_PAGE, USAGE_ID };
