"""Compile exact production pause/Tick/lifecycle methods with explicit host boundaries.
The doubles model queued native IMenu ownership, separate UI/game task queues and
pause counts. They cannot prove Skyrim's real scheduler, ABI, AI or animation pause.
"""
from pathlib import Path
import argparse, hashlib, re, shutil, subprocess, tempfile, zipfile
ROOT = Path(__file__).resolve().parents[1]

def method(source, signature):
    start = source.index(signature)
    body = source.index('{', start)
    depth = 1
    end = body + 1
    # Selected production bodies have no braces in string literals/comments.
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', default='g++')
    parser.add_argument('--fmt-include', required=True)
    parser.add_argument('--sanitize', action='store_true')
    parser.add_argument('--baseline-sync-input', type=Path, help='Optional immutable prior release ZIP for a negative-control replay')
    args = parser.parse_args()
    source = (ROOT / 'src/Engine.cpp').read_text()
    selected = ['Engine& Engine::Get()', 'void Engine::Submit(', 'void Engine::SetMenuOpen(',
                'void Engine::PumpPause15(', 'void Engine::RequestTick(',
                'void Engine::Tick(', 'void Engine::FinishClose(',
                'bool Engine::NativePauseOpen15(', 'bool Engine::NativePauseConfirmed15(', 'bool Engine::OtherNativePause15(',
                'void Engine::SyncNativePause15(', 'void Engine::SyncInput(',
                'void Engine::RestoreTransient(', 'void Engine::Message(']
    code = '#include "PCH.h"\n#include "Engine.hpp"\nnamespace fc {\nusing Clock=std::chrono::steady_clock;\n'
    code += method(source, 'class PauseMenu15 final') + ';\n'
    # Audio engine is an explicit host boundary in this pause seam; AudioMuteLease16 is covered by runtime_tests.
    code += 'void Engine::SyncAudioFreeze16(){}\n'
    code += '\n'.join(method(source, sig) for sig in selected) + '\n}\n'
    overlay = (ROOT/'src/Overlay.cpp').read_text()
    code += '#include "fc/HotkeyState.hpp"\nnamespace fc::overlay {\n'
    code += 'HWND window=reinterpret_cast<HWND>(1); HotkeyState hotkeyState; bool focusKnown{},wasFocused{},menuDrawLogged{},drawFailureLogged{};\n'
    code += method(overlay, 'void PollMenuHotkey()') + '\n}\n'
    # Mandatory policy must survive both old persistence entry points. These are
    # static assertions; their runtime JSON/INI parsing is NOT covered here.
    assert 'pauseMenu_=rt.value("pauseMenu"' not in source
    assert 'pauseLease_.Update(capture && !NativePauseConfirmed15(),paused)' in source
    assert 'case Op::PauseMenu: pauseMenu_=true;' in source
    assert 'L"PauseWorld"' not in source
    assert 'Check("打开面板时暂停世界"' not in (ROOT/'src/Overlay.cpp').read_text()
    assert not re.search(r'numPausesGame\s*(?:[+\-]?=|\+\+|--)|(?:\+\+|--)\s*\w+->numPausesGame', source)
    with tempfile.TemporaryDirectory(prefix='fc-pause15-') as temporary:
        t = Path(temporary)
        pch = (ROOT / 'tests/seam/PCH.h').read_text()
        pch = pch.replace('struct MessagingInterface { struct Message; };', '''struct MessagingInterface {
            struct Message {std::uint32_t type{};void* data{};};
            enum {kDataLoaded,kPreLoadGame,kNewGame,kPostLoadGame};
        };''')
        # Replace only engine boundary doubles, never selected production code.
        pch = re.sub(r'struct UI\{.*?\};\nstruct MenuTopicManager', 'struct UI;\nstruct MenuTopicManager', pch)
        pch = re.sub(r'enum class UI_MESSAGE_TYPE\{.*?\};\nstruct UIMessageQueue\{.*?\};\nstruct CrosshairPickData', 'struct CrosshairPickData', pch)
        pch = pch.replace('struct ProcessLists {', 'struct ProcessLists {bool runDetection{true};')
        pch += '\n#include "pause_boundaries.hpp"\n'
        (t/'PCH.h').write_text(pch)
        shutil.copy2(ROOT/'tests/seam/pause_boundaries.hpp', t/'pause_boundaries.hpp')
        (t/'Engine.hpp').write_text((ROOT/'src/Engine.hpp').read_text().replace('private:', 'public:'))
        shutil.copy2(ROOT/'src/CrimeHooks11.hpp', t/'CrimeHooks11.hpp')
        (t/'production_pause.cpp').write_text(code)
        shutil.copy2(ROOT/'tests/seam/pause_seam.cpp',t/'pause_seam.cpp')
        cmd = [args.compiler, '-std=c++23', '-Wall', '-Wextra', '-Wno-missing-field-initializers',
               '-g', '-DFC_REAL_FMT11', '-I'+str(t), '-I'+str(ROOT/'include'), '-I'+args.fmt_include]
        if args.sanitize:
            cmd += ['-fsanitize=address,undefined', '-fno-omit-frame-pointer']
        cmd += [str(t/'production_pause.cpp'), str(t/'pause_seam.cpp'),
                str(ROOT/'src/Core.cpp'), '-o', str(t/'pause_test')]
        subprocess.run(cmd, check=True, timeout=90)
        subprocess.run([str(t/'pause_test')], check=True, timeout=30)
        if args.baseline_sync_input:
            with zipfile.ZipFile(args.baseline_sync_input) as archive:
                old_source = archive.read('FreedomControl/src/Engine.cpp').decode('utf-8')
            old_sync = method(old_source, 'void Engine::SyncInput(')
            assert 'capture && pauseMenu_' in old_sync, 'Baseline does not contain the prior optional-pause implementation'
            # Replay the exact prior SyncInput under the same boundaries. Do not
            # modify the actual source tree or accept a compile error as a catch.
            (t/'production_pause.cpp').write_text(code.replace(method(source, 'void Engine::SyncInput('),old_sync))
            subprocess.run(cmd, check=True, timeout=90)
            negative = subprocess.run([str(t/'pause_test')], cwd=t, capture_output=True, text=True, timeout=30)
            assert negative.returncode != 0 and 'Pause seam #1 ' in negative.stderr, negative.stdout + negative.stderr
            print('PASS negative control: exact prior SyncInput rejected at mandatory pause assertion #1; baseline SHA256 '+hashlib.sha256(args.baseline_sync_input.read_bytes()).hexdigest())

if __name__ == '__main__':
    main()
