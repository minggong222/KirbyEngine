#pragma once
#include "EditorObjectContext.h"
#include "EditorPanel.h"

class FDetailsPanel : public IEditorPanel
{
public:
    explicit FDetailsPanel(FEditorObjectContext* InContext) : Context(InContext) {}
    bool Init() override;
    void Tick(float DeltaTime) override;
    void OnRender() override;
private:
    FEditorObjectContext* Context = nullptr;    
};
