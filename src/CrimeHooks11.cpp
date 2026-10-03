#include "PCH.h"
#include "CrimeHooks11.hpp"

namespace fc::law11 {
namespace {
std::atomic<bool> noGold{false}, noJail{false}, clearRequest{false};
std::atomic<std::uint64_t> goldCount{0},prisonCount{0},fineCount{0};
bool installed{};
using SetGoldFn=void(*)(RE::PlayerCharacter*,RE::TESFaction*,bool,std::uint32_t);
using ModGoldFn=void(*)(RE::PlayerCharacter*,RE::TESFaction*,bool,std::int32_t);
using PrisonFn=void(*)(RE::PlayerCharacter*,RE::TESFaction*,bool,bool);
using ServeFn=void(*)(RE::PlayerCharacter*);
SetGoldFn previousSet{};ModGoldFn previousMod{};PrisonFn previousPrison{},previousFine{};ServeFn previousServe{};
void SetGold(RE::PlayerCharacter* p,RE::TESFaction* f,bool violent,std::uint32_t amount) {
    if(noGold.load(std::memory_order_relaxed)&&amount) {++goldCount;clearRequest=true;amount=0;}
    previousSet(p,f,violent,amount);
}
void ModGold(RE::PlayerCharacter* p,RE::TESFaction* f,bool violent,std::int32_t amount) {
    if(noGold.load(std::memory_order_relaxed)&&amount>0) {++goldCount;clearRequest=true;return;}
    previousMod(p,f,violent,amount);
}
void Prison(RE::PlayerCharacter* p,RE::TESFaction* f,bool remove,bool actual) {
    if(noJail.load(std::memory_order_relaxed)) {++prisonCount;clearRequest=true;return;}
    previousPrison(p,f,remove,actual);
}
void Serve(RE::PlayerCharacter* p) {
    if(noJail.load(std::memory_order_relaxed)) {++prisonCount;clearRequest=true;return;}
    previousServe(p);
}
void Fine(RE::PlayerCharacter* p,RE::TESFaction* f,bool jail,bool stolen) {
    if(noJail.load(std::memory_order_relaxed)) {++fineCount;clearRequest=true;return;}
    previousFine(p,f,jail,stolen);
}
bool Executable(const void* p) {
    MEMORY_BASIC_INFORMATION info{};
    if(!p||!VirtualQuery(p,&info,sizeof(info))||info.State!=MEM_COMMIT || (info.Protect&(PAGE_GUARD|PAGE_NOACCESS)))return false;
    return (info.Protect&(PAGE_EXECUTE|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY))!=0;
}
}
bool Install() {
    if(installed)return true;
    // Only the runtime admitted in Plugin.cpp; no instruction-pattern patch and no external injector.
    if(REL::Module::get().version()!=REL::Version(1,6,1170,0))return false;
    REL::Relocation<std::uintptr_t> vtable{RE::VTABLE_PlayerCharacter[0]};
    auto** table=reinterpret_cast<void**>(vtable.address());
    constexpr std::size_t slots[]={0xB5,0xB6,0xB9,0xBA,0xBB};
    for(const auto slot:slots)if(!Executable(table[slot])) {spdlog::error("Kernel11: invalid crime vtable slot {}; hooks not installed.",slot);return false;}
    // Preserve earlier hooks; policies remain off until the current save is ready.
    DWORD old{}; auto* begin=&table[0xB5]; constexpr auto bytes=7*sizeof(void*);
    if(!VirtualProtect(begin,bytes,PAGE_READWRITE,&old)) {spdlog::error("Kernel11: cannot protect player vtable; hooks not installed.");return false;}
    previousSet=reinterpret_cast<SetGoldFn>(table[0xB5]);previousMod=reinterpret_cast<ModGoldFn>(table[0xB6]);
    previousPrison=reinterpret_cast<PrisonFn>(table[0xB9]);previousServe=reinterpret_cast<ServeFn>(table[0xBA]);previousFine=reinterpret_cast<PrisonFn>(table[0xBB]);
    InterlockedExchangePointer(reinterpret_cast<void* volatile*>(&table[0xB5]),reinterpret_cast<void*>(&SetGold));
    InterlockedExchangePointer(reinterpret_cast<void* volatile*>(&table[0xB6]),reinterpret_cast<void*>(&ModGold));
    InterlockedExchangePointer(reinterpret_cast<void* volatile*>(&table[0xB9]),reinterpret_cast<void*>(&Prison));
    InterlockedExchangePointer(reinterpret_cast<void* volatile*>(&table[0xBA]),reinterpret_cast<void*>(&Serve));
    InterlockedExchangePointer(reinterpret_cast<void* volatile*>(&table[0xBB]),reinterpret_cast<void*>(&Fine));
    DWORD ignored{};VirtualProtect(begin,bytes,old,&ignored);FlushInstructionCache(GetCurrentProcess(),begin,bytes);
    installed=true;spdlog::info("Kernel11: player crime/jail/fine vtable hooks installed; original hooks chained; enforcement is save-scoped.");return true;
}
void SetPolicy(bool gold,bool jail) {noGold.store(gold,std::memory_order_relaxed);noJail.store(jail,std::memory_order_relaxed);}
bool TakeClearRequest(){return clearRequest.exchange(false);}
Stats Snapshot(){return {installed,goldCount.load(),prisonCount.load(),fineCount.load()};}
}
