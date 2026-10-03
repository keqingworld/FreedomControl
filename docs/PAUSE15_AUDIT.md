# REPAIR15: F8 native full-pause lifecycle

## Verified source cause and repair scope

The shipped 0.5.0, 0.5.1 and 0.5.2 archives use the same optional `freezeTime` implementation; 0.5.3 inherited it. Both INI and co-save settings could disable that path. This establishes an inherited policy gap, not evidence that an earlier release had a full native pause. REPAIR15 ignores those legacy opt-outs, retains their action/save compatibility, and makes the UI setting informational.

A dedicated, movie-less `FreedomControlPause15` IMenu carries `kPausesGame`. Skyrim's normal UI show/hide lifecycle owns its contribution. The plugin never assigns or increments/decrements `numPausesGame`, replaces the total with a saved value, or closes another menu. Other native pauses are distinguished by the actual own-menu stack/flag readback, not by a generic `GameIsPaused()` early exit.

The existing `freezeTime` lease now covers only the asynchronous native-open transition. It restores its captured value as soon as the native pause is confirmed, then stays out of the long-lived native pause and close. Therefore a later external `freezeTime` writer survives F8 close. As with any shared bool, simultaneous external writes during the short acquisition transition cannot reveal their independent intent; there is no claim of perfect ownership of arbitrary mods' flags.

## Execution and close ordering

- Gameplay actions, bounded batches, actors, quests, HUD refresh and deferred commands still run through SKSE `AddTask`
- A separate `AddUITask` ticket services only native pause registration/show/hide/readback and pause/input capture restoration. Submission requires both UI and UIMessageQueue, because SKSE silently drops UI tasks when its legacy UIManager is absent. UI loss/load/revert invalidates old tickets; stale callbacks cannot clear newer tickets. A delayed game task cannot block this lifecycle
- The UI pump can release an empty-queue close. It never runs `Apply`, actor work or deferred commands. Main-task finalization still refreshes the HUD
- Edits wait for native opening readback. Own native pause does not short-circuit edit/batch processing
- Native-menu, travel and rest actions remain in `afterClose` until the own menu is off-stack and another native pause has ended
- Loading/revert and errors stop the appropriate pending work and request release. Repeated or opposite requests are reconciled against the native stack
- Alt-Tab keeps F8 and its pause open. Existing input reset and hotkey re-priming handle focus return. Minimized or zero-size render targets are skipped without releasing pause. Window destruction, a real render failure or a C++ render exception requests emergency close
- Transition logs include desired state, own stack membership, pause flag, total native pauses and explicit full-pause confirmation. Show/hide retry logs are limited to changes or two-second intervals. The UI's world-pause indicator reads `GameIsPaused`, not the fallback bool

## Primary declaration audit

Fetched and inspected from the upstream CommonLibSSE-NG repository on 2026-10-01:

- [IMenu declaration](https://github.com/CharmedBaryon/CommonLibSSE-NG/blob/main/include/RE/I/IMenu.h): IMenu implements its base's abstract Accept method; its added virtual methods are non-pure. Its default members include a null movie and no input context. The pause/stack accessors and pause flag used by REPAIR15 are declared here
- [IMenu implementation](https://github.com/CharmedBaryon/CommonLibSSE-NG/blob/main/src/RE/I/IMenu.cpp): movie event handling, advancement and display check for a movie before using it; no SWF is required for this non-rendering owner
- [UI declaration](https://github.com/CharmedBaryon/CommonLibSSE-NG/blob/main/include/RE/U/UI.h) and [implementation](https://github.com/CharmedBaryon/CommonLibSSE-NG/blob/main/src/RE/U/UI.cpp): registration accepts a creator returning IMenu, menu entries retain GPtr ownership, GetMenu returns that managed pointer, and pause readback checks the native count
- [Allocation base](https://github.com/CharmedBaryon/CommonLibSSE-NG/blob/main/include/RE/G/GRefCountBaseStatImpl.h) and [GMemory allocation operators](https://github.com/CharmedBaryon/CommonLibSSE-NG/blob/main/include/RE/G/GMemory.h): inherited new/delete use the Scaleform memory heap. The plugin returns a newly allocated derived menu to the registered factory contract; it does not manually delete menu-stack objects
- [Task interface](https://github.com/CharmedBaryon/CommonLibSSE-NG/blob/main/include/SKSE/Interfaces.h) and [wrappers](https://github.com/CharmedBaryon/CommonLibSSE-NG/blob/main/src/SKSE/Interfaces.cpp): both game and UI task APIs accept the lambdas used here
- SKSE 2.2.6 [game-task hook](https://github.com/ianpatt/skse64/blob/v2.2.6/skse64/Hooks_Threads.cpp) and [UI-task hook](https://github.com/ianpatt/skse64/blob/v2.2.6/skse64/Hooks_UI.cpp): game tasks drain after the original task processing without a pause conditional; UI tasks drain after the original UI event processing. This supports keeping gameplay work on its original queue, rather than migrating it to a render/UI callback

These are declaration/source checks, not a Windows compiler, ABI or scheduler execution test. The installed CommonLib revision is still recorded by the real build.

## Host tests and limits

`tests/pause_seam_tests.py` extracts unchanged production lifecycle methods, `Tick`, `Message`, `RestoreTransient` and the hotkey polling function. Explicit doubles model separate game/UI task queues, native IMenu messages and per-menu pause contributions. The test covers mandatory legacy opt-out rejection, native open/close readback, edits while paused, bounded batch drain, delayed afterClose, overlapping native menus, repeated/cancelled show/hide, temporarily delayed game tasks, error release, load during open and pending show, Alt-Tab/re-prime, native force-hide recovery, unavailable interfaces, silent-dropped UI tasks, stale callbacks after load, prior input/boolean restoration, later external boolean writes and 100 repeated sessions.

An optional immutable prior-release ZIP replays the exact old SyncInput body under the same harness. It must compile and fail the first mandatory-pause assertion; a compile failure does not count as catching the regression.

The host tests cannot establish Skyrim's actual animation, physics, AI or game-task ordering. A Windows x64/MSVC/CommonLib build and Skyrim 1.6.1170/SKSE 2.2.6 smoke test remain required: open during combat and moving projectiles/NPCs, keep the panel open, edit and scroll/type, Alt-Tab and return, close with queued batches, launch a deferred native menu/travel action, overlap ordinary native menus, and load/revert. Check `NativePause15` log readback alongside observed gameplay; do not treat passing doubles as proof of an in-game freeze.
