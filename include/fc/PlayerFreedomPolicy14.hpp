#pragma once
#include "fc/Core.hpp"
#include <cmath>

namespace fc {
// Intent is runtime-only: never inherit an interaction permission across loads.
// Observe the mapped Activate action, not a physical E key, and bind permission
// to its actor. A nearby forced greeting must not consume somebody else's input.
class PlayerIntent14 {
    ID pendingActor_{}, speaker_{}, scene_{};
    double pendingUntil_{}, openingUntil_{}, sceneOpeningUntil_{}, missingSpeakerUntil_{};
    bool dialogueWasOpen_{};
public:
    void Reset() { *this = {}; }
    void CancelPending() { pendingActor_ = 0; pendingUntil_ = 0; }
    void Pause(bool dialogue) {
        CancelPending();sceneOpeningUntil_=0;
        if(!dialogue)Reset();
    }
    void Activate(ID actor, double now) {
        CancelPending();
        if (!actor || actor == 0x14 || !std::isfinite(now)) return;
        pendingActor_ = actor; pendingUntil_ = now + 2.0;
    }
    struct Result { bool voluntaryDialogue{}, voluntaryScene{}, closeDialogue{}, releaseScene{}; };
    Result Observe(bool dialogue, ID speaker, ID scene, double now, bool enabled, bool preserveVoluntary) {
        Result result;
        if (!std::isfinite(now)) { Reset(); return result; }
        if (!enabled) { Reset(); return result; }
        if (dialogue && !dialogueWasOpen_) openingUntil_ = now + 0.20;
        if (!dialogue && dialogueWasOpen_) {
            speaker_ = 0;
            // A new cutscene after the dialogue ends needs explicit temporary
            // story permission; do not turn one activation into a global pass.
            scene_ = 0;
            CancelPending();sceneOpeningUntil_=missingSpeakerUntil_=0;
        }
        dialogueWasOpen_ = dialogue;
        if (pendingActor_ && now > pendingUntil_) pendingActor_ = 0;
        if (!preserveVoluntary) { pendingActor_ = speaker_ = scene_ = 0; }
        // A different speaker cannot retain the original target's pending pass.
        if(dialogue && speaker && pendingActor_ && speaker!=pendingActor_)CancelPending();
        if (dialogue && speaker && pendingActor_ == speaker && now <= pendingUntil_) {
            speaker_ = speaker; pendingActor_ = 0;
            scene_ = scene;sceneOpeningUntil_=pendingUntil_;
        }
        // Dialogue sessions may last indefinitely, but only with the actor the
        // player activated. Closing/reopening or changing speaker revokes it.
        if (dialogue && speaker_ && speaker && speaker != speaker_) { speaker_ = scene_ = 0; }
        if(dialogue && speaker_ && !speaker) {
            if(!missingSpeakerUntil_)missingSpeakerUntil_=now+0.20;
            if(now>=missingSpeakerUntil_)speaker_=scene_=0;
        } else missingSpeakerUntil_=0;
        result.voluntaryDialogue = dialogue && speaker_ && (!speaker || speaker == speaker_);
        // A later unrelated scene must not inherit an indefinitely long dialogue
        // permission merely because the original opening had no player scene.
        if (result.voluntaryDialogue && scene && !scene_ && now<=sceneOpeningUntil_) scene_ = scene;
        result.voluntaryScene = result.voluntaryDialogue && scene && scene == scene_;
        // Give the UI a short opening grace to publish its speaker handle; never
        // classify unknown speaker as deliberate or grant an unbounded exemption.
        result.closeDialogue = dialogue && !result.voluntaryDialogue && (speaker || now >= openingUntil_);
        result.releaseScene = scene && !result.voluntaryScene &&
            !(dialogue && !speaker && now < openingUntil_);
        return result;
    }
};
}
