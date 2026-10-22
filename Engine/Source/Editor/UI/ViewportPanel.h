#pragma once
#include "EditorPanel.h"
#include "Renderer/Renderer.h"

class FViewportPanel : public IEditorPanel
{
public:
    bool Init() override;
    void Tick(float DeltaTime) override;
    void OnRender() override;
    void SetViewportTexture(ID3D11ShaderResourceView* InSRV) { InSRV = InSRV; }
public:
    ID3D11ShaderResourceView* SRV;
};
