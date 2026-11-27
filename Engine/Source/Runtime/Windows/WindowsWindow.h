#pragma once
#include "Core/STL.h"

// Optional hook that sees every window message first (the editor uses it to feed ImGui).
// Return true when the message was handled and should not reach the default handling.
using FWindowMessageHook = bool (*)(HWND hWnd, uint32 Message, WPARAM wParam, LPARAM lParam);

class FWindowsWindow
{
public:
    static void SetMessageHook(FWindowMessageHook InHook) { MessageHook = InHook; }

    bool Create(HINSTANCE hInstance, int InWidth, int InHeight);
    void Destroy();
    HWND GetHWND() const { return m_hWnd; }
    // 각종 메시지를 처리할 함수
    static LRESULT CALLBACK WndProc(HWND hWnd, uint32 message, WPARAM wParam, LPARAM lParam);
private:
    inline static FWindowMessageHook MessageHook = nullptr;

    HWND m_hWnd = nullptr;
};
