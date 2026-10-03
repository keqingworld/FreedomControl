"""Execute production crime hook dispatch against a TEST vtable and TEST Windows APIs.
Not an ABI, relocation/address, real OS page protection or game compatibility test.
"""
from pathlib import Path
import argparse,subprocess,tempfile,shutil
R=Path(__file__).resolve().parents[1]
STUB=r'''
#pragma once
#include <atomic>
#include <cstdint>
#include <cstddef>
#include <array>
namespace RE {struct PlayerCharacter{};struct TESFaction{};inline std::array<void*,0x200> testTable{};inline std::array<std::uintptr_t,1> VTABLE_PlayerCharacter{};}
namespace REL {struct Version{std::array<int,4> v;Version(int a,int b,int c,int d):v{a,b,c,d}{}bool operator==(const Version&)const=default;};struct Module{Version v{1,6,1170,0};static Module& get(){static Module m;return m;}Version version(){return v;}};template<class T>struct Relocation{T p;Relocation(T v):p(v){}T address(){return p;}};}
namespace spdlog {template<class... T>void info(const T&...){}template<class... T>void error(const T&...){};}
using DWORD=std::uint32_t;
constexpr DWORD MEM_COMMIT=0x1000,PAGE_GUARD=0x100,PAGE_NOACCESS=1,PAGE_READWRITE=4,PAGE_EXECUTE=0x10,PAGE_EXECUTE_READ=0x20,PAGE_EXECUTE_READWRITE=0x40,PAGE_EXECUTE_WRITECOPY=0x80;
struct MEMORY_BASIC_INFORMATION{DWORD State{},Protect{};};
inline bool queryOK=true,protectOK=true;
inline std::size_t VirtualQuery(const void*,MEMORY_BASIC_INFORMATION* out,std::size_t n){out->State=MEM_COMMIT;out->Protect=queryOK?PAGE_EXECUTE_READ:PAGE_NOACCESS;return n;}
inline bool VirtualProtect(void*,std::size_t,DWORD,DWORD* old){*old=PAGE_EXECUTE_READ;return protectOK;}
inline void* InterlockedExchangePointer(void* volatile* p,void* value){void* old=*p;*p=value;return old;}
inline void* GetCurrentProcess(){return nullptr;}inline void FlushInstructionCache(void*,const void*,std::size_t){}
'''
TEST=r'''
#include "PCH.h"
#include "CrimeHooks11.hpp"
#include <iostream>
#include <stdexcept>
int sets{},mods{},prisons{},serves{},fines{};std::uint32_t lastSet{};std::int32_t lastMod{};
void Set(RE::PlayerCharacter*,RE::TESFaction*,bool,std::uint32_t a){++sets;lastSet=a;}
void Mod(RE::PlayerCharacter*,RE::TESFaction*,bool,std::int32_t a){++mods;lastMod=a;}
void Jail(RE::PlayerCharacter*,RE::TESFaction*,bool,bool){++prisons;}
void Serve(RE::PlayerCharacter*){++serves;}
void Fine(RE::PlayerCharacter*,RE::TESFaction*,bool,bool){++fines;}
int main(){using namespace fc::law11;int n=0;auto ck=[&](bool b){++n;if(!b)throw std::runtime_error("crime hooks #"+std::to_string(n));};
 auto& t=RE::testTable;t[0xB5]=reinterpret_cast<void*>(&Set);t[0xB6]=reinterpret_cast<void*>(&Mod);t[0xB7]=reinterpret_cast<void*>(&Set);t[0xB8]=reinterpret_cast<void*>(&Mod);t[0xB9]=reinterpret_cast<void*>(&Jail);t[0xBA]=reinterpret_cast<void*>(&Serve);t[0xBB]=reinterpret_cast<void*>(&Fine);RE::VTABLE_PlayerCharacter[0]=reinterpret_cast<std::uintptr_t>(t.data());
 REL::Module::get().v={1,5,97,0};ck(!Install());ck(t[0xB5]==reinterpret_cast<void*>(&Set));REL::Module::get().v={1,6,1170,0};queryOK=false;ck(!Install());queryOK=true;protectOK=false;ck(!Install());protectOK=true;ck(Install());ck(Snapshot().installed);ck(Install());
 ck(t[0xB5]!=reinterpret_cast<void*>(&Set));ck(t[0xB7]==reinterpret_cast<void*>(&Set)&&t[0xB8]==reinterpret_cast<void*>(&Mod));
 auto set=reinterpret_cast<decltype(&Set)>(t[0xB5]);auto mod=reinterpret_cast<decltype(&Mod)>(t[0xB6]);auto jail=reinterpret_cast<decltype(&Jail)>(t[0xB9]);auto serve=reinterpret_cast<decltype(&Serve)>(t[0xBA]);auto fine=reinterpret_cast<decltype(&Fine)>(t[0xBB]);
 RE::PlayerCharacter p;RE::TESFaction f;set(&p,&f,false,100);mod(&p,&f,true,50);jail(&p,&f,true,true);serve(&p);fine(&p,&f,true,true);
 ck(sets==1&&mods==1&&prisons==1&&serves==1&&fines==1);ck(lastSet==100&&lastMod==50);ck(!TakeClearRequest());
 SetPolicy(true,true);set(&p,&f,false,500);ck(lastSet==0&&sets==2);mod(&p,&f,true,200);ck(mods==1);mod(&p,&f,true,-20);ck(mods==2&&lastMod==-20);set(&p,&f,false,0);ck(sets==3);
 jail(&p,&f,false,false);serve(&p);fine(&p,&f,false,true);ck(prisons==1&&serves==1&&fines==1);ck(TakeClearRequest());ck(!TakeClearRequest());auto s=Snapshot();ck(s.gold==2&&s.prison==2&&s.fine==1);
 SetPolicy(false,false);set(&p,&f,true,600);jail(&p,&f,false,true);serve(&p);fine(&p,&f,true,false);ck(lastSet==600);ck(prisons==2&&serves==2&&fines==2);
 SetPolicy(false,true);mod(&p,&f,false,10);ck(lastMod==10);jail(&p,&f,false,true);ck(prisons==2);
 SetPolicy(true,false);fine(&p,&f,true,true);ck(fines==3);mod(&p,&f,false,0);ck(lastMod==0);
 std::cout<<"PASS: "<<n<<" production crime hook dispatch assertions against explicit TEST vtable / OS APIs. Not Windows memory or game ABI tests.\n";
}
'''
def main():
 p=argparse.ArgumentParser();p.add_argument('--compiler',default='g++');p.add_argument('--sanitize',action='store_true');a=p.parse_args()
 with tempfile.TemporaryDirectory(prefix='fc-crime11-') as tmp:
  t=Path(tmp);(t/'PCH.h').write_text(STUB);(t/'test.cpp').write_text(TEST)
  for name in ('CrimeHooks11.cpp','CrimeHooks11.hpp'):shutil.copy2(R/'src'/name,t/name)
  cmd=[a.compiler,'-std=c++23','-Wall','-Wextra','-g','-I'+str(t)]
  if a.sanitize:cmd+=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
  subprocess.run(cmd+[str(t/'CrimeHooks11.cpp'),str(t/'test.cpp'),'-o',str(t/'test')],check=True,timeout=40);subprocess.run([str(t/'test')],check=True,timeout=20)
if __name__=='__main__':main()
