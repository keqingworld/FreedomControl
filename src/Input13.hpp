#pragma once
#include <Windows.h>
namespace fc::input13 {
void Install(HWND window);
void InstallGameInput();
void GameWheel(float vertical);
void Frame(bool active,int hotkey);
void WindowMessage(HWND window,UINT message,WPARAM wparam,LPARAM lparam);
void Shutdown();
struct Counters {unsigned long long keys{},characters{},wheels{};bool messages{},mouse{};};
Counters Snapshot();
}
