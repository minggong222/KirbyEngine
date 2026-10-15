#include "PanelManager.h"

void FPanelManager::Init()
{
    Panels.Add(new FConsolePanel());
    
    for (auto panel : Panels)
    {
        panel->Init();
    }
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
