#pragma once
#include "ConsolePanel.h"
#include "Core/STL.h"

class FPanelManager
{
public:
    void Init();
    void OnRender();
    void Shutdown();
private:
    TArray<IEditorPanel*> Panels; 
};
