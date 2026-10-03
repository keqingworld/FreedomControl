"""Compile the actual Menu class against production Engine.hpp and explicit test doubles.
Syntax / action aggregate coverage only; NOT real ImGui, Windows, rendering or input.
The original source and its include paths are never edited by this harness.
"""
from pathlib import Path
import argparse, re, shutil, subprocess, tempfile
ROOT=Path(__file__).resolve().parents[1]
def main():
    p=argparse.ArgumentParser();p.add_argument('--compiler',default='g++');p.add_argument('--fmt-include',help='Path containing real fmt/format.h');args=p.parse_args()
    compiler=shutil.which(args.compiler)
    if not compiler:raise SystemExit('Compiler not found')
    source=(ROOT/'src/Overlay.cpp').read_text(encoding='utf-8')
    menu='struct Menu {'+source.split('struct Menu {',1)[1].split('\nMenu menu;',1)[0]
    names=set(re.findall(r'ImGui::(\w+)',menu))
    specialized={'GetIO','GetContentRegionAvail','GetTextLineHeightWithSpacing'}
    stub='''
#include "PCH.h"
#include "Engine.hpp"
struct ImVec2 {float x,y; constexpr ImVec2(float a=0,float b=0):x(a),y(b){}};
struct ImGuiListClipper {int DisplayStart{},DisplayEnd{};void Begin(int){} bool Step(){return false;}};
constexpr int ImGuiCond_Always=1,ImGuiTreeNodeFlags_DefaultOpen=1;
constexpr int ImGuiTableFlags_BordersInnerV=1,ImGuiTableFlags_Borders=1,ImGuiTableFlags_RowBg=2,ImGuiTableFlags_ScrollY=4,ImGuiTableFlags_Resizable=8,ImGuiSelectableFlags_SpanAllColumns=1,ImGuiTableColumnFlags_WidthFixed=1;
namespace ImGui {
struct IO {ImVec2 DisplaySize{1920,1080};float FontGlobalScale{1};};
inline IO& GetIO(){static IO io;return io;}
inline ImVec2 GetContentRegionAvail(){return {1600,900};}
inline float GetTextLineHeightWithSpacing(){return 24;}
'''
    for name in sorted(names-specialized):
        stub+=f'template<class... Args> bool {name}(Args&&...){{return false;}}\n'
    stub+='}\nnamespace fc::input13 { struct Counters {unsigned long long keys{},characters{},wheels{};bool messages{},mouse{};};inline Counters Snapshot(){return {};} }\nnamespace fc::overlay {std::uint64_t polledMousePresses{};\n'+menu+'\n}\n'
    with tempfile.TemporaryDirectory(prefix='fc-ui-syntax-') as temp:
        t=Path(temp);shutil.copy2(ROOT/'tests/seam/PCH.h',t/'PCH.h');shutil.copy2(ROOT/'src/Engine.hpp',t/'Engine.hpp');shutil.copy2(ROOT/'src/CrimeHooks11.hpp',t/'CrimeHooks11.hpp')
        (t/'menu.cpp').write_text(stub,encoding='utf-8')
        cmd=[compiler,'-std=c++23','-fsyntax-only','-I'+str(t),'-I'+str(ROOT/'include')]
        if args.fmt_include:
            if not (Path(args.fmt_include)/'fmt/format.h').is_file():raise SystemExit('Real fmt/format.h was not found at --fmt-include')
            cmd+=['-DFC_REAL_FMT11','-I'+args.fmt_include]
        subprocess.run(cmd+[str(t/'menu.cpp')],check=True,timeout=40)
    print('PASS: production Menu class and Engine.hpp agree under explicit UI/engine stubs. '+('Real fmt format checking enabled. ' if args.fmt_include else 'Format calls use a permissive test double. ')+'NOT real ImGui/Windows API compilation.')
if __name__=='__main__':main()
