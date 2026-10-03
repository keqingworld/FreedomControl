# API declaration review — pinned CommonLib

Target checkout: a898f469851c464d05137bb74b069dd234897643; runtime target 1.6.1170.0. These are primary source declarations inspected when writing this revision, not a proof of Windows compilation or native game behavior.

- https://raw.githubusercontent.com/alandtse/CommonLibSSE-NG/a898f469851c464d05137bb74b069dd234897643/include/RE/A/Actor.h
- https://raw.githubusercontent.com/alandtse/CommonLibSSE-NG/a898f469851c464d05137bb74b069dd234897643/include/RE/A/ActorValueOwner.h
- https://raw.githubusercontent.com/alandtse/CommonLibSSE-NG/a898f469851c464d05137bb74b069dd234897643/include/RE/A/ActorValueList.h
- https://raw.githubusercontent.com/alandtse/CommonLibSSE-NG/a898f469851c464d05137bb74b069dd234897643/include/RE/A/AIProcess.h
- https://raw.githubusercontent.com/alandtse/CommonLibSSE-NG/a898f469851c464d05137bb74b069dd234897643/include/RE/M/Main.h
- https://raw.githubusercontent.com/alandtse/CommonLibSSE-NG/a898f469851c464d05137bb74b069dd234897643/include/RE/T/TESObjectCELL.h
- https://raw.githubusercontent.com/alandtse/CommonLibSSE-NG/a898f469851c464d05137bb74b069dd234897643/include/RE/T/TESObjectREFR.h
- https://raw.githubusercontent.com/alandtse/CommonLibSSE-NG/a898f469851c464d05137bb74b069dd234897643/include/RE/T/TESFullName.h
- https://raw.githubusercontent.com/alandtse/CommonLibSSE-NG/a898f469851c464d05137bb74b069dd234897643/include/RE/T/TESPackage.h
- https://raw.githubusercontent.com/alandtse/CommonLibSSE-NG/a898f469851c464d05137bb74b069dd234897643/include/RE/T/TESShout.h
- https://raw.githubusercontent.com/alandtse/CommonLibSSE-NG/a898f469851c464d05137bb74b069dd234897643/include/RE/H/HUDData.h
- https://raw.githubusercontent.com/alandtse/CommonLibSSE-NG/a898f469851c464d05137bb74b069dd234897643/include/RE/H/HUDMessageTypes.h
- https://raw.githubusercontent.com/alandtse/CommonLibSSE-NG/a898f469851c464d05137bb74b069dd234897643/include/RE/U/UIMessageQueue.h

Do not infer unnamed flag meanings or new runtime addresses from these declarations. Older checks are retained under history/0.1.x.


## Legion9 reviewed against pinned CommonLib commit

Pin: a898f469851c464d05137bb74b069dd234897643. Reviewed declarations and selected wrapper implementations; no full Windows compilation claim.

- Actor: https://github.com/alandtse/CommonLibSSE-NG/blob/a898f469851c464d05137bb74b069dd234897643/include/RE/A/Actor.h
- Actor wrapper implementation: https://github.com/alandtse/CommonLibSSE-NG/blob/a898f469851c464d05137bb74b069dd234897643/src/RE/A/Actor.cpp
- Actor state: https://github.com/alandtse/CommonLibSSE-NG/blob/a898f469851c464d05137bb74b069dd234897643/include/RE/A/ActorState.h
- Package: https://github.com/alandtse/CommonLibSSE-NG/blob/a898f469851c464d05137bb74b069dd234897643/include/RE/T/TESPackage.h
- Base protection flags: https://github.com/alandtse/CommonLibSSE-NG/blob/a898f469851c464d05137bb74b069dd234897643/include/RE/T/TESActorBaseData.h
- NPC: https://github.com/alandtse/CommonLibSSE-NG/blob/a898f469851c464d05137bb74b069dd234897643/include/RE/T/TESNPC.h
- Leveled actors: https://github.com/alandtse/CommonLibSSE-NG/blob/a898f469851c464d05137bb74b069dd234897643/include/RE/T/TESLevCharacter.h
- Reference lifetime/placement: https://github.com/alandtse/CommonLibSSE-NG/blob/a898f469851c464d05137bb74b069dd234897643/include/RE/T/TESObjectREFR.h

VisitFactions: false continues; base entries precede reference overrides. SetPlayerControls(false) switches the movement controller to AI-driven operation. KillImpl and KillImmediate are native death paths; SetDelete marks engine-managed deletion rather than freeing a C++ object from the plugin.


## Freedom10 primary-source review

- Own quest/alias API: https://raw.githubusercontent.com/alandtse/CommonLibSSE-NG/a898f469851c464d05137bb74b069dd234897643/include/RE/T/TESQuest.h
- Reference movement implementation: https://raw.githubusercontent.com/alandtse/CommonLibSSE-NG/a898f469851c464d05137bb74b069dd234897643/src/RE/T/TESObjectREFR.cpp
- ExtraTeleport: https://raw.githubusercontent.com/alandtse/CommonLibSSE-NG/a898f469851c464d05137bb74b069dd234897643/include/RE/E/ExtraTeleport.h
- Spell-cast event: https://raw.githubusercontent.com/alandtse/CommonLibSSE-NG/a898f469851c464d05137bb74b069dd234897643/include/RE/T/TESSpellCastEvent.h
- Object-loaded event: https://raw.githubusercontent.com/alandtse/CommonLibSSE-NG/a898f469851c464d05137bb74b069dd234897643/include/RE/T/TESObjectLoadedEvent.h
- TESGlobal: https://raw.githubusercontent.com/alandtse/CommonLibSSE-NG/a898f469851c464d05137bb74b069dd234897643/include/RE/T/TESGlobal.h
- xEdit TES5 record definitions: https://github.com/TES5Edit/TES5Edit/blob/dev-4.1.5/Core/wbDefinitionsTES5.pas

DoorTransit uses the same engine MoveTo_Impl signature and relocation pair (56227, 56626), with destination transform from ExtraTeleport. Review of a declaration/record schema does NOT establish compatibility through an actual game loader. Package template IDs and non-animation quest structures were also inspected in the user's supplied plugin records, not copied wholesale into the release.


## Kernel11 review

See KERNEL11_AUDIT_zh-CN.md for the pinned crime vtable prototypes, ControlMap masks, PlayerCharacter state members, TESQuest accessors, BGSScene (no C++ Stop method), and xEdit QUST fields. Full Windows/API compilation and gameplay are not claimed.
