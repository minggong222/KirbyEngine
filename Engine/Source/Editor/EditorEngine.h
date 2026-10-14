#pragma once
#include "Renderer/Renderer.h"
#include "UI/ImGuiRenderer.h"
#include "UI/UIManager.h"
#include "Windows/WindowsWindow.h"

class FEditorEngine
{
public:
    bool Init(HINSTANCE hInstance);
    void Run();
    void Shutdown();
private:
    URenderer* Renderer = nullptr;
    FUIManager* UIManager = nullptr;
    FImGuiRenderer* ImGuiRenderer = nullptr;
    FWindowsWindow* WindowsWindow = nullptr;
    
    bool bIsRunning = false;
};
