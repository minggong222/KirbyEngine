
#include <windows.h>

// D3D 사용에 필요한 라이브러리들을 링크합니다.
#pragma comment(lib, "user32")
#pragma comment(lib, "d3d11")
#pragma comment(lib, "d3dcompiler")

// D3D 사용에 필요한 헤더파일들을 포함합니다.
#include "URenderer.h"
#include "FVertexSimple.h"
#include "Sphere.h"

#include "ImGui/imgui.h"
#include "ImGui/imgui_internal.h"
#include "ImGui/imgui_impl_dx11.h"
#include "imGui/imgui_impl_win32.h"

enum ETypePrimitive
{
    EPT_Triangle,
    EPT_Cube,
    EPT_Sphere,
    EPT_Max,
};

ETypePrimitive typePrimitive = EPT_Triangle;

// 삼각형을 하드 코딩
FVertexSimple triangle_vertices[] =
{
    {  0.0f,  1.0f, 0.0f,  1.0f, 0.0f, 0.0f, 1.0f }, // Top vertex (red)
    {  1.0f, -1.0f, 0.0f,  0.0f, 1.0f, 0.0f, 1.0f }, // Bottom-right vertex (green)
    { -1.0f, -1.0f, 0.0f,  0.0f, 0.0f, 1.0f, 1.0f }  // Bottom-left vertex (blue)
};

FVertexSimple cube_vertices[] =
{
    // Front face (Z+)
    { -0.5f, -0.5f,  0.5f,  1.0f, 0.0f, 0.0f, 1.0f }, // Bottom-left (red)
    { -0.5f,  0.5f,  0.5f,  1.0f, 1.0f, 0.0f, 1.0f }, // Top-left (yellow)
    {  0.5f, -0.5f,  0.5f,  0.0f, 1.0f, 0.0f, 1.0f }, // Bottom-right (green)
    { -0.5f,  0.5f,  0.5f,  1.0f, 1.0f, 0.0f, 1.0f }, // Top-left (yellow)
    {  0.5f,  0.5f,  0.5f,  0.0f, 0.0f, 1.0f, 1.0f }, // Top-right (blue)
    {  0.5f, -0.5f,  0.5f,  0.0f, 1.0f, 0.0f, 1.0f }, // Bottom-right (green)

    // Back face (Z-)
    { -0.5f, -0.5f, -0.5f,  0.0f, 1.0f, 1.0f, 1.0f }, // Bottom-left (cyan)
    {  0.5f, -0.5f, -0.5f,  1.0f, 0.0f, 1.0f, 1.0f }, // Bottom-right (magenta)
    { -0.5f,  0.5f, -0.5f,  0.0f, 0.0f, 1.0f, 1.0f }, // Top-left (blue)
    { -0.5f,  0.5f, -0.5f,  0.0f, 0.0f, 1.0f, 1.0f }, // Top-left (blue)
    {  0.5f, -0.5f, -0.5f,  1.0f, 0.0f, 1.0f, 1.0f }, // Bottom-right (magenta)
    {  0.5f,  0.5f, -0.5f,  1.0f, 1.0f, 0.0f, 1.0f }, // Top-right (yellow)

    // Left face (X-)
    { -0.5f, -0.5f, -0.5f,  1.0f, 0.0f, 1.0f, 1.0f }, // Bottom-left (purple)
    { -0.5f,  0.5f, -0.5f,  0.0f, 0.0f, 1.0f, 1.0f }, // Top-left (blue)
    { -0.5f, -0.5f,  0.5f,  0.0f, 1.0f, 0.0f, 1.0f }, // Bottom-right (green)
    { -0.5f,  0.5f, -0.5f,  0.0f, 0.0f, 1.0f, 1.0f }, // Top-left (blue)
    { -0.5f,  0.5f,  0.5f,  1.0f, 1.0f, 0.0f, 1.0f }, // Top-right (yellow)
    { -0.5f, -0.5f,  0.5f,  0.0f, 1.0f, 0.0f, 1.0f }, // Bottom-right (green)

    // Right face (X+)
    {  0.5f, -0.5f, -0.5f,  1.0f, 0.5f, 0.0f, 1.0f }, // Bottom-left (orange)
    {  0.5f, -0.5f,  0.5f,  0.5f, 0.5f, 0.5f, 1.0f }, // Bottom-right (gray)
    {  0.5f,  0.5f, -0.5f,  0.5f, 0.0f, 0.5f, 1.0f }, // Top-left (purple)
    {  0.5f,  0.5f, -0.5f,  0.5f, 0.0f, 0.5f, 1.0f }, // Top-left (purple)
    {  0.5f, -0.5f,  0.5f,  0.5f, 0.5f, 0.5f, 1.0f }, // Bottom-right (gray)
    {  0.5f,  0.5f,  0.5f,  0.0f, 0.0f, 0.5f, 1.0f }, // Top-right (dark blue)

    // Top face (Y+)
    { -0.5f,  0.5f, -0.5f,  0.0f, 1.0f, 0.5f, 1.0f }, // Bottom-left (light green)
    { -0.5f,  0.5f,  0.5f,  0.0f, 0.5f, 1.0f, 1.0f }, // Top-left (cyan)
    {  0.5f,  0.5f, -0.5f,  0.5f, 1.0f, 1.0f, 1.0f }, // Bottom-right (white)
    { -0.5f,  0.5f,  0.5f,  0.0f, 0.5f, 1.0f, 1.0f }, // Top-left (cyan)
    {  0.5f,  0.5f,  0.5f,  0.5f, 0.5f, 0.0f, 1.0f }, // Top-right (brown)
    {  0.5f,  0.5f, -0.5f,  0.5f, 1.0f, 1.0f, 1.0f }, // Bottom-right (white)

    // Bottom face (Y-)
    { -0.5f, -0.5f, -0.5f,  0.5f, 0.5f, 0.0f, 1.0f }, // Bottom-left (brown)
    { -0.5f, -0.5f,  0.5f,  1.0f, 0.0f, 0.0f, 1.0f }, // Top-left (red)
    {  0.5f, -0.5f, -0.5f,  1.0f, 0.0f, 0.5f, 1.0f }, // Bottom-right (purple)
    { -0.5f, -0.5f,  0.5f,  1.0f, 0.0f, 0.0f, 1.0f }, // Top-left (red)
    {  0.5f, -0.5f,  0.5f,  0.0f, 1.0f, 0.0f, 1.0f }, // Top-right (green)
    {  0.5f, -0.5f, -0.5f,  1.0f, 0.0f, 0.5f, 1.0f }, // Bottom-right (purple)
};

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

// 각종 메시지를 처리할 함수
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam))
    {
        return true;
    }

    switch (message)
    {
    case WM_DESTROY:
        // Signal that the app should quit
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }

    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
    // 윈도우 클래스 이름
    WCHAR WindowClass[] = L"JungleWindowClass";

    // 윈도우 타이틀바에 표시될 이름
    WCHAR Title[] = L"Game Tech Lab";

    // 각종 메시지를 처리할 함수인 WndProc의 함수 포인터를 WindowClass 구조체에 넣는다.
    WNDCLASSW wndclass = { 0, WndProc, 0, 0, 0, 0, 0, 0, 0, WindowClass };

    // 윈도우 클래스 등록
    RegisterClassW(&wndclass);

    // 1024 x 1024 크기에 윈도우 생성
    HWND hWnd = CreateWindowExW(0, WindowClass, Title, WS_POPUP | WS_VISIBLE | WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1024, 1024,
        nullptr, nullptr, hInstance, nullptr);

    bool bIsExit = false;

    // Initialize renderer
    URenderer renderer;
    {
        renderer.Create(hWnd);
        renderer.CreateShader();
        renderer.CreateConstantBuffer();
    }

    // Initialize ImGui
    {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        ImGui_ImplWin32_Init((void*)hWnd);
        ImGui_ImplDX11_Init(renderer.GetDevice(), renderer.GetDeviceContext());
    }

    // Renderer와 Shader 생성 이후에 버텍스 버퍼를 생성합니다.
    UINT numVerticesTriangle = sizeof(triangle_vertices) / sizeof(FVertexSimple);
    UINT numVerticesCube = sizeof(cube_vertices) / sizeof(FVertexSimple);
    UINT numVerticesSphere = sizeof(sphere_vertices) / sizeof(FVertexSimple);

    float scaleMod = 0.1f;
    for (UINT i = 0; i < numVerticesSphere; ++i)
    {
        sphere_vertices[i].x *= scaleMod;
        sphere_vertices[i].y *= scaleMod;
        sphere_vertices[i].z *= scaleMod;
    }

    ID3D11Buffer* vertexBufferTriangle = renderer.CreateVertexBuffer(triangle_vertices, sizeof(triangle_vertices));
    ID3D11Buffer* vertexBufferCube = renderer.CreateVertexBuffer(cube_vertices, sizeof(cube_vertices));
    ID3D11Buffer* vertexBufferSphere = renderer.CreateVertexBuffer(sphere_vertices, sizeof(sphere_vertices));

    FVector offset(0.0f);
    FVector velocity(0.0f);

    const float leftBorder = -1.0f;
    const float rightBorder = 1.0f;
    const float topBorder = -1.0f;
    const float bottomBorder = 1.0f;
    const float sphereRadius = 1.0f;

    bool bBoundBallToScreen = true;
    bool bPinballMovement = true;

    velocity.x = (float)(rand() % 100 - 50) * 0.001f;
    velocity.y = (float)(rand() % 100 - 50) * 0.001f;

    // FPS 제한을 위한 설정
    const int targetFPS = 30;
    const double targetFrameTime = 1000.0 / targetFPS;  // 한 프레임의 목표 시간 (밀리초 단위)

    // 고성능 타이머 초기화
    LARGE_INTEGER frequency;
    QueryPerformanceFrequency(&frequency);

    LARGE_INTEGER startTime;
    LARGE_INTEGER endTime;
    double elapsedTime = 0.0;

    // Main Loop (Quit Message가 들어오기 전까지 아래 Loop를 무한히 실행하게 됨)
    while (bIsExit == false)
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

            if (msg.message == WM_QUIT)
            {
                bIsExit = true;

                break;
            }
            else if (msg.message == WM_KEYDOWN) // 키보드 눌렸을 때
            {
                // 눌린 키가 방향키라면 해당 방향에 맞춰서
                // offset 변수의 x, y 맴버 변수의 값을 조정합니다.
                if (msg.wParam == VK_LEFT)
                {
                    offset.x -= 0.01f;
                }
                if (msg.wParam == VK_RIGHT)
                {
                    offset.x += 0.01f;
                }
                if (msg.wParam == VK_UP)
                {
                    offset.y += 0.01f;
                }
                if (msg.wParam == VK_DOWN)
                {
                    offset.y -= 0.01f;
                }

                // 키보드 처리 직후에 하면 밖을 벗어났다면, 화면 안쪽으로 위치시킨다.
                // 화면을 벗어나지 않아야 한다면
                if (bBoundBallToScreen)
                {
                    float renderRadius = sphereRadius * scaleMod;
                    if (offset.x < leftBorder + renderRadius)
                    {
                        offset.x = leftBorder + renderRadius;
                    }
                    if (offset.x > rightBorder - renderRadius)
                    {
                        offset.x = rightBorder - renderRadius;
                    }
                    if (offset.y < topBorder + renderRadius)
                    {
                        offset.y = topBorder + renderRadius;
                    }
                    if (offset.y > bottomBorder - renderRadius)
                    {
                        offset.y = bottomBorder - renderRadius;
                    }
                }
            }
        }

        ////////////////////////////////////////////
        // 매번 실행되는 코드를 여기에 추가합니다.
        
        // 핀볼 움직임이 켜져 있다면
        if (bPinballMovement)
        {
            // 속도를 공위치에 더해 공을 실질적으로 움직임
            offset.x += velocity.x;
            offset.y += velocity.y;
            offset.z += velocity.z;

            // 벽과 충돌 여부를 체크하고 충돌시 속도에 음수를 곱해 방향을 바꿈
            float renderRadius = sphereRadius * scaleMod;
            if (offset.x < leftBorder + renderRadius)
            {
                velocity.x *= -1.0f;
            }
            if (offset.x > rightBorder - renderRadius)
            {
                velocity.x *= -1.0f;
            }
            if (offset.y < topBorder + renderRadius)
            {
                velocity.y *= -1.0f;
            }
            if (offset.y > bottomBorder - renderRadius)
            {
                velocity.y *= -1.0f;
            }
        }

        // Geometry update, render
        {
            // 준비 작업
            renderer.Prepare();
            renderer.PrepareShader();

            // offset을 상수 버퍼로 업데이트 합니다.
            renderer.UpdateConstant(offset);

            // 생성한 버텍스 버퍼를 넘겨, 실질적인 렌더링 요청
            /*
            switch (typePrimitive)
            {
            case EPT_Triangle:
                renderer.RenderPrimitive(vertexBufferTriangle, numVerticesTriangle);
                break;
            case EPT_Cube:
                renderer.RenderPrimitive(vertexBufferCube, numVerticesCube);
                break;
            case EPT_Sphere:
                renderer.RenderPrimitive(vertexBufferSphere, numVerticesSphere);
                break;
            }
            */

            renderer.RenderPrimitive(vertexBufferSphere, numVerticesSphere);
        }

        // ImGui update, render 
        {
            ImGui_ImplDX11_NewFrame();
            ImGui_ImplWin32_NewFrame();
            ImGui::NewFrame();

            // 이후 ImGui UI 컨트롤 추가는 ImGui::NewFrame()과 ImGui::Render() 사이인 여기에 위치합니다.
            ImGui::Begin("Jungle Property Window");
            {
                ImGui::Text("Hello Jungle World!");
                
                /*
                if (ImGui::Button("Quit this app"))
                {
                    // 현재 윈도우에 Quit 메시지를 메시지 큐로 보냄
                    PostMessage(hWnd, WM_QUIT, 0, 0);
                }
                */

                /*
                if (ImGui::Button("Change primitive"))
                {
                    switch (typePrimitive)
                    {
                    case EPT_Triangle:
                        typePrimitive = EPT_Cube;
                        break;
                    case EPT_Cube:
                        typePrimitive = EPT_Sphere;
                        break;
                    case EPT_Sphere:
                        typePrimitive = EPT_Triangle;
                        break;
                    }
                }
                */

                ImGui::Checkbox("Bound Ball To Screen", &bBoundBallToScreen);
                ImGui::Checkbox("Pinball Movement", &bPinballMovement);
            }
            ImGui::End();

            // 이후 ImGui UI 컨트롤 추가는 ImGui::NewFrame()과 ImGui::Render() 사이인 여기에 위치합니다.
            ImGui::Render();
            ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        }

        // 다 그렸으면 버퍼를 교환
        // 현재 화면에 보여지는 버퍼와 그리기 작업을 위한 버퍼를 서로 교환합니다.
        renderer.SwapBuffer();

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

    // Release ImGui
    {
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
    }


    // Release renderer
    {
        // 버텍스 버퍼 소멸은 Renderer 소멸전에 처리합니다.
        renderer.ReleaseVertexBuffer(vertexBufferTriangle);
        renderer.ReleaseVertexBuffer(vertexBufferCube);
        renderer.ReleaseVertexBuffer(vertexBufferSphere);

        // ReleaseShader() 직전에 소멸 함수를 추가합니다.
        renderer.ReleaseConstantBuffer();

        // 렌더러 소멸 직전에 쉐이더를 소멸 시키는 함수를 호출합니다.
        renderer.ReleaseShader();

        renderer.Release();
    }

    return 0;
}
