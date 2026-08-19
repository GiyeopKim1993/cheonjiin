import com.cuime.KeypadView;
import android.content.Context;
import android.view.MotionEvent;
import java.util.*;
import java.lang.reflect.*;

/** KeypadView 터치 판정·배치 검증 (Android SDK 스텁 기반) */
public class KeypadViewTest {
    static int fails = 0;
    static void chk(String n, Object g, Object e) {
        boolean ok = String.valueOf(g).equals(String.valueOf(e));
        System.out.println((ok ? "✅ " : "❌ ") + n + (ok ? "" : "  got=" + g + " exp=" + e));
        if (!ok) fails++;
    }

    static List<String> log = new ArrayList<>();

    static class Rec implements KeypadView.Listener {
        public void onKeyTap(String k){ log.add("tap:"+k); }
        public void onKeyLongPress(String k){ log.add("long:"+k); }
        public void onHanjaDown(){ log.add("hjDown"); }
        public void onHanjaUp(){ log.add("hjUp"); }
        public void onHanjaLongPress(){ log.add("hjLong"); }
        public void onArrow(int d){ log.add("arrow:"+d); }
        public void onBackspace(){ log.add("back"); }
        public void onSpace(){ log.add("space"); }
        public void onEnter(){ log.add("enter"); }
        public void onSymbolPick(String s){ log.add("sym:"+s); }
        public void onCandidatePick(String h,int n){ log.add("cand:"+h+":"+n); }
    }

    /** 좌표로 키를 눌렀다 뗀다 */
    static void touch(KeypadView v, float x, float y) throws Exception {
        MotionEvent d = new MotionEvent(){ public float getX(){return x;} public float getY(){return y;}
            public int getActionMasked(){return MotionEvent.ACTION_DOWN;} };
        MotionEvent u = new MotionEvent(){ public float getX(){return x;} public float getY(){return y;}
            public int getActionMasked(){return MotionEvent.ACTION_UP;} };
        v.onTouchEvent(d); v.onTouchEvent(u);
    }

    /** i번째 키(0~15)의 중심 좌표 계산 — KeypadView.hit 와 동일 공식 */
    static float[] center(int i, float density, int width, float topOff) {
        float pad = 10*density, gap = 8*density, top = topOff + pad;
        float kw = (width - pad*2 - gap*3)/4f, kh = 58*density;
        int r = i/4, c = i%4;
        return new float[]{ pad + c*(kw+gap) + kw/2, top + r*(kh+gap) + kh/2 };
    }

    public static void main(String[] a) throws Exception {
        Context ctx = new Context();
        KeypadView v = new KeypadView(ctx);
        v.setListener(new Rec());
        float dens = 2.0f; int W = 1080;

        // 확정 배치 (docs/00 §2) — 각 키 중심을 눌러 올바른 이벤트가 나오는지
        String[] expect = {
            "tap:K1","tap:K2","tap:K3","back",
            "tap:K4","tap:K5","tap:K6","enter",
            "tap:K7","tap:K8","tap:K9","hjDown|hjUp",
            "arrow:-1","tap:K0","arrow:1","space"
        };
        for (int i = 0; i < 16; i++) {
            log.clear();
            float[] p = center(i, dens, W, 0);
            touch(v, p[0], p[1]);
            String got = String.join("|", log);
            chk("키" + i + " 배치", got, expect[i]);
        }

        // 후보 바가 열리면 키패드가 아래로 밀리는지 (topOffset)
        log.clear();
        v.showCandidates(Arrays.asList("國","局"), 1);
        float[] p0 = center(0, dens, W, 56*dens);
        touch(v, p0[0], p0[1]);
        chk("후보 바 열림 시 오프셋 반영", String.join("|", log), "tap:K1");

        // 후보 직접 터치
        log.clear();
        v.showCandidates(Arrays.asList("國","局"), 1);
        touch(v, 30*dens, 20*dens);          // 후보 바 영역
        chk("후보 터치 → 선택", log.size() > 0 && log.get(0).startsWith("cand:"), true);

        // 방향키가 후보를 소비 (docs/00 §4)
        v.showCandidates(Arrays.asList("國","局","菊"), 1);
        chk("후보 열림 시 방향키 소비", v.consumeArrow(1), true);
        Field fi = KeypadView.class.getDeclaredField("candIndex"); fi.setAccessible(true);
        chk("후보 인덱스 이동", fi.get(v), 1);
        v.consumeArrow(1); v.consumeArrow(1);
        chk("후보 순환", fi.get(v), 0);

        // 팔레트 페이지 전환
        v.showSymbols();
        chk("팔레트 열림 시 방향키 소비", v.consumeArrow(1), true);
        Field fp = KeypadView.class.getDeclaredField("palettePage"); fp.setAccessible(true);
        chk("팔레트 페이지", fp.get(v), 1);

        // 아무것도 안 열렸으면 방향키를 소비하지 않아야 함 (셸이 조합 확정 처리)
        v.setLayout(com.cuime.CheonjiinIME.Layout.HANGUL);
        chk("평상시 방향키 미소비", v.consumeArrow(1), false);

        // 레이아웃 전환 시 후보/팔레트가 닫히는지 (고아 타이머 방지와 동일 맥락)
        v.showSymbols();
        v.setLayout(com.cuime.CheonjiinIME.Layout.ENGLISH);
        chk("레이아웃 전환 시 팔레트 닫힘", v.consumeArrow(1), false);

        System.out.println("\n" + (fails == 0
            ? "✅ KeypadView 전 항목 통과 (터치 판정·배치·후보·팔레트)"
            : "❌ 실패 " + fails + "건"));
        System.exit(fails == 0 ? 0 : 1);
    }
}
