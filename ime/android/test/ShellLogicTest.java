import com.cuime.core.CheonjiinCore;
import com.cuime.core.CheonjiinCore.State;
import com.cuime.HanjaDict;
import java.util.*;

/** CheonjiinIME 의 셸 로직을 Android SDK 없이 검증 (동일 알고리즘 추출) */
public class ShellLogicTest {
  enum Layout{HANGUL,ENGLISH,NUMBER}
  static State core=CheonjiinCore.newState();
  static Layout layout=Layout.HANGUL;
  static boolean shift=false, hjDown=false, hjConsumed=false, enterIsSend=false;
  static long lastBackAt=0; static int sendGuardMs=300;
  static String enKey=null; static int enTap=0;
  static String warn="";
  static Map<String,String[]> ENGLISH=new LinkedHashMap<>();
  static Map<String,String> NUMBER=new LinkedHashMap<>();
  static { ENGLISH.put("K1",new String[]{".",",","?","!","'"});
    ENGLISH.put("K2",new String[]{"a","b","c"}); ENGLISH.put("K3",new String[]{"d","e","f"});
    ENGLISH.put("K4",new String[]{"g","h","i"}); ENGLISH.put("K5",new String[]{"j","k","l"});
    ENGLISH.put("K6",new String[]{"m","n","o"}); ENGLISH.put("K7",new String[]{"p","q","r","s"});
    ENGLISH.put("K8",new String[]{"t","u","v"}); ENGLISH.put("K9",new String[]{"w","x","y","z"});
    for(int i=0;i<=9;i++)NUMBER.put("K"+i,String.valueOf(i)); }
  static int fails=0;
  static void chk(String n,Object g,Object e){ if(!String.valueOf(g).equals(String.valueOf(e))){
    System.out.println("❌ "+n+" got="+g+" exp="+e); fails++; } else System.out.println("✅ "+n); }
  static String txt(){ return core.text(); }
  static void reset(){ core=CheonjiinCore.newState(); enKey=null;enTap=0;shift=false;warn=""; }

  static void tap(String k){
    if(layout==Layout.HANGUL) core=CheonjiinCore.press(core,k);
    else if(layout==Layout.ENGLISH) en(k,false);
    else { String d=NUMBER.get(k); if(d!=null) core=CheonjiinCore.press(core,"CHAR_"+d); }
  }
  static void longPress(String k){
    if(layout==Layout.HANGUL){ if(CheonjiinCore.CONSONANT_CYCLE.containsKey(k))
      core=CheonjiinCore.press(core,"LONG_"+k); }
    else if(layout==Layout.ENGLISH) en(k,true);
  }
  static void en(String k,boolean lp){
    String[] cyc=ENGLISH.get(k);
    if(cyc==null){ if(k.equals("K0"))shift=!shift; return; }
    String ch;
    if(lp){ ch=cyc[cyc.length-1]; enKey=null; enTap=0; }
    else { if(k.equals(enKey)){ core=CheonjiinCore.press(core,"BACK"); enTap=(enTap+1)%cyc.length; }
           else { enKey=k; enTap=0; } ch=cyc[enTap]; }
    if(shift){ ch=ch.toUpperCase(Locale.US); shift=false; }
    core=CheonjiinCore.press(core,"CHAR_"+ch);
  }
  static void hjUp(String sel){
    boolean c=hjConsumed; hjDown=false; hjConsumed=false;
    if(c) return;
    core=CheonjiinCore.press(core,"TIMEOUT");
    if(sel!=null&&!sel.isEmpty()&&isHan(sel)&&!HanjaDict.lookup(sel).isEmpty())
      lastCands=HanjaDict.lookup(sel);
    else lastCands=null;
  }
  static List<String> lastCands=null;
  static boolean isHan(String s){ for(char c:s.toCharArray()) if(c<0xAC00||c>0xD7A3)return false; return true; }
  static void arrow(int d){
    if(hjDown){ if(!hjConsumed){ Layout[] a=Layout.values();
      layout=a[(layout.ordinal()+d+a.length)%a.length]; hjConsumed=true;
      core=CheonjiinCore.press(core,"COMMIT"); enKey=null;enTap=0; lastCands=null; } return; }
    if(lastCands!=null){ candIdx=(candIdx+d+lastCands.size())%lastCands.size(); return; }
    enKey=null;enTap=0; core=CheonjiinCore.press(core,d>0?"KRIGHT":"KLEFT");
  }
  static int candIdx=0;
  static void backspace(){ enKey=null;enTap=0; lastBackAt=System.currentTimeMillis();
    core=CheonjiinCore.backspace(core); }
  static boolean enter(){ enKey=null;enTap=0;
    long since=System.currentTimeMillis()-lastBackAt;
    if(enterIsSend&&lastBackAt>0&&since<sendGuardMs){ lastBackAt=0; warn="blocked"; return false; }
    core=CheonjiinCore.press(core,"ENTER"); return true; }

  public static void main(String[] a) throws Exception {
    // 한글 조합
    for(String k:Arrays.asList("K5","K1","K2","K0","K0","K3","K2","K0","K3","K2","K1","K4","K4","K1")) tap(k);
    chk("한글 나무위키", txt(), "나무위키"); reset();
    // 롱프레스
    longPress("K7"); tap("K1"); tap("K2"); chk("롱프레스 빠", txt(), "빠"); reset();
    longPress("K5"); tap("K1"); tap("K2"); chk("롱프레스 라", txt(), "라"); reset();
    // 겹받침
    for(String k:Arrays.asList("K8","K1","K2","K5","K5","K0","K0")) tap(k);
    chk("겹받침 삶", txt(), "삶"); reset();
    // ▶ 확정
    tap("K4"); arrow(1); tap("K4"); tap("K1"); tap("K2");
    chk("▶확정 ㄱ가", txt(), "ㄱ가"); reset();
    // 코드 레이아웃 전환
    hjDown=true; arrow(1); hjUp(null);
    chk("漢+▶ 영어", layout, Layout.ENGLISH);
    chk("코드 후 후보 없음", lastCands, null);
    tap("K7");tap("K7");tap("K7");tap("K7"); chk("영어 PQRS→s", txt(), "s"); reset();
    longPress("K9"); chk("영어 LP→z", txt(), "z"); reset();
    tap("K0"); tap("K2"); chk("shift→A", txt(), "A"); reset();
    hjDown=true; arrow(1); hjUp(null); chk("→숫자", layout, Layout.NUMBER);
    tap("K0");tap("K1");tap("K0"); chk("숫자 010", txt(), "010"); reset();
    hjDown=true; arrow(1); hjUp(null); chk("3회순환→한글", layout, Layout.HANGUL);
    hjDown=true; arrow(-1); hjUp(null); chk("역방향→숫자", layout, Layout.NUMBER);
    hjDown=true; arrow(1); hjUp(null); layout=Layout.HANGUL;
    // 한자
    reset(); hjUp("대한민국");
    chk("한자 후보", lastCands, Arrays.asList("大韓民國"));
    lastCands=null; hjUp("국");
    chk("글자 후보", lastCands, HanjaDict.lookup("국"));
    candIdx=0; arrow(1); arrow(1); chk("후보 탐색", candIdx, 2);
    lastCands=null;
    // 우선순위: 선택 중 코드
    hjDown=true; arrow(1); hjUp("국");
    chk("코드>한자", layout+"/"+lastCands, "ENGLISH/null"); layout=Layout.HANGUL;
    // M1 가드
    reset(); enterIsSend=true;
    for(String k:Arrays.asList("K4","K1","K2")) tap(k);
    backspace();
    chk("M1 차단", enter(), false);
    chk("M1 경고", warn, "blocked");
    Thread.sleep(350);
    chk("M1 가드 만료 후 통과", enter(), true);
    enterIsSend=false;
    System.out.println("\n"+(fails==0?"✅ Android 셸 로직 전 항목 통과":"❌ 실패 "+fails+"건"));
    System.exit(fails==0?0:1);
  }
}
