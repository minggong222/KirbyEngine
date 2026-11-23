#pragma once

#include "EditorPanel.h"
#include "EditorObjectContext.h"
#include "Core/Stats/FrameStats.h"

class FStatsPanel : public IEditorPanel
{
public:
    FStatsPanel(FFrameStats* InFrameStats, FEditorObjectContext* InContext)
        : FrameStats(InFrameStats), Context(InContext)
    {
    }

    bool Init() override;
    void Tick(float DeltaTime) override;
    void OnRender() override;

private:
    FFrameStats* FrameStats = nullptr;
    FEditorObjectContext* Context = nullptr;
};