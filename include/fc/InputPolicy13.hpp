#pragma once
#include <algorithm>
#include <cstdint>
#include <deque>
#include <string>
#include <utility>
#include <unordered_set>
namespace fc::input13 {
// The input event source broadcasts whole batches. Captured down/held events
// must not become external hotkey releases when F8 closes. A new down edge clears
// a stale entry after a lost key-up (focus loss/device disconnect), so quarantine
// cannot permanently consume the next genuine press. No event nodes are edited.
class ButtonQuarantine {
    std::unordered_set<std::uint64_t> held_;
    static std::uint64_t Key(unsigned device, unsigned code) {
        return (static_cast<std::uint64_t>(device) << 32) | code;
    }
public:
    bool Observe(unsigned device, unsigned code, bool pressed, bool initial) {
        const auto key=Key(device,code);
        if(initial) { held_.erase(key); return false; }
        const bool captured=held_.contains(key);
        if(!pressed)held_.erase(key);
        return captured;
    }
    void Capture(unsigned device, unsigned code, bool pressed) {
        const auto key=Key(device,code);
        if(!pressed)held_.erase(key);
        else if(held_.size()<512)held_.insert(key);
    }
    void Reset(){held_.clear();}
};
// A menu session uses exactly one stream per text/wheel channel.
// First fallback text waits briefly for native WM_CHAR; never emit both streams.
class TextStream {
public:
    enum class Mode { Unknown, Native, Polling };
    Mode mode{Mode::Unknown};
    std::deque<std::pair<double,std::u16string>> pending;
    void Reset(){mode=Mode::Unknown;pending.clear();}
    std::u16string Native(std::u16string text) {
        if(mode==Mode::Polling)return {};
        mode=Mode::Native;pending.clear();return text;
    }
    void UseNative(){mode=Mode::Native;pending.clear();}
    std::u16string Poll(std::u16string text,double now) {
        if(mode==Mode::Native)return {};
        if(mode==Mode::Polling)return text;
        if(pending.size()<128)pending.emplace_back(now,std::move(text));
        return {};
    }
    std::u16string Flush(double now) {
        if(mode!=Mode::Unknown || pending.empty() || now-pending.front().first<0.10)return {};
        mode=Mode::Polling;std::u16string out;
        for(auto& p:pending)out+=p.second;
        pending.clear();return out;
    }
};
class WheelStream {
public:
    enum class Mode{Unknown,Legacy,Raw,Engine};Mode mode{Mode::Unknown};
    void Reset(){mode=Mode::Unknown;}
    std::pair<float,float> Select(float lx,float ly,float rx,float ry,float gameY=0.0f) {
        if(mode==Mode::Unknown) {
            if(gameY!=0)mode=Mode::Engine;
            else if(lx!=0 || ly!=0)mode=Mode::Legacy;
            else if(rx!=0 || ry!=0)mode=Mode::Raw;
        }
        if(mode==Mode::Engine)return {0.0f,gameY};
        return mode==Mode::Legacy?std::pair{lx,ly}:std::pair{rx,ry};
    }
};
}
