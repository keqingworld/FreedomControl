"""Run repeatable host-only regression checks and retain every command/result.

Requires CMake, a native C++ compiler, and real fmt headers. Optional Clang adds
ASan/UBSan runs. This NEVER builds the Windows SKSE DLL or launches Skyrim.
"""
from pathlib import Path
import argparse
from datetime import datetime, timezone
import json
import os
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fmt-include", required=True)
    parser.add_argument("--compiler", default="g++")
    parser.add_argument("--clang", help="Optional Clang C++ executable for ASan/UBSan checks")
    parser.add_argument("--cmake", default="cmake")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--baseline", type=Path, help="Optional original INPUT13 source ZIP")
    parser.add_argument("--disable-leak-detection", action="store_true",
                        help="For ptraced hosts only; ASan and UBSan remain on")
    args = parser.parse_args()
    output = args.output.resolve(); output.mkdir(parents=True, exist_ok=True)
    cmake = shutil.which(args.cmake)
    if not cmake or not (Path(args.fmt_include) / "fmt/format.h").is_file():
        parser.error("CMake and real fmt/format.h are required")
    ctest = str(Path(cmake).with_name("ctest"))
    env = dict(os.environ, CMAKE_EXE=cmake, PYTHONDONTWRITEBYTECODE="1")
    if args.disable_leak_detection:
        env["ASAN_OPTIONS"] = "detect_leaks=0"
    results = []

    def run(name, command, timeout=240):
        print(name + " ...", flush=True)
        try:
            result = subprocess.run([str(x) for x in command], cwd=ROOT, env=env,
                                    capture_output=True, text=True, errors="replace", timeout=timeout)
            code, body = result.returncode, result.stdout + result.stderr
        except subprocess.TimeoutExpired as exc:
            code, body = 124, "TIMEOUT: " + str(exc)
        (output / (name + ".log")).write_text("COMMAND: " + json.dumps([str(x) for x in command]) +
                                             "\nEXIT: " + str(code) + "\n" + body)
        results.append({"name": name, "exit_code": code, "passed": code == 0})
        print(name + (" PASS" if code == 0 else " FAILED (see log)"), flush=True)
        return code == 0

    with tempfile.TemporaryDirectory(prefix="fc-host14-") as temporary:
        for label, compiler, sanitizer in [("gcc", args.compiler, False)] + (
                [("clang-asan-ubsan", args.clang, True)] if args.clang else []):
            build = Path(temporary) / label
            flags = "-fsanitize=address,undefined -fno-omit-frame-pointer" if sanitizer else ""
            if run(label + "-configure", [cmake, "-S", ROOT / "tests", "-B", build,
                                         "-DCMAKE_BUILD_TYPE=Debug", "-DCMAKE_CXX_COMPILER=" + compiler,
                                         "-DCMAKE_CXX_FLAGS=" + flags]):
                if run(label + "-build", [cmake, "--build", build, "--parallel", "4"]):
                    run(label + "-ctest", [ctest, "--test-dir", build, "--output-on-failure"])
                    # CTest pass counts are test programs, not game scenarios.
                    run(label + "-assertions", [ctest, "--test-dir", build, "-V"])
            for test in ("legion_seam_tests", "kernel_seam_tests", "input13_host_tests", "crime_hooks_tests", "pause_seam_tests"):
                cmd = [sys.executable, ROOT / "tests" / (test + ".py"), "--compiler", compiler]
                if test in ("legion_seam_tests", "kernel_seam_tests", "pause_seam_tests"):
                    cmd += ["--fmt-include", args.fmt_include]
                if sanitizer:
                    cmd += ["--sanitize"]
                run(label + "-" + test, cmd)
            run(label + "-ui-syntax", [sys.executable, ROOT / "tests/ui_syntax_tests.py",
                                      "--compiler", compiler, "--fmt-include", args.fmt_include])
            run(label + "-vmargs18-regression", [sys.executable, ROOT / "tests/vmargs18_regression_tests.py",
                "--compiler", compiler, "--fmt-include", args.fmt_include,
                "--output", output / (label + "-vmargs18-details.json")] +
                (["--sanitize"] if sanitizer else []))
        for test in ("player_customization_tests", "runtime_records_tests", "follow_schema_tests", "build_identity_tests", "packaging_tests"):
            run(test, [sys.executable, ROOT / "tests" / (test + ".py")])
        clang_cl, linker = shutil.which("clang-cl"), shutil.which("lld-link")
        if clang_cl and linker:
            # Exact identity translation unit only. No SKSE, CRT, SDK or game
            # code is linked here, and this fixture is never a deliverable.
            from packaging_tests import BUILD_ID, source_sha
            obj = Path(temporary) / "identity-only.obj"
            dll = Path(temporary) / "identity-only.dll"
            if run("identity-only-compile", [clang_cl, "/nologo", "/c", "/WX", "/O2", "/GS-", "/Zl",
                    '/DFC_BUILD_ID="' + BUILD_ID + '"',
                    '/DFC_SOURCE_SHA256="' + source_sha(ROOT) + '"', "/Fo" + str(obj),
                    "/Tp" + str(ROOT / "src/BuildIdentity.cpp")]):
                if run("identity-only-link", [linker, "/dll", "/noentry", "/nodefaultlib", "/machine:x64",
                        "/opt:ref", "/out:" + str(dll), obj]):
                    env["FC_TEST_PE_FILE"] = str(dll)
                    run("packaging-linked-identity-fixture", [sys.executable, ROOT / "tests/packaging_tests.py"])
                    env.pop("FC_TEST_PE_FILE")
        if args.baseline:
            run("upgrade-preservation", [sys.executable, ROOT / "tests/upgrade_preservation_tests.py", args.baseline.resolve()])
    summary = {
        "build": (ROOT / "LATEST_BUILD_ID.txt").read_text().strip(),
        "completed_utc": datetime.now(timezone.utc).isoformat(),
        "scope": "Host policies and production code with explicit engine/OS/UI doubles; packaging fixtures only",
        "not_executed": ["Full Windows/MSVC/CommonLib DLL compile/link", "Windows BAT/PowerShell execution",
                         "Skyrim/SKSE input ordering, AI, animations, scene cleanup and game stability"],
        "leak_detection_disabled": args.disable_leak_detection,
        "results": results,
        "all_passed": all(result["passed"] for result in results),
    }
    (output / "SUMMARY.json").write_text(json.dumps(summary, indent=2) + "\n")
    return 0 if summary["all_passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
