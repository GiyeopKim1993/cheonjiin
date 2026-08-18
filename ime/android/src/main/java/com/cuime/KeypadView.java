package com.cuime;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.RectF;
import android.os.Handler;
import android.os.Looper;
import android.view.MotionEvent;
import android.view.View;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * 천지인 4×4 키패드 뷰 — docs/00-확정스펙.md §2, docs/03-디자인.md
 *
 * 렌더링과 터치 판정만 담당한다. 조합 로직은 일절 갖지 않으며,
 * 모든 입력을 Listener 를 통해 CheonjiinIME(셸)로 넘긴다.
 */
public class KeypadView extends View {

    public interface Listener {
        void onKeyTap(String key);
        void onKeyLongPress(String key);
        void onHanjaDown();
        void onHanjaUp();
        void onHanjaLongPress();
        void onArrow(int dir);
        void onBackspace();
        void onSpace();
        void onEnter();
        void onSymbolPick(String sym);
        void onCandidatePick(String hanja, int replaceLen);
    }

    /* ───────── 확정 배치 (docs/00 §2) ─────────
     *   ㅣ   ㆍ   ㅡ   ⌫
     *  ㄱㅋ ㄴㄹ ㄷㅌ  ↵
     *  ㅂㅍ ㅅㅎ ㅈㅊ  漢
     *   ◀  ㅇㅁ  ▶   ␣
     */
    private static final String[] IDS = {
        "K1","K2","K3","BACK",
        "K4","K5","K6","ENTER",
        "K7","K8","K9","HANJA",
        "LEFT","K0","RIGHT","SPACE"
    };
    private static final String[] LBL_HANGUL = {
        "ㅣ","ㆍ","ㅡ","⌫",
        "ㄱㅋ","ㄴㄹ","ㄷㅌ","↵",
        "ㅂㅍ","ㅅㅎ","ㅈㅊ","漢",
        "◀","ㅇㅁ","▶","␣"
    };
    private static final String[] LBL_ENGLISH = {
        ".,?","ABC","DEF","⌫",
        "GHI","JKL","MNO","↵",
        "PQRS","TUV","WXYZ","漢",
        "◀","⇧","▶","␣"
    };
    private static final String[] LBL_NUMBER = {
        "1","2","3","⌫",
        "4","5","6","↵",
        "7","8","9","漢",
        "◀","0","▶","␣"
    };
    /** 롱프레스 힌트 = 순환열 마지막 (docs/00 §3.2) */
    private static final String[] LP_HANGUL = {
        "","","","",
        "ㄲ","ㄹ","ㄸ","",
        "ㅃ","ㅆ","ㅉ","",
        "","ㅁ","",""
    };
    private static final String[] LP_ENGLISH = {
        "","C","F","",
        "I","L","O","",
        "S","V","Z","",
        "","","",""
    };
    private static final String[] NUMHINT = {
        "1","2","3","", "4","5","6","", "7","8","9","", "","0","",""
    };

    private static final String[][] SYMBOLS = {
        {".",",","?","!","~","…","'","\"","(",")",":",";","-","/","@","#"},
        {"+","-","×","÷","=","±","%","°","℃","₩","$","€","¥","№","※","★"},
        {"[","]","{","}","〈","〉","《","》","「","」","←","→","↑","↓","↔","⇒"},
    };

    /* 색상 (docs/03 §3) */
    private int cBg = 0xFFF2F4F7, cKey = 0xFFFFFFFF, cFn = 0xFFE4E8EE,
                cNav = 0xFFDCE6F5, cHj = 0xFFFDF0D5, cBd = 0xFFD5DAE1,
                cTxt = 0xFF12171D, cSub = 0xFF8A94A0, cAcc = 0xFF3B82F6,
                cHja = 0xFFB4791F, cDanger = 0xFFDC2626, cWarn = 0xFFD97706;

    private final Paint pFill = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint pStroke = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint pText = new Paint(Paint.ANTI_ALIAS_FLAG);

    private Listener listener;
    private CheonjiinIME.Layout layout = CheonjiinIME.Layout.HANGUL;
    private boolean shift = false;
    private String activeKey = null;
    private int activeTap = 0, activeCycleLen = 0;
    private int pressedIdx = -1;

    /* 후보 / 팔레트 */
    private List<String> candidates = null;
    private int candIndex = 0, candReplaceLen = 0;
    private boolean paletteOpen = false;
    private int palettePage = 0;

    private String warning = null;

    private final Handler handler = new Handler(Looper.getMainLooper());
    private Runnable longPressTask;            // DOM/뷰 밖에 보관 — 고아 타이머 방지
    private boolean longPressFired = false;
    private int longpressMs = 300;

    private static final int CANDS_H = 56;     // 후보 바 높이(dp 환산 전 기준)
    private float density = 1f;

    public KeypadView(Context c) {
        super(c);
        density = getResources().getDisplayMetrics().density;
        pStroke.setStyle(Paint.Style.STROKE);
        pStroke.setStrokeWidth(1f * density);
        pText.setTextAlign(Paint.Align.CENTER);
        // 다크 테마
        int night = getResources().getConfiguration().uiMode
                  & android.content.res.Configuration.UI_MODE_NIGHT_MASK;
        if (night == android.content.res.Configuration.UI_MODE_NIGHT_YES) {
            cBg = 0xFF101418; cKey = 0xFF1E252D; cFn = 0xFF2A333D; cNav = 0xFF243040;
            cHj = 0xFF3A2E1A; cBd = 0xFF2E3742; cTxt = 0xFFF5F7FA; cHja = 0xFFE0A800;
        }
    }

    public void setListener(Listener l) { listener = l; }
    public void setLongpressMs(int ms) { longpressMs = ms; }

    public void setLayout(CheonjiinIME.Layout l) {
        layout = l;
        candidates = null; paletteOpen = false;
        cancelLongPressTimer();                      // 레이아웃 전환 시 타이머 확실히 취소
        invalidate();
    }

    public void updateState(String lastKey, int tap, CheonjiinIME.Layout l, boolean sh) {
        activeKey = lastKey; activeTap = tap; layout = l; shift = sh;
        activeCycleLen = 0;
        if (lastKey != null) {
            String[] cyc = com.cuime.core.CheonjiinCore.CONSONANT_CYCLE.get(lastKey);
            if (cyc != null) activeCycleLen = cyc.length;
        }
        invalidate();
    }

    public void showCandidates(List<String> list, int replaceLen) {
        candidates = new ArrayList<>(list);
        candIndex = 0; candReplaceLen = replaceLen;
        paletteOpen = false;
        invalidate();
    }

    public void showSymbols() {
        paletteOpen = true; palettePage = 0; candidates = null;
        invalidate();
    }

    public void flashWarning(String msg) {
        warning = msg;
        invalidate();
        handler.postDelayed(new Runnable() {
            @Override public void run() { warning = null; invalidate(); }
        }, 1500);
    }

    /** 방향키를 후보/팔레트가 소비했으면 true (docs/00 §4) */
    public boolean consumeArrow(int dir) {
        if (candidates != null && !candidates.isEmpty()) {
            int n = candidates.size();
            candIndex = ((candIndex + dir) % n + n) % n;
            invalidate();
            return true;
        }
        if (paletteOpen) {
            palettePage = ((palettePage + dir) % SYMBOLS.length + SYMBOLS.length) % SYMBOLS.length;
            invalidate();
            return true;
        }
        return false;
    }

    @Override protected void onMeasure(int wSpec, int hSpec) {
        int w = MeasureSpec.getSize(wSpec);
        int keyH = (int) (58 * density);
        int h = keyH * 4 + (int) (10 * density) * 5;
        if (candidates != null || paletteOpen) h += (int) (CANDS_H * density);
        setMeasuredDimension(w, h);
    }

    private float topOffset() {
        return (candidates != null || paletteOpen) ? CANDS_H * density : 0;
    }

    @Override protected void onDraw(Canvas cv) {
        pFill.setColor(cBg);
        cv.drawRect(0, 0, getWidth(), getHeight(), pFill);

        if (candidates != null) drawCandidates(cv);
        else if (paletteOpen) drawPalette(cv);

        float pad = 10 * density, gap = 8 * density, top = topOffset() + pad;
        float kw = (getWidth() - pad * 2 - gap * 3) / 4f;
        float kh = 58 * density;
        String[] lbl = labels();
        String[] lp = lpHints();

        for (int i = 0; i < 16; i++) {
            int r = i / 4, c = i % 4;
            float x = pad + c * (kw + gap), y = top + r * (kh + gap);
            RectF rc = new RectF(x, y, x + kw, y + kh);
            String id = IDS[i];

            int fill = cKey;
            if (id.equals("BACK") || id.equals("ENTER") || id.equals("SPACE")) fill = cFn;
            else if (id.equals("LEFT") || id.equals("RIGHT")) fill = cNav;
            else if (id.equals("HANJA")) fill = cHj;
            if (i == pressedIdx) fill = blend(fill, cAcc, 0.16f);

            pFill.setColor(fill);
            cv.drawRoundRect(rc, 10 * density, 10 * density, pFill);
            pStroke.setColor(cBd);
            cv.drawRoundRect(rc, 10 * density, 10 * density, pStroke);

            // 라벨
            int tc = cTxt;
            if (id.equals("BACK")) tc = cDanger;
            else if (id.equals("HANJA")) tc = cHja;
            else if (id.equals("ENTER") || id.equals("SPACE")
                  || id.equals("LEFT") || id.equals("RIGHT")) tc = cAcc;
            pText.setColor(tc);
            String t = lbl[i];
            pText.setTextSize((t.length() >= 4 ? 16 : t.length() == 3 ? 18 : 21) * density);
            pText.setFakeBoldText(true);
            cv.drawText(t, rc.centerX(), rc.centerY() + 7 * density, pText);

            // 숫자 각인
            if (layout == CheonjiinIME.Layout.HANGUL && !NUMHINT[i].isEmpty()) {
                pText.setColor(cSub);
                pText.setTextSize(10 * density);
                pText.setFakeBoldText(false);
                pText.setTextAlign(Paint.Align.RIGHT);
                cv.drawText(NUMHINT[i], rc.right - 7 * density, rc.top + 15 * density, pText);
                pText.setTextAlign(Paint.Align.CENTER);
            }
            // 롱프레스 힌트
            if (!lp[i].isEmpty()) {
                pText.setColor(cSub);
                pText.setTextSize(9 * density);
                pText.setFakeBoldText(false);
                pText.setTextAlign(Paint.Align.RIGHT);
                cv.drawText(lp[i], rc.right - 7 * density, rc.bottom - 5 * density, pText);
                pText.setTextAlign(Paint.Align.CENTER);
            }
            // 멀티탭 순환 인디케이터 (docs/03 §4.1)
            if (id.equals(activeKey) && activeCycleLen > 1) {
                float dr = 2.2f * density, dgap = 3 * density;
                float total = activeCycleLen * dr * 2 + (activeCycleLen - 1) * dgap;
                float sx = rc.centerX() - total / 2 + dr;
                for (int d = 0; d < activeCycleLen; d++) {
                    pFill.setColor(d == activeTap ? cAcc : cBd);
                    cv.drawCircle(sx + d * (dr * 2 + dgap), rc.top + 8 * density, dr, pFill);
                }
            }
        }

        if (warning != null) {
            pFill.setColor(0xF0000000);
            RectF wr = new RectF(20 * density, topOffset() + 6 * density,
                                 getWidth() - 20 * density, topOffset() + 34 * density);
            cv.drawRoundRect(wr, 8 * density, 8 * density, pFill);
            pText.setColor(cWarn);
            pText.setTextSize(13 * density);
            pText.setFakeBoldText(true);
            cv.drawText(warning, wr.centerX(), wr.centerY() + 5 * density, pText);
        }
    }

    private void drawCandidates(Canvas cv) {
        float h = CANDS_H * density, pad = 8 * density;
        pFill.setColor(cKey);
        cv.drawRect(0, 0, getWidth(), h, pFill);
        float x = pad;
        for (int i = 0; i < candidates.size(); i++) {
            String s = candidates.get(i);
            pText.setTextSize(22 * density);
            float w = pText.measureText(s) + 24 * density;
            RectF rc = new RectF(x, pad, x + w, h - pad);
            pFill.setColor(i == candIndex ? cAcc : cKey);
            cv.drawRoundRect(rc, 8 * density, 8 * density, pFill);
            pStroke.setColor(cBd);
            cv.drawRoundRect(rc, 8 * density, 8 * density, pStroke);
            pText.setColor(i == candIndex ? Color.WHITE : cTxt);
            pText.setFakeBoldText(true);
            cv.drawText(s, rc.centerX(), rc.centerY() + 8 * density, pText);
            x += w + 6 * density;
            if (x > getWidth()) break;
        }
    }

    private void drawPalette(Canvas cv) {
        float h = CANDS_H * density, pad = 6 * density;
        pFill.setColor(cKey);
        cv.drawRect(0, 0, getWidth(), h, pFill);
        String[] page = SYMBOLS[palettePage];
        float w = (getWidth() - pad * 2) / 8f;
        for (int i = 0; i < 8 && i < page.length; i++) {
            float x = pad + i * w;
            RectF rc = new RectF(x + 2, pad, x + w - 2, h - pad);
            pFill.setColor(cFn);
            cv.drawRoundRect(rc, 6 * density, 6 * density, pFill);
            pText.setColor(cTxt);
            pText.setTextSize(17 * density);
            pText.setFakeBoldText(false);
            cv.drawText(page[i], rc.centerX(), rc.centerY() + 6 * density, pText);
        }
    }

    /* ───────── 터치 ───────── */
    @Override public boolean onTouchEvent(MotionEvent e) {
        float ex = e.getX(), ey = e.getY();
        switch (e.getActionMasked()) {
            case MotionEvent.ACTION_DOWN: {
                if (ey < topOffset()) { handleTopTouch(ex); return true; }
                final int idx = hit(ex, ey);
                pressedIdx = idx;
                invalidate();
                if (idx < 0) return true;
                final String id = IDS[idx];
                longPressFired = false;
                cancelLongPressTimer();
                if (id.equals("HANJA")) {
                    if (listener != null) listener.onHanjaDown();
                    longPressTask = new Runnable() {
                        @Override public void run() {
                            longPressTask = null; longPressFired = true;
                            if (listener != null) listener.onHanjaLongPress();
                        }
                    };
                    handler.postDelayed(longPressTask, longpressMs);
                } else if (id.startsWith("K")) {
                    longPressTask = new Runnable() {
                        @Override public void run() {
                            longPressTask = null; longPressFired = true;
                            if (listener != null) listener.onKeyLongPress(id);
                        }
                    };
                    handler.postDelayed(longPressTask, longpressMs);
                }
                return true;
            }
            case MotionEvent.ACTION_UP: {
                int idx = pressedIdx;
                pressedIdx = -1;
                invalidate();
                cancelLongPressTimer();
                if (idx < 0) return true;
                String id = IDS[idx];
                if (longPressFired) {                 // 롱프레스가 이미 처리
                    longPressFired = false;
                    if (id.equals("HANJA")) return true;
                    return true;
                }
                if (listener == null) return true;
                if (id.equals("HANJA")) listener.onHanjaUp();
                else if (id.equals("BACK")) listener.onBackspace();
                else if (id.equals("ENTER")) listener.onEnter();
                else if (id.equals("SPACE")) listener.onSpace();
                else if (id.equals("LEFT")) listener.onArrow(-1);
                else if (id.equals("RIGHT")) listener.onArrow(1);
                else listener.onKeyTap(id);
                return true;
            }
            case MotionEvent.ACTION_CANCEL:
                pressedIdx = -1; cancelLongPressTimer(); invalidate();
                return true;
        }
        return true;
    }

    private void handleTopTouch(float x) {
        if (candidates != null && listener != null) {
            float px = 8 * density, cx = px;
            for (int i = 0; i < candidates.size(); i++) {
                pText.setTextSize(22 * density);
                float w = pText.measureText(candidates.get(i)) + 24 * density;
                if (x >= cx && x < cx + w) {
                    String pick = candidates.get(i);
                    candidates = null;
                    listener.onCandidatePick(pick, candReplaceLen);
                    invalidate();
                    return;
                }
                cx += w + 6 * density;
            }
        } else if (paletteOpen && listener != null) {
            float pad = 6 * density, w = (getWidth() - pad * 2) / 8f;
            int i = (int) ((x - pad) / w);
            String[] page = SYMBOLS[palettePage];
            if (i >= 0 && i < 8 && i < page.length) listener.onSymbolPick(page[i]);
        }
    }

    private int hit(float x, float y) {
        float pad = 10 * density, gap = 8 * density, top = topOffset() + pad;
        float kw = (getWidth() - pad * 2 - gap * 3) / 4f, kh = 58 * density;
        for (int i = 0; i < 16; i++) {
            int r = i / 4, c = i % 4;
            float kx = pad + c * (kw + gap), ky = top + r * (kh + gap);
            if (x >= kx && x < kx + kw && y >= ky && y < ky + kh) return i;
        }
        return -1;
    }

    private void cancelLongPressTimer() {
        if (longPressTask != null) { handler.removeCallbacks(longPressTask); longPressTask = null; }
    }

    private String[] labels() {
        switch (layout) {
            case ENGLISH: return LBL_ENGLISH;
            case NUMBER:  return LBL_NUMBER;
            default:      return LBL_HANGUL;
        }
    }
    private String[] lpHints() {
        switch (layout) {
            case ENGLISH: return LP_ENGLISH;
            case NUMBER:  return new String[]{"","","","","","","","","","","","","","","",""};
            default:      return LP_HANGUL;
        }
    }

    private static int blend(int a, int b, float t) {
        int ar = (a >> 16) & 0xFF, ag = (a >> 8) & 0xFF, ab = a & 0xFF;
        int br = (b >> 16) & 0xFF, bg = (b >> 8) & 0xFF, bb = b & 0xFF;
        return 0xFF000000 | ((int)(ar + (br - ar) * t) << 16)
                          | ((int)(ag + (bg - ag) * t) << 8)
                          |  (int)(ab + (bb - ab) * t);
    }
}
