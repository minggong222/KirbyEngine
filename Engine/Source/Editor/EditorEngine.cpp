#include "EditorEngine.h"

#include "EditorCameraController.h"
#include "EditorObjectPicker.h"
#include "OBJStaticMeshImporter.h"
#include "TextureAssetImporter.h"

#include "Actors/Actor.h"
#include "Actors/CameraActor.h"
#include "Actors/StaticMeshActor.h"

#include "Components/ActorComponent.h"
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
#include "World/Level.h"

#include <algorithm>
#include <cmath>

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

    Viewports[0].Type = EEditorViewportType::Perspective;
    Viewports[1].Type = EEditorViewportType::Front;
    Viewports[2].Type = EEditorViewportType::Right;
    Viewports[3].Type = EEditorViewportType::Top;

    ViewportPanel = new FViewportPanel();
    ViewportPanel->SetViewportTexture(Renderer->GetViewportSRV());
    ViewportPanel->SetEditorSettings(&EditorSettings);
    ViewportPanel->SetAssetContext(&AssetManager, Renderer);
    ViewportPanel->SetGizmoContext(&ObjectContext, Viewports);

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

    while (bIsRunning)
    {
        const double CurrentTime = FPlatformTime::Seconds();
        const double DeltaTime = CurrentTime - PreviousTime;

        PreviousTime = CurrentTime;

        FrameStats.Update(DeltaTime);
        WorldRenderer.TickMaterials(ObjectContext.World, static_cast<float>(DeltaTime));

        MSG Message;

        while (PeekMessage(&Message, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&Message);
            DispatchMessage(&Message);

            if (Message.message == WM_QUIT)
            {
                bIsRunning = false;
                break;
            }
        }

        if (!bIsRunning)
        {
            break;
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

        uint32 DesiredWidth = 0;
        uint32 DesiredHeight = 0;

        if (ViewportPanel->ConsumeDesiredSize(DesiredWidth, DesiredHeight))
        {
            Renderer->ResizeViewportRenderTarget(DesiredWidth, DesiredHeight);
            ViewportPanel->SetViewportTexture(Renderer->GetViewportSRV());
        }

        MainCamera->SetMouseSensitivity(EditorSettings.CameraSensitivity);

        UpdateViewports();

        const int32 ActiveViewportIndex = ViewportPanel->GetActiveViewportIndex();
        const bool bPerspectiveViewportActive = !EditorSettings.bFourViewports || ActiveViewportIndex == 0;
        const bool bCameraInputAllowed = bPerspectiveViewportActive && ViewportPanel->IsViewportHovered();

        CameraController.Update(*MainCamera, *WindowsWindow, static_cast<float>(DeltaTime), bCameraInputAllowed);

        UpdateViewports();

        ViewportPanel->SetGizmoContext(&ObjectContext, Viewports);

        ProcessViewportPicking();

        const uint32 ViewportWidth = Renderer->GetViewportWidth();
        const uint32 ViewportHeight = Renderer->GetViewportHeight();

        if (ViewportWidth > 0 && ViewportHeight > 0)
        {
            Renderer->SetViewMode(EditorSettings.ViewMode);

            Renderer->Prepare();
            Renderer->BeginViewportRender();
            Renderer->PrepareShader();

            const int32 ViewportCount = EditorSettings.bFourViewports ? 4 : 1;

            for (int32 ViewportIndex = 0; ViewportIndex < ViewportCount; ++ViewportIndex)
            {
                FEditorViewportPane& Viewport = Viewports[ViewportIndex];

                if (Viewport.Width <= 0.0f || Viewport.Height <= 0.0f)
                {
                    continue;
                }

                Renderer->SetViewportRegion(Viewport.X, Viewport.Y, Viewport.Width, Viewport.Height);

                WorldRenderer.Render(ObjectContext.World, *Renderer, Viewport.View, Viewport.Projection, ObjectContext.GetSelectedObject(), EditorSettings.ShowFlags);

                ULevel* Level = ObjectContext.World != nullptr ? ObjectContext.World->GetPersistentLevel() : nullptr;

                LineRenderer.BeginFrame();

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

                LineRenderer.Flush(*Renderer, Viewport.View, Viewport.Projection);

                TextRenderer.BeginFrame();

                if (EditorSettings.ShowFlags.IsEnabled(EEngineShowFlag::BillboardText) && Level != nullptr)
                {
                    const FVector CameraPosition = Viewport.CameraPosition;
                    const FVector CameraRight = Viewport.CameraRight;
                    const FVector CameraUp = Viewport.CameraUp;

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
                        const float WorldUnitsPerPixel = FBillboardRenderer::GetWorldUnitsPerPixel(LabelAnchor, CameraPosition, Viewport.Projection, static_cast<uint32>(Viewport.Height));

                        if (TextSize.x <= 0.0f || WorldUnitsPerPixel <= 0.0f)
                        {
                            continue;
                        }

                        const FVector TextPosition = LabelAnchor - CameraRight * (TextSize.x * 0.5f * WorldUnitsPerPixel) + CameraUp * (2.0f * WorldUnitsPerPixel);
                        const FVector BillboardCenter = TextPosition + CameraRight * (TextSize.x * 0.5f * WorldUnitsPerPixel) + CameraUp * (TextSize.y * 0.5f * WorldUnitsPerPixel);

                        BillboardRenderer.DrawScreenAlignedQuad(*Renderer, BillboardCenter, CameraPosition, CameraRight, CameraUp, FVector2D(TextSize.x + 8.0f, TextSize.y + 6.0f), static_cast<uint32>(Viewport.Height), FVector4(0.025f, 0.025f, 0.025f, 0.75f), Viewport.View, Viewport.Projection);
                        TextRenderer.AddText(Label, TextPosition, CameraRight, CameraUp, WorldUnitsPerPixel, FVector4(1.0f, 1.0f, 1.0f, 1.0f));
                    }
                }

                TextRenderer.Flush(*Renderer, Viewport.View, Viewport.Projection);
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

    if (WindowsWindow != nullptr)
    {
        CameraController.Stop(*WindowsWindow);
    }
}

void FEditorEngine::UpdateViewports()
{
    if (Renderer == nullptr || MainCamera == nullptr)
    {
        return;
    }

    const uint32 Width = Renderer->GetViewportWidth();
    const uint32 Height = Renderer->GetViewportHeight();

    if (Width == 0 || Height == 0)
    {
        return;
    }

    if (!EditorSettings.bFourViewports || Width < 2 || Height < 2)
    {
        Viewports[0].X = 0.0f;
        Viewports[0].Y = 0.0f;
        Viewports[0].Width = static_cast<float>(Width);
        Viewports[0].Height = static_cast<float>(Height);

        for (int32 Index = 1; Index < 4; ++Index)
        {
            Viewports[Index].X = 0.0f;
            Viewports[Index].Y = 0.0f;
            Viewports[Index].Width = 0.0f;
            Viewports[Index].Height = 0.0f;
        }
    }
    else
    {
        const float SplitX = std::isfinite(EditorSettings.ViewportSplitX) ? std::clamp(EditorSettings.ViewportSplitX, 0.2f, 0.8f) : 0.5f;
        const float SplitY = std::isfinite(EditorSettings.ViewportSplitY) ? std::clamp(EditorSettings.ViewportSplitY, 0.2f, 0.8f) : 0.5f;

        const uint32 LeftWidth = std::clamp(static_cast<uint32>(static_cast<float>(Width) * SplitX), 1u, Width - 1u);
        const uint32 TopHeight = std::clamp(static_cast<uint32>(static_cast<float>(Height) * SplitY), 1u, Height - 1u);

        const uint32 RightWidth = Width - LeftWidth;
        const uint32 BottomHeight = Height - TopHeight;

        Viewports[0].X = 0.0f;
        Viewports[0].Y = 0.0f;
        Viewports[0].Width = static_cast<float>(LeftWidth);
        Viewports[0].Height = static_cast<float>(TopHeight);

        Viewports[1].X = static_cast<float>(LeftWidth);
        Viewports[1].Y = 0.0f;
        Viewports[1].Width = static_cast<float>(RightWidth);
        Viewports[1].Height = static_cast<float>(TopHeight);

        Viewports[2].X = 0.0f;
        Viewports[2].Y = static_cast<float>(TopHeight);
        Viewports[2].Width = static_cast<float>(LeftWidth);
        Viewports[2].Height = static_cast<float>(BottomHeight);

        Viewports[3].X = static_cast<float>(LeftWidth);
        Viewports[3].Y = static_cast<float>(TopHeight);
        Viewports[3].Width = static_cast<float>(RightWidth);
        Viewports[3].Height = static_cast<float>(BottomHeight);
    }

    if (Viewports[0].Height > 0.0f)
    {
        MainCamera->SetAspectRatio(Viewports[0].Width / Viewports[0].Height);
    }

    const int32 ViewportCount = EditorSettings.bFourViewports && Width >= 2 && Height >= 2 ? 4 : 1;

    for (int32 Index = 0; Index < ViewportCount; ++Index)
    {
        ViewportClient.Update(Viewports[Index], MainCamera);
    }
}

void FEditorEngine::ProcessViewportPicking()
{
    float U = 0.0f;
    float V = 0.0f;
    int32 PaneIndex = 0;

    if (ViewportPanel == nullptr || !ViewportPanel->ConsumeClick(U, V, PaneIndex))
    {
        return;
    }

    if (PaneIndex < 0 || PaneIndex >= 4)
    {
        return;
    }

    const FEditorViewportPane& Pane = Viewports[PaneIndex];

    if (Pane.Width <= 0.0f || Pane.Height <= 0.0f)
    {
        return;
    }

    const uint32 PaneWidth = static_cast<uint32>(Pane.Width);
    const uint32 PaneHeight = static_cast<uint32>(Pane.Height);

    UStaticMeshComponent* Component = EditorPicking::PickComponent(ObjectContext.World, U, V, PaneWidth, PaneHeight, Pane.View, Pane.Projection);

    if (Component != nullptr)
    {
        ObjectContext.SelectComponent(Component);
    }
    else
    {
        ObjectContext.ClearSelection();
    }
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