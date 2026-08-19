#include "cheonjiin.hpp"
#include <fstream>
using namespace cuime;
int main(){ std::ofstream o("/tmp/xl/cpp.txt"); const Tables&tb=T();
 for(auto&c:tb.CHO)for(auto&v:tb.JUNG)for(size_t i=0;i<tb.JONG.size();i++){
  std::string j=tb.JONG[i]; if(j==" ")j="";
  std::string ch=compose(c,v,j); o<<ch<<"\t";
  auto e=encode(ch,false); for(size_t k=0;k<e.size();k++){o<<e[k];if(k+1<e.size())o<<" ";}
  o<<"\n";} return 0;}
