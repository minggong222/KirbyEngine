#include "ImGuiRenderer.h"

#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"


void FImGuiRenderer::Init(HWND Hwnd, ID3D11Device* Device, ID3D11DeviceContext* DeviceContext)
{
    // Initialize ImGui
    {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        ImGui_ImplWin32_Init((void*)Hwnd);
        ImGui_ImplDX11_Init(Device, DeviceContext);
    }
}

void FImGuiRenderer::OnRender()
{
    // ImGui update, render 
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    
    // 이후 ImGui UI 컨트롤 추가는 ImGui::NewFrame()과 ImGui::Render() 사이인 여기에 위치합니다.
    ImGui::Begin("Jungle Property Window");
    {
        ImGui::Text("Hello Jungle World!");
    }
    ImGui::End();
    
    // 이후 ImGui UI 컨트롤 추가는 ImGui::NewFrame()과 ImGui::Render() 사이인 여기에 위치합니다.
    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

void FImGuiRenderer::Shutdown()
{
    // Release ImGui
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}
