"""Buildfix8 regression: content identities, plugin-only cache reset, and real PE linking.

The PE fixture links the actual BuildIdentity.cpp plus a tiny probe, NOT SKSE or
Skyrim code. Requires CMake, Python; two linking tests also require clang-cl/lld.
No downloaded dependencies. Test binaries stay in temporary directories.
"""
from pathlib import Path
import hashlib
import json
import os
import shutil
import subprocess
import tempfile
import unittest

from packaging_tests import PROJECT, CMAKE, BUILD_ID, copy_identity_sources, source_sha

CLANG = shutil.which('clang-cl')
LINKER = shutil.which('lld-link')
NINJA = shutil.which('ninja')


def run(args, *, cwd=None, okay=True):
    result = subprocess.run([str(a) for a in args], cwd=cwd, capture_output=True,
                            text=True, encoding='utf-8', errors='replace', timeout=60)
    if okay and result.returncode:
        raise AssertionError(result.stdout + result.stderr)
    return result


class IdentityTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix='fc-build8-')
        self.addCleanup(self.tmp.cleanup)
        self.base = Path(self.tmp.name)
        self.root = self.base / 'FreedomControl'
        copy_identity_sources(self.root)
        self.build = self.root / 'build/vs2026'
        self.build.mkdir(parents=True)
        self.cache = self.build / 'CMakeCache.txt'
        self.cache.write_text(f'CMAKE_HOME_DIRECTORY:INTERNAL={self.root.as_posix()}\n')

    def cmake_sha(self):
        script = self.base / 'identity.cmake'
        script.write_text('cmake_minimum_required(VERSION 3.25)\n'
                          f'include("{(self.root / "cmake/SourceIdentity.cmake").as_posix()}")\n'
                          'fc_source_identity("${ROOT}" _id _sha _paths)\n'
                          'message(STATUS "HASH=${_sha}")\n')
        result = run([CMAKE, f'-DROOT={self.root}', '-P', script])
        return result.stdout.split('HASH=')[1].splitlines()[0].strip()

    def reset(self, okay=True, root=None, build=None):
        return run([CMAKE, f'-DFC_ROOT={root or self.root}', f'-DFC_BUILD={build or self.build}',
                    '-P', PROJECT / 'cmake/ResetPluginCache.cmake'], okay=okay)

    def seed_outputs(self):
        self.objects = self.build / 'FreedomControl.dir/Release'
        self.objects.mkdir(parents=True)
        (self.objects / 'Plugin.obj').write_bytes(b'stale plugin')
        (self.objects / 'FreedomControl.pch').write_bytes(b'stale pch')
        (self.build / 'Release').mkdir()
        for ext in ('dll', 'lib', 'exp', 'pdb', 'ilk'):
            (self.build / f'Release/FreedomControl.{ext}').write_bytes(b'stale plugin output')
        self.keep = [self.cache,
                     self.build / '_commonlib/Release/CommonLibSSE.lib',
                     self.build / 'vcpkg_installed/downloads/keep.zip',
                     self.build / 'tests/Release/fc_core_tests.exe',
                     self.build / 'FreedomControl.vcxproj',
                     self.root / 'build-settings.json',
                     self.root / 'dist/previous-MO2.zip']
        for p in self.keep[1:]:
            p.parent.mkdir(parents=True, exist_ok=True)
            p.write_bytes(b'KEEP UNCHANGED')
        self.keep += list((self.root / 'src').glob('*'))
        self.before = {p: (p.read_bytes(), p.stat().st_mtime_ns) for p in self.keep}

    def test_01_python_and_cmake_content_hash_agree(self):
        self.assertEqual(self.cmake_sha(), source_sha(self.root))

    def test_02_timestamp_only_change_does_not_change_hash(self):
        before = self.cmake_sha()
        os.utime(self.root / 'src/Engine.cpp', (1, 1))
        self.assertEqual(before, self.cmake_sha())

    def test_03_content_change_with_old_timestamp_changes_hash(self):
        p = self.root / 'src/Engine.cpp'
        before = self.cmake_sha()
        p.write_bytes(p.read_bytes() + b'\n// edited\n')
        os.utime(p, (1, 1))
        self.assertNotEqual(before, self.cmake_sha())

    def test_04_path_change_does_not_change_hash(self):
        before = self.cmake_sha()
        new = self.base / 'path [x] & ! 100% 中文'
        self.root.rename(new)
        self.root = new
        self.assertEqual(before, self.cmake_sha())

    def test_05_new_header_changes_hash(self):
        before = self.cmake_sha()
        (self.root / 'include/fc/New.h').write_text('// new input\n')
        self.assertNotEqual(before, self.cmake_sha())

    def test_06_docs_do_not_force_plugin_rebuild(self):
        before = self.cmake_sha()
        (self.root / 'README_zh-CN.md').write_text('docs only\n')
        self.assertEqual(before, self.cmake_sha())

    def test_07_dependency_manifest_changes_hash(self):
        before = self.cmake_sha()
        p = self.root / 'vcpkg.json'
        p.write_bytes(p.read_bytes() + b'\n')
        self.assertNotEqual(before, self.cmake_sha())

    def test_08_reset_removes_only_plugin_release(self):
        self.seed_outputs()
        result = self.reset()
        self.assertIn('PLUGIN-ONLY RESET', result.stdout)
        self.assertFalse(self.objects.exists())
        self.assertFalse((self.build / 'Release/FreedomControl.dll').exists())
        for p, (data, timestamp) in self.before.items():
            self.assertEqual(p.read_bytes(), data, str(p))
            self.assertEqual(p.stat().st_mtime_ns, timestamp, str(p))

    def test_09_reset_with_no_objects_is_idempotent(self):
        self.reset()
        self.reset()
        self.assertTrue(self.cache.exists())

    def test_10_reset_rejects_foreign_root(self):
        self.seed_outputs()
        self.cache.write_text(f'CMAKE_HOME_DIRECTORY:INTERNAL={self.base.as_posix()}\n')
        self.assertNotEqual(self.reset(okay=False).returncode, 0)
        self.assertTrue((self.objects/'Plugin.obj').exists())

    def test_11_reset_rejects_outside_build(self):
        outside = self.base / 'otherbuild'
        shutil.copytree(self.build, outside)
        self.assertNotEqual(self.reset(okay=False, build=outside).returncode, 0)
        self.assertTrue((outside/'CMakeCache.txt').exists())

    def test_12_reset_rejects_directory_named_dll(self):
        self.seed_outputs()
        p = self.build/'Release/FreedomControl.dll'
        p.unlink()
        p.mkdir()
        self.assertNotEqual(self.reset(okay=False).returncode, 0)
        self.assertTrue((self.objects/'Plugin.obj').exists())

    @unittest.skipUnless(CLANG and LINKER, 'clang-cl/lld-link unavailable')
    def test_13_link_exact_identity_translation_unit(self):
        obj = self.base/'identity.obj'
        dll = self.base/'identity.dll'
        expected = source_sha(self.root)
        run([CLANG, '/nologo', '/c', '/WX', '/O2', '/GS-', '/Zl',
             f'/DFC_BUILD_ID="{BUILD_ID}"', f'/DFC_SOURCE_SHA256="{expected}"',
             f'/Fo{obj}', self.root/'src/BuildIdentity.cpp'])
        run([LINKER, '/dll', '/noentry', '/nodefaultlib', '/machine:x64', '/opt:ref',
             f'/out:{dll}', obj])
        data = dll.read_bytes()
        self.assertIn(BUILD_ID.encode()+b'\0', data)
        self.assertIn(('FC-SOURCE-SHA256:'+expected).encode()+b'\0', data)
        self.assertIn(b'FreedomControl_BuildID\0', data)
        self.assertIn(b'FreedomControl_SourceSHA256\0', data)

    @unittest.skipUnless(CLANG and LINKER and NINJA, 'clang-cl/lld-link/Ninja unavailable')
    def test_14_incremental_build_recompiles_backdated_changed_source(self):
        # Same fc_apply_build_identity() function as the real plugin CMakeLists.
        harness = self.base/'harness'
        harness.mkdir()
        probe = self.root/'src/compile_probe.cpp'
        probe.write_text('extern "C" __declspec(dllexport) const char ProbeData[] = "PROBE:before";\n')
        toolchain = harness/'toolchain.cmake'
        toolchain.write_text(
            'set(CMAKE_SYSTEM_NAME Windows)\nset(CMAKE_SYSTEM_PROCESSOR AMD64)\n'
            f'set(CMAKE_CXX_COMPILER "{Path(CLANG).as_posix()}")\n'
            f'set(CMAKE_LINKER "{Path(LINKER).as_posix()}")\n'
            'set(CMAKE_CXX_COMPILER_WORKS TRUE)\n'
            'set(CMAKE_CXX_STANDARD_LIBRARIES "" CACHE STRING "" FORCE)\n'
            'set(CMAKE_SHARED_LINKER_FLAGS "/nodefaultlib /noentry /manifest:no /incremental:no" CACHE STRING "" FORCE)\n')
        (harness/'CMakeLists.txt').write_text(
            'cmake_minimum_required(VERSION 3.25)\nproject(Fixture LANGUAGES CXX)\n'
            'include("${FC_SOURCE}/cmake/SourceIdentity.cmake")\n'
            'add_library(Fixture SHARED "${FC_SOURCE}/src/compile_probe.cpp")\n'
            'fc_apply_build_identity(Fixture "${FC_SOURCE}")\n')
        out = harness/'build'
        configure = [CMAKE, '-S', harness, '-B', out, '-G', 'Ninja',
                     '-DCMAKE_BUILD_TYPE=Release', f'-DCMAKE_TOOLCHAIN_FILE={toolchain}',
                     f'-DFC_SOURCE={self.root}']
        run(configure)
        run([CMAKE, '--build', out, '-v'])
        dll = out/'Fixture.dll'
        self.assertIn(b'PROBE:before\0', dll.read_bytes())
        before_sha = source_sha(self.root)
        # The changed source is now much older than its .obj, reproducing a ZIP overlay.
        probe.write_text('extern "C" __declspec(dllexport) const char ProbeData[] = "PROBE:after!";\n')
        os.utime(probe, (1, 1))
        run(configure)  # BUILD.bat always explicitly configures first.
        rebuilt = run([CMAKE, '--build', out, '-v'])
        data = dll.read_bytes()
        self.assertIn(b'PROBE:after!\0', data)
        self.assertNotIn(b'PROBE:before\0', data)
        self.assertNotEqual(before_sha, source_sha(self.root))
        self.assertIn(('FC-SOURCE-SHA256:'+source_sha(self.root)).encode()+b'\0', data)
        self.assertIn('compile_probe.cpp', rebuilt.stdout)
        # An unchanged third build must remain incremental.
        run(configure)
        no_work = run([CMAKE, '--build', out, '-v'])
        self.assertIn('no work to do', no_work.stdout)


if __name__ == '__main__':
    result = unittest.TextTestRunner(verbosity=2).run(
        unittest.defaultTestLoader.loadTestsFromTestCase(IdentityTests))
    report = {'tests_run': result.testsRun, 'failures': len(result.failures),
              'errors': len(result.errors), 'skipped': len(result.skipped),
              'scope': 'Content hash and actual CMake reset; SDK-free clang-cl/lld identity fixture, not the SKSE plugin.',
              'cmake': CMAKE, 'clang_cl': CLANG, 'lld_link': LINKER, 'host_os': os.name}
    if os.environ.get('FC_TEST_REPORT'):
        Path(os.environ['FC_TEST_REPORT']).write_text(json.dumps(report, indent=2)+'\n')
    raise SystemExit(0 if result.wasSuccessful() else 1)
