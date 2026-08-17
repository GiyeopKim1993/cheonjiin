/* 천지인 코어 JS 포팅 검증 — Python 참조 구현과 동일 결과여야 한다 */
const C = require('./cheonjiin.js');
let fails = [];
const chk = (n, g, e) => {
  const ok = JSON.stringify(g) === JSON.stringify(e);
  if (!ok) fails.push({ n, got: g, exp: e });
  return ok;
};

/* 1) 중성 21자 */
for (const v of C.JUNG) {
  const seq = ['K0'].concat(C.vowelSeq(v));
  chk('중성 ' + v, C.typeKeys(seq), C.compose('ㅇ', v));
}
console.log('✅ 중성 21자');

/* 2) 초성 19자 (멀티탭) */
for (const c of C.CHO) {
  const [k, taps] = C.KEY_OF_JAMO[c];
  chk('초성 ' + c, C.typeKeys(new Array(taps).fill(k).concat(['K1', 'K2'])), C.compose(c, 'ㅏ'));
}
console.log('✅ 초성 19자');

/* 3) 롱프레스 = 순환열 마지막 (docs/07 §3.1) */
const LP = { K4: 'ㄲ', K5: 'ㄹ', K6: 'ㄸ', K7: 'ㅃ', K8: 'ㅆ', K9: 'ㅉ', K0: 'ㅁ' };
chk('롱프레스 매핑', C.LONGPRESS, LP);
for (const k in LP) {
  chk('롱프레스 ' + LP[k], C.typeKeys(['LONG_' + k, 'K1', 'K2']), C.compose(LP[k], 'ㅏ'));
}
console.log('✅ 롱프레스 7종');

/* 4) 겹받침 11종 */
for (const cl in C.JONG_SPLIT) {
  const ch = C.compose('ㄱ', 'ㅏ', cl);
  chk('겹받침 ' + cl, C.typeKeys(C.encode(ch, false)), ch);
  chk('겹받침(LP) ' + cl, C.typeKeys(C.encode(ch, true)), ch);
}
console.log('✅ 겹받침 11종 (멀티탭/롱프레스 양쪽)');

/* 5) 백스페이스 자모 역추적 */
const bs = (keys) => {
  let s = C.newState();
  for (const k of keys) s = C.press(s, k);
  return C.flush(C.press(s, 'BACK')).committed;
};
chk('각⌫→가', bs(['K4', 'K1', 'K2', 'K5']), '가');
chk('괴⌫→고', bs(['K4', 'K2', 'K3', 'K1']), '고');
chk('갃⌫→각', bs(['K4', 'K1', 'K2', 'K4', 'KRIGHT', 'K8']), '각');
chk('ㅋ⌫→∅', bs(['K4', 'K4']), '');
chk('나⌫→니', bs(['K5', 'K1', 'K2']), '니');
console.log('✅ 백스페이스 역추적');

/* 6) 방향키 확정 */
chk('▶ 확정', C.typeKeys(['K4', 'KRIGHT', 'K4', 'K1', 'K2']), 'ㄱ가');
chk('연타 순환', C.typeKeys(['K4', 'K4']), 'ㅋ');
chk('◀ 확정', C.typeKeys(['K8', 'K8', 'KLEFT', 'K8']), 'ㅎㅅ');
console.log('✅ 방향키 확정');

/* 7) 실전 단어 */
['나무위키', '한글', '안녕하세요', '고양이', '꽃', '빨래', '웃음', '짜장면',
 '떡볶이', '괜찮아', '없다', '값', '닭', '삶', '밟', '읊', '오늘 날씨 좋다',
 '회의 일정 확인 부탁드립니다'].forEach((w) => {
  chk('단어 ' + w, C.typeKeys(C.encode(w, false)), w);
  chk('단어(LP) ' + w, C.typeKeys(C.encode(w, true)), w);
});
console.log('✅ 실전 단어 18종');

/* 8) 현대 한글 11,172자 전수 — 멀티탭 / 롱프레스 양쪽 */
let n = 0, tapsMT = 0, tapsLP = 0, bad = [];
for (const c of C.CHO) for (const v of C.JUNG) for (const j of C.JONG) {
  const ch = C.compose(c, v, j.trim());
  const mt = C.encode(ch, false), lp = C.encode(ch, true);
  if (C.typeKeys(mt) !== ch) bad.push([ch, 'MT']);
  if (C.typeKeys(lp) !== ch) bad.push([ch, 'LP']);
  tapsMT += mt.filter((k) => k !== 'KRIGHT').length;
  tapsLP += lp.filter((k) => k !== 'KRIGHT').length;
  n++;
}
chk('전수 11,172자', bad.length, 0);
console.log(`✅ 전수 ${n}자 (멀티탭+롱프레스 = ${n * 2}회 왕복)`);

const avgMT = tapsMT / n, avgLP = tapsLP / n;
console.log(`\n   평균 타수 (멀티탭)   : ${avgMT.toFixed(3)}`);
console.log(`   평균 타수 (롱프레스) : ${avgLP.toFixed(3)}`);

/* Python 참조 구현과 수치 일치 확인 */
chk('Python 일치: 멀티탭 7.026', avgMT.toFixed(3), '7.026');
chk('Python 일치: 롱프레스 5.894', avgLP.toFixed(3), '5.894');

console.log('\n' + (fails.length === 0
  ? `✅ 전 항목 통과 — Python 참조 구현과 완전 일치`
  : `❌ 실패 ${fails.length}건\n` + JSON.stringify(fails.slice(0, 10), null, 1)));
process.exit(fails.length ? 1 : 0);
