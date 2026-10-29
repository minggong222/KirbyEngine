#include "EditorCameraController.h"

#include "imgui.h"
#include "Components/CameraComponent.h"
#include "Core/Math/MathUtility.h"
#include "Windows/WindowsWindow.h"

void FEditorCameraController::Update(UCameraComponent& Camera, FWindowsWindow& Window, float DeltaTime)
{
    const bool bRightMouseDown = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
    HWND WindowHandle = Window.GetHWND();
        
    if (bRightMouseDown && !bCameraLookActive)
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
    else if(!bRightMouseDown && bCameraLookActive)
    {
        bCameraLookActive = false;
        if (GetCapture() == WindowHandle)
        {
            ReleaseCapture();
        }
        ShowCursor(TRUE);
    }
    
    if (bCameraLookActive)
    {
        RECT ClientRect{};
        GetClientRect(WindowHandle, &ClientRect);
        POINT Center = { (ClientRect.left+ClientRect.right) / 2, (ClientRect.top+ClientRect.bottom) / 2 };
        
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
            
            CameraTransform.Location += MoveDirection * (Camera.GetMoveSpeed() * static_cast<float>(DeltaTime));
        }
        Camera.SetTransform(CameraTransform);
        SetCursorPos(Center.x, Center.y);
    }
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
