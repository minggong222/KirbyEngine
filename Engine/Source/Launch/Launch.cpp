#include "EngineLoop.h"

#pragma comment(lib, "user32")
#pragma comment(lib, "d3d11")
#pragma comment(lib, "d3dcompiler")

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
    FEngineLoop EngineLoop;

    if (!EngineLoop.PreInit(hInstance))
    {
        EngineLoop.Exit();
        return -1;
    }

    while (!EngineLoop.ShouldExit())
    {
        EngineLoop.Tick();
    }

    EngineLoop.Exit();
    return 0;
}
