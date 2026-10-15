#pragma once
#include "Core/Stats/FrameStats.h"
#include "Renderer/Renderer.h"
#include "UI/ImGuiRenderer.h"
#include "UI/PanelManager.h"
#include "Windows/WindowsWindow.h"

class FEditorEngine
{
public:
    bool Init(HINSTANCE hInstance);
    void Run();
    void Shutdown();
private:
    FFrameStats FrameStats;
    
    URenderer* Renderer = nullptr;
    FPanelManager* UIManager = nullptr;
    FImGuiRenderer* ImGuiRenderer = nullptr;
    FWindowsWindow* WindowsWindow = nullptr;
    
    bool bIsRunning = false;
};
