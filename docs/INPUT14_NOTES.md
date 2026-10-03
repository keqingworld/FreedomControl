# Input14: F8 isolation and deliberate dialogue

Review: 2026-10-01. Target: Skyrim Steam 1.6.1170.0.

## Changes from Input13

The supplied Input13 source observed mouse-wheel events and always returned
`kContinue`. Blocking the old window procedure and setting the native control
mask to zero did not stop later SKSE input listeners from receiving number keys.
That is a concrete leakage path; it does not identify which mod opened the
reported menu or prove that Poser Maker caused the symptom.

`GameInput13.cpp` now installs an input sink with `PrependEventSink` at DataLoaded.
While F8 is published open, it copies wheel values to the existing UI input queue
and returns `kStop` for the entire input-event batch. Keyboard text continues
through the existing Input13 Win32/IME/polling route. The menu toggle still uses
its existing independent polling route, so capture does not disable closing F8.
No event node, head pointer, linkage, or native input-device registration is
modified; there is no raw-address hook or global keyboard hook.

Buttons captured in F8 remain quarantined through their held/release events after
close, avoiding key-up hotkeys from the editing session. A genuinely new down
edge clears a stale quarantine entry after a missing release. Wheel events are
transient and never create held-button entries. Mixed batches are handled as a
whole, including unrelated events in the same batch.

This covers listeners downstream of this sink on this event source. It cannot
block a later-installed listener that prepends itself ahead of this one,
independent `GetAsyncKeyState`/DirectInput polling, or a different input source.
Changing MO2 priority alone does not prove a native listener's runtime order.

## NPC intent and control boundaries

Outside captured input, only initial mapped Activate presses on supported
keyboard/mouse/gamepad devices can establish the actor-specific opening token.
Held/release events and scripted activation notifications do not grant it. A
later invalid/item activation or a different speaker cancels pending permission.

A menu-close observer publishes an atomic dialogue generation; it performs no
engine mutation. Both a new activation and the game-thread freedom check consume
that generation before inspecting intent. Thus close/reopen between ticks cannot
reuse the old conversation, and an intentional activation after close is fresh.
An accepted same-speaker conversation has no arbitrary duration limit. Missing
speaker identity receives only 0.20 seconds of tolerance. A new scene can inherit
that conversation only during the original 2-second activation opening window.
Later unrelated scenes are released without deliberately stopping the accepted
conversation's dialogue; native package cleanup semantics still require testing.

Loading, death, unavailable player state, rest, policy disable, and temporary
story permission clear transient intent. Paused/F8/native item/modal/application
UI cancels pending input without extending unknown-speaker deadlines. An already
accepted conversation can resume after pausing. Non-pausing native menus without
dialogue are excluded from control restoration. No item-removal or quest-reset
policy was added.

## Pinned declaration/source audit

The following CommonLibSSE-NG sources were inspected at revision
`a898f469851c464d05137bb74b069dd234897643`:

- [BSTEvent.h](https://raw.githubusercontent.com/alandtse/CommonLibSSE-NG/a898f469851c464d05137bb74b069dd234897643/include/RE/B/BSTEvent.h): `PrependEventSink` inserts ahead of existing sinks only outside notification; pending additions during notification are appended later. `SendEvent` breaks its sink loop on `kStop`. This is dispatch stopping, not game-action cancellation.
- [BSInputDeviceManager.h](https://raw.githubusercontent.com/alandtse/CommonLibSSE-NG/a898f469851c464d05137bb74b069dd234897643/include/RE/B/BSInputDeviceManager.h): inherits `BSTEventSource<InputEvent*>`.
- [MenuOpenCloseEvent.h](https://raw.githubusercontent.com/alandtse/CommonLibSSE-NG/a898f469851c464d05137bb74b069dd234897643/include/RE/M/MenuOpenCloseEvent.h): `BSFixedString menuName` and `bool opening` match the observer.
- [UI.h](https://raw.githubusercontent.com/alandtse/CommonLibSSE-NG/a898f469851c464d05137bb74b069dd234897643/include/RE/U/UI.h): the templated menu-event sink registration and native-menu predicates exist.
- [ButtonEvent.h](https://raw.githubusercontent.com/alandtse/CommonLibSSE-NG/a898f469851c464d05137bb74b069dd234897643/include/RE/B/ButtonEvent.h): `IsDown` is an initial press, `IsPressed` includes held input, and `GetDevice`/`GetIDCode` provide device-bound identity (device accessor inherited from InputEvent).

## Verification boundary and required native gates

The actual Input13/GameInput13 host seam (61 assertions) checks numeric text plus downstream
isolation, unchanged batch pointers, held/release quarantine, missing releases,
mixed/non-button batches, wheel routing, remapped Activate, and close generations.
Player policy (31 assertions) and Kernel11 production seam (151 assertions) cover permission expiry, actor/scene changes,
menu/death/load suspension, close/reopen, fresh activation, and quest preservation.
These doubles are not the Windows ABI, real IME, or Skyrim execution.
GCC runs passed; Clang ASan/UBSan input and kernel runs also passed with
`ASAN_OPTIONS=detect_leaks=0`. Default LeakSanitizer could not run under this
workspace's ptrace environment; no leak-check pass is claimed.

Before claiming in-game compatibility:

1. Edit F8 health with number row/numpad/decimal/backspace/Ctrl+A; confirm the
   actual conflicting mod stays closed. Test F8 close while a digit is held.
2. Start holding movement, attack, sprint, or a mod hotkey BEFORE opening F8;
   release it inside F8; close and check for stuck movement/action/mod state.
   Whole-batch stopping also hides releases from native/downstream handlers;
   pre-existing handler state is not synthesized or reset by this implementation.
3. Test mouse/gamepad, rapid open/close, lost focus, IME, and device reconnect.
4. Test intentional NPC talk, remapped Activate, interrupted speaker publication,
   pause/resume, fast close/reopen, unsolicited force-greet and scene takeover.
5. Record exact mod version and key/mode if leakage remains. Independent-polling
   hotkeys require that mod's menu-aware behavior or an explicitly scoped fix.
