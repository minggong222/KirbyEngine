#pragma once
#include <d3d11.h>

#include "UIManager.h"

class FImGuiRenderer
{
public:
    void Init(HWND Hwnd, ID3D11Device* Device, ID3D11DeviceContext* DeviceContext);
    void OnRender();
    void Shutdown();
private:
    FUIManager* UIManager;
};
