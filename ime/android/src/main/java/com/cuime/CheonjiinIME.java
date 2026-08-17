package com.cuime;

import android.inputmethodservice.InputMethodService;
import android.os.Handler;
import android.os.Looper;
import android.text.TextUtils;
import android.view.KeyEvent;
import android.view.View;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;
import android.view.inputmethod.ExtractedTextRequest;
import android.view.inputmethod.ExtractedText;

import com.cuime.core.CheonjiinCore;
import com.cuime.core.CheonjiinCore.State;

import java.util.*;

/**
 * 천지인 Android IME 셸 — docs/00-확정스펙.md §9
 *
 * 코어(CheonjiinCore)는 시간을 모른다. 이 클래스가 타이머를 소유하고
 * TIMEOUT 키를 주입하며, preedit 표시와 커밋을 담당한다.
 */
public class CheonjiinIME extends InputMethodService implements KeypadView.Listener {

    /* 설정 (docs/00 §10) */
    private int multitapMs  = 800;
    private int longpressMs = 300;   // 실사용 조정 대상
    private int sendGuardMs = 300;   // M1 가드

    public enum Layout { HANGUL, ENGLISH, NUMBER }

    private State core = CheonjiinCore.newState();
    private Layout layout = Layout.HANGUL;
    private boolean shift = false;

    private final Handler handler = new Handler(Looper.getMainLooper());
    private Runnable multitapTask;

    /* 漢 키 코드 판정 (docs/00 §5) */
    private boolean hjDown = false, hjConsumed = false;
    private long hjDownAt = 0;

    /* M1 전송 가드 (docs/00 §7) */
    private long lastBackAt = 0;
    private boolean enterIsSend = false;

    /* 영어 멀티탭 */
    private String enKey = null;
    private int enTap = 0;

    private static final Map<String, String[]> ENGLISH = new LinkedHashMap<>();
    private static final Map<String, String> NUMBER = new LinkedHashMap<>();
    static {
        ENGLISH.put("K1", new String[]{".", ",", "?", "!", "'"});
        ENGLISH.put("K2", new String[]{"a","b","c"}); ENGLISH.put("K3", new String[]{"d","e","f"});
        ENGLISH.put("K4", new String[]{"g","h","i"}); ENGLISH.put("K5", new String[]{"j","k","l"});
        ENGLISH.put("K6", new String[]{"m","n","o"}); ENGLISH.put("K7", new String[]{"p","q","r","s"});
        ENGLISH.put("K8", new String[]{"t","u","v"}); ENGLISH.put("K9", new String[]{"w","x","y","z"});
        for (int i = 0; i <= 9; i++) NUMBER.put("K" + i, String.valueOf(i));
    }

    private KeypadView keypad;

    @Override public View onCreateInputView() {
        keypad = new KeypadView(this);
        keypad.setListener(this);
        keypad.setLayout(layout);
        return keypad;
    }

    @Override public void onStartInput(EditorInfo info, boolean restarting) {
        super.onStartInput(info, restarting);
        core = CheonjiinCore.newState();
        cancelMultitap();
        enKey = null; enTap = 0;
        // 전송형 액션이면 M1 가드 활성 (docs/00 §7)
        int act = info.imeOptions & EditorInfo.IME_MASK_ACTION;
        enterIsSend = (act == EditorInfo.IME_ACTION_SEND || act == EditorInfo.IME_ACTION_GO);
        // preedit 미지원 컨텍스트 감지
        composingSupported = (info.inputType != EditorInfo.TYPE_NULL);
    }
    private boolean composingSupported = true;

    @Override public void onFinishInput() {
        super.onFinishInput();
        commitAll();
    }

    /* ───────── 화면 반영 ───────── */
    private void render() {
        InputConnection ic = getCurrentInputConnection();
        if (ic == null) return;
        String pre = core.preedit();
        if (!core.committed.isEmpty()) {
            ic.commitText(core.committed, 1);
            core.committed = "";
        }
        if (composingSupported) {
            if (pre.isEmpty()) ic.finishComposingText();
            else ic.setComposingText(pre, 1);
        } else if (!pre.isEmpty()) {
            // preedit 미지원: 매 변경마다 즉시 커밋 (docs/02 §7.4)
            ic.commitText(pre, 1);
            core = CheonjiinCore.press(core, "COMMIT");
            core.committed = "";
        }
        if (keypad != null) keypad.updateState(core.lastKey, core.tap, layout, shift);
    }

    private void commitAll() {
        core = CheonjiinCore.press(core, "COMMIT");
        InputConnection ic = getCurrentInputConnection();
        if (ic != null) {
            if (!core.committed.isEmpty()) { ic.commitText(core.committed, 1); core.committed = ""; }
            ic.finishComposingText();
        }
        cancelMultitap();
    }

    /* ───────── 멀티탭 타이머 — 셸이 소유, 코어에 TIMEOUT 주입 ───────── */
    private void armMultitap() {
        cancelMultitap();
        multitapTask = new Runnable() {
            @Override public void run() {
                multitapTask = null;
                core = CheonjiinCore.press(core, "TIMEOUT");
                enKey = null; enTap = 0;
                render();
            }
        };
        handler.postDelayed(multitapTask, multitapMs);
    }
    private void cancelMultitap() {
        if (multitapTask != null) { handler.removeCallbacks(multitapTask); multitapTask = null; }
    }

    /* ───────── KeypadView.Listener ───────── */

    @Override public void onKeyTap(String key) {
        if (layout == Layout.HANGUL) {
            core = CheonjiinCore.press(core, key);
            armMultitap();
        } else if (layout == Layout.ENGLISH) {
            typeEnglish(key, false);
            armMultitap();
        } else {
            String d = NUMBER.get(key);
            if (d != null) core = CheonjiinCore.press(core, "CHAR_" + d);
        }
        render();
    }

    /** 롱프레스 = 순환열 마지막 항목 (docs/00 §3.2) */
    @Override public void onKeyLongPress(String key) {
        cancelMultitap();
        if (layout == Layout.HANGUL) {
            if (CheonjiinCore.CONSONANT_CYCLE.containsKey(key))
                core = CheonjiinCore.press(core, "LONG_" + key);
        } else if (layout == Layout.ENGLISH) {
            typeEnglish(key, true);
        }
        render();
    }

    private void typeEnglish(String k, boolean isLong) {
        String[] cyc = ENGLISH.get(k);
        if (cyc == null) { if (k.equals("K0")) shift = !shift; return; }
        String ch;
        if (isLong) {
            ch = cyc[cyc.length - 1];
            enKey = null; enTap = 0;
        } else {
            if (k.equals(enKey)) {
                core = CheonjiinCore.press(core, "BACK");
                enTap = (enTap + 1) % cyc.length;
            } else { enKey = k; enTap = 0; }
            ch = cyc[enTap];
        }
        if (shift) { ch = ch.toUpperCase(Locale.US); shift = false; }
        core = CheonjiinCore.press(core, "CHAR_" + ch);
    }

    /* 漢 키 — 누를 때가 아니라 뗄 때 판정 (docs/00 §5) */
    @Override public void onHanjaDown() {
        hjDown = true; hjConsumed = false; hjDownAt = System.currentTimeMillis();
    }

    @Override public void onHanjaUp() {
        boolean consumed = hjConsumed;
        hjDown = false; hjConsumed = false;
        if (consumed) return;                       // 코드로 소비됨 → 팔레트 안 뜸
        cancelMultitap();
        core = CheonjiinCore.press(core, "TIMEOUT");
        String sel = getSelectedText();
        if (!TextUtils.isEmpty(sel) && isHangul(sel)) showHanjaCandidates(sel);
        else showSymbolPalette();
        render();
    }

    @Override public void onHanjaLongPress() {
        hjConsumed = true;
        showSymbolPalette();
    }

    /** 방향키 — 문맥 의존 4중 동작 (docs/00 §4) */
    @Override public void onArrow(int dir) {
        // (1) 코드 → 레이아웃 전환
        if (hjDown) {
            if (!hjConsumed && System.currentTimeMillis() - hjDownAt >= 0) {
                Layout[] all = Layout.values();
                int i = (layout.ordinal() + dir + all.length) % all.length;
                layout = all[i];
                hjConsumed = true;
                cancelMultitap();
                commitAll();
                enKey = null; enTap = 0;
                if (keypad != null) keypad.setLayout(layout);
                render();
            }
            return;
        }
        // (2) 후보 탐색 / (3) 팔레트 페이지는 KeypadView가 처리
        if (keypad != null && keypad.consumeArrow(dir)) return;
        // (4) 조합 확정 / 커서 이동
        cancelMultitap();
        enKey = null; enTap = 0;
        String pre = core.preedit();
        core = CheonjiinCore.press(core, dir > 0 ? "KRIGHT" : "KLEFT");
        render();
        if (pre.isEmpty()) {
            InputConnection ic = getCurrentInputConnection();
            if (ic != null) ic.sendKeyEvent(new KeyEvent(KeyEvent.ACTION_DOWN,
                dir > 0 ? KeyEvent.KEYCODE_DPAD_RIGHT : KeyEvent.KEYCODE_DPAD_LEFT));
        }
    }

    @Override public void onBackspace() {
        cancelMultitap();
        enKey = null; enTap = 0;
        lastBackAt = System.currentTimeMillis();     // M1 가드 기록
        if (core.preedit().isEmpty() && core.committed.isEmpty()) {
            InputConnection ic = getCurrentInputConnection();
            if (ic != null) ic.deleteSurroundingText(1, 0);
            return;
        }
        core = CheonjiinCore.backspace(core);
        render();
    }

    @Override public void onSpace() {
        cancelMultitap();
        enKey = null; enTap = 0;
        core = CheonjiinCore.press(core, "SPACE");
        render();
    }

    /** 엔터 — M1 전송 가드 (docs/00 §7) */
    @Override public void onEnter() {
        cancelMultitap();
        enKey = null; enTap = 0;
        long since = System.currentTimeMillis() - lastBackAt;
        if (enterIsSend && lastBackAt > 0 && since < sendGuardMs) {
            lastBackAt = 0;
            if (keypad != null) keypad.flashWarning("⌫ 직후 전송이 차단되었습니다");
            return;                                  // 1회 무시
        }
        commitAll();
        InputConnection ic = getCurrentInputConnection();
        if (ic == null) return;
        EditorInfo ei = getCurrentInputEditorInfo();
        int act = ei == null ? 0 : (ei.imeOptions & EditorInfo.IME_MASK_ACTION);
        if (act != 0 && act != EditorInfo.IME_ACTION_NONE) ic.performEditorAction(act);
        else ic.commitText("\n", 1);
    }

    @Override public void onSymbolPick(String sym) {
        core = CheonjiinCore.press(core, "CHAR_" + sym);
        render();
    }

    @Override public void onCandidatePick(String hanja, int replaceLen) {
        InputConnection ic = getCurrentInputConnection();
        if (ic == null) return;
        commitAll();
        ic.deleteSurroundingText(replaceLen, 0);
        ic.commitText(hanja, 1);
    }

    /* ───────── 보조 ───────── */
    private String getSelectedText() {
        InputConnection ic = getCurrentInputConnection();
        if (ic == null) return null;
        CharSequence cs = ic.getSelectedText(0);
        if (cs != null && cs.length() > 0) return cs.toString();
        // 선택이 없으면 조합 중 / 직전 글자를 대상으로
        String pre = core.preedit();
        if (!pre.isEmpty()) return pre;
        CharSequence before = ic.getTextBeforeCursor(1, 0);
        return before == null ? null : before.toString();
    }

    private static boolean isHangul(String s) {
        for (char c : s.toCharArray())
            if (c < 0xAC00 || c > 0xD7A3) return false;
        return true;
    }

    private void showHanjaCandidates(String target) {
        List<String> list = HanjaDict.lookup(target);
        if (list == null || list.isEmpty()) { showSymbolPalette(); return; }
        if (keypad != null) keypad.showCandidates(list, target.length());
    }

    private void showSymbolPalette() {
        if (keypad != null) keypad.showSymbols();
    }
}
