#include "PanelManager.h"

void FPanelManager::Init()
{
}

void FPanelManager::OnRender()
{
    for (IEditorPanel* Panel : Panels)
    {
        if (Panel != nullptr)
        {
            Panel->OnRender();
        }
    }
}

void FPanelManager::Shutdown()
{
    for (int32 Index = Panels.Num() - 1; Index >= 0; --Index)
    {
        delete Panels[Index];
    }

    Panels.Reset();
}

bool FPanelManager::AddPanel(IEditorPanel* Panel)
{
    if (Panel == nullptr || Panels.Contains(Panel))
    {
        return false;
    }

    if (!Panel->Init())
    {
        delete Panel;
        return false;
    }

    Panels.Add(Panel);
    return true;
}