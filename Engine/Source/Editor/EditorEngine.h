#pragma once

#include "Engine/Engine.h"
#include "Viewport/EditorCameraController.h"
#include "EditorObjectContext.h"
#include "Viewport/EditorViewport.h"

#include "Core/Stats/FrameStats.h"

#include "EditorSettings.h"
#include "RHI/RHI.h"
#include "Renderer/BatchLineRenderer.h"
#include "Renderer/BillboardRenderer.h"
#include "Renderer/TextRenderer.h"

#include "Resources/AssetManager.h"

#include "UI/ImGuiRenderer.h"
#include "UI/Panels/ConsolePanel.h"
#include "UI/Panels/ViewportPanel.h"

#include "Windows/WindowsWindow.h"

#include "EditorUndoManager.h"
#include "Renderer/WorldRenderer.h"

struct FStaticMeshData;
class UCameraComponent;
class FObjectListPanel;
class FDetailsPanel;
class FMainMenuPanel;
class FStatsPanel;
class FContentBrowserPanel;

// Editor flavour of the engine (Unreal's UEditorEngine). Driven by FEngineLoop.
class FEditorEngine : public FEngine
{
public:
    bool Init(HINSTANCE hInstance) override;
    void Tick(float DeltaTime) override;
    void Shutdown() override;

    UWorld* GetWorld() const override { return ObjectContext.World; }

private:
    struct FActorLabel
    {
        FString Text;
        FVector Anchor;
    };

    // Everything one frame's render command needs, copied on the game thread so the
    // rendering thread never reads UObjects or editor state.
    struct FFrameRenderData
    {
        const FScene* Scene = nullptr;
        const void* SelectedObject = nullptr;
        FEngineShowFlags ShowFlags;
        EViewMode ViewMode = EViewMode::Lit;
        float GridExtent = 0.0f;
        float GridSpacing = 0.0f;
        int32 MajorGridInterval = 0;
        int32 ViewportCount = 1;
        FEditorViewportPane Viewports[4];
        TArray<FActorLabel> Labels;
    };

    void UpdateViewports();
    void ProcessViewportPicking();
    void ProcessEditorShortcuts();

    FFrameRenderData BuildFrameRenderData() const;
    void RenderFrame(const FFrameRenderData& Data);

    FFrameStats FrameStats;

    FEditorObjectContext ObjectContext;

    FObjectListPanel* ObjectListPanel = nullptr;
    FDetailsPanel* DetailsPanel = nullptr;

    FEditorCameraController CameraController;
    FEditorSettings EditorSettings;

    FRHI* Renderer = nullptr;
    FWorldRenderer WorldRenderer;
    FBatchLineRenderer LineRenderer;
    FBillboardRenderer BillboardRenderer;
    FTextRenderer TextRenderer;

    FEditorViewportClient ViewportClient;
    FEditorViewportPane Viewports[4];

    FImGuiRenderer* ImGuiRenderer = nullptr;
    FWindowsWindow* WindowsWindow = nullptr;

    FConsolePanel* ConsolePanel = nullptr;
    FViewportPanel* ViewportPanel = nullptr;
    FMainMenuPanel* MainMenuPanel = nullptr;
    FStatsPanel* StatsPanel = nullptr;
    FContentBrowserPanel* ContentBrowserPanel = nullptr;

    UCameraComponent* MainCamera = nullptr;

    FAssetManager AssetManager;
    FEditorUndoManager UndoManager;
};