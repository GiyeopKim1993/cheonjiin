#include "cheonjiin.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdio>
using namespace cuime;
static int fails=0;
static void chk(const std::string&n,const std::string&g,const std::string&e){
  if(g!=e){ std::cout<<"❌ "<<n<<"  got="<<g<<" exp="<<e<<"\n"; fails++; }
}
int main(){
  const Tables& tb=T();
  // 중성 21
  for(auto&v:tb.JUNG){ std::vector<std::string> s{"K0"}; for(auto&k:vowelSeq(v))s.push_back(k);
    chk("중성 "+v, typeKeys(s), compose(u8"ㅇ",v,"")); }
  std::cout<<"✅ 중성 21자\n";
  // 초성 19
  for(auto&c:tb.CHO){ std::string ch=compose(c,u8"ㅏ",""); chk("초성 "+c, typeKeys(encode(ch,false)), ch); }
  std::cout<<"✅ 초성 19자\n";
  // 롱프레스
  const char* lpk[]={"K4","K5","K6","K7","K8","K9","K0"};
  const char* lpv[]={u8"ㄲ",u8"ㄹ",u8"ㄸ",u8"ㅃ",u8"ㅆ",u8"ㅉ",u8"ㅁ"};
  for(int i=0;i<7;i++){ chk(std::string("LP ")+lpv[i], tb.LONGPRESS.at(lpk[i]), lpv[i]);
    chk(std::string("LP타 ")+lpv[i], typeKeys({std::string("LONG_")+lpk[i],"K1","K2"}), compose(lpv[i],u8"ㅏ","")); }
  std::cout<<"✅ 롱프레스 7종\n";
  // 겹받침
  for(auto&kv:tb.JONG_SPLIT){ std::string ch=compose(u8"ㄱ",u8"ㅏ",kv.first);
    chk("겹 "+kv.first, typeKeys(encode(ch,false)), ch);
    chk("겹LP "+kv.first, typeKeys(encode(ch,true)), ch); }
  std::cout<<"✅ 겹받침 11종\n";
  // 백스페이스
  auto bs=[](std::vector<std::string> ks){ State s=newState(); for(auto&k:ks)s=press(s,k);
    s=press(s,"BACK"); return press(s,"COMMIT").committed; };
  chk("각⌫",bs({"K4","K1","K2","K5"}),u8"가");
  chk("괴⌫",bs({"K4","K2","K3","K1"}),u8"고");
  chk("나⌫",bs({"K5","K1","K2"}),u8"니");
  chk("ㅋ⌫",bs({"K4","K4"}),"");
  std::cout<<"✅ 백스페이스\n";
  chk("▶확정",typeKeys({"K4","KRIGHT","K4","K1","K2"}),u8"ㄱ가");
  chk("연타",typeKeys({"K4","K4"}),u8"ㅋ");
  std::cout<<"✅ 방향키 확정\n";
  // 공용 벡터
  std::ifstream f("/home/user/cheonjiin/spec/test-vectors-16key.json");
  std::stringstream ss; ss<<f.rdbuf(); std::string js=ss.str();
  int n=0; size_t p=0;
  while((p=js.find("\"keys\"",p))!=std::string::npos){
    size_t a=js.find('[',p), b=js.find(']',a);
    std::string ks=js.substr(a+1,b-a-1);
    std::vector<std::string> keys; std::string cur;
    for(char c:ks){ if(c=='"'){ if(!cur.empty()){ 
        std::string k=cur; if(k=="RIGHT")k="KRIGHT"; else if(k=="LEFT")k="KLEFT";
        else if(k!="SPACE"&&k!="BACK")k="K"+k; keys.push_back(k); cur.clear(); } else cur=" "; }
      else if(cur==" "&&c!=','&&c!=' ')cur=std::string(1,c); else if(cur.size()&&cur!=" "&&c!=','&&c!=' ')cur+=c; }
    size_t ep=js.find("\"expect\"",b); size_t q1=js.find('"',ep+8), q2=js.find('"',q1+1);
    std::string ex=js.substr(q1+1,q2-q1-1);
    chk("vec#"+std::to_string(n), typeKeys(keys), ex);
    n++; p=b;
  }
  std::cout<<"✅ 공용 테스트 벡터 "<<n<<"건\n";
  // 전수
  long long mt=0,lp=0; int cnt=0;
  for(auto&c:tb.CHO)for(auto&v:tb.JUNG)for(size_t ji=0;ji<tb.JONG.size();ji++){
    std::string j=tb.JONG[ji]; if(j==" ")j="";
    std::string ch=compose(c,v,j);
    auto a=encode(ch,false), b=encode(ch,true);
    if(typeKeys(a)!=ch)chk("MT "+ch,"x",ch);
    if(typeKeys(b)!=ch)chk("LP "+ch,"x",ch);
    for(auto&k:a)if(k!="KRIGHT")mt++;
    for(auto&k:b)if(k!="KRIGHT")lp++;
    cnt++;
  }
  std::cout<<"✅ 전수 "<<cnt<<"자 (멀티탭+롱프레스 = "<<cnt*2<<"회 왕복)\n";
  char buf[64];
  snprintf(buf,64,"%.3f",(double)mt/cnt); std::string amt=buf;
  snprintf(buf,64,"%.3f",(double)lp/cnt); std::string alp=buf;
  std::cout<<"\n   평균 타수 (멀티탭)   : "<<amt<<"\n   평균 타수 (롱프레스) : "<<alp<<"\n";
  chk("JS/Java 일치 7.026",amt,"7.026");
  chk("JS/Java 일치 5.894",alp,"5.894");
  std::cout<<"\n"<<(fails==0?"✅ C++ 포팅 전 항목 통과 — JS/Java/Python 참조와 완전 일치":"❌ 실패")<<"\n";
  return fails?1:0;
}
