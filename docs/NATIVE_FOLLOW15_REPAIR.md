# Native follow repair evidence (Freedom15)

## Findings

The shipped FreedomControlRuntime.esp is byte-identical in the supplied 0.5.0,
0.5.1, 0.5.2 and 0.5.3 archives (SHA-256
`f832556ab884706142d20d846e12def6707e4db248f5dc9a17d6cf7415d1a24c`).
This rules out a new ESP byte change between those versions. It does not establish
one unique cause for the user's observed standstill or prove that Skyrim loaded it.

The former native control path had these demonstrable gaps:

1. Recruitment and friendly spawning were rejected unless the quest was already
   running. A delayed or refused quest start had no independent fallback.
2. `SetFollowPackage` returned immediately when alias binding failed. Its advertised
   actor-local fallback was therefore unreachable in that failure case.
3. It accepted only the predicted distance-band/lane pointer. The runtime's own
   unconditional fallback, which is deliberately last in every alias stack, was
   misclassified as failure and could be cancelled by reset-AI evaluations and
   repeated interrupt replacement.
4. That replacement reused the conditional desired package rather than the
   unconditional fallback, retaining faction/global selector prerequisites.
5. Birth confirmation likewise demanded an alias binding and the exact predicted
   package even when the actual fallback was running.
6. Stuck detection repeatedly interrupted an already selected own package after
   2.5 seconds without progress, before the existing recovery timeout.

These are source/control-path findings. The affected user's game executable,
active actor packages and save were not available for native reproduction.

## Repair

- The authored unconditional package `0x80A` remains required. A running quest or
  filled alias is no longer required to retain recruitment or use that package.
- High-priority reference aliases remain the normal selection route. After bounded
  attempts without an observed own package, hard/enhanced control can apply the
  authored unconditional package to this actor reference only.
- Any existing own authored follow package is allowed to keep running. Routine
  evaluation no longer requests `resetAI=true`. A newly acquired alias requests
  one selection refresh to promote a temporary fallback to the configured distance
  or dragon orbit; the same binding never triggers repeated refreshes. A refresh
  waits through combat/loading. Explicit distance changes also reevaluate.
- The progress tolerance follows the observed package's real band/fallback radius.
- A selected own package is not interrupted repeatedly at the preliminary stuck
  threshold. Existing bounded recovery remains available if progress never arrives.
- Existing wait/freeze/combat safeguards and release/restoration remain in force.
  Faction isolation and teammate/actor-value changes are reference-local. No shared
  NPC, race, package record, external follower quest or external mod script is edited.
- Friendly-spawn model readiness and actual own-package observation remain separate.
  An observed package still does not prove movement, animation or valid pathfinding.

## Generic architecture reference

[PapyrusUtil's primary PackageData.cpp source](https://github.com/eeveelo/PapyrusUtil/blob/master/PackageData.cpp)
implements reference-keyed package overrides, priority selection and an optional
condition bypass at package selection. This demonstrates the generic separation
between per-reference control and shared actor templates. The repair introduces no
PapyrusUtil dependency and does not copy its hooks or hard-coded addresses.

[CommonLib Actor.cpp](https://github.com/CharmedBaryon/CommonLibSSE-NG/blob/main/src/RE/A/Actor.cpp)
provides EvaluatePackage, EndInterruptPackage and PutCreatedPackage. Its
GetCurrentPackage delegates to the process's running package.
[AIProcess.cpp](https://github.com/CharmedBaryon/CommonLibSSE-NG/blob/main/src/RE/A/AIProcess.cpp)
checks the run-once package before the ordinary current package. A request and a
subsequent observation are distinct; neither is proof that navigation progressed.

The actual direct-player Follow package schema is independently corroborated by
[AppleCatPackage in the neutral HappyCat.esp fixture](https://github.com/DanW1206/SkyrimFollowerMod/blob/main/HappyCat.esp)
and [xEdit's primary TES5 definitions](https://github.com/TES5Edit/TES5Edit/blob/dev-4.1.6/Core/wbDefinitionsTES5.pas).
No speculative binary-format rewrite was made.

## Verification boundaries

`tests/runtime_fixture15.py` reads the actual shipped ESP's packages, target,
conditions and alias ordering. The production native-control seam uses those facts
in an explicitly limited condition/selection model. Wrong band/lane and dragon
packages are rejected; queued evaluation, alias rejection and quest-start refusal
are tested. The fixture is not Skyrim's loader, procedure-tree evaluator,
character controller or pathfinder. It must not be described as in-game validation.

Windows CommonLib/SKSE compilation and Skyrim 1.6.1170 testing are separate gates.
In-game acceptance must include humanoid, silent ground creature, swimming/flying
creature and dragon; normal travel and cross-cell recovery; wait/resume,
freeze/thaw, combat, release; save/reload; and same-base unregistered actors remaining
unaffected. There is no E-key/dialogue prerequisite.
