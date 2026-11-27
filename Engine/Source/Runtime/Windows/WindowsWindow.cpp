#include "WindowsWindow.h"

bool FWindowsWindow::Create(HINSTANCE hInstance, int InWidth, int InHeight)
{
    const wchar_t Title[] = L"Kirby Engine";
    const wchar_t ClassName[] = L"EngineWindowClass";

    WNDCLASSW WindowClass = {};
    WindowClass.lpfnWndProc = WndProc;
    WindowClass.hInstance = hInstance;
    WindowClass.lpszClassName = ClassName;

    if (RegisterClassW(&WindowClass) == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
    {
        return false;
    }

    m_hWnd = CreateWindowExW(0, ClassName, Title, WS_POPUP | WS_VISIBLE | WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, InWidth, InHeight, nullptr, nullptr, hInstance, nullptr);

    return m_hWnd != nullptr;
}

void FWindowsWindow::Destroy()
{
    if (m_hWnd != nullptr)
    {
        if (IsWindow(m_hWnd))
        {
            DestroyWindow(m_hWnd);
        }

        m_hWnd = nullptr;
    }
}

LRESULT FWindowsWindow::WndProc(HWND hWnd, uint32 message, WPARAM wParam, LPARAM lParam)
{
    if (MessageHook != nullptr && MessageHook(hWnd, message, wParam, lParam))
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
