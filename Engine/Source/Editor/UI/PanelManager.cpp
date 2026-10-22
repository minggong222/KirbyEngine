#include "PanelManager.h"

#include "ViewportPanel.h"
#include "Core/Math.h"

void FPanelManager::Init()
{
    Panels.Add(new FConsolePanel());
    Panels.Add(new FViewportPanel());
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
