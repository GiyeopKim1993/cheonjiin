/* USB HID 전송 계층 검증 — node-hid 설치 없이
 *
 * 가짜 HID 를 주입해 장치 선택 로직을 검증한다. 핵심은 복합장치에서
 * **부트 키보드가 아니라 커스텀 HID 인터페이스를** 골라야 한다는 것.
 * 둘은 VID/PID 가 같아서 그것만으로는 구분되지 않는다.
 */
'use strict';
const T = require('./transport-hid.js');
let fails=0;
const chk=(n,g,w)=>{const a=JSON.stringify(g),b=JSON.stringify(w);
  if(a===b)console.log('✅ '+n);else{console.log(`❌ ${n}\n   got =${a}\n   want=${b}`);fails++;}};

// 복합장치를 흉내 낸다: 같은 VID/PID 로 부트 키보드 + 커스텀 HID 둘 다 잡힘
const fakeHID = (devs, openFails) => ({
  devices: () => devs,
  HID: function(path){ if(openFails) throw new Error('permission denied');
                       this.path=path; this.on=()=>{}; this.write=()=>{}; this.close=()=>{}; }
});
const BOOT = { vendorId:0x1209, productId:0xCE01, usagePage:0x0001, usage:0x06, path:'boot' };
const RAW  = { vendorId:0x1209, productId:0xCE01, usagePage:0xFF60, usage:0x61, path:'raw'  };
const OTHER= { vendorId:0x046d, productId:0xc31c, usagePage:0x0001, usage:0x06, path:'logi' };

console.log('── USB HID 전송 계층 검증 ──');

// 1. 복합장치에서 커스텀 HID 만 골라야 한다 (부트 키보드를 열면 안 됨)
{
  const r = T.findDevice(fakeHID([OTHER, BOOT, RAW]), {});
  chk('복합장치에서 커스텀 HID 선택', r.device.path, 'raw');
}
// 순서가 바뀌어도 동일
{
  const r = T.findDevice(fakeHID([RAW, BOOT]), {});
  chk('순서 무관하게 커스텀 HID 선택', r.device.path, 'raw');
}
// 2. 부트 키보드만 있으면 명확한 오류
{
  try { T.findDevice(fakeHID([BOOT]), {}); chk('부트만 있을 때 오류',false,true); }
  catch(e){ chk('부트만 있으면 NO_RAW_INTERFACE', e.code, 'NO_RAW_INTERFACE');
            chk('오류에 원인 안내 포함', /입력 모니터링|RAWHID/.test(e.message), true); }
}
// 3. 장치 없음
{
  try { T.findDevice(fakeHID([OTHER]), {}); chk('장치없음 오류',false,true); }
  catch(e){ chk('장치 없으면 NOT_FOUND', e.code, 'NOT_FOUND'); }
}
// 4. 권한 거부 (macOS 시나리오)
{
  try { T.open({ HID: fakeHID([RAW], true) }); chk('권한거부 오류',false,true); }
  catch(e){ chk('열기 실패 시 OPEN_FAILED', e.code, 'OPEN_FAILED');
            chk('macOS 권한 안내 포함', /입력 모니터링/.test(e.message), true); }
}
// 5. send 가 리포트 ID 0 을 앞에 붙이는가
{
  let written=null;
  const H = { devices:()=>[RAW],
    HID: function(){ this.on=()=>{}; this.write=(a)=>{written=a;}; this.close=()=>{}; } };
  const tr = T.open({ HID:H });
  tr.send(new Uint8Array([1,0x81,0,0]));
  chk('리포트 ID 0 선행', written.slice(0,3), [0,1,0x81]);
  chk('길이 = 원본+1', written.length, 5);
}
console.log('\n'+(fails===0?'✅ 전송 계층 전 항목 통과':`❌ 실패 ${fails}건`));
process.exit(fails?1:0);
