/*!
 * 천지인 물리 키보드 브리지 — 원시 키 이벤트 -> IME 셸
 *
 * 디바이스는 "몇 번 키가 눌렸다/떨어졌다"만 보낸다. 조합·멀티탭 타이머·
 * 롱프레스 판정·프리에딧·한자 후보는 전부 여기(호스트)서 한다.
 *
 * 이 파일은 **전송 계층을 모른다**. USB HID든 BLE든 테스트용 가짜든,
 * { on(cb), send(buf), close() } 인터페이스만 만족하면 붙는다.
 * 그래서 실기기 없이도 전 로직을 검증할 수 있다.
 *
 * 프로토콜: firmware/hal/cuime_rawhid.h 와 반드시 일치해야 한다.
 */
(function (root, factory) {
  const api = factory();
  if (typeof module !== 'undefined' && module.exports) module.exports = api;
  if (typeof window !== 'undefined') window.CheonjiinDevice = api;
})(this, function () {
  'use strict';

  // ── 프로토콜 상수 (firmware/hal/cuime_rawhid.h 와 동일) ──
  const PROTO_VER   = 1;
  const REPORT_SIZE = 32;
  const USAGE_PAGE  = 0xFF60;
  const USAGE_ID    = 0x61;

  // 디바이스 -> 호스트
  const EV_KEY   = 0x01;
  const EV_HELLO = 0x02;
  const EV_PONG  = 0x03;

  // 호스트 -> 디바이스
  const CMD_HELLO = 0x81;
  const CMD_BYE   = 0x82;
  const CMD_PING  = 0x83;
  const CMD_LED   = 0x84;

  const PING_INTERVAL_MS = 1000;   // 펌웨어 타임아웃 3초의 1/3

  /* 펌웨어 cuime_key_t 순서. firmware/core/cuime.h 와 동일해야 한다.
   * 이 배열이 어긋나면 엉뚱한 키가 입력된다 — 테스트로 대조한다. */
  const KEY_NAMES = [
    'K1', 'K2', 'K3', 'K4', 'K5', 'K6', 'K7', 'K8', 'K9', 'K0',
    'KLEFT', 'KRIGHT', 'BACK', 'ENTER', 'SPACE', 'HANJA'
  ];

  /** 리포트 1개를 해석한다. 알 수 없으면 null. */
  function decode(buf) {
    if (!buf || buf.length < 4) return null;
    if (buf[0] !== PROTO_VER) return null;         // 버전 불일치는 버린다

    switch (buf[1]) {
      case EV_KEY: {
        const idx = buf[2];
        if (idx >= KEY_NAMES.length) return null;  // 범위 밖 키 방어
        // 타임스탬프는 리틀엔디언 32비트. 디바이스 기준 시각이라
        // USB 지연이 흔들려도 멀티탭 간격을 정확히 잴 수 있다.
        const ts = (buf[4] | (buf[5] << 8) | (buf[6] << 16) | (buf[7] << 24)) >>> 0;
        return { type: 'key', key: KEY_NAMES[idx], pressed: buf[3] !== 0, ts };
      }
      case EV_HELLO:
        return {
          type: 'hello',
          protoVer: buf[2],
          keyCount: buf[3],
          name: String.fromCharCode(...Array.from(buf.slice(4, 20)).filter((c) => c > 0))
        };
      case EV_PONG:
        return { type: 'pong', nonce: (buf[2] | (buf[3] << 8)) >>> 0 };
      default:
        return null;
    }
  }

  /** 호스트 -> 디바이스 리포트를 만든다. 항상 32바이트 고정. */
  function encode(cmd, payload) {
    const b = new Uint8Array(REPORT_SIZE);
    b[0] = PROTO_VER;
    b[1] = cmd;
    if (payload) for (let i = 0; i < payload.length && i + 2 < REPORT_SIZE; i++) {
      b[2 + i] = payload[i];
    }
    return b;
  }

  /**
   * 브리지 생성.
   *
   * @param shell     CheonjiinShell.create(...) 인스턴스
   * @param transport { on(cb), send(buf), close() }
   * @param opts      { onStatus, autoPing }
   */
  function attach(shell, transport, opts) {
    opts = opts || {};
    const state = {
      connected: false,
      deviceName: null,
      protoVer: null,
      lastPong: 0,
      pingTimer: null,
      nonce: 0,
      /* 눌린 키 추적. 뗌 없이 다음 눌림이 오면(패킷 유실) 정리해야
       * 롱프레스 타이머가 영영 남는 사고를 막는다. */
      down: new Set()
    };

    const status = (s, extra) => {
      if (opts.onStatus) opts.onStatus(Object.assign({ status: s }, extra || {}));
    };

    function handle(buf) {
      const msg = decode(buf);
      if (!msg) return;              // 모르는 리포트는 조용히 무시

      switch (msg.type) {
        case 'hello':
          state.connected = true;
          state.deviceName = msg.name;
          state.protoVer = msg.protoVer;
          state.lastPong = Date.now();
          if (msg.protoVer !== PROTO_VER) {
            // 버전이 다르면 키 배열이 바뀌었을 수 있다. 붙이지 않는다.
            status('version-mismatch', { deviceVer: msg.protoVer, hostVer: PROTO_VER });
            state.connected = false;
            return;
          }
          status('connected', { name: msg.name, keyCount: msg.keyCount });
          break;

        case 'pong':
          state.lastPong = Date.now();
          break;

        case 'key':
          if (!state.connected) return;
          if (msg.pressed) {
            /* 같은 키가 뗌 없이 또 눌리면 이전 눌림을 먼저 닫는다.
             * (USB 패킷 유실 시 롱프레스 타이머가 남는 것 방지) */
            if (state.down.has(msg.key)) shell.keyUp(msg.key);
            state.down.add(msg.key);
            dispatchDown(msg.key);
          } else {
            if (!state.down.has(msg.key)) return;   // 짝 없는 뗌은 무시
            state.down.delete(msg.key);
            dispatchUp(msg.key);
          }
          break;
      }
    }

    /* 漢 키는 셸에서 별도 API 다 (누를 때/뗄 때 판정이 다름) */
    function dispatchDown(key) {
      if (key === 'HANJA') { shell.hanjaDown(); return; }
      if (key === 'BACK')  { shell.backspace(); return; }
      if (key === 'ENTER') { shell.enter(); return; }
      if (key === 'SPACE') { shell.space(); return; }
      if (key === 'KLEFT')  { shell.arrow(-1); return; }
      if (key === 'KRIGHT') { shell.arrow(+1); return; }
      shell.keyDown(key);          // 글자키만 롱프레스 타이머 대상
    }

    function dispatchUp(key) {
      if (key === 'HANJA') { shell.hanjaUp(); return; }
      // 나머지 제어키는 뗄 때 할 일이 없다
      if (['BACK', 'ENTER', 'SPACE', 'KLEFT', 'KRIGHT'].indexOf(key) >= 0) return;
      shell.keyUp(key);
    }

    // 전송 계층 구독
    const unsub = transport.on(handle);

    // 접속 인사 — 디바이스가 이걸 받아야 RAW 모드로 전환한다
    transport.send(encode(CMD_HELLO));

    // 하트비트. 이게 끊기면 디바이스가 두벌식 폴백으로 돌아간다.
    if (opts.autoPing !== false) {
      state.pingTimer = setInterval(() => {
        state.nonce = (state.nonce + 1) & 0xFFFF;
        transport.send(encode(CMD_PING, [state.nonce & 0xFF, state.nonce >> 8]));
      }, PING_INTERVAL_MS);
      if (state.pingTimer.unref) state.pingTimer.unref();
    }

    return {
      get connected() { return state.connected; },
      get deviceName() { return state.deviceName; },

      /** LED 제어 (0 소등 / 1 조합중 / 2 후보열림) */
      setLed(v) { transport.send(encode(CMD_LED, [v & 0xFF])); },

      /** 정상 종료 — BYE 를 보내 디바이스를 즉시 폴백시킨다 */
      detach() {
        if (state.pingTimer) clearInterval(state.pingTimer);
        // 눌린 채 남은 키를 정리하지 않으면 셸에 타이머가 남는다
        state.down.forEach((k) => dispatchUp(k));
        state.down.clear();
        try { transport.send(encode(CMD_BYE)); } catch (e) { /* 이미 끊김 */ }
        if (unsub) unsub();
        if (transport.close) transport.close();
        state.connected = false;
        status('detached');
      },

      _state: state
    };
  }

  return {
    attach, decode, encode,
    PROTO_VER, REPORT_SIZE, USAGE_PAGE, USAGE_ID, KEY_NAMES,
    EV_KEY, EV_HELLO, EV_PONG,
    CMD_HELLO, CMD_BYE, CMD_PING, CMD_LED
  };
});
