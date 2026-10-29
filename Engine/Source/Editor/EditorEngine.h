#pragma once
#include "EditorCameraController.h"
#include "Core/Stats/FrameStats.h"
#include "Renderer/Renderer.h"
#include "UI/ImGuiRenderer.h"
#include "UI/PanelManager.h"
#include "UI/ViewportPanel.h"
#include "Windows/WindowsWindow.h"

class FEditorEngine
{
public:
    bool Init(HINSTANCE hInstance);
    void Run();
    void Shutdown();
private:
    FFrameStats FrameStats;
    
    FEditorCameraController CameraController;
    
    URenderer* Renderer = nullptr;
    FImGuiRenderer* ImGuiRenderer = nullptr;
    FWindowsWindow* WindowsWindow = nullptr;
    
    FConsolePanel* ConsolePanel = nullptr;
    FViewportPanel* ViewportPanel = nullptr;
    
    bool bIsRunning = false;
};
