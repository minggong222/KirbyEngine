#include "EditorEngine.h"

#include "Components/SphereComp.h"
#include "Core/STL.h"
#include "Core/Time/PlatformTime.h"
#include "UObject/Object.h"

bool FEditorEngine::Init(HINSTANCE hInstance)
{
   
    Renderer = new URenderer();
    ImGuiRenderer = new FImGuiRenderer();
    WindowsWindow = new FWindowsWindow();
    WindowsWindow->Create(hInstance, 1920, 1080);
    {
        Renderer->Create(WindowsWindow->GetHWND());
        Renderer->CreateShader();
        Renderer->CreateConstantBuffer();
    }
    ImGuiRenderer->Init(WindowsWindow->GetHWND(), Renderer->GetDevice(), Renderer->GetDeviceContext());
    
    ViewportPanel = new FViewportPanel();
    ImGuiRenderer->GetPanelManager()->AddPanel(ViewportPanel);
    ConsolePanel = new FConsolePanel();
    ImGuiRenderer->GetPanelManager()->AddPanel(ConsolePanel);
    
    bIsRunning = true;
    UE_LOG(Info, "Info");
    UE_LOG(Error, "Error");
    UE_LOG(Warning, "Warning");
    return true;
}

void FEditorEngine::Run()
{

    double PreviousTime = FPlatformTime::Seconds();
    USphereComp* Test = new USphereComp();
    ID3D11Buffer* vertexBufferSphere = Renderer->CreateVertexBuffer(Test->GetVertex().GetData(), Test->GetVertex().Num());
    
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
            Renderer->UpdateConstant(Test->GetLocation());
            Renderer->RenderPrimitive(vertexBufferSphere, Test->GetVertex().Num());
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
