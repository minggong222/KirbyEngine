#pragma once
#include "EditorCameraController.h"
#include "EditorObjectContext.h"
#include "Core/Stats/FrameStats.h"
#include "EditorSettings.h"
#include "Renderer/Renderer.h"
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

class FEditorEngine
{
public:
    bool Init(HINSTANCE hInstance);
    void Run();
    void Shutdown();
private:
    void ProcessViewportPicking(const UCameraComponent& Camera);
    
    FFrameStats FrameStats;
    
    FEditorObjectContext ObjectContext;
    FObjectListPanel* ObjectListPanel = nullptr;
    FDetailsPanel* DetailsPanel = nullptr;
    
    FEditorCameraController CameraController;
    FEditorSettings EditorSettings;
    
    
    URenderer* Renderer = nullptr;
    FWorldRenderer WorldRenderer;
    FBatchLineRenderer LineRenderer;
    FBillboardRenderer BillboardRenderer;
    FTextRenderer TextRenderer;
    FImGuiRenderer* ImGuiRenderer = nullptr;
    FWindowsWindow* WindowsWindow = nullptr;
    
    FConsolePanel* ConsolePanel = nullptr;
    FViewportPanel* ViewportPanel = nullptr;
    FMainMenuPanel* MainMenuPanel = nullptr;
    FStatsPanel* StatsPanel = nullptr;
    
    UCameraComponent* MainCamera = nullptr;
    
    FAssetManager AssetManager;
    FEditorUndoManager UndoManager;
    
    bool bIsRunning = false;
};
