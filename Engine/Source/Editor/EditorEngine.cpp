#include "EditorEngine.h"

#include "Viewport/EditorCameraController.h"
#include "Viewport/EditorObjectPicker.h"
#include "Importers/OBJStaticMeshImporter.h"
#include "Importers/TextureAssetImporter.h"

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
#include "Renderer/Scene.h"
#include "Renderer/PrimitiveSceneProxy.h"
#include "RenderCore/RenderingThread.h"

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
    Renderer = RHICreate();
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

    if (!RegisterBuiltinStaticMeshes(AssetManager, *Renderer))
    {
        UE_LOG(Error, "Failed to register builtin static meshes");
        Shutdown();
        return false;
    }

    // Packages first: assets with an up-to-date .kasset load without their importer.
    // The importers then only handle sources that have no package yet (or changed).
    AssetManager.LoadPackages(*Renderer);

    OBJImporter::RegisterImportedStaticMeshes(AssetManager, *Renderer);
    TextureImporter::RegisterImportedTextureMaterials(AssetManager, *Renderer);

    AssetManager.SaveOutdatedPackages();

    ObjectContext.World = NewObject<UWorld>(nullptr, FName("EditorWorld"));

    if (ObjectContext.World != nullptr)
    {
        ObjectContext.World->SetWorldType(EWorldType::Editor);
    }

    if (ObjectContext.World == nullptr || ObjectContext.World->GetPersistentLevel() == nullptr)
    {
        UE_LOG(Error, "Editor World initialization failed");
        Shutdown();
        return false;
    }

    if (!ImGuiRenderer->Init(WindowsWindow->GetHWND(), Renderer->GetNativeDevice(), Renderer->GetNativeDeviceContext()))
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
    
    if (!TextRenderer.Initialize(L"Malgun Gothic", 32))
    {
        UE_LOG(Error, "[TextRenderer] Initialize failed.");
    }
    else
    {
        UE_LOG(Info, "[TextRenderer] Initialize succeeded.");
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

    return true;
}

// One editor frame. OS messages and frame timing are handled by FEngineLoop.
//
// Game thread: world tick, input, editor UI, then the end-of-frame scene update.
// Rendering thread: one command draws the viewports, the ImGui draw lists and presents.
// The frame ends with FlushRenderingCommands, so the next frame's game-thread work
// never overlaps rendering.
void FEditorEngine::Tick(float DeltaTime)
{
    FrameStats.Update(DeltaTime);

    // Editor world tick: only actors that opt in (ShouldTickIfViewportsOnly) run,
    // and actors destroyed during the previous frame are deleted here.
    if (ObjectContext.World != nullptr)
    {
        ObjectContext.World->Tick(DeltaTime);
    }

    WorldRenderer.TickMaterials(ObjectContext.World, DeltaTime);

    RECT ClientRect = {};

    if (GetClientRect(WindowsWindow->GetHWND(), &ClientRect))
    {
        const LONG ClientWidth = ClientRect.right - ClientRect.left;
        const LONG ClientHeight = ClientRect.bottom - ClientRect.top;

        if (ClientWidth > 0 && ClientHeight > 0)
        {
            const uint32 Width = static_cast<uint32>(ClientWidth);
            const uint32 Height = static_cast<uint32>(ClientHeight);

            ENQUEUE_RENDER_COMMAND(ResizeBackBuffer)([this, Width, Height]()
            {
                Renderer->ResizeBackBuffer(Width, Height);
            });
        }
    }

    uint32 DesiredWidth = 0;
    uint32 DesiredHeight = 0;

    if (ViewportPanel->ConsumeDesiredSize(DesiredWidth, DesiredHeight))
    {
        ENQUEUE_RENDER_COMMAND(ResizeViewportRenderTarget)([this, DesiredWidth, DesiredHeight]()
        {
            Renderer->ResizeViewportRenderTarget(DesiredWidth, DesiredHeight);
        });

        // The panel needs the new texture handle this frame.
        FlushRenderingCommands();
        ViewportPanel->SetViewportTexture(Renderer->GetViewportSRV());
    }

    MainCamera->SetMouseSensitivity(EditorSettings.CameraSensitivity);

    UpdateViewports();

    const int32 ActiveViewportIndex = ViewportPanel->GetActiveViewportIndex();
    const bool bPerspectiveViewportActive = !EditorSettings.bFourViewports || ActiveViewportIndex == 0;
    const bool bCameraInputAllowed = bPerspectiveViewportActive && ViewportPanel->IsViewportHovered();

    CameraController.Update(*MainCamera, *WindowsWindow, DeltaTime, bCameraInputAllowed);

    UpdateViewports();

    ViewportPanel->SetGizmoContext(&ObjectContext, Viewports);

    ProcessViewportPicking();

    ImGuiRenderer->BuildFrame();

    ProcessEditorShortcuts();

    // All of this frame's changes are in: copy them to the render-side scene.
    if (ObjectContext.World != nullptr)
    {
        ObjectContext.World->SendAllEndOfFrameUpdates();
    }

    ENQUEUE_RENDER_COMMAND(DrawEditorFrame)([this, FrameData = BuildFrameRenderData()]()
    {
        RenderFrame(FrameData);
        ImGuiRenderer->RenderDrawData();
        Renderer->SwapBuffer();
    });

    FlushRenderingCommands();
}

void FEditorEngine::ProcessEditorShortcuts()
{
    const ImGuiIO& IO = ImGui::GetIO();

    const bool bEditorFocused = WindowsWindow != nullptr && GetForegroundWindow() == WindowsWindow->GetHWND();
    const bool bCanProcessShortcut = bEditorFocused && !IO.WantTextInput && !IO.WantCaptureKeyboard && !ImGui::IsAnyItemActive();

    if (!bCanProcessShortcut)
    {
        return;
    }

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

// Game thread: copies everything the frame's render command needs, so it never touches UObjects.
FEditorEngine::FFrameRenderData FEditorEngine::BuildFrameRenderData() const
{
    FFrameRenderData Data;
    Data.Scene = ObjectContext.World != nullptr ? ObjectContext.World->GetScene() : nullptr;
    Data.SelectedObject = ObjectContext.GetSelectedObject();
    Data.ShowFlags = EditorSettings.ShowFlags;
    Data.ViewMode = EditorSettings.ViewMode;
    Data.GridExtent = EditorSettings.GridExtent;
    Data.GridSpacing = EditorSettings.GridSpacing;
    Data.MajorGridInterval = EditorSettings.MajorGridInterval;
    Data.ViewportCount = EditorSettings.bFourViewports ? 4 : 1;

    for (int32 Index = 0; Index < 4; ++Index)
    {
        Data.Viewports[Index] = Viewports[Index];
    }

    ULevel* Level = ObjectContext.World != nullptr ? ObjectContext.World->GetPersistentLevel() : nullptr;

    if (Data.ShowFlags.IsEnabled(EEngineShowFlag::BillboardText) && Level != nullptr)
    {
        for (AActor* Actor : Level->GetActors())
        {
            if (!IsValid(Actor))
            {
                continue;
            }

            FActorLabel ActorLabel;
            ActorLabel.Anchor = Actor->GetActorTransform().Location;

            if (UStaticMeshComponent* Component = Actor->FindComponentByClass<UStaticMeshComponent>())
            {
                const FBox Bounds = Component->CalcBounds();

                if (Bounds.IsValid())
                {
                    const FVector& Min = Bounds.GetMin();
                    const FVector& Max = Bounds.GetMax();
                    ActorLabel.Anchor = FVector((Min.x + Max.x) * 0.5f, (Min.y + Max.y) * 0.5f, Max.z + 0.1f);
                }
            }

            ActorLabel.Text = Actor->GetName();
            ActorLabel.Text.Append("_");
            ActorLabel.Text.Append(std::to_string(Actor->GetUUID()));

            Data.Labels.Add(ActorLabel);
        }
    }

    return Data;
}

// Rendering thread: draws every editor viewport into the viewport render target.
void FEditorEngine::RenderFrame(const FFrameRenderData& Data)
{
    const uint32 ViewportWidth = Renderer->GetViewportWidth();
    const uint32 ViewportHeight = Renderer->GetViewportHeight();

    if (ViewportWidth == 0 || ViewportHeight == 0)
    {
        return;
    }

    Renderer->SetViewMode(Data.ViewMode);

    Renderer->Prepare();
    Renderer->BeginViewportRender();
    Renderer->PrepareShader();

    for (int32 ViewportIndex = 0; ViewportIndex < Data.ViewportCount; ++ViewportIndex)
    {
        const FEditorViewportPane& Viewport = Data.Viewports[ViewportIndex];

        if (Viewport.Width <= 0.0f || Viewport.Height <= 0.0f)
        {
            continue;
        }

        Renderer->SetViewportRegion(Viewport.X, Viewport.Y, Viewport.Width, Viewport.Height);

        if (Data.Scene != nullptr)
        {
            WorldRenderer.Render(*Data.Scene, *Renderer, Viewport.View, Viewport.Projection, Data.SelectedObject, Data.ShowFlags);
        }

        LineRenderer.BeginFrame();

        if (Data.ShowFlags.IsEnabled(EEngineShowFlag::Bounds) && Data.Scene != nullptr)
        {
            for (const FPrimitiveSceneProxy* Proxy : Data.Scene->GetPrimitives())
            {
                if (Proxy == nullptr || !Proxy->GetBounds().IsValid())
                {
                    continue;
                }

                const bool bSelected = Data.SelectedObject != nullptr && (Proxy->GetPrimitiveId() == Data.SelectedObject || Proxy->GetOwnerId() == Data.SelectedObject);
                const FVector4 Color = bSelected ? FVector4(1.0f, 0.75f, 0.1f, 1.0f) : FVector4(0.1f, 0.85f, 1.0f, 0.8f);

                LineRenderer.AddBox(Proxy->GetBounds(), Color);
            }
        }

        if (Data.ShowFlags.IsEnabled(EEngineShowFlag::Grid) || Data.ShowFlags.IsEnabled(EEngineShowFlag::Axes))
        {
            LineRenderer.AddGrid(Data.GridExtent, Data.GridSpacing, Data.MajorGridInterval, Data.ShowFlags.IsEnabled(EEngineShowFlag::Grid), Data.ShowFlags.IsEnabled(EEngineShowFlag::Axes));
        }

        LineRenderer.Flush(*Renderer, Viewport.View, Viewport.Projection);

        TextRenderer.BeginFrame();

        for (const FActorLabel& ActorLabel : Data.Labels)
        {
            const FVector CameraPosition = Viewport.CameraPosition;
            const FVector CameraRight = Viewport.CameraRight;
            const FVector CameraUp = Viewport.CameraUp;

            const FVector2D TextSize = TextRenderer.MeasureText(ActorLabel.Text);
            const float WorldUnitsPerPixel = FBillboardRenderer::GetWorldUnitsPerPixel(ActorLabel.Anchor, CameraPosition, Viewport.Projection, static_cast<uint32>(Viewport.Height));

            if (TextSize.x <= 0.0f || WorldUnitsPerPixel <= 0.0f)
            {
                continue;
            }

            const FVector TextPosition = ActorLabel.Anchor - CameraRight * (TextSize.x * 0.5f * WorldUnitsPerPixel) + CameraUp * (2.0f * WorldUnitsPerPixel);
            const FVector BillboardCenter = TextPosition + CameraRight * (TextSize.x * 0.5f * WorldUnitsPerPixel) + CameraUp * (TextSize.y * 0.5f * WorldUnitsPerPixel);

            BillboardRenderer.DrawScreenAlignedQuad(*Renderer, BillboardCenter, CameraPosition, CameraRight, CameraUp, FVector2D(TextSize.x + 8.0f, TextSize.y + 6.0f), static_cast<uint32>(Viewport.Height), FVector4(0.025f, 0.025f, 0.025f, 0.75f), Viewport.View, Viewport.Projection);
            TextRenderer.AddText(ActorLabel.Text, TextPosition, CameraRight, CameraUp, WorldUnitsPerPixel, FVector4(1.0f, 1.0f, 1.0f, 1.0f));
        }

        TextRenderer.Flush(*Renderer, Viewport.View, Viewport.Projection);
    }

    Renderer->EndViewportRender();
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
    RequestExit();

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

    // Assets edited this session (e.g. a material saved to .kmat) get fresh packages.
    AssetManager.SaveOutdatedPackages();
    AssetManager.Shutdown();

    if (Renderer != nullptr)
    {
        Renderer->Shutdown();
        RHIDestroy();
        Renderer = nullptr;
    }

    if (WindowsWindow != nullptr)
    {
        WindowsWindow->Destroy();
        delete WindowsWindow;
        WindowsWindow = nullptr;
    }
}