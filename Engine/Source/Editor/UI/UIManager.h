#pragma once
#include "ConsolePanel.h"
#include "Core/STL.h"

class FUIManager
{
public:
    void Init();
    void OnRender();
    void Shutdown();
private:
    TArray<IEditorPanel*> Panels; 
};
