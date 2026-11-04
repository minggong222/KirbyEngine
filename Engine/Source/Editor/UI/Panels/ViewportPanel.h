#pragma once
#include "EditorPanel.h"
#include "Renderer/Renderer.h"

class FViewportPanel : public IEditorPanel
{
public:
    bool Init() override;
    void Tick(float DeltaTime) override;
    void OnRender() override;
    void SetViewportTexture(ID3D11ShaderResourceView* InSRV) { SRV = InSRV; }
    void SetDesiredSize(uint32 Width, uint32 Height);
    bool ConsumeDesiredSize(uint32& OutWidth, uint32& OutHeight);
    bool ConsumeClick(float& OutU, float& OutV);
private:
    ID3D11ShaderResourceView* SRV = nullptr;
    uint32 DesiredWidth = 0;
    uint32 DesiredHeight = 0;
    bool bHasPendingSize = false;
    
    float ClickU = 0.0f;
    float ClickV = 0.0f;
    bool bHasPendingClick = false;
};
