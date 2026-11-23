#pragma once
#include "EditorPanel.h"
#include "EditorTransformGizmo.h"
#include "Renderer/Renderer.h"

class FViewportPanel : public IEditorPanel
{
public:
    bool Init() override;
    void Tick(float DeltaTime) override;
    void OnRender() override;
    void SetViewportTexture(ID3D11ShaderResourceView* InSRV) { SRV = InSRV; }
    void SetGizmoContext(FEditorObjectContext* InContext, const FMatrix& InView, const FMatrix& InProjection)
    {
        ObjectContext = InContext;

        if (ObjectContext != nullptr)
        {
            ObjectContext->TransformGizmo = &TransformGizmo;
        }

        ViewMatrix = InView;
        ProjectionMatrix = InProjection;
    }
    void SetDesiredSize(uint32 Width, uint32 Height);
    bool ConsumeDesiredSize(uint32& OutWidth, uint32& OutHeight);
    bool ConsumeClick(float& OutU, float& OutV);
    
    bool IsViewportHovered() const { return bViewportHovered; }
    void CancelGizmoDrag() { TransformGizmo.CancelDrag(); }
private:
    ID3D11ShaderResourceView* SRV = nullptr;
    
    FEditorObjectContext* ObjectContext = nullptr;
    FEditorTransformGizmo TransformGizmo;
    
    FMatrix ViewMatrix = FMatrix::Identity;
    FMatrix ProjectionMatrix = FMatrix::Identity;
    
    uint32 DesiredWidth = 0;
    uint32 DesiredHeight = 0;
    bool bHasPendingSize = false;
    
    float ClickU = 0.0f;
    float ClickV = 0.0f;
    bool bHasPendingClick = false;
    
    bool bViewportHovered = false;
};
