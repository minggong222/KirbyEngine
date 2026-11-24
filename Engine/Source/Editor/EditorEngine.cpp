#include "EditorEngine.h"

#include "EditorCameraController.h"
#include "EditorObjectPicker.h"
#include "OBJStaticMeshImporter.h"
#include "TextureAssetImporter.h"
#include "Actors/Actor.h"
#include "Actors/CameraActor.h"
#include "Actors/StaticMeshActor.h"
#include "Components/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/STL.h"
#include "Core/Math/MathUtility.h"
#include "Core/Time/PlatformTime.h"
#include "Renderer/WorldRenderer.h"
#include "Resources/Material.h"
#include "Resources/PrimitiveMeshAssets.h"
#include "Resources/StaticMesh.h"
#include "Resources/Texture2D.h"
#include "UI/Panels/DetailsPanel.h"
#include "UI/Panels/MainMenuPanel.h"
#include "UI/Panels/ObjectListPanel.h"
#include "UI/Panels/StatsPanel.h"
#include "UI/Panels/ContentBrowserPanel.h"
#include "UObject/Object.h"
#include "World/World.h"

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
        
        EditorSettings.Load("Config/EditorSettings.ini");

        if (!TextRenderer.Initialize(L"Malgun Gothic", 32))
        {
            UE_LOG(Warning, "TextRenderer initialization failed");
        }
        
        if (!RegisterBuiltinStaticMeshes(AssetManager, *Renderer))
        {
            UE_LOG(Error, "Failed to register builtin static meshes");
            Shutdown();
            return false;
        }
        
        OBJImporter::RegisterImportedStaticMeshes(AssetManager, *Renderer);
        TextureImporter::RegisterImportedTextureMaterials(AssetManager, *Renderer);
    }

    ObjectContext.World = NewObject<UWorld>(nullptr, FName("EditorWorld"));

    if (ObjectContext.World == nullptr || ObjectContext.World->GetPersistentLevel() == nullptr)
    {
        UE_LOG(Error, "Editor World initialization failed");
        Shutdown();
        return false;
    }
    
    if (!ImGuiRenderer->Init(WindowsWindow->GetHWND(), Renderer->GetDevice(), Renderer->GetDeviceContext()))
    {
        UE_LOG(Error, "ImGui initialization failed");
        Shutdown();
        return false;
    }
    
    MainCamera = new UCameraComponent();
    MainCamera->SetMouseSensitivity(EditorSettings.CameraSensitivity);
    
    ViewportPanel = new FViewportPanel();
    ViewportPanel->SetViewportTexture(Renderer->GetViewportSRV());
    ViewportPanel->SetEditorSettings(&EditorSettings);
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
    
    ObjectListPanel = new FObjectListPanel(&ObjectContext, &AssetManager, ViewportPanel);
    if (!ImGuiRenderer->GetPanelManager()->AddPanel(ObjectListPanel))
    {
        ObjectListPanel = nullptr;
        Shutdown();
        return false;
    }
    
    DetailsPanel = new FDetailsPanel(&ObjectContext, &AssetManager);
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
    
    ContentBrowserPanel = new FContentBrowserPanel(&ObjectContext, &AssetManager, Renderer);
    if (!ImGuiRenderer->GetPanelManager()->AddPanel(ContentBrowserPanel))
    {
        ContentBrowserPanel = nullptr;
        Shutdown();
        return false;
    }
    
    ConsolePanel->SetStatsPanel(StatsPanel);
    MainMenuPanel = new FMainMenuPanel(&ObjectContext, &AssetManager, WindowsWindow->GetHWND(), MainCamera, Renderer, ImGuiRenderer->GetPanelManager());
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
        MainCamera->SetMouseSensitivity(EditorSettings.CameraSensitivity);
        CameraController.Update(*MainCamera, *WindowsWindow, static_cast<float>(DeltaTime), ViewportPanel->IsViewportHovered());
        
        const FMatrix View = MainCamera->GetViewMatrix();
        const FMatrix Projection = MainCamera->GetProjectionMatrix();
        
        ViewportPanel->SetGizmoContext(&ObjectContext, View, Projection);
        
        {
            // 준비 작업
            Renderer->SetViewMode(EditorSettings.ViewMode);

            LineRenderer.BeginFrame();
            TextRenderer.BeginFrame();

            Renderer->Prepare();
            Renderer->BeginViewportRender();
            Renderer->PrepareShader();
            
            WorldRenderer.Render(ObjectContext.World, *Renderer, View, Projection, ObjectContext.GetSelectedObject(), EditorSettings.ShowFlags);
            
            ULevel* Level = ObjectContext.World != nullptr ? ObjectContext.World->GetPersistentLevel() : nullptr;

            if (EditorSettings.ShowFlags.IsEnabled(EEngineShowFlag::Bounds) && Level != nullptr)
            {
                for (AActor* Actor : Level->GetActors())
                {
                    if (Actor == nullptr)
                    {
                        continue;
                    }

                    for (UActorComponent* ActorComponent : Actor->GetComponents())
                    {
                        UStaticMeshComponent* Component = Cast<UStaticMeshComponent>(ActorComponent);

                        if (Component == nullptr)
                        {
                            continue;
                        }

                        const FBox Bounds = Component->CalcBounds();

                        const bool bSelected = ObjectContext.SelectedActor == Actor || ObjectContext.SelectedComponent == Component;
                        const FVector4 Color = bSelected ? FVector4(1.0f, 0.75f, 0.1f, 1.0f) : FVector4(0.1f, 0.85f, 1.0f, 0.8f);

                        LineRenderer.AddBox(Bounds, Color);
                    }
                }
            }
            
            if (EditorSettings.ShowFlags.IsEnabled(EEngineShowFlag::Grid) || EditorSettings.ShowFlags.IsEnabled(EEngineShowFlag::Axes))
            {
                LineRenderer.AddGrid(EditorSettings.GridExtent, EditorSettings.GridSpacing, EditorSettings.MajorGridInterval, EditorSettings.ShowFlags.IsEnabled(EEngineShowFlag::Grid), EditorSettings.ShowFlags.IsEnabled(EEngineShowFlag::Axes));
            }

            LineRenderer.Flush(*Renderer, View, Projection);
            
            if (EditorSettings.ShowFlags.IsEnabled(EEngineShowFlag::BillboardText) && Level != nullptr)
            {
                const FTransform CameraTransform = MainCamera->GetTransform();

                const FVector CameraPosition = CameraTransform.Location;
                const FVector CameraRight = CameraTransform.GetRight();
                const FVector CameraUp = CameraTransform.GetUp();

                for (AActor* Actor : Level->GetActors())
                {
                    if (Actor == nullptr)
                    {
                        continue;
                    }

                    UStaticMeshComponent* Component = nullptr;

                    for (UActorComponent* ActorComponent : Actor->GetComponents())
                    {
                        Component = Cast<UStaticMeshComponent>(ActorComponent);

                        if (Component != nullptr)
                        {
                            break;
                        }
                    }

                    FVector LabelAnchor = Actor->GetActorTransform().Location;

                    if (Component != nullptr)
                    {
                        const FBox Bounds = Component->CalcBounds();

                        if (Bounds.IsValid())
                        {
                            const FVector& Min = Bounds.GetMin();
                            const FVector& Max = Bounds.GetMax();

                            LabelAnchor = FVector((Min.x + Max.x) * 0.5f, (Min.y + Max.y) * 0.5f, Max.z + 0.1f);
                        }
                    }

                    FString Label = Actor->GetName();
                    Label.Append("_");
                    Label.Append(std::to_string(Actor->GetUUID()));

                    const FVector2D TextSize = TextRenderer.MeasureText(Label);
                    const float WorldUnitsPerPixel = FBillboardRenderer::GetWorldUnitsPerPixel(LabelAnchor, CameraPosition, Projection, Renderer->GetViewportHeight());

                    if (TextSize.x <= 0.0f || WorldUnitsPerPixel <= 0.0f)
                    {
                        continue;
                    }

                    const FVector TextPosition = LabelAnchor - CameraRight * (TextSize.x * 0.5f * WorldUnitsPerPixel) + CameraUp * (2.0f * WorldUnitsPerPixel);
                    const FVector BillboardCenter = TextPosition + CameraRight * (TextSize.x * 0.5f * WorldUnitsPerPixel) + CameraUp * (TextSize.y * 0.5f * WorldUnitsPerPixel);

                    BillboardRenderer.DrawScreenAlignedQuad(*Renderer, BillboardCenter, CameraPosition, CameraRight, CameraUp, FVector2D(TextSize.x + 8.0f, TextSize.y + 6.0f), Renderer->GetViewportHeight(), FVector4(0.025f, 0.025f, 0.025f, 0.75f), View, Projection);
                    TextRenderer.AddText(Label, TextPosition, CameraRight, CameraUp, WorldUnitsPerPixel, FVector4(1.0f, 1.0f, 1.0f, 1.0f));
                }
            }

            TextRenderer.Flush(*Renderer, View, Projection);
            
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
    ContentBrowserPanel = nullptr;

    UndoManager.Clear();
    ObjectContext.ClearSelection();

    if (ObjectContext.World != nullptr)
    {
        delete ObjectContext.World;
        ObjectContext.World = nullptr;
    }

    if (MainCamera != nullptr)
    {
        delete MainCamera;
        MainCamera = nullptr;
    }
    
    EditorSettings.Save("Config/EditorSettings.ini");
    TextRenderer.Shutdown();
    
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

    UStaticMeshComponent* Component = EditorPicking::PickComponent(ObjectContext.World, U, V, Renderer->GetViewportWidth(), Renderer->GetViewportHeight(), Camera.GetViewMatrix(), Camera.GetProjectionMatrix());

    if (Component != nullptr)
    {
        ObjectContext.SelectComponent(Component);
    }
    else
    {
        ObjectContext.ClearSelection();
    }
}