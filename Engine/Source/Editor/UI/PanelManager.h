#pragma once
#include "Core/STL.h"
#include "Panels/EditorPanel.h"

class FPanelManager
{
public:
    void Init();
    void OnRender();
    void Shutdown();
    void AddPanel(IEditorPanel* panel);
private:
    TArray<IEditorPanel*> Panels;
};
