#pragma once
#include "ConsolePanel.h"
#include "Core/STL.h"

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
