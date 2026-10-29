#include "PanelManager.h"

void FPanelManager::Init()
{
}

void FPanelManager::OnRender()
{
    for (auto panel : Panels)
    {
        panel->OnRender();
    }
}

void FPanelManager::Shutdown()
{
}

void FPanelManager::AddPanel(IEditorPanel* panel)
{
    Panels.Add(panel);
    panel->Init();
}
