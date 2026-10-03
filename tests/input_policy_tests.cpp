#include "fc/InputPolicy13.hpp"
#include <stdexcept>
#define FC_CHECK(v) do { if(!(v)) throw std::runtime_error("Input policy assertion failed: " #v); } while(false)
#include <iostream>
int main(){
 using namespace fc::input13;
 TextStream s;
 FC_CHECK(s.Poll(u"1",0).empty());FC_CHECK(s.Flush(.05).empty());
 FC_CHECK(s.Native(u"1")==u"1");FC_CHECK(s.Flush(.2).empty());FC_CHECK(s.Poll(u"1",.2).empty());
 s.Reset();FC_CHECK(s.Poll(u"2",0).empty());FC_CHECK(s.Poll(u"3",.01).empty());FC_CHECK(s.Flush(.11)==u"23");
 FC_CHECK(s.Native(u"23").empty());FC_CHECK(s.Poll(u"4",.2)==u"4");
 s.Reset();FC_CHECK(s.Flush(10).empty());FC_CHECK(s.Native(u"搜索")==u"搜索");
 s.Reset();s.Poll(u"a",1);s.UseNative();FC_CHECK(s.Flush(2).empty());FC_CHECK(s.Native(u"中")==u"中");
 WheelStream w;FC_CHECK(w.Select(0,1,0,1).second==1);FC_CHECK(w.Select(0,0,0,1).second==0);
 w.Reset();FC_CHECK(w.Select(0,0,0,-.5f).second==-.5f);FC_CHECK(w.Select(0,-.5f,0,-.5f).second==-.5f);
 w.Reset();FC_CHECK(w.Select(.25f,0,.25f,0).first==.25f);
 w.Reset();FC_CHECK(w.Select(0,1,0,1,1).second==1);FC_CHECK(w.Select(0,1,0,1,0).second==0);
 std::cout<<"PASS: text routing, Unicode, delayed fallback, wheel dedup and reset.\n";
}
