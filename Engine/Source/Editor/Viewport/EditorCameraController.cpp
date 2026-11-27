#include "Viewport/EditorCameraController.h"

#include "imgui.h"
#include "Components/CameraComponent.h"
#include "Core/Math/MathUtility.h"
#include "Windows/WindowsWindow.h"

void FEditorCameraController::Update(UCameraComponent& Camera, FWindowsWindow& Window, float DeltaTime, bool bViewportHovered)
{
    HWND WindowHandle = Window.GetHWND();

    if (WindowHandle == nullptr)
    {
        bCameraLookActive = false;
        return;
    }

    const bool bRightMouseDown = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
    const bool bWindowFocused = GetForegroundWindow() == WindowHandle;
    const bool bCaptureLost = bCameraLookActive && GetCapture() != WindowHandle;

    if (!bCameraLookActive && bRightMouseDown && bViewportHovered && bWindowFocused)
    {
        bCameraLookActive = true;

        SetCapture(WindowHandle);
        ShowCursor(FALSE);

        RECT ClientRect{};
        GetClientRect(WindowHandle, &ClientRect);

        POINT Center = {(ClientRect.left + ClientRect.right) / 2, (ClientRect.top + ClientRect.bottom) / 2};

        ClientToScreen(WindowHandle, &Center);
        SetCursorPos(Center.x, Center.y);
    }
    else if (bCameraLookActive && (!bRightMouseDown || !bWindowFocused || bCaptureLost))
    {
        Stop(Window);
        return;
    }

    if (!bCameraLookActive)
    {
        return;
    }

    RECT ClientRect{};
    GetClientRect(WindowHandle, &ClientRect);

    POINT Center = {(ClientRect.left + ClientRect.right) / 2, (ClientRect.top + ClientRect.bottom) / 2};
    ClientToScreen(WindowHandle, &Center);

    POINT MousePosition{};
    GetCursorPos(&MousePosition);

    const float DeltaX = static_cast<float>(MousePosition.x - Center.x);
    const float DeltaY = static_cast<float>(MousePosition.y - Center.y);

    FTransform CameraTransform = Camera.GetTransform();

    CameraTransform.Rotation.Yaw += DeltaX * Camera.GetMouseSensitivity();
    CameraTransform.Rotation.Pitch -= DeltaY * Camera.GetMouseSensitivity();

    CameraTransform.Rotation.Pitch = std::clamp(CameraTransform.Rotation.Pitch, -89.0f, 89.0f);

    const FVector Forward = CameraTransform.GetForward();
    const FVector Right = CameraTransform.GetRight();

    FVector MoveDirection = FVector::Zero;

    if (GetAsyncKeyState('W') & 0x8000)
    {
        MoveDirection += Forward;
    }

    if (GetAsyncKeyState('S') & 0x8000)
    {
        MoveDirection -= Forward;
    }

    if (GetAsyncKeyState('D') & 0x8000)
    {
        MoveDirection += Right;
    }

    if (GetAsyncKeyState('A') & 0x8000)
    {
        MoveDirection -= Right;
    }

    if (GetAsyncKeyState('E') & 0x8000)
    {
        MoveDirection += EngineAxis::Up;
    }

    if (GetAsyncKeyState('Q') & 0x8000)
    {
        MoveDirection -= EngineAxis::Up;
    }

    if (MoveDirection.LengthSquared() > EPSILON_1E_6)
    {
        MoveDirection.Normalize();
        CameraTransform.Location += MoveDirection * (Camera.GetMoveSpeed() * DeltaTime);
    }

    Camera.SetTransform(CameraTransform);
    SetCursorPos(Center.x, Center.y);
}

void FEditorCameraController::Stop(FWindowsWindow& Window)
{
    HWND WindowHandle = Window.GetHWND();

    if (bCameraLookActive)
    {
        if (GetCapture() == WindowHandle)
        {
            ReleaseCapture();
        }

        ShowCursor(TRUE);

        bCameraLookActive = false;
    }
}
