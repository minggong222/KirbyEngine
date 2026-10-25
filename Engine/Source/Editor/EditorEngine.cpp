#include "EditorEngine.h"

#include "Components/CameraComponent.h"
#include "Components/SphereComp.h"
#include "Core/STL.h"
#include "Core/Math/MathUtility.h"
#include "Core/Time/PlatformTime.h"
#include "UObject/Object.h"

bool FEditorEngine::Init(HINSTANCE hInstance)
{
   
    Renderer = new URenderer();
    ImGuiRenderer = new FImGuiRenderer();
    WindowsWindow = new FWindowsWindow();
    WindowsWindow->Create(hInstance, 1920, 1080);
    {
        Renderer->Create(WindowsWindow->GetHWND());
        Renderer->CreateShader();
        Renderer->CreateConstantBuffer();
        Renderer->CreateViewportRenderTarget(1280, 720);
    }
    ImGuiRenderer->Init(WindowsWindow->GetHWND(), Renderer->GetDevice(), Renderer->GetDeviceContext());
    
    ViewportPanel = new FViewportPanel();
    ViewportPanel->SetViewportTexture(Renderer->GetViewportSRV());
    ImGuiRenderer->GetPanelManager()->AddPanel(ViewportPanel);
    ConsolePanel = new FConsolePanel();
    ImGuiRenderer->GetPanelManager()->AddPanel(ConsolePanel);
    
    bIsRunning = true;
    UE_LOG(Info, "Info");
    UE_LOG(Error, "Error");
    UE_LOG(Warning, "Warning");
    return true;
}

void FEditorEngine::Run()
{

    double PreviousTime = FPlatformTime::Seconds();
    UCameraComponent* MainCamera = new UCameraComponent();
    USphereComp* Test = new USphereComp();
    TArray<FVertexSimple> Vertices = Test->GetVertex();
    ID3D11Buffer* vertexBufferSphere = Renderer->CreateVertexBuffer(Vertices.GetData(), static_cast<uint32>(sizeof(FVertexSimple) * Vertices.Num()));
    
    HWND WindowHandle = WindowsWindow->GetHWND();

    bool bCameraLookActive = false;
    
    while (bIsRunning == true)
    {
        const double CurrentTime = FPlatformTime::Seconds();
        const double DeltaTime = CurrentTime - PreviousTime;
        
        PreviousTime = CurrentTime;
        FrameStats.Update(DeltaTime);
        
        MSG msg;
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);

            
            //입력은 누가 가지냐
            if (msg.message == WM_QUIT)
            {
                bIsRunning = false;

                break;
            }
        }
        
        const bool bRightMouseDown = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
        
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
            FTransform CameraTransform = MainCamera->GetTransform();
            
            CameraTransform.Rotation.Yaw += DeltaX * MainCamera->GetMouseSensitivity();
            CameraTransform.Rotation.Pitch -= DeltaY * MainCamera->GetMouseSensitivity();
            
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
                
                CameraTransform.Location += MoveDirection * (MainCamera->GetMoveSpeed() * static_cast<float>(DeltaTime));
            }
            MainCamera->SetTransform(CameraTransform);
            SetCursorPos(Center.x, Center.y);
        }
        
        {
            // 준비 작업
            Renderer->Prepare();
            Renderer->BeginViewportRender();
            Renderer->PrepareShader();
            const FMatrix World = Test->GetTransform().GetLocalMatrix();
            const FMatrix View = MainCamera->GetViewMatrix();
            const FMatrix Projection = MainCamera->GetProjectionMatrix();
            Renderer->UpdateConstant(World, View, Projection);
            Renderer->RenderPrimitive(vertexBufferSphere, static_cast<uint32>(Vertices.Num()));
            Renderer->EndViewportRender();
            
        }

        ImGuiRenderer->OnRender();
        Renderer->SwapBuffer();
    }
    
    if (bCameraLookActive)
    {
        if (GetCapture() == WindowHandle)
        {
            ReleaseCapture();
        }
        ShowCursor(TRUE);
    }
    
    Renderer->ReleaseVertexBuffer(vertexBufferSphere);
    delete Test;
    delete MainCamera;
    
}

void FEditorEngine::Shutdown()
{
    ImGuiRenderer->Shutdown();
    Renderer->Shutdown();
}
