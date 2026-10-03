# SexFast16 host validation

Build identity: `FC-0.5.5-SEXFAST16-20261002-L`.

Completed in the Linux host environment (not Skyrim):

- 7/7 portable CTest programs passed with GCC.
- 7/7 portable CTest programs passed with Clang ASan/UBSan (LeakSanitizer disabled for the ptraced host).
- Production UI Menu class / Engine.hpp syntax harness passed with real fmt format checking.
- Production REPAIR15 pause lifecycle seam passed 288 assertions. The audio device itself is an explicit host boundary; AudioMuteLease16 is separately exercised in runtime tests.
- Kernel/Quest/PlayerFreedom seam passed 151 assertions.
- Input13/GameInput13 host seam passed 61 assertions.
- Crime-hook dispatch seam passed 26 assertions.
- Player customization production tests passed 57 assertions.
- Runtime ESP/catalogue structural tests passed 17 tests.
- Follow-record schema tests passed 4 tests.
- Build identity/cache regression passed 14 tests.
- CMake packaging regression passed 56 tests.
- Static source audit passed with all declared operation kinds having a handler; SexLab QuickStart / StopAll / after-close dispatch and master-audio lease wiring present.

Not executed here:

- Full Windows/MSVC/CommonLib compile/link of FreedomControl.dll.
- Windows BAT/PowerShell execution.
- SKSE loading, real Skyrim audio behavior, Papyrus VM/SexLab dispatch, SLAL/Creature animation matching, or gameplay stability.
