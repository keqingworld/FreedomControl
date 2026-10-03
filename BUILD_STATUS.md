# VMARGS18 verification status

Build: `FC-0.5.5-VMARGS18-20261002-N`  
Base: FreedomControl 0.5.5 COMPILE17.

## Scope

VMARGS18 fixes the logged C2027 in SexLab16.cpp / RE/F/FunctionArguments.h. The production adapter copies arguments by value before passing rvalues to the CommonLib factory. It does not intentionally change game logic, UI, audio, following, ESP records, or dependencies.

## Automated validation from the source archive

- GCC 14.2 and Clang 17 targeted checks.
- Clang targeted runtime test with ASan + UBSan.
- Existing COMPILE17 regressions.
- Engine-independent CTest programs.
- Packaging fixtures.
- Build identity/content hash/cache-preservation checks.
- Runtime-record and follow-schema tests.

## Native status

The original source package documentation did not claim a complete Windows/MSVC + Skyrim runtime validation in its automated Linux-side checks. The project owner has subsequently compiled and tested the release in the intended Windows/Skyrim environment. Users should still treat compatibility with third-party mod lists as environment-dependent.

## Build target

- Skyrim AE 1.6.1170
- SKSE 2.2.6
- Windows x64 / MSVC
- CommonLibSSE-NG
