#include "UIManager.h"

void FUIManager::Init()
{
    Panels.Add(new FConsolePanel());
    
    for (auto panel : Panels)
    {
        panel->Init();
    }
}

void FUIManager::OnRender()
{
    for (auto panel : Panels)
    {
        panel->OnRender();
    }
}

void FUIManager::Shutdown()
{
}
