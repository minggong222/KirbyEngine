#pragma once

#include "EditorPanel.h"
#include "EditorObjectContext.h"
#include "Core/Stats/FrameStats.h"

enum class EStatOverlayMode
{
    None,
    FPS,
    Memory
};

class FStatsPanel : public IEditorPanel
{
public:
    FStatsPanel(FFrameStats* InFrameStats, FEditorObjectContext* InContext)
        : FrameStats(InFrameStats), Context(InContext)
    {
    }

    bool Init() override;
    void Tick(float DeltaTime) override;
    const char* GetPanelName() const override { return "Stats"; }
    void OnRender() override;

    void SetOverlayMode(EStatOverlayMode InMode)
    {
        OverlayMode = InMode;
    }

    EStatOverlayMode GetOverlayMode() const
    {
        return OverlayMode;
    }

private:
    void RenderOverlay();

    FFrameStats* FrameStats = nullptr;
    FEditorObjectContext* Context = nullptr;

    EStatOverlayMode OverlayMode = EStatOverlayMode::None;
};