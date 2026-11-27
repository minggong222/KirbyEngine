#pragma once

#include "PanelManager.h"

class FImGuiRenderer
{
public:
    // NativeDevice / NativeDeviceContext come from GRHI->GetNativeDevice*() (D3D11 objects).
    bool Init(HWND Hwnd, void* NativeDevice, void* NativeDeviceContext);

    // Game thread: runs the panels and builds this frame's ImGui draw lists.
    void BuildFrame();
    // Rendering thread: submits the draw lists built by BuildFrame.
    void RenderDrawData();
    void Shutdown();
    void DrawDockHost();
    FPanelManager* GetPanelManager() const {return PanelManager;}
private:
    FPanelManager* PanelManager = nullptr;
    bool bContextCreated = false;
    bool bWin32Initialized = false;
    bool bDX11Initialized = false;
};
