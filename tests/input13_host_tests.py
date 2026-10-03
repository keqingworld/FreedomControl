"""Compile and execute actual Input13.cpp with explicit Win32/ImGui doubles.
Not native Windows input delivery or in-game/IME acceptance testing.
"""
from pathlib import Path
import argparse,re,shutil,subprocess,tempfile
parser=argparse.ArgumentParser();parser.add_argument('--compiler',default='g++');parser.add_argument('--sanitize',action='store_true');args=parser.parse_args()
R=Path(__file__).resolve().parents[1]
src=(R/'src/Input13.cpp').read_text()
vk={'VK_BACK':8,'VK_TAB':9,'VK_RETURN':13,'VK_SHIFT':16,'VK_CONTROL':17,'VK_MENU':18,'VK_CAPITAL':20,'VK_ESCAPE':27,'VK_SPACE':32,'VK_PRIOR':33,'VK_NEXT':34,'VK_END':35,'VK_HOME':36,'VK_LEFT':37,'VK_UP':38,'VK_RIGHT':39,'VK_DOWN':40,'VK_INSERT':45,'VK_DELETE':46,'VK_LWIN':91,'VK_RWIN':92,'VK_NUMPAD0':96,'VK_NUMPAD9':105,'VK_MULTIPLY':106,'VK_ADD':107,'VK_SUBTRACT':109,'VK_DECIMAL':110,'VK_DIVIDE':111,'VK_F1':112,'VK_F12':123,'VK_NUMLOCK':144,'VK_LSHIFT':160,'VK_RSHIFT':161,'VK_LCONTROL':162,'VK_RCONTROL':163,'VK_LMENU':164,'VK_RMENU':165,'VK_OEM_1':186,'VK_OEM_PLUS':187,'VK_OEM_COMMA':188,'VK_OEM_MINUS':189,'VK_OEM_PERIOD':190,'VK_OEM_2':191,'VK_OEM_3':192,'VK_OEM_4':219,'VK_OEM_5':220,'VK_OEM_6':221,'VK_OEM_7':222}
keys=sorted(set(re.findall(r'ImGuiKey_\w+',src)) - {'ImGuiKey_0','ImGuiKey_A','ImGuiKey_F1','ImGuiKey_Keypad0'})
stub=r'''
#pragma once
#include <array>
#include <atomic>
#include <mutex>
#include <deque>
#include <vector>
#include <string>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <algorithm>
#include <cstdint>
#define CALLBACK
using HWND=void*;using HHOOK=void*;using HIMC=void*;using HKL=void*;using HRAWINPUT=void*;
using UINT=unsigned;using DWORD=unsigned;using BYTE=unsigned char;using SHORT=short;using WPARAM=std::uintptr_t;using LPARAM=std::intptr_t;using LRESULT=std::intptr_t;
constexpr int GA_ROOT=1,HC_ACTION=0,PM_REMOVE=1,WHEEL_DELTA=120,RID_INPUT=1,RIM_TYPEMOUSE=0,RI_MOUSE_WHEEL=0x400,RI_MOUSE_HWHEEL=0x800,MAPVK_VSC_TO_VK_EX=3,MAPVK_VK_TO_VSC=0,WH_GETMESSAGE=3,WH_MOUSE=7;
constexpr int WM_CHAR=0x102,WM_KEYDOWN=0x100,WM_KEYUP=0x101,WM_SYSKEYDOWN=0x104,WM_SYSKEYUP=0x105,WM_MOUSEWHEEL=0x20a,WM_MOUSEHWHEEL=0x20e,WM_INPUT=0xff;
inline unsigned HIWORD(std::uintptr_t x){return static_cast<unsigned>((x>>16)&0xffff);}
struct RAWINPUTHEADER{unsigned dwType{};};struct RAWMOUSE{unsigned short usButtonFlags{},usButtonData{};};struct RAWINPUT{RAWINPUTHEADER header;struct {RAWMOUSE mouse;}data;};
struct MSG{HWND hwnd{};UINT message{};WPARAM wParam{};LPARAM lParam{};};struct MOUSEHOOKSTRUCTEX{HWND hwnd{};DWORD mouseData{};};
inline HWND foreground=reinterpret_cast<HWND>(1);inline bool imeOpen{};inline std::array<bool,256> physical{};inline double clockNow{};
inline HWND GetForegroundWindow(){return foreground;}inline HWND GetAncestor(HWND w,int){return w;}
inline UINT MapVirtualKeyW(UINT k,UINT){return k;}inline UINT MapVirtualKeyExW(UINT k,UINT,HKL){return k;}
inline UINT GetRawInputData(HRAWINPUT,UINT,RAWINPUT*,UINT*,UINT){return static_cast<UINT>(-1);}
inline LRESULT CallNextHookEx(HHOOK,int,WPARAM,LPARAM){return 0;}
inline SHORT GetKeyState(int){return 0;}inline SHORT GetAsyncKeyState(int k){return physical[k]?static_cast<SHORT>(0x8000):0;}
inline HKL GetKeyboardLayout(DWORD){return nullptr;}
inline int ToUnicodeEx(UINT vk,UINT,const BYTE* state,wchar_t* buf,int,UINT flags,HKL){
 if(flags!=4)throw std::runtime_error("must not mutate translator state");
 if(vk>='0' && vk<='9'){buf[0]=static_cast<wchar_t>(vk);return 1;}
 if(vk>='A' && vk<='Z'){buf[0]=static_cast<wchar_t>(vk+(state[16]?0:32));return 1;}return 0;
}
inline DWORD GetCurrentProcessId(){return 42;}inline DWORD GetWindowThreadProcessId(HWND,DWORD* p){*p=42;return 7;}
using HOOKPROC=LRESULT(*)(int,WPARAM,LPARAM);inline HHOOK SetWindowsHookExW(int,HOOKPROC,void*,DWORD tid){if(tid!=7)throw std::runtime_error("thread-local only");return reinterpret_cast<HHOOK>(1);}
inline bool UnhookWindowsHookEx(HHOOK){return true;}inline HIMC ImmGetContext(HWND){return reinterpret_cast<HIMC>(1);}
constexpr int GCS_COMPSTR=8;inline long ImmGetCompositionStringW(HIMC,int,void*,int){return imeOpen?2:0;}inline void ImmReleaseContext(HWND,HIMC){}
namespace spdlog{template<class...T>void info(const char*,T...) {}}
'''
stub+='\n'+''.join(f'constexpr unsigned {k}={v};\n' for k,v in vk.items())
stub+='using ImWchar16=char16_t;\nenum ImGuiKey{ImGuiKey_0=100,ImGuiKey_A=200,ImGuiKey_F1=300,ImGuiKey_Keypad0=400,'+','.join(f'{k}={500+i}' for i,k in enumerate(keys))+',ImGuiMod_Ctrl=1000,ImGuiMod_Shift,ImGuiMod_Alt,ImGuiMod_Super};\n'
stub+=r'''
namespace ImGui{
struct IO{bool WantTextInput{true};std::u16string text;std::vector<std::pair<int,bool>> keys;std::vector<std::pair<float,float>> wheels;
 void AddInputCharacterUTF16(char16_t c){text+=c;}void AddKeyEvent(ImGuiKey k,bool d){keys.emplace_back(k,d);}
 bool focused{};void AddFocusEvent(bool value){focused=value;}void AddMouseWheelEvent(float x,float y){wheels.emplace_back(x,y);}};
inline IO io;inline IO& GetIO(){return io;}inline double GetTime(){return clockNow;}
}
'''
stub+=r'''
namespace RE {
enum class BSEventNotifyControl{kContinue,kStop};enum class INPUT_DEVICE{kKeyboard,kMouse,kGamepad,kNone};
template<class T>struct BSTEventSource{};
template<class T>struct BSTEventSink{virtual ~BSTEventSink()=default;virtual BSEventNotifyControl ProcessEvent(T const*,BSTEventSource<T>*)=0;};
struct ButtonEvent;struct InputEvent{InputEvent* next{};ButtonEvent* button{};ButtonEvent* AsButtonEvent(){return button;}};
struct ButtonEvent:InputEvent{unsigned id{};bool pressed{},down{};std::string userEvent;INPUT_DEVICE device{INPUT_DEVICE::kMouse};
 INPUT_DEVICE GetDevice()const{return device;}bool IsPressed()const{return pressed;}bool IsDown()const{return down;}unsigned GetIDCode()const{return id;}const std::string& GetUserEvent()const{return userEvent;}};
struct UserEvents{std::string activate{"Activate"};static UserEvents* GetSingleton(){static UserEvents e;return &e;}};
struct ControlMap{static constexpr unsigned kInvalid=0xff;std::array<unsigned,3> keys{18,2,4096};static ControlMap* GetSingleton(){static ControlMap c;return &c;}unsigned GetMappedKey(const char* name,INPUT_DEVICE device){if(std::string(name)!="Activate"||device==INPUT_DEVICE::kNone)return kInvalid;return keys.at(static_cast<int>(device));}};
struct MenuOpenCloseEvent{std::string menuName;bool opening{};};
struct UI{int adds{};BSTEventSink<MenuOpenCloseEvent>* sink{};static UI* GetSingleton(){static UI ui;return &ui;}template<class T>void AddEventSink(BSTEventSink<T>* s){++adds;sink=s;}};
struct BSInputDeviceManager{int adds{};std::vector<BSTEventSink<InputEvent*>*> sinks;static BSInputDeviceManager* GetSingleton(){static BSInputDeviceManager manager;return &manager;}void PrependEventSink(BSTEventSink<InputEvent*>* s){++adds;sinks.insert(sinks.begin(),s);}void Send(InputEvent*& event){for(auto* s:sinks)if(s->ProcessEvent(&event,nullptr)==BSEventNotifyControl::kStop)break;}};
}
namespace fc {inline int activations14{},lateActivations14{};inline bool nativeInputRan14{},overlayOpen14{};inline unsigned closedDialogues14{};struct Engine{static Engine& Get(){static Engine e;return e;}bool MenuOpen()const{return overlayOpen14;}void RecordPlayerDialogueClosed14(){++closedDialogues14;}void RecordPlayerActivation14(){++activations14;if(nativeInputRan14)++lateActivations14;}};}
'''
test=r'''
#include "Input13.cpp"
#include "GameInput13.cpp"
struct NativeInput:RE::BSTEventSink<RE::InputEvent*>{int calls{};RE::BSEventNotifyControl ProcessEvent(RE::InputEvent*const*,RE::BSTEventSource<RE::InputEvent*>*)override{++calls;fc::nativeInputRan14=true;return RE::BSEventNotifyControl::kContinue;}};
int main(){using namespace fc::input13;int n=0;auto ck=[&](bool b){++n;if(!b)throw std::runtime_error("Input13 #"+std::to_string(n));};
 auto w=reinterpret_cast<HWND>(1);Install(w);Frame(true,119);
 physical['1']=true;clockNow=.01;Frame(true,119);ck(ImGui::io.text.empty());
 clockNow=.12;Frame(true,119);ck(ImGui::io.text==u"1");
 physical['1']=false;Frame(true,119);physical['2']=true;clockNow=.14;Frame(true,119);ck(ImGui::io.text==u"12");
 clockNow=.55;Frame(true,119);ck(ImGui::io.text==u"122");
 // Native chars arriving after fallback selection cannot duplicate text.
 MSG native{w,WM_CHAR,'2',0};GetMessage13(HC_ACTION,PM_REMOVE,reinterpret_cast<LPARAM>(&native));Frame(true,119);ck(ImGui::io.text==u"122");
 Frame(false,119);physical.fill(false);ImGui::io.text.clear();Frame(true,119);
 native.wParam=0x4e2d;GetMessage13(HC_ACTION,PM_REMOVE,reinterpret_cast<LPARAM>(&native));Frame(true,119);ck(ImGui::io.text==u"中");
 physical['A']=true;Frame(true,119);clockNow=1;Frame(true,119);ck(ImGui::io.text==u"中");
 // Hook survives an unrelated WndProc because no WindowMessage call is needed.
 Push({Event::Wheel,0,false,0,1});Push({Event::RawWheel,0,false,0,1});Frame(true,119);ck(ImGui::io.wheels.size()==1);ck(ImGui::io.wheels.back().second==1);
 Push({Event::Wheel,0,false,0,-.5f});Frame(true,119);ck(ImGui::io.wheels.back().second==-.5f);
 foreground=reinterpret_cast<HWND>(2);native.wParam='9';GetMessage13(HC_ACTION,PM_REMOVE,reinterpret_cast<LPARAM>(&native));Frame(true,119);ck(ImGui::io.text==u"中");
 foreground=w;physical.fill(false);physical['7']=true;Frame(true,119);clockNow=2;Frame(true,119);ck(ImGui::io.text==u"中");
 physical['7']=false;Frame(true,119);physical['7']=true;clockNow=2.01;Frame(true,119);clockNow=2.12;Frame(true,119);ck(ImGui::io.text==u"中7");
 Frame(false,119);physical.fill(false);ImGui::io.text.clear();Frame(true,119);
 physical[VK_LCONTROL]=true;physical['V']=true;clockNow=3;Frame(true,119);clockNow=3.2;Frame(true,119);ck(ImGui::io.text.empty());
 ck(down[VK_LCONTROL]);ck(down['V']);Frame(false,119);ck(!down[VK_LCONTROL]);ck(!down['V']);
 // Non-removing queue peeks must not duplicate characters.
 physical.fill(false);Frame(true,119);native.wParam='5';GetMessage13(HC_ACTION,0,reinterpret_cast<LPARAM>(&native));Frame(true,119);ck(ImGui::io.text.empty());
 imeOpen=true;physical['A']=true;Frame(true,119);clockNow=4;Frame(true,119);ck(ImGui::io.text.empty());
 native.wParam=0x6587;GetMessage13(HC_ACTION,PM_REMOVE,reinterpret_cast<LPARAM>(&native));Frame(true,119);ck(ImGui::io.text==u"文");
 Frame(false,119);physical.fill(false);imeOpen=false;Frame(true,119);
 NativeInput nativeInput;auto* manager=RE::BSInputDeviceManager::GetSingleton();manager->sinks.push_back(&nativeInput);
 InstallGameInput();InstallGameInput();ck(manager->adds==1);ck(manager->sinks.front()==&sink);
 auto* ui=RE::UI::GetSingleton();ck(ui->adds==1&&ui->sink==&dialogueSink);
 RE::MenuOpenCloseEvent menuEvent{"Dialogue Menu",true};ui->sink->ProcessEvent(&menuEvent,nullptr);ck(fc::closedDialogues14==0);
 menuEvent.opening=false;ui->sink->ProcessEvent(&menuEvent,nullptr);ck(fc::closedDialogues14==1);
 menuEvent.menuName="InventoryMenu";ui->sink->ProcessEvent(&menuEvent,nullptr);ck(fc::closedDialogues14==1);
 RE::ButtonEvent activate;activate.button=&activate;activate.down=activate.pressed=true;activate.device=RE::INPUT_DEVICE::kKeyboard;activate.id=51;
 auto* mapping=RE::ControlMap::GetSingleton();mapping->keys[0]=51;RE::InputEvent* activationHead=&activate;
 manager->Send(activationHead);ck(fc::activations14==1&&fc::lateActivations14==0&&nativeInput.calls==1);ck(activationHead==&activate);
 // Rebound keyboard, mouse and gamepad mappings work even before native user-event labeling.
 fc::nativeInputRan14=false;activate.device=RE::INPUT_DEVICE::kMouse;activate.id=2;manager->Send(activationHead);ck(fc::activations14==2&&nativeInput.calls==2);
 fc::nativeInputRan14=false;activate.device=RE::INPUT_DEVICE::kGamepad;activate.id=4096;manager->Send(activationHead);ck(fc::activations14==3&&nativeInput.calls==3);
 fc::nativeInputRan14=false;activate.device=RE::INPUT_DEVICE::kKeyboard;activate.id=18;manager->Send(activationHead);ck(fc::activations14==3); // Physical E is not assumed.
 activate.id=51;activate.down=false;activate.pressed=true;manager->Send(activationHead);ck(fc::activations14==3); // Held is not a new down edge.
 activate.pressed=false;manager->Send(activationHead);ck(fc::activations14==3); // Release cannot grant permission.
 fc::nativeInputRan14=false;activate.down=true;activate.id=RE::ControlMap::kInvalid;mapping->keys[0]=RE::ControlMap::kInvalid;manager->Send(activationHead);ck(fc::activations14==3);
 activate.device=RE::INPUT_DEVICE::kNone;manager->Send(activationHead);ck(fc::activations14==3);
 fc::nativeInputRan14=false;activate.device=RE::INPUT_DEVICE::kKeyboard;activate.id=999;activate.userEvent="Activate";manager->Send(activationHead);ck(fc::activations14==4&&fc::lateActivations14==0);
 activate.userEvent="Jump";manager->Send(activationHead);ck(fc::activations14==4);
 ck(sink.ProcessEvent(nullptr,nullptr)==RE::BSEventNotifyControl::kContinue);RE::InputEvent* empty=nullptr;ck(sink.ProcessEvent(&empty,nullptr)==RE::BSEventNotifyControl::kContinue);
 // F8 numeric editing reaches ImGui but not downstream native/mod hotkey sinks.
 Frame(false,119);physical.fill(false);ImGui::io.text.clear();Frame(true,119);
 RE::ButtonEvent digit;digit.button=&digit;digit.device=RE::INPUT_DEVICE::kKeyboard;digit.id=2;digit.down=digit.pressed=true;
 RE::InputEvent* digitHead=&digit;const auto beforeCapture=nativeInput.calls;const auto beforeIntent=fc::activations14;
 fc::overlayOpen14=true;physical['1']=true;manager->Send(digitHead);clockNow=5;Frame(true,119);clockNow=5.11;Frame(true,119);
 ck(ImGui::io.text==u"1"&&nativeInput.calls==beforeCapture&&fc::activations14==beforeIntent);ck(digitHead==&digit&&digit.next==nullptr);
 // Held/released inputs stay quarantined after the panel closes, then a fresh press works.
 fc::overlayOpen14=false;digit.down=false;manager->Send(digitHead);ck(nativeInput.calls==beforeCapture);
 digit.pressed=false;manager->Send(digitHead);ck(nativeInput.calls==beforeCapture);
 digit.down=digit.pressed=true;manager->Send(digitHead);ck(nativeInput.calls==beforeCapture+1);
 // A missing release cannot poison the next initial down after focus/device recovery.
 fc::overlayOpen14=true;manager->Send(digitHead);fc::overlayOpen14=false;manager->Send(digitHead);ck(nativeInput.calls==beforeCapture+2);
 // All nodes in a mixed batch are quarantined; no half-batch mutation or partial intent.
 fc::overlayOpen14=true;activate.device=RE::INPUT_DEVICE::kKeyboard;activate.userEvent="Activate";activate.down=activate.pressed=true;
 digit.next=&activate;manager->Send(digitHead);ck(fc::activations14==beforeIntent&&digit.next==&activate);
 fc::overlayOpen14=false;digit.down=activate.down=false;manager->Send(digitHead);ck(nativeInput.calls==beforeCapture+2);
 digit.pressed=activate.pressed=false;manager->Send(digitHead);ck(nativeInput.calls==beforeCapture+2);digit.next=nullptr;
 fc::overlayOpen14=true;
 RE::ButtonEvent scroll;scroll.button=&scroll;scroll.id=8;scroll.pressed=true;RE::InputEvent* head=&scroll;
 ck(sink.ProcessEvent(&head,nullptr)==RE::BSEventNotifyControl::kStop);ck(head==&scroll);
 Push({Event::Wheel,0,false,0,1});Frame(true,119);ck(ImGui::io.wheels.back().second==1);
 scroll.id=9;sink.ProcessEvent(&head,nullptr);Frame(true,119);ck(ImGui::io.wheels.back().second==-1);
 auto wheelCount=ImGui::io.wheels.size();scroll.id=0;sink.ProcessEvent(&head,nullptr);Frame(true,119);ck(ImGui::io.wheels.size()==wheelCount);
 // Non-button events are isolated too; neither pointer nor linkage is replaced.
 RE::InputEvent motion;RE::InputEvent* motionHead=&motion;ck(sink.ProcessEvent(&motionHead,nullptr)==RE::BSEventNotifyControl::kStop);ck(motionHead==&motion);
 fc::overlayOpen14=false;ck(sink.ProcessEvent(&motionHead,nullptr)==RE::BSEventNotifyControl::kContinue);
 // Closing/focus loss discards queued native characters, delayed fallback and modifiers.
 Frame(false,119);physical.fill(false);ImGui::io.text.clear();Frame(true,119);
 physical['3']=true;clockNow=8;Frame(true,119);ck(ImGui::io.text.empty());
 native.wParam='4';GetMessage13(HC_ACTION,PM_REMOVE,reinterpret_cast<LPARAM>(&native));
 Frame(false,119);ck(!ImGui::io.focused&&!down['3']&&textStream.pending.empty());
 physical.fill(false);clockNow=8.3;Frame(true,119);ck(ImGui::io.text.empty()&&ImGui::io.focused);
 Shutdown();ck(!Snapshot().messages);std::cout<<"PASS: "<<n<<" actual Input13/GameInput13 control-flow assertions with explicit Windows/ImGui/engine doubles, including mapped Activate, dialogue close generations, F8 capture and release quarantine.\n";
}
'''
with tempfile.TemporaryDirectory(prefix='fc-input13-') as tmp:
 t=Path(tmp);(t/'PCH.h').write_text(stub)
 for f in ('Windows.h','imgui.h','imm.h'):(t/f).write_text('#include "PCH.h"\n')
 (t/'Engine.hpp').write_text('#include "PCH.h"\n')
 for name in ('RE/U/UserEvents.h','RE/M/MenuOpenCloseEvent.h'):
  f=t/name;f.parent.mkdir(parents=True,exist_ok=True);f.write_text('#include "PCH.h"\n')
 for f in ('Input13.cpp','Input13.hpp','GameInput13.cpp'):shutil.copy2(R/'src'/f,t/f)
 (t/'test.cpp').write_text(test)
 cmd=[args.compiler,'-std=c++23','-Wall','-Wextra','-I'+str(t),'-I'+str(R/'include')]
 if args.sanitize:cmd+=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
 subprocess.run(cmd+[str(t/'test.cpp'),'-o',str(t/'test')],check=True)
 subprocess.run([str(t/'test')],check=True)
