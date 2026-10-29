#include "EditorEngine.h"

#include "EditorCameraController.h"
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
        
        CameraController.Update(*MainCamera, *WindowsWindow, static_cast<float>(DeltaTime));
        
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
    
   CameraController.Stop(*WindowsWindow);
    
    Renderer->ReleaseVertexBuffer(vertexBufferSphere);
    delete Test;
    delete MainCamera;
    
}

void FEditorEngine::Shutdown()
{
    ImGuiRenderer->Shutdown();
    Renderer->Shutdown();
}
