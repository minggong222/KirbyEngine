#include "EditorEngine.h"

#include "EditorCameraController.h"
#include "EditorObjectPicker.h"
#include "Components/CameraComponent.h"
#include "Components/SphereComp.h"
#include "Components/CubeComp.h"
#include "Components/StaticMeshComponent.h"
#include "Core/STL.h"
#include "Core/Math/MathUtility.h"
#include "Core/Time/PlatformTime.h"
#include "Resources/PrimitiveMeshAssets.h"
#include "UI/Panels/DetailsPanel.h"
#include "UI/Panels/ObjectListPanel.h"
#include "UI/Panels/MainMenuPanel.h"
#include "UI/Panels/StatsPanel.h"
#include "UObject/Object.h"
#include "Resources/StaticMesh.h"

bool FEditorEngine::Init(HINSTANCE hInstance)
{
   
    Renderer = new URenderer();
    ImGuiRenderer = new FImGuiRenderer();
    WindowsWindow = new FWindowsWindow();

    if (WindowsWindow == nullptr || !WindowsWindow->Create(hInstance, 1920, 1080))
    {
        Shutdown();
        return false;
    }
    RECT ClientRect = {};
    GetClientRect(WindowsWindow->GetHWND(), &ClientRect);
    const uint32 InitialWidth = ClientRect.right > ClientRect.left ? static_cast<uint32>(ClientRect.right - ClientRect.left) : 1;
    const uint32 InitialHeight = ClientRect.bottom > ClientRect.top ? static_cast<uint32>(ClientRect.bottom - ClientRect.top) : 1;  
    {
        Renderer->Create(WindowsWindow->GetHWND());
        Renderer->CreateShader();
        Renderer->CreateConstantBuffer();
        Renderer->CreateViewportRenderTarget(InitialWidth, InitialHeight);

        if (!Renderer->IsReady())
        {
            UE_LOG(Error, "Renderer initialization failed");
            Shutdown();
            return false;
        }
        
        if (!RegisterBuiltinStaticMeshes(AssetManager, *Renderer))
        {
            UE_LOG(Error, "Failed to register builtin static meshes");
            Shutdown();
            return false;
        }
    }

    if (!ImGuiRenderer->Init(WindowsWindow->GetHWND(), Renderer->GetDevice(), Renderer->GetDeviceContext()))
    {
        UE_LOG(Error, "ImGui initialization failed");
        Shutdown();
        return false;
    }
    
    MainCamera = new UCameraComponent();
    
    ViewportPanel = new FViewportPanel();
    ViewportPanel->SetViewportTexture(Renderer->GetViewportSRV());
    if (!ImGuiRenderer->GetPanelManager()->AddPanel(ViewportPanel))
    {
        ViewportPanel = nullptr;
        Shutdown();
        return false;
    }
    
    ConsolePanel = new FConsolePanel();
    if (!ImGuiRenderer->GetPanelManager()->AddPanel(ConsolePanel))
    {
        ConsolePanel = nullptr;
        Shutdown();
        return false;
    }
    
    ObjectListPanel = new FObjectListPanel(&ObjectContext, &AssetManager, ViewportPanel, Renderer);
    if (!ImGuiRenderer->GetPanelManager()->AddPanel(ObjectListPanel))
    {
        ObjectListPanel = nullptr;
        Shutdown();
        return false;
    }
    
    DetailsPanel = new FDetailsPanel(&ObjectContext);
    if (!ImGuiRenderer->GetPanelManager()->AddPanel(DetailsPanel))
    {
        DetailsPanel = nullptr;
        Shutdown();
        return false;
    }
    
    StatsPanel = new FStatsPanel(&FrameStats, &ObjectContext);
    if (!ImGuiRenderer->GetPanelManager()->AddPanel(StatsPanel))
    {
        StatsPanel = nullptr;
        Shutdown();
        return false;
    }
    
    ConsolePanel->SetStatsPanel(StatsPanel);
    MainMenuPanel = new FMainMenuPanel(&ObjectContext, &AssetManager, WindowsWindow->GetHWND(), MainCamera, Renderer);
    if (!ImGuiRenderer->GetPanelManager()->AddPanel(MainMenuPanel))
    {
        MainMenuPanel = nullptr;
        Shutdown();
        return false;
    }
    
    ObjectContext.UndoManager = &UndoManager;
    
    bIsRunning = true;
    return true;
}

void FEditorEngine::Run()
{

    double PreviousTime = FPlatformTime::Seconds();
    
    if (Renderer->GetViewportWidth() > 0 && Renderer->GetViewportHeight() > 0)
    {
        const float AspectRatio = static_cast<float>(Renderer->GetViewportWidth()) / static_cast<float>(Renderer->GetViewportHeight());
        MainCamera->SetAspectRatio(AspectRatio);
    }
    
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
        
        RECT ClientRect = {};
        if (GetClientRect(WindowsWindow->GetHWND(), &ClientRect))
        {
            const LONG ClientWidth = ClientRect.right - ClientRect.left;
            const LONG ClientHeight = ClientRect.bottom - ClientRect.top;
            if (ClientWidth > 0 && ClientHeight > 0)
            {
                Renderer->ResizeBackBuffer(static_cast<uint32>(ClientWidth), static_cast<uint32>(ClientHeight));
            }
        }
        ProcessViewportPicking(*MainCamera);
        
        uint32 DesiredWidth = 0;
        uint32 DesiredHeight = 0;
        if (ViewportPanel->ConsumeDesiredSize(DesiredWidth, DesiredHeight))
        {
            Renderer->ResizeViewportRenderTarget(DesiredWidth, DesiredHeight);
            ViewportPanel->SetViewportTexture(Renderer->GetViewportSRV());
        }
        
        const uint32 ViewportWidth = Renderer->GetViewportWidth();
        const uint32 ViewportHeight = Renderer->GetViewportHeight();
        if (ViewportWidth > 0 && ViewportHeight > 0)
        {
            const float AspectRatio = static_cast<float>(ViewportWidth) / static_cast<float>(ViewportHeight);
            MainCamera->SetAspectRatio(AspectRatio);
        }
        CameraController.Update(*MainCamera, *WindowsWindow, static_cast<float>(DeltaTime), ViewportPanel->IsViewportHovered());
        
        const FMatrix View = MainCamera->GetViewMatrix();
        const FMatrix Projection = MainCamera->GetProjectionMatrix();
        
        ViewportPanel->SetGizmoContext(&ObjectContext, View, Projection);
        
        {
            // 준비 작업
            Renderer->Prepare();
            Renderer->BeginViewportRender();
            Renderer->PrepareShader();
            
            for (UObject* Object : ObjectContext.Objects)
            {
                if (Object == nullptr)
                {
                    continue;
                }
                
                UStaticMeshComponent* Component = Cast<UStaticMeshComponent>(Object);
                
                if (Component == nullptr)
                {
                    continue;
                }
                
                const UStaticMesh* Mesh  = Component->GetStaticMesh();
                
                if (Mesh  == nullptr)
                {
                    continue;
                }
                
                const FStaticMeshRenderData& RenderData = Mesh->GetRenderData();
                
                if (RenderData.VertexBuffer == nullptr || RenderData.VertexCount == 0)
                {
                    continue;
                }
                
                const FMatrix World = Component->GetWorldMatrix();
                Renderer->UpdateConstant(World, View, Projection, false);
                Renderer->RenderPrimitive(RenderData.VertexBuffer, RenderData.VertexCount);
            }
            
            UStaticMeshComponent* SelectedComponent = Cast<UStaticMeshComponent>(ObjectContext.SelectedObject);

            if (SelectedComponent != nullptr)
            {
                const UStaticMesh* SelectedMesh = SelectedComponent->GetStaticMesh();

                if (SelectedMesh != nullptr)
                {
                    const FStaticMeshRenderData& SelectedRenderData = SelectedMesh->GetRenderData();

                    if (SelectedRenderData.VertexBuffer != nullptr && SelectedRenderData.VertexCount > 0)
                    {
                        const FMatrix OutlineScale = FMatrix::CreateScale(FVector(1.03f, 1.03f, 1.03f));
                        const FMatrix OutlineWorld = OutlineScale * SelectedComponent->GetWorldMatrix();

                        Renderer->UpdateConstant(OutlineWorld, View, Projection, true);
                        Renderer->RenderPrimitive(SelectedRenderData.VertexBuffer, SelectedRenderData.VertexCount, true);
                    }
                }
            }
            
            Renderer->EndViewportRender();            
        }
        
        ImGuiRenderer->OnRender();
        
        const ImGuiIO& IO = ImGui::GetIO();

        const bool bEditorFocused = WindowsWindow != nullptr && GetForegroundWindow() == WindowsWindow->GetHWND();
        const bool bCanProcessShortcut = bEditorFocused && !IO.WantTextInput && !IO.WantCaptureKeyboard && !ImGui::IsAnyItemActive();

        if (bCanProcessShortcut)
        {
            if (IO.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_C, false))
            {
                UndoManager.CopySelected(ObjectContext);
            }
            else if (IO.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_V, false))
            {
                UndoManager.PasteCopied(ObjectContext);
            }
            else if (IO.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_W, false))
            {
                ObjectListPanel->DuplicateSelectedObject();
            }
            else if (IO.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z, false))
            {
                UndoManager.Undo(ObjectContext);
            }
            else if (IO.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y, false))
            {
                UndoManager.Redo(ObjectContext);
            }
            else if (ImGui::IsKeyPressed(ImGuiKey_Delete, false))
            {
                ObjectListPanel->DeleteSelectedObject();
            }
        }
        
        
        Renderer->SwapBuffer();
    }
    
   CameraController.Stop(*WindowsWindow);
}

void FEditorEngine::Shutdown()
{
    bIsRunning = false;

    if (WindowsWindow != nullptr)
    {
        CameraController.Stop(*WindowsWindow);
    }

    if (ImGuiRenderer != nullptr)
    {
        ImGuiRenderer->Shutdown();
        delete ImGuiRenderer;
        ImGuiRenderer = nullptr;
    }

    ViewportPanel = nullptr;
    ConsolePanel = nullptr;
    ObjectListPanel = nullptr;
    DetailsPanel = nullptr;
    MainMenuPanel = nullptr;
    StatsPanel = nullptr;

    for (UObject* Object : ObjectContext.Objects)
    {
        delete Object;
    }

    ObjectContext.Objects.Reset();
    ObjectContext.SelectedObject = nullptr;

    if (MainCamera != nullptr)
    {
        delete MainCamera;
        MainCamera = nullptr;
    }

    UndoManager.Clear();
    
    AssetManager.Shutdown();

    if (Renderer != nullptr)
    {
        Renderer->Shutdown();
        delete Renderer;
        Renderer = nullptr;
    }

    if (WindowsWindow != nullptr)
    {
        WindowsWindow->Destroy();
        delete WindowsWindow;
        WindowsWindow = nullptr;
    }
}

void FEditorEngine::ProcessViewportPicking(const UCameraComponent& Camera)
{
    float U = 0.0f;
    float V = 0.0f;
    if (!ViewportPanel->ConsumeClick(U, V))
    {
        return;
    }
    
    const uint32 Width = Renderer->GetViewportWidth();
    const uint32 Height = Renderer->GetViewportHeight();
    
    if (Width == 0 || Height == 0)
    {
        ObjectContext.SelectedObject = nullptr;
        return;
    }
    
    ObjectContext.SelectedObject = EditorPicking::PickObject(ObjectContext, U, V, Width, Height, Camera.GetViewMatrix(), Camera.GetProjectionMatrix());
}
