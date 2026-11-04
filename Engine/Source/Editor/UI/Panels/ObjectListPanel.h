#pragma once
#include "EditorObjectContext.h"
#include "EditorPanel.h"

class URenderer;
class FObjectListPanel : public IEditorPanel
{
public:
    explicit FObjectListPanel(FEditorObjectContext* InContext, URenderer* InRenderer) : Context(InContext), Renderer(InRenderer) {}
    bool Init() override;
    void Tick(float DeltaTime) override;
    void OnRender() override;
private:
    FEditorObjectContext* Context = nullptr;
    URenderer* Renderer = nullptr;
};
