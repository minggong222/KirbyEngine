#include "WindowsWindow.h"

void FWindowsWindow::Create(HINSTANCE hInstance, int InWidth, int InHeight)
{
    // 각종 메시지를 처리할 함수인 WndProc의 함수 포인터를 WindowClass 구조체에 넣는다.
    const wchar_t Title[] = L"Kirby Engine";
    const wchar_t CLASS_NAME[] = L"EngineWindowClass";
    WNDCLASSW wndclass;
    wndclass = {0, WndProc, 0, 0, 0, 0, 0, 0, 0, CLASS_NAME};
    RegisterClassW(&wndclass);

    // 1024 x 1024 크기에 윈도우 생성
    m_hWnd = CreateWindowExW(0, CLASS_NAME, Title, WS_POPUP | WS_VISIBLE | WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, InWidth, InHeight,
        nullptr, nullptr, hInstance, nullptr);
}

HWND FWindowsWindow::GetHWND()
{
    return m_hWnd;
}

LRESULT FWindowsWindow::WndProc(HWND hWnd, uint32 message, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam))
    {
        return true;
    }

    switch (message)
    {
    case WM_DESTROY:
        // Signal that the app should quit
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }

    return 0;
}
