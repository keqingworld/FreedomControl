#include "PCH.h"
#include "Input13.hpp"
#include "Engine.hpp"
#include "fc/InputPolicy13.hpp"
#include <RE/U/UserEvents.h>
#include <RE/M/MenuOpenCloseEvent.h>
namespace fc::input13 {
namespace {
class GameInput final : public RE::BSTEventSink<RE::InputEvent*> {
    ButtonQuarantine quarantine_;
public:
    RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* events,RE::BSTEventSource<RE::InputEvent*>*) override {
        if(!events || !*events)return RE::BSEventNotifyControl::kContinue;
        // Read the published menu flag directly, not last render frame's text
        // focus. All F8 controls own input, including the first click into a field.
        const bool capture=Engine::Get().MenuOpen();
        bool stop=capture;
        for(auto* event=*events;event;event=event->next)if(const auto* button=event->AsButtonEvent()) {
            const auto device=static_cast<unsigned>(button->GetDevice());
            stop=quarantine_.Observe(device,button->GetIDCode(),button->IsPressed(),button->IsDown()) || stop;
        }
        if(stop) {
            for(auto* event=*events;event;event=event->next)if(const auto* button=event->AsButtonEvent()) {
                // Skyrim mouse IDs 8/9 are transient wheel events, not held keys.
                const auto device=button->GetDevice();const auto id=button->GetIDCode();
                if(device==RE::INPUT_DEVICE::kMouse && (id==8 || id==9)) {
                    if(capture && button->IsPressed())GameWheel(id==8?1.0f:-1.0f);
                } else quarantine_.Capture(static_cast<unsigned>(device),id,button->IsPressed());
            }
            // kStop stops later listeners on this source. It is not an engine
            // cancellation API and cannot block mods polling keys independently
            // or a listener that was prepended ahead of this one after install.
            return RE::BSEventNotifyControl::kStop;
        }
        for(auto* event=*events;event;event=event->next) {
            const auto* button=event->AsButtonEvent();
            if(!button || !button->IsDown())continue;
            const auto device=button->GetDevice();
            if(device!=RE::INPUT_DEVICE::kKeyboard && device!=RE::INPUT_DEVICE::kMouse && device!=RE::INPUT_DEVICE::kGamepad)continue;
            if(auto* names=RE::UserEvents::GetSingleton()) {
                bool activate=button->GetUserEvent()==names->activate;
                if(!activate)if(auto* mapping=RE::ControlMap::GetSingleton()) {
                    const auto key=mapping->GetMappedKey(names->activate.c_str(),device);
                    activate=key!=RE::ControlMap::kInvalid && key==button->GetIDCode();
                }
                if(activate)Engine::Get().RecordPlayerActivation14();
            }
        }
        return RE::BSEventNotifyControl::kContinue;
    }
};
class DialogueEvents final : public RE::BSTEventSink<RE::MenuOpenCloseEvent> {
public:
    RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* event,RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override {
        // Only publish a generation. A fast close/reopen can happen between game
        // tasks; never let the old speaker lease authorize that new conversation.
        if(event && !event->opening && event->menuName=="Dialogue Menu")Engine::Get().RecordPlayerDialogueClosed14();
        return RE::BSEventNotifyControl::kContinue;
    }
};
GameInput sink;DialogueEvents dialogueSink;bool registered{},dialogueRegistered{};
}
void InstallGameInput(){
    if(!registered)if(auto* manager=RE::BSInputDeviceManager::GetSingleton()) {
        // DataLoaded is outside input dispatch. Prepending protects downstream
        // vanilla/SKSE listeners without replacing a vtable or an event list.
        manager->PrependEventSink(&sink);registered=true;
        spdlog::info("Freedom14: F8 input capture + mapped Activate observer installed before existing input listeners.");
    }
    if(!dialogueRegistered)if(auto* ui=RE::UI::GetSingleton()) {
        ui->AddEventSink<RE::MenuOpenCloseEvent>(&dialogueSink);dialogueRegistered=true;
    }
}
}
