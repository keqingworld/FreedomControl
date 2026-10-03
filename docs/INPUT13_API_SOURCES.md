# Input13 historical API references

This describes the 0.5.2 baseline. Freedom14 changes game-event propagation during F8 capture; see `INPUT14_NOTES.md`.
- https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwindowshookexw
- https://learn.microsoft.com/en-us/windows/win32/winmsg/getmsgproc
- https://learn.microsoft.com/en-us/windows/win32/winmsg/mouseproc
- https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-tounicodeex
- https://raw.githubusercontent.com/ocornut/imgui/master/backends/imgui_impl_win32.cpp
- https://ng.commonlib.dev/classRE_1_1BSInputDeviceManager.html
- https://ng.commonlib.dev/ButtonEvent_8h_source.html
- https://ng.commonlib.dev/InputMap_8h_source.html

Only current-process, game-window-thread hooks are used. No global keyboard hook, raw-input registration replacement, or typed-text logging is introduced. Hook callbacks enqueue data; ImGui is called on the render thread. In the Input13 baseline, game event lists were observed without modification and propagation continued. In Freedom14, intent observation still does not modify event lists, but F8 capture explicitly returns kStop for downstream delivery.
