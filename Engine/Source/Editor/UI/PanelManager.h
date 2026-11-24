#pragma once
#include "Core/STL.h"
#include "Panels/EditorPanel.h"

class FPanelManager
{
public:
    void Init();
    void OnRender();
    void Shutdown();
    bool AddPanel(IEditorPanel* Panel);
    IEditorPanel* FindPanel(const char* PanelName) const;
    void SetPanelOpen(const char* PanelName, bool bOpen);
private:
    TArray<IEditorPanel*> Panels;
};
