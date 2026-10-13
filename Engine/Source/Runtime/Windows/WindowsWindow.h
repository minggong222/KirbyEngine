#pragma once
#include "FVertexSimple.h"
#include "Core/STL.h"

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, uint32 msg, WPARAM wParam, LPARAM lParam);

class FWindowsWindow
{
public:
    void Create(HINSTANCE hInstance, int InWidth, int InHeight);
    HWND GetHWND();
    // 각종 메시지를 처리할 함수
    static LRESULT CALLBACK WndProc(HWND hWnd, uint32 message, WPARAM wParam, LPARAM lParam);
private:
    HWND m_hWnd;
};
