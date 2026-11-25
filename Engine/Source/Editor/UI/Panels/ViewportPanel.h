#pragma once

#include "EditorPanel.h"
#include "EditorTransformGizmo.h"
#include "EditorViewport.h"
#include "Renderer/Renderer.h"

class FEditorSettings;
class FAssetManager;

class FViewportPanel : public IEditorPanel
{
public:
    bool Init() override;
    void Tick(float DeltaTime) override;
    const char* GetPanelName() const override { return "Viewport"; }
    void OnRender() override;

    void SetViewportTexture(ID3D11ShaderResourceView* InSRV)
    {
        SRV = InSRV;
    }

    void SetGizmoContext(FEditorObjectContext* InContext, FEditorViewportPane* InViewports)
    {
        ObjectContext = InContext;
        Viewports = InViewports;

        if (ObjectContext != nullptr)
        {
            ObjectContext->TransformGizmo = &TransformGizmo;
        }
    }

    void SetAssetContext(FAssetManager* InAssetManager, URenderer* InRenderer)
    {
        AssetManager = InAssetManager;
        Renderer = InRenderer;
    }

    void SetDesiredSize(uint32 Width, uint32 Height);
    bool ConsumeDesiredSize(uint32& OutWidth, uint32& OutHeight);
    bool ConsumeClick(float& OutU, float& OutV, int32& OutPaneIndex);

    bool IsViewportHovered() const
    {
        return bViewportHovered;
    }

    int32 GetActiveViewportIndex() const
    {
        return ActiveViewportIndex;
    }

    void CancelGizmoDrag()
    {
        TransformGizmo.CancelDrag();
    }

    void SetEditorSettings(FEditorSettings* InSettings);

private:
    ID3D11ShaderResourceView* SRV = nullptr;

    FEditorObjectContext* ObjectContext = nullptr;
    FEditorTransformGizmo TransformGizmo;

    FEditorSettings* EditorSettings = nullptr;
    FAssetManager* AssetManager = nullptr;
    URenderer* Renderer = nullptr;

    FEditorViewportPane* Viewports = nullptr;

    uint32 DesiredWidth = 0;
    uint32 DesiredHeight = 0;
    bool bHasPendingSize = false;

    float ClickU = 0.0f;
    float ClickV = 0.0f;
    int32 ClickPaneIndex = 0;
    bool bHasPendingClick = false;

    int32 ActiveViewportIndex = 0;

    bool bDraggingVerticalSplitter = false;
    bool bDraggingHorizontalSplitter = false;
    bool bViewportHovered = false;
};