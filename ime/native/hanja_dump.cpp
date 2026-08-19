#include "hanja.hpp"
#include <fstream>
#include <iostream>
int main(){ std::ifstream in("/tmp/queries.txt"); std::ofstream o("/tmp/hj_cpp.txt");
 std::string q;
 while(std::getline(in,q)){ if(q.empty())continue; o<<q<<"\t";
  auto v=cuime::hanja::lookup(q);
  for(size_t i=0;i<v.size();i++){o<<v[i];if(i+1<v.size())o<<" ";}
  o<<"\n"; }
 return 0;}
