
#include <windows.h>

#pragma comment(lib, "user32")
#pragma comment(lib, "d3d11")
#pragma comment(lib, "d3dcompiler")

#include "EditorEngine.h"

#include "imgui.h"

#include "Core/Math.h"
#include "Core/STL.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
    FEditorEngine* engine = new FEditorEngine();
    
    if (!engine->Init(hInstance)) return -1;
    {
        engine->Run();
    }
    return 0;
}
