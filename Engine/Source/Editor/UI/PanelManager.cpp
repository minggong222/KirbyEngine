#include "PanelManager.h"
#include <cstring>

void FPanelManager::Init()
{
}

void FPanelManager::OnRender()
{
    for (IEditorPanel* Panel : Panels)
    {
        if (Panel != nullptr && std::strcmp(Panel->GetPanelName(), "Main Menu") != 0)
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

IEditorPanel* FPanelManager::FindPanel(const char* PanelName) const
{
    if (PanelName == nullptr)
    {
        return nullptr;
    }

    for (IEditorPanel* Panel : Panels)
    {
        if (Panel != nullptr && std::strcmp(Panel->GetPanelName(), PanelName) == 0)
        {
            return Panel;
        }
    }

    return nullptr;
}

void FPanelManager::SetPanelOpen(const char* PanelName, bool bOpen)
{
    IEditorPanel* Panel = FindPanel(PanelName);

    if (Panel != nullptr)
    {
        Panel->SetOpen(bOpen);
    }
}

void FPanelManager::OnRenderMainMenu()
{
    IEditorPanel* MainMenu = FindPanel("Main Menu");

    if (MainMenu != nullptr)
    {
        MainMenu->OnRender();
    }
}