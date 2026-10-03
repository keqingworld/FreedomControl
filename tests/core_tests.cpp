#include "fc/Core.hpp"
#include "fc/HotkeyState.hpp"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>

int main() {
    unsigned checks=0;
    auto check=[&](bool ok,const char* what) { ++checks; if(!ok) { std::cerr<<"FAIL: "<<what<<'\n'; std::exit(1); } };
    check(fc::ParseID("00000014")==0x14U,"player RefID");
    check(fc::ParseID("fe012abc")==0xFE012ABCU,"ESL FormID");
    check(fc::ParseID("0XFFFFFFFF")==0xFFFFFFFFU,"dynamic FormID");
    check(fc::ParseID(" 0x1234\t")==0x1234U,"trim and prefix");
    check(fc::ParseID("00000000")==0U,"zero parses but is not a valid selected ref");
    for(auto* bad:{"","0x","-1","100000000","GG123456","1234 garbage","12\n34","12 34","0x-14"}) check(!fc::ParseID(bad),"invalid ID");
    check(fc::Hex(0xFE00ABCD)=="FE00ABCD","hex formatting");
    check(fc::Hex(0x14)=="00000014","zero padded hex");
    for(auto id:{0U,1U,20U,0xFEABC123U,0xFFFFFFFFU}) check(fc::ParseID(fc::Hex(id))==id,"hex roundtrip");
    check(fc::SafeCommand("setav health 500"),"valid command");
    check(fc::SafeCommand("additem 0000000f 1000"),"valid inventory command");
    check(!fc::SafeCommand(""),"empty command");
    check(!fc::SafeCommand("tgm\ntcl"),"multiline command");
    check(!fc::SafeCommand("tgm\rtcl"),"CR command");
    check(!fc::SafeCommand(std::string("x\0y",3)),"embedded NUL command");
    check(fc::SafeCommand(std::string(512,'x')),"max command length");
    check(!fc::SafeCommand(std::string(513,'x')),"command too long");
    check(fc::Identifier("WhiterunDragonsreach"),"cell editor ID");
    check(fc::Identifier("MOD_Cell_01"),"underscore identifier");
    check(!fc::Identifier("foo bar"),"space in identifier");
    check(!fc::Identifier("x; tgm"),"punctuation in identifier");
    check(!fc::Identifier(""),"empty identifier");
    check(fc::Contains("Iron Sword Skyrim.esm 00012EB7","sWoRd"),"ASCII case folding");
    check(fc::Contains("ABc",""),"empty filter");
    check(!fc::Contains("ABc","abcd"),"long filter");
    check(fc::Contains("\xe5\xa4\xa9\xe9\x99\x85","\xe9\x99\x85"),"UTF-8 byte-safe substring");
    check(fc::Finite(10,0,0,5)==5,"upper clamp");
    check(fc::Finite(-10,0,0,5)==0,"lower clamp");
    check(fc::Finite(std::numeric_limits<float>::quiet_NaN(),2,0,5)==2,"NaN fallback");
    check(fc::Finite(std::numeric_limits<float>::infinity(),3,0,5)==3,"infinity fallback");
    check(fc::Distance({0,0,0},{3,4,0})==5,"distance");
    auto north=fc::LocalOffset(0,100,0,25);
    check(std::abs(north[0])<0.001f && std::abs(north[1]-100)<0.001f && north[2]==25,"north local offset");
    auto east=fc::LocalOffset(1.57079632679f,100,0,0);
    check(std::abs(east[0]-100)<0.001f && std::abs(east[1])<0.001f,"east local offset");
    auto right=fc::LocalOffset(0,0,30,0);
    check(right[0]==30 && right[1]==0,"strafe offset");
    // These are logic tests, not an emulation of Windows input or Skyrim.
    fc::HotkeyState hotkey;
    check(!hotkey.Update(false,false),"hotkey: inactive idle");
    check(!hotkey.Update(false,true),"hotkey: background press ignored");
    check(!hotkey.Update(true,true),"hotkey: focus gained while held primes state");
    check(!hotkey.Update(true,true),"hotkey: held after focus does not toggle");
    check(!hotkey.Update(true,false),"hotkey: release does not toggle");
    check(hotkey.Update(true,true),"hotkey: next physical press opens");
    bool repeated=false;
    for (int i=0;i<10000;++i) repeated=hotkey.Update(true,true)||repeated;
    check(!repeated,"hotkey: holding never auto-repeats");
    check(!hotkey.Update(true,false),"hotkey: key-up does not close menu");
    check(hotkey.Update(true,true),"hotkey: next press closes");
    check(!hotkey.Update(false,true),"hotkey: losing focus resets state");
    check(!hotkey.Update(false,false),"hotkey: background release ignored");
    check(!hotkey.Update(true,false),"hotkey: refocus idle primes state");
    check(hotkey.Update(true,true),"hotkey: press after refocus works");
    fc::HotkeyState fresh;
    check(!fresh.Update(true,true),"hotkey: held during initialization is ignored");
    check(!fresh.Update(true,false),"hotkey: initialization release ignored");
    check(fresh.Update(true,true),"hotkey: first intentional press accepted");
    fc::HotkeyState cycles;
    (void)cycles.Update(true,false);
    unsigned toggles=0;
    for (int i=0;i<1000;++i) {
        toggles+=cycles.Update(true,true) ? 1U : 0U;
        toggles+=cycles.Update(true,true) ? 1U : 0U;
        toggles+=cycles.Update(true,false) ? 1U : 0U;
    }
    check(toggles==1000U,"hotkey: 1000 press/hold/release cycles toggle once each");
    std::cout<<"PASS: "<<checks<<" engine-independent checks. No Skyrim/Windows runtime was tested.\n";
}
