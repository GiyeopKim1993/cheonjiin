// 공용 셸(shell.hpp) 검증 — IBus·TSF·IMK 가 공유하는 상태 머신
#include "shell.hpp"
#include <iostream>
using namespace cuime;
static int fails=0;
static void chk(const std::string&n,const std::string&g,const std::string&e){
  if(g!=e){std::cout<<"❌ "<<n<<" got="<<g<<" exp="<<e<<"\n";fails++;}
  else std::cout<<"✅ "<<n<<"\n"; }
static void chki(const std::string&n,long g,long e){ chk(n,std::to_string(g),std::to_string(e)); }
int main(){
  { Shell s; for(auto k:{"K5","K1","K2","K0","K0","K3","K2","K0","K3","K2","K1","K4","K4","K1"})s.tap(k);
    chk("한글 나무위키",s.text(),u8"나무위키"); }
  { Shell s; s.longPress("K7"); s.tap("K1"); s.tap("K2"); chk("롱프레스 빠",s.text(),u8"빠"); }
  { Shell s; s.longPress("K5"); s.tap("K1"); s.tap("K2"); chk("롱프레스 라",s.text(),u8"라"); }
  { Shell s; for(auto k:{"K8","K1","K2","K5","K5","K0","K0"})s.tap(k); chk("겹받침 삶",s.text(),u8"삶"); }
  { Shell s; s.tap("K4"); s.arrow(1); s.tap("K4"); s.tap("K1"); s.tap("K2");
    chk("▶확정",s.text(),u8"ㄱ가"); }
  { Shell s; s.hanjaDown(); s.arrow(1); s.hanjaUp("",false,{});
    chki("漢+▶ 영어",(long)s.layout,1); chki("코드후 팔레트X",s.paletteOpen?1:0,0);
    s.tap("K7");s.tap("K7");s.tap("K7");s.tap("K7"); chk("영어 PQRS→s",s.text(),"s"); }
  { Shell s; s.hanjaDown(); s.arrow(1); s.hanjaUp("",false,{}); s.longPress("K9");
    chk("영어 LP→z",s.text(),"z"); }
  { Shell s; s.hanjaDown(); s.arrow(1); s.hanjaUp("",false,{}); s.tap("K0"); s.tap("K2");
    chk("shift→A",s.text(),"A"); }
  { Shell s; s.hanjaDown(); s.arrow(-1); s.hanjaUp("",false,{});
    chki("역방향→숫자",(long)s.layout,2);
    s.tap("K0");s.tap("K1");s.tap("K0"); chk("숫자 010",s.text(),"010"); }
  { Shell s; for(int i=0;i<3;i++){s.hanjaDown();s.arrow(1);s.hanjaUp("",false,{});}
    chki("3회순환→한글",(long)s.layout,0); }
  { Shell s; s.hanjaUp(u8"대한민국",true,{u8"大韓民國"});
    chki("한자 후보수",(long)s.candidates.size(),1);
    chk("후보값",s.candidates[0],u8"大韓民國"); }
  { Shell s; s.hanjaUp(u8"국",true,{u8"國",u8"局",u8"菊",u8"鞠"});
    s.arrow(1); s.arrow(1); chki("후보 탐색",s.candIndex,2); }
  { Shell s; s.hanjaDown(); s.arrow(1); s.hanjaUp(u8"국",true,{u8"國"});
    chki("코드>한자 layout",(long)s.layout,1); chki("코드>한자 후보X",(long)s.candidates.size(),0); }
  { Shell s; s.hanjaUp("",false,{}); chki("팔레트 열림",s.paletteOpen?1:0,1);
    s.arrow(1); chki("페이지 전환",s.palettePage,1);
    s.pickSymbol(u8"★",false); chk("기호 입력",s.text(),u8"★"); }
  { Shell s; s.cfg.enterIsSend=true; s.nowMs=1000;
    for(auto k:{"K4","K1","K2"})s.tap(k);
    s.backspace(); s.nowMs=1100;
    chki("M1 차단",s.enter()?1:0,0);
    chk("M1 경고",s.warning.empty()?"":"warn","warn");
    s.nowMs=1500; chki("M1 만료후 통과",s.enter()?1:0,1); }
  { Shell s; s.cfg.enterIsSend=false; s.nowMs=1000;
    for(auto k:{"K4","K1","K2"})s.tap(k); s.backspace(); s.nowMs=1100;
    chki("비전송 컨텍스트 가드X",s.enter()?1:0,1); }
  std::cout<<"\n"<<(fails==0?"✅ C++ IBus 셸 전 항목 통과":"❌ 실패")<<"\n";
  return fails?1:0;
}
