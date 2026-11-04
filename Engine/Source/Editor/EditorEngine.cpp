#include "EditorEngine.h"

#include "EditorCameraController.h"
#include "Components/CameraComponent.h"
#include "Components/SphereComp.h"
#include "Components/CubeComp.h"
#include "Core/STL.h"
#include "Core/Math/MathUtility.h"
#include "Core/Time/PlatformTime.h"
#include "UI/Panels/DetailsPanel.h"
#include "UI/Panels/ObjectListPanel.h"
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
    ObjectListPanel = new FObjectListPanel(&ObjectContext, Renderer);
    ImGuiRenderer->GetPanelManager()->AddPanel(ObjectListPanel);
    DetailsPanel = new FDetailsPanel(&ObjectContext);
    ImGuiRenderer->GetPanelManager()->AddPanel(DetailsPanel);
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
            const FMatrix View = MainCamera->GetViewMatrix();
            const FMatrix Projection = MainCamera->GetProjectionMatrix();
            
            for (const FEditorObjectEntry& Entry : ObjectContext.Objects)
            {
                UObject* Object = Entry.Object;
                if (Object == nullptr)
                {
                    continue;
                }
                
                if (Entry.VertexBuffer == nullptr || Entry.VertexCount == 0)
                {
                    continue;
                }
                
                USceneComponent* Scene = Cast<USceneComponent>(Object);
                if (Scene == nullptr)
                {
                    continue;
                }
                
                const FMatrix World = Scene->GetTransform().GetLocalMatrix();
                Renderer->UpdateConstant(World, View, Projection);
                Renderer->RenderPrimitive(Entry.VertexBuffer, Entry.VertexCount);
            }
            Renderer->EndViewportRender();            
        }

        ImGuiRenderer->OnRender();
        Renderer->SwapBuffer();
    }
    
   CameraController.Stop(*WindowsWindow);
    delete MainCamera;
    
}

void FEditorEngine::Shutdown()
{
    for (FEditorObjectEntry& Entry : ObjectContext.Objects)
    {
        if (Entry.VertexBuffer != nullptr)
        {
            Renderer->ReleaseVertexBuffer(Entry.VertexBuffer);
            Entry.VertexBuffer = nullptr;
        }
        
        if (Entry.Object != nullptr)
        {
            delete Entry.Object;
            Entry.Object = nullptr;
        }
    }
    ObjectContext.Objects.Reset();
    ObjectContext.SelectedObject = nullptr;
    ImGuiRenderer->Shutdown();
    Renderer->Shutdown();
}
