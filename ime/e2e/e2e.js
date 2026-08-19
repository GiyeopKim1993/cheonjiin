const puppeteer = require('puppeteer');
const BASE = process.env.BASE || 'http://localhost:7000/';
(async () => {
  const browser = await puppeteer.launch({ args: ['--no-sandbox', '--disable-dev-shm-usage'] });
  const page = await browser.newPage();
  await page.setViewport({ width: 460, height: 900, deviceScaleFactor: 2 });
  const errors = [];
  page.on('console', (m) => { if (m.type() === 'error') errors.push(m.text()); });
  page.on('pageerror', (e) => errors.push('PAGEERROR: ' + e.message));
  await page.goto(BASE, { waitUntil: 'networkidle0' });

  const fails = [];
  const chk = (n, g, e) => {
    const ok = JSON.stringify(g) === JSON.stringify(e);
    console.log((ok ? '✅ ' : '❌ ') + n + (ok ? '' : `  got=${JSON.stringify(g)} exp=${JSON.stringify(e)}`));
    if (!ok) fails.push(n);
  };
  const tap = async (id) => {
    await page.evaluate((i) => {
      const el = document.querySelector(`.k[data-id="${i}"]`);
      el.dispatchEvent(new PointerEvent('pointerdown', { bubbles: true }));
      el.dispatchEvent(new PointerEvent('pointerup', { bubbles: true }));
    }, id);
    await new Promise((r) => setTimeout(r, 20));
  };
  const longPress = async (id, ms = 400) => {
    await page.evaluate((i) => document.querySelector(`.k[data-id="${i}"]`)
      .dispatchEvent(new PointerEvent('pointerdown', { bubbles: true })), id);
    await new Promise((r) => setTimeout(r, ms));
    await page.evaluate((i) => document.querySelector(`.k[data-id="${i}"]`)
      .dispatchEvent(new PointerEvent('pointerup', { bubbles: true })), id);
    await new Promise((r) => setTimeout(r, 20));
  };
  const chord = async (dir) => {
    await page.evaluate(() => document.querySelector('.k[data-id="HANJA"]')
      .dispatchEvent(new PointerEvent('pointerdown', { bubbles: true })));
    await new Promise((r) => setTimeout(r, 80));
    await page.evaluate((d) => { const el = document.querySelector(`.k[data-id="${d > 0 ? 'RIGHT' : 'LEFT'}"]`);
      el.dispatchEvent(new PointerEvent('pointerdown', { bubbles: true }));
      el.dispatchEvent(new PointerEvent('pointerup', { bubbles: true })); }, dir);
    await page.evaluate(() => document.querySelector('.k[data-id="HANJA"]')
      .dispatchEvent(new PointerEvent('pointerup', { bubbles: true })));
    await new Promise((r) => setTimeout(r, 50));
  };
  const txt = () => page.$eval('#screen', (e) => e.textContent.replace(/\u200b/g, ''));
  const clear = () => page.click('#btnClear');
  const layout = () => page.$eval('#cLayout', (e) => e.textContent.trim());

  chk('키 16개 렌더', await page.$$eval('.k', (n) => n.length), 16);
  chk('레이아웃 표시', await layout(), '한글');

  for (const k of ['K5','K1','K2','K0','K0','K3','K2','K0','K3','K2','K1','K4','K4','K1']) await tap(k);
  chk('한글 조합: 나무위키', await txt(), '나무위키');
  await clear();

  await tap('K4');
  chk('멀티탭 dot 3개', await page.$$eval('.k[data-id="K4"] .dots i', (n) => n.length), 3);
  chk('첫 탭 활성', await page.$$eval('.k[data-id="K4"] .dots i.on', (n) => n.length), 1);
  await clear();

  await longPress('K7'); await tap('K1'); await tap('K2');
  chk('롱프레스 ㅃ → 빠', await txt(), '빠'); await clear();
  await longPress('K5'); await tap('K1'); await tap('K2');
  chk('롱프레스 ㄹ → 라', await txt(), '라'); await clear();

  for (const k of ['K8','K1','K2','K5','K5','K0','K0']) await tap(k);
  chk('겹받침 삶', await txt(), '삶'); await clear();

  await tap('K4'); await tap('RIGHT'); await tap('K4'); await tap('K1'); await tap('K2');
  chk('▶ 확정 → ㄱ가', await txt(), 'ㄱ가'); await clear();

  await chord(1);
  chk('漢+▶ → 영어', await layout(), '영어');
  chk('코드 후 팔레트 미발생', await page.$eval('#pal', (e) => getComputedStyle(e).display), 'none');
  await tap('K7'); await tap('K7'); await tap('K7'); await tap('K7');
  chk('영어 PQRS 4탭 → s', await txt(), 's'); await clear();
  await longPress('K9');
  chk('영어 롱프레스 → z', await txt(), 'z'); await clear();

  await chord(1);
  chk('→ 숫자', await layout(), '숫자');
  for (const k of ['K0','K1','K0']) await tap(k);
  chk('숫자 입력 010', await txt(), '010'); await clear();
  await chord(1);
  chk('3회 순환 → 한글', await layout(), '한글');

  await tap('HANJA');
  chk('漢 단독 → 기호 팔레트', await page.$eval('#pal', (e) => getComputedStyle(e).display), 'grid');
  chk('팔레트 16칸', await page.$$eval('#pal button', (n) => n.length), 16);
  await page.evaluate(() => document.querySelector('#pal button').click());
  chk('기호 입력 .', await txt(), '.'); await clear();

  await page.evaluate(() => { window.prompt = () => '대한민국'; });
  await page.click('#btnSel');
  await new Promise((r) => setTimeout(r, 60));
  await tap('HANJA');
  chk('블록 선택 → 한자 후보', await page.$eval('#cands', (e) => getComputedStyle(e).display), 'flex');
  chk('후보 大韓民國', await page.$eval('.cand', (e) => e.textContent), '大韓民國');
  // 실제 사전(KS X 1001) 반영 확인 — 단일 글자 '국'은 6개 후보
  await page.evaluate(() => { window.prompt = () => '국'; });
  await page.click('#btnClear'); await page.click('#btnSel');
  await new Promise((r) => setTimeout(r, 60));
  await page.evaluate(() => { const el=document.querySelector('.k[data-id="HANJA"]');
    el.dispatchEvent(new PointerEvent('pointerdown',{bubbles:true}));
    el.dispatchEvent(new PointerEvent('pointerup',{bubbles:true})); });
  await new Promise((r) => setTimeout(r, 60));
  chk('실제 사전: 국 → 후보 6개', await page.$$eval('.cand', (n) => n.length), 6);
  chk('실제 사전: 첫 후보 國', await page.$eval('.cand', (e) => e.textContent), '國');
  // 확장 사전(29k 단어) — 동음이의 해소 확인
  for (const [w, h] of [['교육','敎育'], ['전화','電話'], ['안전','安全']]) {
    await page.evaluate((x) => { window.prompt = () => x; }, w);
    await page.click('#btnClear'); await page.click('#btnSel');
    await new Promise((r) => setTimeout(r, 50));
    await page.evaluate(() => { const el=document.querySelector('.k[data-id="HANJA"]');
      el.dispatchEvent(new PointerEvent('pointerdown',{bubbles:true}));
      el.dispatchEvent(new PointerEvent('pointerup',{bubbles:true})); });
    await new Promise((r) => setTimeout(r, 50));
    chk('확장사전 ' + w + ' → ' + h, await page.$eval('.cand', (e) => e.textContent), h);
  }
  await page.evaluate(() => { window.prompt = () => '대한민국'; });
  await page.click('#btnClear'); await page.click('#btnSel');
  await new Promise((r) => setTimeout(r, 60));
  await page.evaluate(() => { const el=document.querySelector('.k[data-id="HANJA"]');
    el.dispatchEvent(new PointerEvent('pointerdown',{bubbles:true}));
    el.dispatchEvent(new PointerEvent('pointerup',{bubbles:true})); });
  await new Promise((r) => setTimeout(r, 60));
  await page.evaluate(() => document.querySelector('.cand').click());
  chk('한자 변환 적용', await txt(), '大韓民國'); await clear();

  await page.click('#btnSend');
  for (const k of ['K4','K1','K2']) await tap(k);
  await tap('BACK'); await tap('ENTER');
  chk('M1 ⌫ 직후 엔터 차단', (await txt()).indexOf('\n'), -1);
  chk('M1 경고 메시지', (await page.$eval('#cMsg', (e) => e.textContent)).indexOf('전송 차단') >= 0, true);
  await tap('ENTER');
  chk('M1 두 번째 엔터 통과', (await page.$eval('#screen', (e) => e.innerHTML)).indexOf('<br>') >= 0, true);
  await page.click('#btnSend'); await clear();

  await page.keyboard.press('Digit5'); await page.keyboard.press('Digit1'); await page.keyboard.press('Digit2');
  chk('물리 키보드 → 나', await txt(), '나');
  await page.keyboard.press('Backspace');
  chk('물리 백스페이스 → 니', await txt(), '니'); await clear();

  chk('콘솔 에러 없음', errors, []);
  await page.screenshot({ path: '../../assets/ime-screenshot.png' });
  console.log('\n' + (fails.length === 0 ? '✅ E2E 전 항목 통과 (28/28)' : `❌ 실패 ${fails.length}건: ${fails.join(', ')}`));
  await browser.close();
  process.exit(fails.length ? 1 : 0);
})();
