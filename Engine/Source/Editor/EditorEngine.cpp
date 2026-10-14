#include "EditorEngine.h"

#include "FVertexSimple.h"
#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"
#include "Sphere.h"
#include "Core/STL.h"
#include "Core/Time/PlatformTime.h"

bool FEditorEngine::Init(HINSTANCE hInstance)
{
   
    Renderer = new URenderer();
    UIManager = new FUIManager();
    ImGuiRenderer = new FImGuiRenderer();
    WindowsWindow = new FWindowsWindow();
    WindowsWindow->Create(hInstance, 1920, 1080);
    {
        Renderer->Create(WindowsWindow->GetHWND());
        Renderer->CreateShader();
        Renderer->CreateConstantBuffer();
    }
    ImGuiRenderer->Init(WindowsWindow->GetHWND(), Renderer->GetDevice(), Renderer->GetDeviceContext());
    
    bIsRunning = true;
    UE_LOG(Info, "Info");
    UE_LOG(Error, "Error");
    UE_LOG(Warning, "Warning");
    return true;
}

void FEditorEngine::Run()
{
    /*
    // Renderer와 Shader 생성 이후에 버텍스 버퍼를 생성합니다.
    uint32 numVerticesTriangle = sizeof(triangle_vertices) / sizeof(FVertexSimple);
    uint32 numVerticesCube = sizeof(cube_vertices) / sizeof(FVertexSimple);
    uint32 numVerticesSphere = sizeof(sphere_vertices) / sizeof(FVertexSimple);
    ID3D11Buffer* vertexBufferTriangle = Renderer->CreateVertexBuffer(triangle_vertices, sizeof(triangle_vertices));
    ID3D11Buffer* vertexBufferCube = Renderer->CreateVertexBuffer(cube_vertices, sizeof(cube_vertices));
    ID3D11Buffer* vertexBufferSphere = Renderer->CreateVertexBuffer(sphere_vertices, sizeof(sphere_vertices));*/

    double PreviousTime = FPlatformTime::Seconds();
    
    while (bIsRunning == true)
    {
        const double CurrentTime = FPlatformTime::Seconds();
        const double DeltaTime = CurrentTime - PreviousTime;
        
        PreviousTime = CurrentTime;
        FrameStats.Update(DeltaTime);
        
        MSG msg;
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);

            
            //입력은 누가 가지냐
            if (msg.message == WM_QUIT)
            {
                bIsRunning = false;

                break;
            }
            else if (msg.message == WM_KEYDOWN)
            {
                if (msg.wParam == VK_LEFT)
                {
                }
                if (msg.wParam == VK_RIGHT)
                {
                }
                if (msg.wParam == VK_UP)
                {
                }
                if (msg.wParam == VK_DOWN)
                {
                }
            }
        }
        {
            // 준비 작업
            Renderer->Prepare();
            Renderer->PrepareShader();

            // offset을 상수 버퍼로 업데이트 합니다.
            Renderer->UpdateConstant(FVector(0.0f, 0.0f, 0.0f));
            /*
            // 생성한 버텍스 버퍼를 넘겨, 실질적인 렌더링 요청
            Renderer->RenderPrimitive(vertexBufferSphere, numVerticesSphere);*/
        }

        ImGuiRenderer->OnRender();
        Renderer->SwapBuffer();
    }
    
}

void FEditorEngine::Shutdown()
{
    ImGuiRenderer->Shutdown();
    Renderer->Shutdown();
}
