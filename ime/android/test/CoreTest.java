import com.cuime.core.CheonjiinCore;
import com.cuime.core.CheonjiinCore.State;
import java.util.*;
import java.nio.file.*;

/** Java 포팅 검증 — JS/Python 참조 구현과 동일 결과여야 한다 */
public class CoreTest {
    static int fails = 0;
    static void chk(String n, Object g, Object e) {
        boolean ok = String.valueOf(g).equals(String.valueOf(e));
        if (!ok) { System.out.println("❌ " + n + "  got=" + g + " exp=" + e); fails++; }
    }
    static List<String> L(String... a) { return Arrays.asList(a); }

    public static void main(String[] args) throws Exception {
        // 1) 중성 21자
        for (char v : CheonjiinCore.JUNG.toCharArray()) {
            List<String> seq = new ArrayList<>(L("K0"));
            seq.addAll(CheonjiinCore.vowelSeq(String.valueOf(v)));
            chk("중성 " + v, CheonjiinCore.typeKeys(seq),
                CheonjiinCore.compose("ㅇ", String.valueOf(v), ""));
        }
        System.out.println("✅ 중성 21자");

        // 2) 초성 19자
        for (char c : CheonjiinCore.CHO.toCharArray()) {
            String s = String.valueOf(c);
            List<String> seq = new ArrayList<>(CheonjiinCore.encode(
                CheonjiinCore.compose(s, "ㅏ", ""), false));
            chk("초성 " + c, CheonjiinCore.typeKeys(seq), CheonjiinCore.compose(s, "ㅏ", ""));
        }
        System.out.println("✅ 초성 19자");

        // 3) 롱프레스 = 순환열 마지막
        Map<String,String> lp = new LinkedHashMap<>();
        lp.put("K4","ㄲ"); lp.put("K5","ㄹ"); lp.put("K6","ㄸ"); lp.put("K7","ㅃ");
        lp.put("K8","ㅆ"); lp.put("K9","ㅉ"); lp.put("K0","ㅁ");
        chk("롱프레스 매핑", CheonjiinCore.LONGPRESS.toString(), lp.toString());
        for (Map.Entry<String,String> e : lp.entrySet())
            chk("롱프레스 " + e.getValue(),
                CheonjiinCore.typeKeys(L("LONG_" + e.getKey(), "K1", "K2")),
                CheonjiinCore.compose(e.getValue(), "ㅏ", ""));
        System.out.println("✅ 롱프레스 7종");

        // 4) 겹받침 11종
        for (String cl : CheonjiinCore.jongSplit().keySet()) {
            String ch = CheonjiinCore.compose("ㄱ", "ㅏ", cl);
            chk("겹받침 " + cl, CheonjiinCore.typeKeys(CheonjiinCore.encode(ch, false)), ch);
            chk("겹받침LP " + cl, CheonjiinCore.typeKeys(CheonjiinCore.encode(ch, true)), ch);
        }
        System.out.println("✅ 겹받침 11종");

        // 5) 백스페이스
        chk("각⌫", bs(L("K4","K1","K2","K5")), "가");
        chk("괴⌫", bs(L("K4","K2","K3","K1")), "고");
        chk("나⌫", bs(L("K5","K1","K2")), "니");
        chk("ㅋ⌫", bs(L("K4","K4")), "");
        System.out.println("✅ 백스페이스 역추적");

        // 6) 방향키 확정
        chk("▶확정", CheonjiinCore.typeKeys(L("K4","KRIGHT","K4","K1","K2")), "ㄱ가");
        chk("연타순환", CheonjiinCore.typeKeys(L("K4","K4")), "ㅋ");
        System.out.println("✅ 방향키 확정");

        // 7) 공용 테스트 벡터 (spec/test-vectors-16key.json)
        String json = new String(Files.readAllBytes(
            Paths.get("/home/user/cheonjiin/spec/test-vectors-16key.json")), "UTF-8");
        int n = 0;
        for (String blk : json.split("\\{\\s*\"category\"")) {
            if (!blk.contains("\"keys\"")) continue;
            List<String> keys = new ArrayList<>();
            String ks = blk.substring(blk.indexOf("\"keys\""));
            ks = ks.substring(ks.indexOf('[') + 1, ks.indexOf(']'));
            for (String p : ks.split(",")) {
                p = p.trim().replaceAll("^\"|\"$", "");
                if (p.isEmpty()) continue;
                keys.add(mapKey(p));
            }
            String ex = blk.substring(blk.indexOf("\"expect\"") + 8);
            ex = ex.substring(ex.indexOf('"') + 1);
            ex = ex.substring(0, ex.indexOf('"'));
            ex = ex.replace("\\n", "\n");
            chk("vector#" + n, CheonjiinCore.typeKeys(keys), ex);
            n++;
        }
        System.out.println("✅ 공용 테스트 벡터 " + n + "건");

        // 8) 현대 한글 11,172자 전수 (멀티탭 + 롱프레스)
        long tapsMT = 0, tapsLP = 0; int cnt = 0;
        for (char c : CheonjiinCore.CHO.toCharArray())
        for (char v : CheonjiinCore.JUNG.toCharArray())
        for (char j : CheonjiinCore.JONG.toCharArray()) {
            String ch = CheonjiinCore.compose(String.valueOf(c), String.valueOf(v),
                                              String.valueOf(j).trim());
            List<String> mt = CheonjiinCore.encode(ch, false);
            List<String> lps = CheonjiinCore.encode(ch, true);
            if (!CheonjiinCore.typeKeys(mt).equals(ch)) { chk("전수MT " + ch, "x", ch); }
            if (!CheonjiinCore.typeKeys(lps).equals(ch)) { chk("전수LP " + ch, "x", ch); }
            for (String k : mt) if (!k.equals("KRIGHT")) tapsMT++;
            for (String k : lps) if (!k.equals("KRIGHT")) tapsLP++;
            cnt++;
        }
        System.out.println("✅ 전수 " + cnt + "자 (멀티탭+롱프레스 = " + (cnt*2) + "회 왕복)");

        double avgMT = (double) tapsMT / cnt, avgLP = (double) tapsLP / cnt;
        System.out.printf("%n   평균 타수 (멀티탭)   : %.3f%n", avgMT);
        System.out.printf("   평균 타수 (롱프레스) : %.3f%n", avgLP);
        chk("JS/Python 일치 7.026", String.format("%.3f", avgMT), "7.026");
        chk("JS/Python 일치 5.894", String.format("%.3f", avgLP), "5.894");

        System.out.println();
        System.out.println(fails == 0
            ? "✅ Java 포팅 전 항목 통과 — JS/Python 참조와 완전 일치"
            : "❌ 실패 " + fails + "건");
        System.exit(fails == 0 ? 0 : 1);
    }

    static String mapKey(String p) {
        switch (p) {
            case "RIGHT": return "KRIGHT";
            case "LEFT":  return "KLEFT";
            case "SPACE": return "SPACE";
            case "BACK":  return "BACK";
            default:      return "K" + p;
        }
    }

    static String bs(List<String> keys) {
        State s = CheonjiinCore.newState();
        for (String k : keys) s = CheonjiinCore.press(s, k);
        s = CheonjiinCore.press(s, "BACK");
        return CheonjiinCore.press(s, "COMMIT").committed;
    }
}
