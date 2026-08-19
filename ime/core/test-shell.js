/* 셸 계층 검증 — 漢 판정, 코드, M1 가드, 레이아웃 (docs/06 §8, 07 §2.3) */
const S = require('./shell.js');
let fails = [];
const chk = (n, g, e) => {
  const ok = JSON.stringify(g) === JSON.stringify(e);
  console.log((ok ? '✅ ' : '❌ ') + n + (ok ? '' : `  got=${JSON.stringify(g)} exp=${JSON.stringify(e)}`));
  if (!ok) fails.push(n);
};
const mk = (o) => S.create(Object.assign({ longpressMs: 10000, multitapMs: 10000 }, o));

/* R1 — 블록 선택 시 한자 변환 */
let a = mk();
a.setSelection('대한민국', true);
a.hanjaDown(); a.hanjaUp();
chk('R1 단어 블록 → 한자 후보', a.view().candidates, ['大韓民國']);
a.pickCandidate(0);
chk('R1 후보 적용', a.view().text, '大韓民國');

let b = mk();
b.setSelection('국', true);
b.hanjaDown(); b.hanjaUp();
chk('R1 글자 블록 → 다중 후보', b.view().candidates, ['國','局','菊','鞠','鞫','麴']);
b.arrow(1); b.arrow(1);
chk('R1 방향키 후보 탐색', b.view().candIndex, 2);
b.pickCandidate(b.view().candIndex);
chk('R1 후보 확정', b.view().text, '菊');

/* R2 — 선택 없으면 기호 팔레트 */
let c = mk();
c.hanjaDown(); c.hanjaUp();
chk('R2 선택 없음 → 기호 팔레트', c.view().paletteOpen, true);
c.arrow(1);
chk('R2 방향키 → 페이지 전환', c.view().palettePage, 1);
c.pickSymbol('★', false);
chk('R2 기호 입력', c.view().text, '★');
chk('R2 팔레트 닫힘', c.view().paletteOpen, false);

/* R3 — 코드로 레이아웃 전환 */
let d = mk();
d.hanjaDown(); d.arrow(1); d.hanjaUp();
chk('R3 漢+▶ → 영어', d.view().layout, '영어');
chk('R3 코드 후 팔레트 미발생 (hjConsumed)', d.view().paletteOpen, false);
d.hanjaDown(); d.arrow(1); d.hanjaUp();
chk('R3 → 숫자', d.view().layout, '숫자');
d.hanjaDown(); d.arrow(1); d.hanjaUp();
chk('R3 3회 순환 → 한글 복귀', d.view().layout, '한글');
d.hanjaDown(); d.arrow(-1); d.hanjaUp();
chk('R3 역방향 → 숫자', d.view().layout, '숫자');

/* 우선순위: 선택 중에도 코드 우선 */
let e = mk();
e.setSelection('국', true);
e.hanjaDown(); e.arrow(1); e.hanjaUp();
chk('우선순위 코드 > 한자변환', [e.view().layout, e.view().candidates], ['영어', null]);

/* 한글 입력 + 롱프레스 */
let f = mk();
['K4', 'K1', 'K2'].forEach((k) => f.tap(k));
chk('한글 조합 가', f.view().text, '가');
f.longPress('K4');
chk('롱프레스 ㄲ (순환 마지막)', f.view().text, '갂');   // 가 + ㄲ받침
let g = mk();
g.longPress('K7'); g.tap('K1'); g.tap('K2');
chk('롱프레스 ㅃ 초성', g.view().text, '빠');

/* 영어 레이아웃 E.161 */
let h = mk();
h.hanjaDown(); h.arrow(1); h.hanjaUp();
h.tap('K7'); h.tap('K7'); h.tap('K7'); h.tap('K7');
chk('영어 PQRS 4탭 → s', h.view().text, 's');
let i2 = mk();
i2.hanjaDown(); i2.arrow(1); i2.hanjaUp();
i2.longPress('K9');
chk('영어 롱프레스 → z (순환 마지막)', i2.view().text, 'z');
let i3 = mk();
i3.hanjaDown(); i3.arrow(1); i3.hanjaUp();
i3.tap('K0'); i3.tap('K2');
chk('영어 shift → A', i3.view().text, 'A');

/* 숫자 레이아웃 */
let j = mk();
j.hanjaDown(); j.arrow(-1); j.hanjaUp();
chk('숫자 레이아웃 진입', j.view().layout, '숫자');
['K0', 'K1', 'K0', 'K1', 'K2', 'K3', 'K4'].forEach((k) => j.tap(k));
chk('숫자 연속 입력', j.view().text, '0101234');

/* M1 전송 가드 (docs/07 §2.3) */
let m = mk({ enterIsSend: true, sendGuardMs: 300 });
['K4', 'K1', 'K2'].forEach((k) => m.tap(k));
m.backspace();
m.enter();
chk('M1 ⌫ 직후 엔터 차단', m.view().text.indexOf('\n'), -1);
chk('M1 경고 표시', m.view().message.indexOf('전송 차단') >= 0, true);
m.enter();
chk('M1 두 번째 엔터는 통과', m.view().text.indexOf('\n') >= 0, true);

let m2 = mk({ enterIsSend: false });
['K4', 'K1', 'K2'].forEach((k) => m2.tap(k));
m2.backspace(); m2.enter();
chk('M1 비전송 컨텍스트는 가드 없음', m2.view().text.indexOf('\n') >= 0, true);

/* 방향키 = 조합 확정 */
let n = mk();
n.tap('K4'); n.arrow(1); n.tap('K4'); n.tap('K1'); n.tap('K2');
chk('▶ 확정 → ㄱ가', n.view().text, 'ㄱ가');

console.log('\n' + (fails.length === 0
  ? `✅ 셸 전 항목 통과 (${28 - fails.length}/28)`
  : `❌ 실패 ${fails.length}건: ${fails.join(', ')}`));
process.exit(fails.length ? 1 : 0);
