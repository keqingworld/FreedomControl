#include "PCH.h"
#include "Input13.hpp"
#include "fc/InputPolicy13.hpp"
#include <imgui.h>
#include <imm.h>

namespace fc::input13 {
namespace {
struct Event { enum Kind{Key,Text,Wheel,RawWheel,EngineWheel}kind; unsigned code{}; bool down{}; float x{},y{}; };
std::mutex eventsMutex;
std::deque<Event> events;
std::atomic<bool> capture{},textWanted{},messageInstalled{},mouseInstalled{};
std::atomic<HWND> target{};
HHOOK messageHook{},mouseHook{};DWORD ownerThread{};
bool overflow{},wasActive{};
std::array<bool,256> down{},blocked{};
std::array<double,256> repeatAt{};
TextStream textStream;WheelStream wheelStream;
Counters counters;
bool Focused() {
    HWND w=target.load(),f=GetForegroundWindow();
    return w && f && (w==f || GetAncestor(w,GA_ROOT)==GetAncestor(f,GA_ROOT));
}
void Push(Event e) {
    if(!capture.load() || !Focused())return;
    std::scoped_lock lock(eventsMutex);
    if(!capture.load())return;
    if(events.size()<2048)events.push_back(e);else {events.clear();overflow=true;}
}
void NativeMessage(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp) {
    if(hwnd!=target.load() || !capture.load() || !Focused())return;
    if(msg==WM_CHAR && textWanted.load() && wp>=32 && wp<=0xffff)
        Push({Event::Text,static_cast<unsigned>(wp)});
    if(msg==WM_KEYDOWN || msg==WM_KEYUP || msg==WM_SYSKEYDOWN || msg==WM_SYSKEYUP) {
        unsigned vk=static_cast<unsigned>(wp);
        if(vk==VK_SHIFT)vk=MapVirtualKeyW((lp>>16)&0xff,MAPVK_VSC_TO_VK_EX);
        if(vk==VK_CONTROL)vk=(lp&(1LL<<24))?VK_RCONTROL:VK_LCONTROL;
        if(vk==VK_MENU)vk=(lp&(1LL<<24))?VK_RMENU:VK_LMENU;
        if(vk<256)Push({Event::Key,vk,msg==WM_KEYDOWN || msg==WM_SYSKEYDOWN});
    }
}
void RawWheel(LPARAM lp) {
    RAWINPUT input{};UINT bytes=sizeof(input);
    if(GetRawInputData(reinterpret_cast<HRAWINPUT>(lp),RID_INPUT,&input,&bytes,sizeof(RAWINPUTHEADER))==static_cast<UINT>(-1))return;
    if(input.header.dwType!=RIM_TYPEMOUSE)return;
    const auto& m=input.data.mouse;
    const float delta=static_cast<SHORT>(m.usButtonData)/static_cast<float>(WHEEL_DELTA);
    if(m.usButtonFlags & RI_MOUSE_WHEEL)Push({Event::RawWheel,0,false,0,delta});
    if(m.usButtonFlags & RI_MOUSE_HWHEEL)Push({Event::RawWheel,0,false,-delta,0});
}
LRESULT CALLBACK GetMessage13(int code,WPARAM wp,LPARAM lp) {
    if(code==HC_ACTION && wp==PM_REMOVE && capture.load()) {
        const auto* m=reinterpret_cast<const MSG*>(lp);
        if(m && m->hwnd==target.load()) {
            NativeMessage(m->hwnd,m->message,m->wParam,m->lParam);
            if(m->message==WM_INPUT)RawWheel(m->lParam);
            if(!mouseInstalled.load() && (m->message==WM_MOUSEWHEEL || m->message==WM_MOUSEHWHEEL)) {
                float d=static_cast<SHORT>(HIWORD(m->wParam))/static_cast<float>(WHEEL_DELTA);
                Push({Event::Wheel,0,false,m->message==WM_MOUSEHWHEEL?-d:0,m->message==WM_MOUSEWHEEL?d:0});
            }
        }
    }
    return CallNextHookEx(nullptr,code,wp,lp);
}
LRESULT CALLBACK Mouse(int code,WPARAM wp,LPARAM lp) {
    if(code==HC_ACTION && capture.load() && (wp==WM_MOUSEWHEEL || wp==WM_MOUSEHWHEEL)) {
        const auto* m=reinterpret_cast<const MOUSEHOOKSTRUCTEX*>(lp);
        if(m && m->hwnd==target.load()) {
            const float d=static_cast<SHORT>(HIWORD(m->mouseData))/static_cast<float>(WHEEL_DELTA);
            Push({Event::Wheel,0,false,wp==WM_MOUSEHWHEEL?-d:0,wp==WM_MOUSEWHEEL?d:0});
        }
    }
    return CallNextHookEx(nullptr,code,wp,lp);
}
ImGuiKey Key(unsigned vk) {
    if(vk>='0' && vk<='9')return static_cast<ImGuiKey>(ImGuiKey_0+vk-'0');
    if(vk>='A' && vk<='Z')return static_cast<ImGuiKey>(ImGuiKey_A+vk-'A');
    if(vk>=VK_F1 && vk<=VK_F12)return static_cast<ImGuiKey>(ImGuiKey_F1+vk-VK_F1);
    if(vk>=VK_NUMPAD0 && vk<=VK_NUMPAD9)return static_cast<ImGuiKey>(ImGuiKey_Keypad0+vk-VK_NUMPAD0);
    switch(vk) {
    case VK_TAB:return ImGuiKey_Tab;case VK_LEFT:return ImGuiKey_LeftArrow;case VK_RIGHT:return ImGuiKey_RightArrow;
    case VK_UP:return ImGuiKey_UpArrow;case VK_DOWN:return ImGuiKey_DownArrow;case VK_PRIOR:return ImGuiKey_PageUp;
    case VK_NEXT:return ImGuiKey_PageDown;case VK_HOME:return ImGuiKey_Home;case VK_END:return ImGuiKey_End;
    case VK_INSERT:return ImGuiKey_Insert;case VK_DELETE:return ImGuiKey_Delete;case VK_BACK:return ImGuiKey_Backspace;
    case VK_SPACE:return ImGuiKey_Space;case VK_RETURN:return ImGuiKey_Enter;case VK_ESCAPE:return ImGuiKey_Escape;
    case VK_LCONTROL:return ImGuiKey_LeftCtrl;case VK_RCONTROL:return ImGuiKey_RightCtrl;
    case VK_LSHIFT:return ImGuiKey_LeftShift;case VK_RSHIFT:return ImGuiKey_RightShift;
    case VK_LMENU:return ImGuiKey_LeftAlt;case VK_RMENU:return ImGuiKey_RightAlt;
    case VK_LWIN:return ImGuiKey_LeftSuper;case VK_RWIN:return ImGuiKey_RightSuper;
    case VK_OEM_1:return ImGuiKey_Semicolon;case VK_OEM_PLUS:return ImGuiKey_Equal;case VK_OEM_COMMA:return ImGuiKey_Comma;
    case VK_OEM_MINUS:return ImGuiKey_Minus;case VK_OEM_PERIOD:return ImGuiKey_Period;case VK_OEM_2:return ImGuiKey_Slash;
    case VK_OEM_3:return ImGuiKey_GraveAccent;case VK_OEM_4:return ImGuiKey_LeftBracket;case VK_OEM_5:return ImGuiKey_Backslash;
    case VK_OEM_6:return ImGuiKey_RightBracket;case VK_OEM_7:return ImGuiKey_Apostrophe;
    case VK_DECIMAL:return ImGuiKey_KeypadDecimal;case VK_DIVIDE:return ImGuiKey_KeypadDivide;
    case VK_MULTIPLY:return ImGuiKey_KeypadMultiply;case VK_SUBTRACT:return ImGuiKey_KeypadSubtract;case VK_ADD:return ImGuiKey_KeypadAdd;
    default:return ImGuiKey_None;
    }
}
void Emit(const std::u16string& text) {
    for(char16_t c:text){ImGui::GetIO().AddInputCharacterUTF16(static_cast<ImWchar16>(c));++counters.characters;}
}
void Modifiers() {
    auto& io=ImGui::GetIO();
    io.AddKeyEvent(ImGuiMod_Ctrl,down[VK_LCONTROL]||down[VK_RCONTROL]);
    io.AddKeyEvent(ImGuiMod_Shift,down[VK_LSHIFT]||down[VK_RSHIFT]);
    io.AddKeyEvent(ImGuiMod_Alt,down[VK_LMENU]||down[VK_RMENU]);
    io.AddKeyEvent(ImGuiMod_Super,down[VK_LWIN]||down[VK_RWIN]);
}
void Translate(unsigned vk,double now) {
    if(!textWanted.load() || vk==VK_RETURN || vk==VK_TAB || vk==VK_BACK || vk==VK_ESCAPE)return;
    const bool altGr=down[VK_RMENU] && (down[VK_LCONTROL]||down[VK_RCONTROL]);
    if(!altGr && (down[VK_LCONTROL]||down[VK_RCONTROL]||down[VK_LMENU]||down[VK_RMENU]||down[VK_LWIN]||down[VK_RWIN]))return;
    BYTE state[256]{};
    for(unsigned i=0;i<256;++i)state[i]=down[i]?0x80:0;
    state[VK_SHIFT]=(down[VK_LSHIFT]||down[VK_RSHIFT])?0x80:0;
    state[VK_CONTROL]=(down[VK_LCONTROL]||down[VK_RCONTROL])?0x80:0;
    state[VK_MENU]=(down[VK_LMENU]||down[VK_RMENU])?0x80:0;
    state[VK_CAPITAL]|=static_cast<BYTE>(GetKeyState(VK_CAPITAL)&1);
    state[VK_NUMLOCK]|=static_cast<BYTE>(GetKeyState(VK_NUMLOCK)&1);
    const HKL layout=GetKeyboardLayout(ownerThread);
    wchar_t buffer[8]{};
    const int count=ToUnicodeEx(vk,MapVirtualKeyExW(vk,MAPVK_VK_TO_VSC,layout),state,buffer,8,4,layout);
    // Flag 4 avoids mutating the game's dead-key/TranslateMessage state.
    std::u16string text;
    for(int i=0;i<std::clamp(count,0,8);++i)if(buffer[i]>=32)text.push_back(static_cast<char16_t>(buffer[i]));
    if(!text.empty())Emit(textStream.Poll(std::move(text),now));
}
void ChangeKey(unsigned vk,bool pressed,double now,int hotkey) {
    if(vk>=256 || static_cast<int>(vk)==hotkey || Key(vk)==ImGuiKey_None)return;
    if(blocked[vk]) {if(!pressed)blocked[vk]=false;return;}
    if(down[vk]==pressed)return;
    down[vk]=pressed;Modifiers();ImGui::GetIO().AddKeyEvent(Key(vk),pressed);++counters.keys;
    if(pressed){repeatAt[vk]=now+0.40;Translate(vk,now);}else repeatAt[vk]=0;
}
void Reset() {
    auto& io=ImGui::GetIO();
    for(unsigned vk=0;vk<256;++vk)if(down[vk] && Key(vk)!=ImGuiKey_None)io.AddKeyEvent(Key(vk),false);
    down.fill(false);blocked.fill(false);repeatAt.fill(0);Modifiers();
    textStream.Reset();wheelStream.Reset();
}
}
void Install(HWND hwnd) {
    if(target.load())return;
    target=hwnd;DWORD process{};ownerThread=GetWindowThreadProcessId(hwnd,&process);
    if(!ownerThread || process!=GetCurrentProcessId())return;
    messageHook=SetWindowsHookExW(WH_GETMESSAGE,GetMessage13,nullptr,ownerThread);
    mouseHook=SetWindowsHookExW(WH_MOUSE,Mouse,nullptr,ownerThread);
    messageInstalled=messageHook!=nullptr;mouseInstalled=mouseHook!=nullptr;
    spdlog::info("Input13: thread message hook={}, mouse hook={}; keyboard polling fallback ready.",messageHook!=nullptr,mouseHook!=nullptr);
}
void Frame(bool active,int hotkey) {
    active=active && Focused();auto& io=ImGui::GetIO();
    if(!active){capture=false;textWanted=false;}
    std::deque<Event> batch;bool reset{};
    {std::scoped_lock lock(eventsMutex);batch.swap(events);reset=overflow;overflow=false;}
    if(!active || !wasActive || reset) {
        Reset();batch.clear();io.AddFocusEvent(active);
        if(active)for(unsigned vk=0;vk<256;++vk)blocked[vk]=(GetAsyncKeyState(vk)&0x8000)!=0;
    }
    wasActive=active;if(!active)return;
    capture=true;textWanted=io.WantTextInput;
    // Keep IME composition on the native UTF-16 path; ordinary polling never emits pinyin.
    if(HIMC ime=ImmGetContext(target.load())) {
        if(ImmGetCompositionStringW(ime,GCS_COMPSTR,nullptr,0)>0)textStream.UseNative();
        ImmReleaseContext(target.load(),ime);
    }
    const double now=ImGui::GetTime();float lx{},ly{},rx{},ry{},gy{};
    // Native text must win before the same physical press can become fallback text.
    for(const auto& e:batch)if(e.kind==Event::Text && io.WantTextInput)Emit(textStream.Native(std::u16string(1,static_cast<char16_t>(e.code))));
    for(const auto& e:batch) {
        if(e.kind==Event::Key)ChangeKey(e.code,e.down,now,hotkey);
        if(e.kind==Event::Wheel){lx+=e.x;ly+=e.y;}
        if(e.kind==Event::RawWheel){rx+=e.x;ry+=e.y;}
        if(e.kind==Event::EngineWheel)gy+=e.y;
    }
    // Modifiers first, then text keys: Shift/Control state belongs to this frame.
    for(unsigned vk:{VK_LCONTROL,VK_RCONTROL,VK_LSHIFT,VK_RSHIFT,VK_LMENU,VK_RMENU,VK_LWIN,VK_RWIN})
        ChangeKey(vk,(GetAsyncKeyState(vk)&0x8000)!=0,now,hotkey);
    for(unsigned vk=8;vk<256;++vk) {
        ChangeKey(vk,(GetAsyncKeyState(vk)&0x8000)!=0,now,hotkey);
        if(down[vk] && repeatAt[vk]>0 && now>=repeatAt[vk]) {Translate(vk,now);repeatAt[vk]=now+0.05;}
    }
    if(io.WantTextInput)Emit(textStream.Flush(now));else textStream.Reset();
    const auto [x,y]=wheelStream.Select(lx,ly,rx,ry,gy);
    if(x!=0 || y!=0){io.AddMouseWheelEvent(x,y);++counters.wheels;}
}
void WindowMessage(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp) {
    if(!messageInstalled.load()) {
        NativeMessage(hwnd,msg,wp,lp);
        if(msg==WM_INPUT)RawWheel(lp);
        if(!mouseInstalled.load() && (msg==WM_MOUSEWHEEL || msg==WM_MOUSEHWHEEL)) {
            float d=static_cast<SHORT>(HIWORD(wp))/static_cast<float>(WHEEL_DELTA);
            Push({Event::Wheel,0,false,msg==WM_MOUSEHWHEEL?-d:0,msg==WM_MOUSEWHEEL?d:0});
        }
    }
}
void GameWheel(float vertical){Push({Event::EngineWheel,0,false,0,vertical});}
void Shutdown() {
    capture=false;textWanted=false;
    if(messageHook)UnhookWindowsHookEx(messageHook);
    if(mouseHook)UnhookWindowsHookEx(mouseHook);
    messageHook=mouseHook=nullptr;messageInstalled=false;mouseInstalled=false;target=nullptr;
}
Counters Snapshot(){auto c=counters;c.messages=messageInstalled.load();c.mouse=mouseInstalled.load();return c;}
}
