#include "EditorEngine.h"

#include "FVertexSimple.h"
#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"
#include "Sphere.h"
#include "Core/STL.h"

bool FEditorEngine::Init(HINSTANCE hInstance)
{
   
    Renderer = new URenderer();
    UIManager = new FUIManager();
    ImGuiRenderer = new FImGuiRenderer();
    WindowsWindow = new FWindowsWindow();
    WindowsWindow->Create(hInstance, 1920, 1080);
    // Initialize renderer
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
    // FPS 제한을 위한 설정
    // 타이머도 만들어야되네;
    const int32 targetFPS = 30;
    const double targetFrameTime = 1000.0 / targetFPS;  // 한 프레임의 목표 시간 (밀리초 단위)

    // 고성능 타이머 초기화
    LARGE_INTEGER frequency;
    QueryPerformanceFrequency(&frequency);

    LARGE_INTEGER startTime;
    LARGE_INTEGER endTime;
    
    double elapsedTime = 0.0f;
    
    // Main Loop (Quit Message가 들어오기 전까지 아래 Loop를 무한히 실행하게 됨)
    while (bIsRunning == true)
    {
        MSG msg;

        // 루프 시작 시간 기록
        QueryPerformanceCounter(&startTime);

        // 처리할 메시지가 더 이상 없을때 까지 수행
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            // 키 입력 메시지를 번역
            TranslateMessage(&msg);

            // 메시지를 적절한 윈도우 프로시저에 전달, 메시지가 위에서 등록한 WndProc 으로 전달됨
            DispatchMessage(&msg);

            
            //입력은 누가 가지냐
            if (msg.message == WM_QUIT)
            {
                bIsRunning = false;

                break;
            }
            else if (msg.message == WM_KEYDOWN) // 키보드 눌렸을 때
            {
                // 눌린 키가 방향키라면 해당 방향에 맞춰서
                // offset 변수의 x, y 맴버 변수의 값을 조정합니다.
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

        ////////////////////////////////////////////
        // 매번 실행되는 코드를 여기에 추가합니다.

        // Geometry update, render
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

        // 다 그렸으면 버퍼를 교환
        // 현재 화면에 보여지는 버퍼와 그리기 작업을 위한 버퍼를 서로 교환합니다.
        Renderer->SwapBuffer();

        // 프레임 갱신 시간 고정시키기
        do
        {
            Sleep(0);

            // 루프 종료 시간 기록
            QueryPerformanceCounter(&endTime);

            // 한 프레임이 소요된 시간 계산 (밀리초 단위로 변환)
            elapsedTime = (endTime.QuadPart - startTime.QuadPart) * 1000.0 / frequency.QuadPart;

        } while (elapsedTime < targetFrameTime);


        ////////////////////////////////////////////
    }
    
}

void FEditorEngine::Shutdown()
{
    ImGuiRenderer->Shutdown();
    Renderer->Shutdown();
}
