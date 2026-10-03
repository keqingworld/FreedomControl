# Player14: verified API surface and safety limits

Review date: 2026-10-01. Target runtime: Skyrim Steam 1.6.1170.0.

CommonLibSSE-NG revision reviewed: `a898f469851c464d05137bb74b069dd234897643`.
The headers and selected implementations below were downloaded directly from that
revision and inspected. Doxygen pages were used as a secondary cross-check. This
is declaration/source verification, not a Windows build or a native-game test.

## Findings that determine the design

- The existing Kernel11 guard treats every open `Dialogue Menu` as intentional.
  Forced greeting uses the same menu, so that exemption also protects unwanted
  interruptions from the control-restoration policy.
- A mapped, initial Activate button press can establish short-lived intent for
  its specific aimed reference. An activation notification whose `actionRef` is
  the player is insufficient evidence of a physical player action: the event
  does not identify the calling script or input device.
- Menu and activation event sinks observe events. Their return value is not an
  engine cancellation result. `kStop` stops delivery to later event sinks; it
  must not be advertised as cancelling a dialogue, item addition, or quest.
- A scoped reactive guard can stop the current unsolicited speaker, request the
  dialogue menu to hide, and release selected player control restrictions. It
  cannot promise to intercept every external instruction before execution or
  undo fragments/scripts that have already run.
- `BGSScene` has no named native C++ `Stop()` method in this revision. Its mutable
  playback fields and action unknowns are not a supported stop API. Clearing
  only the player's current scene link is narrower than stopping every scene or
  parent quest, but is still an interruption that can affect quest progress.
- Disabling a guard is reversible as a policy change. It does not replay dialogue
  or restore a previously interrupted scene/quest to its exact prior state.

## Deliberate Activate observation

Verified declarations:

- `ButtonEvent::IsDown()` detects an initial nonzero-value press with zero held
  duration; `IsPressed()` also includes held/repeated input.
- `ButtonEvent::GetUserEvent()` returns the semantic event name.
- `UserEvents::GetSingleton()->activate` names the Activate action.
- `ControlMap::GetMappedKey(std::string_view, INPUT_DEVICE, InputContextID) const`
  resolves the device's gameplay mapping. Its implementation returns `kInvalid`
  when absent. Validate the device enum and result before comparing to
  `ButtonEvent::GetIDCode()`; do not bake in keyboard E or translate the gamepad
  code to a Windows virtual key.
- `CrosshairPickData::GetActiveTarget() const` returns `ObjectRefHandle`.
  Resolve the handle and keep the resulting smart pointer alive while using it.
- `BSInputDeviceManager` inherits `BSTEventSource<InputEvent*>`.
- An input sink overrides `ProcessEvent(InputEvent* const*,
  BSTEventSource<InputEvent*>*)` and returns `BSEventNotifyControl`.
- `BSTEventSource::PrependEventSink(Sink*)` exists at the reviewed revision.
  Prepending outside dispatch places an observer ahead of existing sinks;
  prepending during notification joins pending registrations and does not
  guarantee it precedes previously registered sinks. It is not a licence to
  consume, reorder, edit, or free the event list.

Sources:

- [ButtonEvent.h](https://raw.githubusercontent.com/alandtse/CommonLibSSE-NG/a898f469851c464d05137bb74b069dd234897643/include/RE/B/ButtonEvent.h)
- [UserEvents.h](https://raw.githubusercontent.com/alandtse/CommonLibSSE-NG/a898f469851c464d05137bb74b069dd234897643/include/RE/U/UserEvents.h)
- [ControlMap.h](https://raw.githubusercontent.com/alandtse/CommonLibSSE-NG/a898f469851c464d05137bb74b069dd234897643/include/RE/C/ControlMap.h)
- [ControlMap.cpp](https://raw.githubusercontent.com/alandtse/CommonLibSSE-NG/a898f469851c464d05137bb74b069dd234897643/src/RE/C/ControlMap.cpp)
- [CrosshairPickData.h](https://raw.githubusercontent.com/alandtse/CommonLibSSE-NG/a898f469851c464d05137bb74b069dd234897643/include/RE/C/CrosshairPickData.h)
- [BSInputDeviceManager.h](https://raw.githubusercontent.com/alandtse/CommonLibSSE-NG/a898f469851c464d05137bb74b069dd234897643/include/RE/B/BSInputDeviceManager.h)
- [BSTEvent.h](https://raw.githubusercontent.com/alandtse/CommonLibSSE-NG/a898f469851c464d05137bb74b069dd234897643/include/RE/B/BSTEvent.h)

Recommended policy precautions, not guarantees supplied by the engine:

1. Establish intent only in gameplay, with the overlay closed and no relevant
   native modal/item/application menu, load transition, or dead-player state.
   `UI` exposes `GameIsPaused()`, `IsItemMenuOpen()`, `IsModalMenuOpen()` and
   `IsApplicationMenuOpen()`; pause alone does not identify every menu.
2. Scope intent to the reference that was deliberately activated. A generic
   recent keypress must not whitelist a different nearby force-greeting NPC.
3. Allow normal dialogue duration after the matching speaker is accepted, rather
   than expiring the entire conversation after the short opening window.
4. Reset transient intent on close, save/load/revert, target change, and policy
   disable; do not serialize active input leases into a co-save.
5. Account for event ordering. A deferred game-thread check can observe speaker
   resolution after menu opening, but must revalidate policy, speaker, and intent
   immediately before acting. A queued old close must not close a newer accepted
   conversation.
6. Keep the NPC-intent and menu-close observers free of engine mutations;
   perform scene/control changes on the existing game-thread task path. F8
   input capture is separate: it explicitly returns kStop for downstream
   whole-batch delivery while leaving event nodes and linkage unchanged. See
   INPUT14_NOTES.md for listener-order and held-input compatibility limits.

## Activation and menu notifications

`TESActivateEvent` has `NiPointer<TESObjectREFR>` members `objectActivated` and
`actionRef`. Register its sink through
`ScriptEventSourceHolder::GetSingleton()->AddEventSink<TESActivateEvent>(sink)`.
This event has no input-origin, voluntary, or cancellation field.

`MenuOpenCloseEvent` supplies `BSFixedString menuName` and `bool opening`.
`UI::GetSingleton()->AddEventSink<MenuOpenCloseEvent>(sink)` is available.
`DialogueMenu::MENU_NAME` is `"Dialogue Menu"`. Do not infer a speaker from
`DialogueMenu::Data` or its unnamed runtime array.

- [TESActivateEvent.h](https://raw.githubusercontent.com/alandtse/CommonLibSSE-NG/a898f469851c464d05137bb74b069dd234897643/include/RE/T/TESActivateEvent.h)
- [ScriptEventSourceHolder.h](https://raw.githubusercontent.com/alandtse/CommonLibSSE-NG/a898f469851c464d05137bb74b069dd234897643/include/RE/S/ScriptEventSourceHolder.h)
- [MenuOpenCloseEvent.h](https://raw.githubusercontent.com/alandtse/CommonLibSSE-NG/a898f469851c464d05137bb74b069dd234897643/include/RE/M/MenuOpenCloseEvent.h)
- [UI.h](https://raw.githubusercontent.com/alandtse/CommonLibSSE-NG/a898f469851c464d05137bb74b069dd234897643/include/RE/U/UI.h)
- [DialogueMenu.h](https://raw.githubusercontent.com/alandtse/CommonLibSSE-NG/a898f469851c464d05137bb74b069dd234897643/include/RE/D/DialogueMenu.h)

## Actual dialogue speaker, source diagnostics, and hide request

The named singleton is `RE::MenuTopicManager`, not `DialogueManager`.
`MenuTopicManager::GetSingleton()` returns its pointer. Public members include:

- `ObjectRefHandle speaker`, `lastSpeaker`
- `TESTopicInfo* currentTopicInfo`, `lastTopicInfo`, `rootTopicInfo`
- `bool menuOpen`, `isGreetingPlayer`, `forceGoodbye`, `shutMenu`

`IsCurrentSpeaker(const ObjectRefHandle&) const` checks both `menuOpen` and handle
equality. `lastSpeaker` can persist after the menu closes while that NPC continues
speaking. Do not apply a new-menu decision to this stale handle. Likewise,
`isGreetingPlayer` alone is not proof of an unwanted greeting. Inspect only the
current relevant state; do not overwrite the singleton's handles/booleans or
unknown fields to simulate a teardown.

Read-only source attribution can follow
`currentTopicInfo->parentTopic->ownerQuest`, checking every pointer. The topic's
`data.subtype` can equal `DIALOGUE_DATA::Subtype::kForceGreet` or `kScene`.
These are diagnostic indicators, not proof of which script caused a transfer.
`DialogueItem` has a speaker reference and quest/topic fields, but it is not the
active-menu singleton and should not be constructed merely to discover a speaker.

`UIMessageQueue::AddMessage(const BSFixedString&, UI_MESSAGE_TYPE,
IUIMessageData*)` accepts `UI_MESSAGE_TYPE::kHide` with null data. This enqueues
a hide request; it does not provide synchronous confirmation or roll back a
quest. Bound retries and verify the resulting menu state instead of filling the
queue every frame. A hide request and native dialogue stop have different scopes.

- [MenuTopicManager.h](https://raw.githubusercontent.com/alandtse/CommonLibSSE-NG/a898f469851c464d05137bb74b069dd234897643/include/RE/M/MenuTopicManager.h)
- [DialogueItem.h](https://raw.githubusercontent.com/alandtse/CommonLibSSE-NG/a898f469851c464d05137bb74b069dd234897643/include/RE/D/DialogueItem.h)
- [TESTopicInfo.h](https://raw.githubusercontent.com/alandtse/CommonLibSSE-NG/a898f469851c464d05137bb74b069dd234897643/include/RE/T/TESTopicInfo.h)
- [TESTopic.h](https://raw.githubusercontent.com/alandtse/CommonLibSSE-NG/a898f469851c464d05137bb74b069dd234897643/include/RE/T/TESTopic.h)
- [UIMessageQueue.h](https://raw.githubusercontent.com/alandtse/CommonLibSSE-NG/a898f469851c464d05137bb74b069dd234897643/include/RE/U/UIMessageQueue.h)
- [UIMessage.h](https://raw.githubusercontent.com/alandtse/CommonLibSSE-NG/a898f469851c464d05137bb74b069dd234897643/include/RE/U/UIMessage.h)

## Actor and scene operations

Actor declarations verified at the pin:

- `bool SetDialogueWithPlayer(bool, bool, TESTopicInfo*)` (virtual override,
  annotated slot `0x41`; the second boolean is `a_forceGreet`)
- `BGSScene* GetCurrentScene() const`
- `void SetCurrentScene(BGSScene*)`
- `void StopCurrentDialogue()`
- `void EndDialogue()`
- `void EndInterruptPackage(bool a_skipDialogue)`
- Runtime bit `Actor::BOOL_BITS::kForceGreetingPlayer`

The declaration does not establish cleanup semantics for
`SetDialogueWithPlayer(false, false, nullptr)`: there is no corresponding native
implementation in CommonLib to prove the null-topic case or a complete teardown.
It is not used as an additional speculative cleanup call.

The first method is a candidate interception boundary, not evidence that a hook
captures all dialogue paths. Any hook requires appropriate concrete vtables,
chain preservation, runtime/ABI validation, closing-call pass-through and actual
1.6.1170 testing. A header's virtual-slot comment alone is not such validation.

`PlayerCharacter::SetAIDriven(bool)` is a named wrapper. Its implementation uses
CommonLib relocation IDs rather than a hand-written game address. ControlMap's
`GetControlsState(uint32_t&, uint32_t&) const` and
`SetControlsState(uint32_t, uint32_t)` allow retaining the stored-state word and
unknown bits while restoring only selected known input categories. Do not use an
all-bits reset to restore a selected subset.

`ActorState::GetLifeState() const` and
`Actor::SetLifeState(ACTOR_LIFE_STATE)` are also available.
`ACTOR_LIFE_STATE::kRestrained` and `kAlive` are named enum values. A restriction
release must check the exact restrained state first; it must not rewrite dead,
dying, reanimated, unconscious, or bleedout states to alive. The setter is a
native relocated wrapper, not a direct bit-field write.

`BGSScene` exposes alias IDs in `actors` and `parentQuest`.
`TESQuest::GetAliasedRef(uint32_t) const` can resolve an alias for read-only
player-participation checks. Scene actor IDs are alias IDs, not actor FormIDs.
The scene's `isPlaying`, `isShuttingDown`, and `currentPhaseIndex` are engine
state; `kInterruptible`, progression flags, and `BGSSceneAction::ClearActiveFlags`
do not constitute a verified graceful-stop sequence. Do not call unknown virtual
methods, zero scene/action flags, stop all parent quests, or clear all actors'
scene links as a generic release mechanism.

- [Actor.h](https://raw.githubusercontent.com/alandtse/CommonLibSSE-NG/a898f469851c464d05137bb74b069dd234897643/include/RE/A/Actor.h)
- [Actor.cpp](https://raw.githubusercontent.com/alandtse/CommonLibSSE-NG/a898f469851c464d05137bb74b069dd234897643/src/RE/A/Actor.cpp)
- [ActorState.h](https://raw.githubusercontent.com/alandtse/CommonLibSSE-NG/a898f469851c464d05137bb74b069dd234897643/include/RE/A/ActorState.h)
- [PlayerCharacter.h](https://raw.githubusercontent.com/alandtse/CommonLibSSE-NG/a898f469851c464d05137bb74b069dd234897643/include/RE/P/PlayerCharacter.h)
- [PlayerCharacter.cpp](https://raw.githubusercontent.com/alandtse/CommonLibSSE-NG/a898f469851c464d05137bb74b069dd234897643/src/RE/P/PlayerCharacter.cpp)
- [BGSScene.h](https://raw.githubusercontent.com/alandtse/CommonLibSSE-NG/a898f469851c464d05137bb74b069dd234897643/include/RE/B/BGSScene.h)
- [BGSSceneAction.h](https://raw.githubusercontent.com/alandtse/CommonLibSSE-NG/a898f469851c464d05137bb74b069dd234897643/include/RE/B/BGSSceneAction.h)
- [BGSSceneActionDialogue.h](https://raw.githubusercontent.com/alandtse/CommonLibSSE-NG/a898f469851c464d05137bb74b069dd234897643/include/RE/B/BGSSceneActionDialogue.h)
- [TESQuest.h](https://raw.githubusercontent.com/alandtse/CommonLibSSE-NG/a898f469851c464d05137bb74b069dd234897643/include/RE/T/TESQuest.h)

## Unidentified acquisition/event source

Neither `add libary` nor `add libray` was found in the supplied FreedomControl
source/include/package tree. The source does reference Address Library as a
native-address dependency; that is not sufficient evidence that the user's
in-game event has been identified. Do not rename the unknown event to that
dependency, disable it, or declare another mod responsible without record/log
evidence from the actual occurrence.

`TESContainerChangedEvent` exposes container IDs, base object ID, item count,
reference handle and unique ID. It contains no script caller, source quest,
reward/loot distinction, or cancellation operation. Blanket removal of newly
received items can remove intentional loot, purchases, crafting output, and
rewards. A later removal would still not undo earlier script side effects.
Read-only observation can help identify the object and timing; a specific block
needs the real item's/source's identity and explicitly bounded policy.

- [TESContainerChangedEvent.h](https://raw.githubusercontent.com/alandtse/CommonLibSSE-NG/a898f469851c464d05137bb74b069dd234897643/include/RE/T/TESContainerChangedEvent.h)

## Build and runtime verification boundary

The supplied root CMake project intentionally rejects non-Windows or non-MSVC
builds. It requires a full sibling CommonLib checkout (including the OpenVR
submodule verified by build.ps1), MSVC x64, a Windows SDK/D3D libraries, and vcpkg
dependencies. The reviewed workspace did not contain that checkout or discoverable
MSVC/Windows SDK installation. Downloading selected public headers does not
supply those dependencies and cannot substantiate a DLL build claim.

The build manifest includes DirectXMath, DirectXTK, fmt, spdlog, rapidcsv,
nlohmann-json, and ImGui's DX11/Win32 bindings. Preserve the checked-in baseline
and the project's recorded CommonLib revision when producing a Windows build.
The plugin explicitly admits only runtime 1.6.1170.0; installing the newest SKSE
without matching the game's runtime is not a valid substitute. The official
[SKSE site](https://skse.silverlock.org/) currently lists a newer Steam runtime
and offers archived builds; select the build that actually matches 1.6.1170.

Portable policy tests and mocked-engine seams can verify decisions, reset paths,
serialization, and function wiring. They do not verify vtable addresses, real
menu/input ordering, animation packages, Papyrus effects, or native scene cleanup.
Real-game acceptance should cover at least normal NPC activation (keyboard,
remapped input, gamepad), guard force-greet, scripted dialogue, consent speaker
switch, item/furniture activation, scene takeover, overlay/menu suspension,
save/load during a conversation, and policy disable/re-enable.

### Integration declaration audit

The newly introduced `PlayerFreedom14.cpp`, `PlayerFreedomPolicy14.hpp`, and
mapped-Activate observer were read against the pinned headers. The new singleton,
smart-pointer conditional expression, menu predicates, input getters, form-ID
accessors, scene getters/setters, actor stop/interrupt methods, runtime flags,
message queue call, and restrained-state setter all have matching declarations.
In particular, `ObjectRefHandle::get()` returns `NiPointer<TESObjectREFR>`, matching
the explicitly typed empty branch used for a missing topic manager.
This audit found no declaration-name/type mismatch in those expressions. It is
still not a compiler or linker result; Windows compilation and native input
ordering remain required acceptance gates.

The implemented NPC intent model deliberately does not infer permission for an
arbitrary later scene merely because a door/item/furniture was activated. Typical
furniture use is governed separately by the existing explicit furniture-break
switch. A script-driven player scene that follows such an action can still be
released by the scene policy; use the exposed temporary story-permission control
when choosing to allow that sequence. Do not describe this as universal causal
detection of every voluntary scripted interaction.
