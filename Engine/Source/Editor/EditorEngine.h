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
private:
    URenderer* Renderer;
    FUIManager* UIManager;
    FImGuiRenderer* ImGuiRenderer;
    FWindowsWindow* WindowsWindow;
};
