#pragma once
#include <d3d11.h>

#include "PanelManager.h"

class FImGuiRenderer
{
public:
    void Init(HWND Hwnd, ID3D11Device* Device, ID3D11DeviceContext* DeviceContext);
    void OnRender();
    void Shutdown();
    void DrawDockHost();

private:
    FPanelManager* UIManager = nullptr;
};
