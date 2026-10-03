"""Exercise the actual CMake packager using TEST-ONLY PE fixtures, not Skyrim.

Python 3.9+ and CMake 3.25+ are needed only to run these regression tests.
No network, SDK, PowerShell module, or game is required for packaging itself.
"""
from __future__ import annotations
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import tempfile
import unittest
import zipfile

PROJECT = Path(__file__).resolve().parents[1]
SCRIPT = PROJECT / 'cmake' / 'PackageRuntime.cmake'
CMAKE = os.environ.get('CMAKE_EXE') or shutil.which('cmake')


def sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


BUILD_ID = (PROJECT / 'LATEST_BUILD_ID.txt').read_text().strip()
_release = re.fullmatch(r'FC-(\d+\.\d+\.\d+)-([A-Z0-9]+)-\d{8}-[A-Z0-9]+', BUILD_ID)
if not _release:
    raise RuntimeError('LATEST_BUILD_ID.txt has an unrecognized release identity')
ZIP_NAME = f'FreedomControl-{_release[1]}-{_release[2]}-MO2.zip'


def source_sha(root: Path) -> str:
    paths = {'CMakeLists.txt', 'LATEST_BUILD_ID.txt', 'vcpkg.json', 'cmake/SourceIdentity.cmake', 'package/FreedomControlRuntime.esp', 'package/SKSE/Plugins/FreedomControl.creatures.json'}
    for folder in ('src', 'include'):
        paths.update(p.relative_to(root).as_posix() for p in (root / folder).rglob('*')
                     if p.is_file() and p.suffix in ('.cpp', '.h', '.hpp'))
    manifest = 'FreedomControl-source-sha256-v1\n' + ''.join(
        f'{p}={sha((root / p).read_bytes())}\n' for p in sorted(paths))
    return sha(manifest.encode('utf-8'))


def copy_identity_sources(root: Path) -> None:
    root.mkdir(parents=True, exist_ok=True)
    for name in ('src', 'include'):
        shutil.copytree(PROJECT / name, root / name, dirs_exist_ok=True)
    shutil.copytree(PROJECT / 'package', root / 'package', dirs_exist_ok=True)
    (root / 'cmake').mkdir(exist_ok=True)
    for name in ('CMakeLists.txt', 'LATEST_BUILD_ID.txt', 'vcpkg.json', 'cmake/SourceIdentity.cmake'):
        shutil.copy2(PROJECT / name, root / name)


def pe_fixture() -> bytes:
    # A deliberately minimal header fixture. Never a working game plugin.
    supplied = os.environ.get('FC_TEST_PE_FILE')
    if supplied:
        return Path(supplied).read_bytes()
    data = bytearray(1024)
    data[:2] = b'MZ'
    struct.pack_into('<I', data, 60, 128)
    data[128:132] = b'PE\x00\x00'
    struct.pack_into('<HH', data, 132, 0x8664, 1)
    struct.pack_into('<HH', data, 148, 240, 0x2022)
    struct.pack_into('<H', data, 152, 0x20B)
    data[700:732] = b'PACKAGING TEST ONLY - NOT SKYRIM!!'
    marker=BUILD_ID.encode('ascii')
    data[800:800+len(marker)]=marker
    fingerprint=('FC-SOURCE-SHA256:'+source_sha(PROJECT)).encode('ascii')
    data[900:900+len(fingerprint)] = fingerprint
    return bytes(data)


class PackagingTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not CMAKE:
            raise RuntimeError('CMake is required for packaging regression tests')
        cls.original_pe = pe_fixture()
        cls.pe_offset = struct.unpack_from('<I', cls.original_pe, 60)[0]

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix='fc-pack-test-')
        self.addCleanup(self.tmp.cleanup)
        self.base = Path(self.tmp.name)
        self.make_tree('FreedomControl')

    def make_tree(self, folder):
        self.root = self.base / folder
        copy_identity_sources(self.root)
        self.build = self.root / 'build' / 'vs2026'
        self.dll = self.build / 'Release' / 'FreedomControl.dll'
        self.dll.parent.mkdir(parents=True)
        self.dll.write_bytes(self.original_pe)
        self.ini = self.root / 'package/SKSE/Plugins/FreedomControl.ini'
        self.ini.parent.mkdir(parents=True,exist_ok=True)
        self.ini.write_bytes(b'[Input]\r\nMenuVirtualKey=119\r\n')
        self.lib = self.base / 'CommonLib Fixture'
        self.lib.mkdir(exist_ok=True)
        (self.lib / 'LICENSE').write_text('Test fixture notice. Not actual CommonLib.\n')
        self.share = self.build / 'vcpkg_installed/x64-windows-static-md/share/fmt'
        self.share.mkdir(parents=True)
        (self.share / 'copyright').write_text('Fixture-only dependency notice.\n')
        self.cache = self.build / 'CMakeCache.txt'
        self.cache.write_text(
            f'CMAKE_HOME_DIRECTORY:INTERNAL={self.root.as_posix()}\n'
            f'CMAKE_COMMAND:INTERNAL={Path(CMAKE).as_posix()}\n'
            f'COMMONLIBSSE_PATH:PATH={self.lib.as_posix()}\n'
            f'CMAKE_GENERATOR:INTERNAL=Visual Studio 18 2026\n'
            '# Entries above are test metadata, not a real Windows build.\n',
            encoding='utf-8')
        for name in ('README_zh-CN.md', 'FEATURES_zh-CN.md', 'LICENSE.txt', 'BUILD_STATUS.md'):
            (self.root / name).write_text('Packaging regression fixture only.\n', encoding='utf-8')
        (self.root / 'docs').mkdir()
        (self.root / 'docs/README.md').write_text('fixture\n')
        (self.root / 'build-settings.json').write_text('{"Jobs":3}\n')
        (self.root / 'commonlib.lock.txt').write_text('a898f469851c464d05137bb74b069dd234897643\n')
        self.dist = self.root / 'dist'
        self.dist.mkdir()
        self.output = self.dist / ZIP_NAME

    def run_pack(self, success=True, extra=(), choose_build=False):
        args = [CMAKE, f'-DFC_ROOT={self.root}']
        if choose_build:
            args.append(f'-DFC_BUILD={self.build}')
        args += list(extra) + ['-P', str(SCRIPT)]
        result = subprocess.run(args, text=True, encoding='utf-8', errors='replace',
                                capture_output=True, timeout=40)
        if success:
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn('PACKAGE COMPLETE:', result.stdout)
        else:
            self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
        return result

    def contents(self):
        with zipfile.ZipFile(self.output) as z:
            self.assertIsNone(z.testzip())
            return {n: z.read(n) for n in z.namelist() if not n.endswith('/')}

    def fail_preserving_release(self):
        self.output.write_bytes(b'EXISTING-RELEASE-MUST-REMAIN')
        self.run_pack(False)
        self.assertEqual(self.output.read_bytes(), b'EXISTING-RELEASE-MUST-REMAIN')

    def corrupt(self, offset, fmt, value):
        data = bytearray(self.dll.read_bytes())
        struct.pack_into(fmt, data, offset, value)
        self.dll.write_bytes(data)

    def test_01_real_script_zip_layout_and_bytes(self):
        self.run_pack()
        c = self.contents()
        self.assertEqual(c['SKSE/Plugins/FreedomControl.dll'], self.original_pe)
        self.assertEqual(c['SKSE/Plugins/FreedomControl.ini'], self.ini.read_bytes())
        self.assertTrue(all(n=='FreedomControlRuntime.esp' or n.startswith(('SKSE/', 'FreedomControl-docs/')) for n in c))
        self.assertFalse(any('..' in Path(n).parts or n.startswith('/') or '\\' in n for n in c))

    def test_02_hashes_and_default_status(self):
        self.run_pack()
        info = self.contents()['FreedomControl-docs/BUILD-INFO.txt'].decode()
        self.assertIn(sha(self.original_pe), info)
        self.assertIn(sha(self.ini.read_bytes()), info)
        self.assertIn('NOT_RERUN', info)
        self.assertNotIn('PASSED_IN_THIS_BUILD', info)
        line = Path(str(self.output) + '.sha256').read_text().strip()
        self.assertEqual(line, sha(self.output.read_bytes()) + '  ' + ZIP_NAME)

    def test_03_explicit_pass_status(self):
        self.run_pack(extra=['-DFC_TEST_STATUS=PASSED_IN_THIS_BUILD: fixture status only'])
        self.assertIn(b'PASSED_IN_THIS_BUILD: fixture status only',
                      self.contents()['FreedomControl-docs/BUILD-INFO.txt'])

    def test_04_spaces_in_path(self):
        self.make_tree('New Folder With Spaces')
        self.run_pack()
        self.contents()

    def test_05_unicode_path(self):
        self.make_tree('\u4e2d\u6587\u9879\u76ee \u6d4b\u8bd5')
        self.run_pack()
        self.contents()

    def test_06_brackets_and_shell_metacharacters(self):
        self.make_tree("Project [test] & ! 100% (x)=1' $ "+'\u4e2d\u6587')
        self.run_pack(choose_build=True)
        self.contents()

    def test_07_dependency_path_with_brackets(self):
        new = self.base / 'Dependencies [test]'
        shutil.copytree(self.build / 'vcpkg_installed', new)
        with self.cache.open('a') as f:
            f.write(f'VCPKG_INSTALLED_DIR:PATH={new.as_posix()}\n')
        self.run_pack()
        self.assertIn('FreedomControl-docs/third-party/fmt-copyright.txt', self.contents())

    def test_08_vs2022_fallback(self):
        target = self.root / 'build/vs2022'
        self.build.rename(target)
        self.build = target
        self.run_pack()
        self.contents()

    def test_09_vs2026_preferred_when_both_exist(self):
        target = self.root / 'build/vs2022'
        shutil.copytree(self.build, target)
        (target / 'Release/FreedomControl.dll').write_bytes(self.original_pe + b'older build')
        self.run_pack()
        self.assertEqual(self.contents()['SKSE/Plugins/FreedomControl.dll'], self.original_pe)

    def test_10_explicit_build_directory(self):
        target = self.root / 'build/custom'
        self.build.rename(target)
        self.build = target
        self.run_pack(choose_build=True)
        self.contents()

    def test_11_missing_dll(self):
        self.dll.unlink()
        self.fail_preserving_release()

    def test_12_missing_ini(self):
        self.ini.unlink()
        self.fail_preserving_release()

    def test_13_not_pe(self):
        self.dll.write_bytes(b'NOT A PE' + bytes(1024))
        self.fail_preserving_release()

    def test_14_short_pe(self):
        self.dll.write_bytes(b'MZ')
        self.fail_preserving_release()

    def test_15_wrong_architecture(self):
        self.corrupt(self.pe_offset + 4, '<H', 0x14C)
        self.fail_preserving_release()

    def test_16_missing_dll_flag(self):
        self.corrupt(self.pe_offset + 22, '<H', 0x22)
        self.fail_preserving_release()

    def test_17_e_lfanew_out_of_bounds(self):
        self.corrupt(60, '<I', 0xFFFFFFFF)
        self.fail_preserving_release()

    def test_18_wrong_optional_magic(self):
        self.corrupt(self.pe_offset + 24, '<H', 0x10B)
        self.fail_preserving_release()

    def test_19_zero_sections(self):
        self.corrupt(self.pe_offset + 6, '<H', 0)
        self.fail_preserving_release()

    def test_20_truncated_sections(self):
        self.corrupt(self.pe_offset + 20, '<H', 65535)
        self.fail_preserving_release()

    def test_21_foreign_source_cache(self):
        self.cache.write_text('CMAKE_HOME_DIRECTORY:INTERNAL=/some/other/project\n')
        self.fail_preserving_release()

    def test_22_missing_commonlib_notice(self):
        (self.lib / 'LICENSE').unlink()
        self.fail_preserving_release()

    def test_23_missing_dependency_notices(self):
        shutil.rmtree(self.build / 'vcpkg_installed')
        self.fail_preserving_release()

    def test_24_missing_docs(self):
        (self.root / 'FEATURES_zh-CN.md').unlink()
        self.fail_preserving_release()

    def test_25_repack_replaces_zip_cleanly(self):
        self.run_pack()
        self.dll.write_bytes(self.original_pe + b'updated fixture')
        self.run_pack()
        self.assertEqual(self.contents()['SKSE/Plugins/FreedomControl.dll'], self.dll.read_bytes())
        self.assertEqual(len(list(self.dist.glob('.fc-package-*'))), 0)

    def test_26_preserves_compiled_cache_settings_and_old_stage(self):
        objects = self.build / '_commonlib/Release/CommonLibSSE.lib'
        objects.parent.mkdir(parents=True)
        objects.write_bytes(b'EXISTING LIB DO NOT TOUCH')
        oldstage = self.dist / 'MO2/untouched.txt'
        oldstage.parent.mkdir()
        oldstage.write_text('unchanged')
        before = {p: p.read_bytes() for p in self.root.rglob('*') if p.is_file()}
        self.run_pack()
        for p, data in before.items():
            self.assertEqual(p.read_bytes(), data, str(p))
        self.assertFalse(any('untouched' in n for n in self.contents()))

    def test_27_no_powershell_or_get_filehash_dependency(self):
        # A real run succeeds even when PATH contains no PowerShell, Git, or compiler.
        args = [CMAKE, f'-DFC_ROOT={self.root}', '-P', str(SCRIPT)]
        env = dict(os.environ)
        empty = self.base / 'empty-path'
        empty.mkdir()
        env['PATH'] = str(empty)
        r = subprocess.run(args, capture_output=True, timeout=40, env=env)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        self.contents()

    def test_28_cache_is_not_executed(self):
        with self.cache.open('a') as f:
            f.write('\nmessage(FATAL_ERROR "CACHE MUST NOT EXECUTE")\n')
        self.run_pack()
        self.contents()

    def test_29_sidecar_directory_does_not_fail_valid_zip(self):
        Path(str(self.output) + '.sha256').mkdir()
        result = self.run_pack()
        self.assertIn('sidecar', result.stderr)
        self.contents()

    def test_30_zip_publish_failure_preserves_existing_directory(self):
        self.output.mkdir()
        keep = self.output / 'must-not-delete'
        keep.write_text('preserve')
        self.run_pack(False)
        self.assertEqual(keep.read_text(), 'preserve')

    def test_31_semicolon_path_rejected_before_output(self):
        self.make_tree('Project;unsupported')
        self.fail_preserving_release()

    def test_32_backslashes_in_cache_paths(self):
        with self.cache.open('a') as f:
            # Explicit installed path ensures separator normalization is exercised.
            path = str(self.build / 'vcpkg_installed').replace('/', '\\')
            f.write(f'VCPKG_INSTALLED_DIR:PATH={path}\n')
        self.run_pack()
        self.contents()

    def test_33_bad_pe_signature(self):
        self.corrupt(self.pe_offset, '<I', 0)
        self.fail_preserving_release()

    def test_34_missing_cache(self):
        self.cache.unlink()
        self.fail_preserving_release()

    def test_35_commonlib_notice_filenames_with_brackets(self):
        new = self.base / 'CommonLib [fixture]'
        self.lib.rename(new)
        self.cache.write_text(self.cache.read_text().replace(self.lib.as_posix(), new.as_posix()))
        self.run_pack()
        self.contents()



    def test_36_actual_commonlib_notice_layout(self):
        (self.lib / 'LICENSE').unlink()
        (self.lib / 'COPYING.txt').write_text('COPYING fixture, not actual license text.\n')
        (self.lib / 'EXCEPTIONS.md').write_text('Exception fixture.\n')
        (self.lib / 'licenses').mkdir()
        (self.lib / 'licenses/MIT.txt').write_text('Nested notice fixture.\n')
        self.run_pack()
        c = self.contents()
        self.assertIn('FreedomControl-docs/third-party/CommonLib-COPYING.txt', c)
        self.assertIn('FreedomControl-docs/third-party/CommonLib-EXCEPTIONS.md', c)
        self.assertIn('FreedomControl-docs/third-party/CommonLib-licenses/MIT.txt', c)

    def test_37_notice_directory_without_root_license(self):
        (self.lib / 'LICENSE').unlink()
        (self.lib / 'licenses').mkdir()
        (self.lib / 'licenses/NOTICE.txt').write_text('Directory-only notice fixture.\n')
        self.run_pack()
        self.assertIn('FreedomControl-docs/third-party/CommonLib-licenses/NOTICE.txt', self.contents())

    def test_38_dot_suffix_source_path(self):
        # PACKAGE_ONLY.bat passes %~dp0. so the path never ends in a quoted backslash.
        self.run_pack(extra=[f'-DFC_ROOT={self.root}/.'], choose_build=True)
        self.contents()

    def test_39_missing_release_marker_rejected(self):
        data=self.dll.read_bytes().replace(BUILD_ID.encode(), b"X"*len(BUILD_ID))
        self.dll.write_bytes(data)
        self.fail_preserving_release()

    def test_40_wrong_release_marker_rejected(self):
        wrong = (BUILD_ID[:-1] + ("Z" if BUILD_ID[-1] != "Z" else "Y")).encode()
        data=self.dll.read_bytes().replace(BUILD_ID.encode(),wrong)
        self.dll.write_bytes(data)
        self.fail_preserving_release()

    def test_41_build_info_reports_checked_marker(self):
        self.run_pack()
        self.assertIn(BUILD_ID.encode('ascii'),self.contents()["FreedomControl-docs/BUILD-INFO.txt"])

    def test_42_missing_source_hash_rejected(self):
        self.dll.write_bytes(self.dll.read_bytes().replace(b'FC-SOURCE-SHA256:', b'XX-SOURCE-SHA256:'))
        self.fail_preserving_release()

    def test_43_changed_source_with_old_mtime_rejected(self):
        p = self.root / 'src/Engine.cpp'
        mtime = p.stat().st_mtime
        p.write_bytes(p.read_bytes() + b'\n// changed contents, same archive timestamp\n')
        os.utime(p, (mtime, mtime))
        self.fail_preserving_release()

    def test_44_literal_id_not_regex(self):
        marker = BUILD_ID.encode()
        self.dll.write_bytes(self.dll.read_bytes().replace(marker, marker.replace(b'.', b'X')))
        self.fail_preserving_release()

    def test_45_build_id_must_be_null_terminated(self):
        self.dll.write_bytes(self.dll.read_bytes().replace(BUILD_ID.encode()+b'\0', BUILD_ID.encode()+b'X'))
        self.fail_preserving_release()

    def test_46_same_bytes_new_timestamp_accepted(self):
        os.utime(self.root / 'src/Engine.cpp', (1, 1))
        self.run_pack()

    def test_47_missing_identity_source_rejected(self):
        (self.root / 'src/BuildIdentity.cpp').unlink()
        self.fail_preserving_release()

    def test_48_info_records_source_fingerprint(self):
        self.run_pack()
        info = self.contents()['FreedomControl-docs/BUILD-INFO.txt'].decode()
        self.assertIn('Verified source SHA256: '+source_sha(self.root), info)

    def test_49_error_explains_exact_input_and_repair(self):
        self.dll.write_bytes(self.dll.read_bytes().replace(BUILD_ID.encode(), b'X'*len(BUILD_ID)))
        r = self.run_pack(False)
        self.assertIn('FC_IDENTITY_MISMATCH', r.stderr)
        self.assertIn('REPAIR_BUILD8.bat', r.stderr)
        self.assertIn('FreedomControl.dll', r.stderr)

    def test_50_check_only_does_not_package(self):
        self.output.write_bytes(b'EXISTING')
        r = subprocess.run([CMAKE, f'-DFC_ROOT={self.root}', '-DFC_CHECK_ONLY=ON',
                            '-P', str(SCRIPT)], capture_output=True, text=True, timeout=40)
        self.assertEqual(r.returncode, 0, r.stdout+r.stderr)
        self.assertIn('SOURCE_SHA256_OK', r.stdout)
        self.assertEqual(self.output.read_bytes(), b'EXISTING')

    def test_51_runtime_and_catalog_inside_zip(self):
        self.run_pack()
        c=self.contents()
        for name in ('FreedomControlRuntime.esp','SKSE/Plugins/FreedomControl.creatures.json'):
            self.assertEqual(c[name],(self.root/'package'/name).read_bytes())

    def test_52_missing_runtime_esp_rejected(self):
        (self.root/'package/FreedomControlRuntime.esp').unlink()
        self.fail_preserving_release()

    def test_53_missing_creature_manifest_rejected(self):
        (self.root/'package/SKSE/Plugins/FreedomControl.creatures.json').unlink()
        self.fail_preserving_release()

    def test_54_changed_runtime_records_require_recompile(self):
        p=self.root/'package/FreedomControlRuntime.esp'
        p.write_bytes(p.read_bytes()+b'edited-runtime')
        self.fail_preserving_release()

    def test_55_changed_catalog_requires_recompile(self):
        p=self.root/'package/SKSE/Plugins/FreedomControl.creatures.json'
        p.write_bytes(p.read_bytes()+b' ')
        self.fail_preserving_release()

    def test_56_asset_hashes_in_build_info(self):
        self.run_pack()
        info=self.contents()['FreedomControl-docs/BUILD-INFO.txt'].decode()
        for name in ('FreedomControlRuntime.esp','SKSE/Plugins/FreedomControl.creatures.json'):
            self.assertIn(sha((self.root/'package'/name).read_bytes()),info)
        self.assertIn('Enable FreedomControlRuntime.esp',info)


if __name__ == '__main__':
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(PackagingTests)
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    report = {'scope': 'Executed actual CMake packaging code with test-only PE fixtures; NOT a Skyrim build.',
              'tests_run': result.testsRun, 'failures': len(result.failures),
              'errors': len(result.errors), 'skipped': len(result.skipped),
              'cmake': CMAKE, 'python_os': os.name,
              'fixture': 'clang/lld-linked test DLL' if os.environ.get('FC_TEST_PE_FILE') else 'synthetic PE header fixture'}
    output = os.environ.get('FC_TEST_REPORT')
    if output:
        Path(output).write_text(json.dumps(report, indent=2) + '\n')
    raise SystemExit(0 if result.wasSuccessful() else 1)
