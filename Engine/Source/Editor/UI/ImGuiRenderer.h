#pragma once
#include <d3d11.h>

#include "PanelManager.h"

class FImGuiRenderer
{
public:
    bool Init(HWND Hwnd, ID3D11Device* Device, ID3D11DeviceContext* DeviceContext);
    void OnRender();
    void Shutdown();
    void DrawDockHost();
    FPanelManager* GetPanelManager() const {return PanelManager;}
private:
    FPanelManager* PanelManager = nullptr;
    bool bContextCreated = false;
    bool bWin32Initialized = false;
    bool bDX11Initialized = false;
};
