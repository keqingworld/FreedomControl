#pragma once
#include "fc/LegionPolicy.hpp"
#include <array>
#include <span>
#include <vector>
#include <limits>
#include <charconv>
#include <optional>
#include <string_view>

namespace fc {
inline constexpr std::array<float,8> FollowBands10={96,160,240,400,700,1100,1800,3000};
inline std::size_t FollowBand10(float wanted) {
    wanted=Finite(wanted,240,64,10000); std::size_t best=0;
    for(std::size_t i=1;i<FollowBands10.size();++i)
        if(std::abs(FollowBands10[i]-wanted)<std::abs(FollowBands10[best]-wanted)) best=i;
    return best;
}
inline std::optional<std::uint64_t> ParseSeed10(std::string_view text) {
    if(text.empty())return std::uint64_t{0};
    std::uint64_t seed{};
    auto [end,error]=std::from_chars(text.data(),text.data()+text.size(),seed,10);
    if(error!=std::errc{} || end!=text.data()+text.size())return std::nullopt;
    return seed;
}
class AliasSlots10 {
    std::array<ID,MaxLegionMembers> owners_{};
public:
    void Reset(){owners_.fill(0);}
    int Find(ID id) const { if(!id)return -1;for(std::size_t i=0;i<owners_.size();++i)if(owners_[i]==id)return static_cast<int>(i);return -1; }
    int Acquire(ID id) {
        if(!id || id==0x14)return -1;
        if(int old=Find(id);old>=0)return old;
        for(std::size_t i=0;i<owners_.size();++i)if(!owners_[i]){owners_[i]=id;return static_cast<int>(i);}
        return -1;
    }
    void Release(ID id){if(int slot=Find(id);slot>=0)owners_[static_cast<std::size_t>(slot)]=0;}
    ID Owner(std::size_t i)const{return i<owners_.size()?owners_[i]:0;}
};
// SplitMix64: reproducible seeded selections; unbiased rejection avoids modulo bias.
class Random10 {
    std::uint64_t state_;
public:
    explicit Random10(std::uint64_t seed):state_(seed){}
    std::uint64_t Next(){std::uint64_t z=(state_+=0x9e3779b97f4a7c15ULL);z=(z^(z>>30))*0xbf58476d1ce4e5b9ULL;z=(z^(z>>27))*0x94d049bb133111ebULL;return z^(z>>31);}
    std::size_t Index(std::size_t n){if(!n)return 0;auto bound=static_cast<std::uint64_t>(n);auto threshold=(0ULL-bound)%bound;for(;;){auto x=Next();if(x>=threshold)return static_cast<std::size_t>(x%bound);}}
};
inline std::vector<ID> RandomBatch10(std::span<const ID> source,int count,std::uint64_t seed) {
    std::vector<ID> valid(source.begin(),source.end());
    std::sort(valid.begin(),valid.end()); valid.erase(std::unique(valid.begin(),valid.end()),valid.end());
    valid.erase(std::remove(valid.begin(),valid.end(),0),valid.end());
    if(valid.empty() || count<=0)return {};
    Random10 rng(seed);std::vector<ID> result;result.reserve(static_cast<std::size_t>(std::min(count,MaxSpawnBatch)));
    for(int i=0;i<std::min(count,MaxSpawnBatch);++i)result.push_back(valid[rng.Index(valid.size())]);
    return result;
}
struct ClearFacts10 {ID id{},base{};float distance{};bool actor{},sameArea{},friendly{},deleted{};};
inline bool ClearEligible10(const ClearFacts10& f,float radius,bool world,bool protectArmy) {
    if(!f.id || f.id==0x14 || !f.base || !f.actor || f.deleted || (protectArmy && f.friendly))return false;
    return world || (f.sameArea && std::isfinite(f.distance) && f.distance>=0 && f.distance<=Finite(radius,2000,1,100000));
}
enum class BirthResult10 {Pending,Ready,Failed};
inline BirthResult10 CheckBirth10(bool exists,bool disabled,bool dead,bool has3D,bool hasProcess,float age) {
    if(!exists || dead)return BirthResult10::Failed;
    if(!disabled && has3D && hasProcess)return BirthResult10::Ready;
    return age>=15.0f?BirthResult10::Failed:BirthResult10::Pending;
}
inline bool IsStuck10(float noProgress,float delay) {return std::isfinite(noProgress) && noProgress>=Finite(delay,12,5,120);}
}
