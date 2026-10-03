#pragma once
// TEST ONLY: explicit menu stack/message queue behavior, not real Skyrim code.
namespace RE {
enum class UI_MENU_FLAGS : std::uint32_t { kPausesGame=1, kOnStack=64 };
struct IMenu {
    virtual ~IMenu()=default;
    Flags<UI_MENU_FLAGS> menuFlags;
    bool OnStack()const{return menuFlags.any(UI_MENU_FLAGS::kOnStack);}
    bool PausesGame()const{return menuFlags.any(UI_MENU_FLAGS::kPausesGame);}
};
struct UI {
    using Create_t=IMenu*();
    struct Entry{Create_t* create{};std::unique_ptr<IMenu> menu;};
    std::unordered_map<std::string,Entry> menus;
    std::uint32_t numPausesGame{};
    static inline bool available{true};
    static UI* GetSingleton(){static UI ui;return available?&ui:nullptr;}
    bool GameIsPaused()const{return numPausesGame>0;}
    IMenu* GetMenu(std::string_view name){auto i=menus.find(std::string(name));return i==menus.end()?nullptr:i->second.menu.get();}
    bool IsMenuOpen(std::string_view name){auto* m=GetMenu(name);return m&&m->OnStack();}
    void Register(std::string_view name,Create_t* create){menus.try_emplace(std::string(name),Entry{create,{}});}
};
enum class UI_MESSAGE_TYPE{kShow,kHide,kForceHide};
struct UIMessageQueue {
    struct Message {std::string menu;UI_MESSAGE_TYPE type;};
    std::deque<Message> queued;
    static inline bool available{true};
    static UIMessageQueue* GetSingleton(){static UIMessageQueue q;return available?&q:nullptr;}
    void AddMessage(const BSFixedString& menu,UI_MESSAGE_TYPE type,void*){queued.push_back({menu,type});}
    void Drain() {
        auto* ui=UI::GetSingleton();if(!ui)return;
        auto work=std::move(queued);queued.clear();
        for(const auto& message:work) {
            auto i=ui->menus.find(message.menu);if(i==ui->menus.end())continue;
            auto& entry=i->second;
            if(message.type==UI_MESSAGE_TYPE::kShow) {
                if(!entry.menu)entry.menu.reset(entry.create());
                if(!entry.menu->OnStack()) {
                    entry.menu->menuFlags.set(UI_MENU_FLAGS::kOnStack);
                    if(entry.menu->PausesGame())++ui->numPausesGame;
                }
            } else if(entry.menu&&entry.menu->OnStack()) {
                if(entry.menu->PausesGame()) {
                    if(ui->numPausesGame==0)throw std::logic_error("native pause underflow");
                    --ui->numPausesGame;
                }
                entry.menu->menuFlags.reset(UI_MENU_FLAGS::kOnStack);
            }
        }
    }
};
struct Main {
    struct Runtime{bool freezeTime{};} data;
    static inline bool available{true};
    static Main* GetSingleton(){static Main main;return available?&main:nullptr;}
    Runtime& GetRuntimeData(){return data;}
};
}
namespace SKSE {
inline bool inGameTask{},inUITask{};
struct TaskInterface {
    std::deque<std::function<void()>> game,ui;
    void AddTask(std::function<void()> f){game.push_back(std::move(f));}
    void AddUITask(std::function<void()> f){if(RE::UIMessageQueue::GetSingleton())ui.push_back(std::move(f));} // Actual SKSE silently drops without its UIManager.
    void GameFrame() {auto work=std::move(game);game.clear();inGameTask=true;for(auto& f:work)f();inGameTask=false;}
    void UIFrame() {
        if(auto* queue=RE::UIMessageQueue::GetSingleton())queue->Drain();
        auto work=std::move(ui);ui.clear();inUITask=true;for(auto& f:work)f();inUITask=false;
    }
};
inline bool tasksAvailable{true};
inline TaskInterface* GetTaskInterface(){static TaskInterface tasks;return tasksAvailable?&tasks:nullptr;}
}
namespace fc::runtime10 {inline void InstallEvents(){}}
namespace fc::input13 {inline void InstallGameInput(){}}
namespace fc::overlay {inline bool Install(){return true;}}

using HWND=void*;
inline HWND fakeForeground=reinterpret_cast<HWND>(1);
inline bool fakeKeyDown{};
inline constexpr int GA_ROOT=2;
inline HWND GetForegroundWindow(){return fakeForeground;}
inline HWND GetAncestor(HWND window,int){return window;}
inline int GetAsyncKeyState(int){return fakeKeyDown?0x8000:0;}
namespace fc::overlay {void PollMenuHotkey();}
