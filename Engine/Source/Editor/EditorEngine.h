#pragma once
#include "EditorCameraController.h"
#include "EditorObjectContext.h"
#include "Core/Stats/FrameStats.h"
#include "Renderer/Renderer.h"
#include "Resources/AssetManager.h"
#include "UI/ImGuiRenderer.h"
#include "UI/Panels/ConsolePanel.h"
#include "UI/Panels/ViewportPanel.h"
#include "Windows/WindowsWindow.h"

struct FStaticMeshData;
class FObjectListPanel;
class FDetailsPanel;

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
    
    URenderer* Renderer = nullptr;
    FImGuiRenderer* ImGuiRenderer = nullptr;
    FWindowsWindow* WindowsWindow = nullptr;
    
    FConsolePanel* ConsolePanel = nullptr;
    FViewportPanel* ViewportPanel = nullptr;
    
    FAssetManager AssetManager;
    
    bool bIsRunning = false;
};
