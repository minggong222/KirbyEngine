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
    void OnRenderMainMenu();

    // Window > Reset Layout: the dock host rebuilds the default layout on the next frame.
    void RequestLayoutReset() { bLayoutResetRequested = true; }
    bool ConsumeLayoutReset()
    {
        const bool bRequested = bLayoutResetRequested;
        bLayoutResetRequested = false;
        return bRequested;
    }
private:
    TArray<IEditorPanel*> Panels;
    bool bLayoutResetRequested = false;
};
