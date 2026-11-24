#include "ImGuiRenderer.h"

#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"
#include "imgui_internal.h"


bool FImGuiRenderer::Init(HWND Hwnd, ID3D11Device* Device, ID3D11DeviceContext* DeviceContext)
{
    if (Hwnd == nullptr || Device == nullptr || DeviceContext == nullptr)
    {
        return false;
    }

    IMGUI_CHECKVERSION();

    ImGui::CreateContext();
    bContextCreated = true;

    ImGuiIO& IO = ImGui::GetIO();
    IO.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    IO.IniFilename = "imgui.ini";

    if (!ImGui_ImplWin32_Init(reinterpret_cast<void*>(Hwnd)))
    {
        Shutdown();
        return false;
    }

    bWin32Initialized = true;

    if (!ImGui_ImplDX11_Init(Device, DeviceContext))
    {
        Shutdown();
        return false;
    }

    bDX11Initialized = true;

    PanelManager = new FPanelManager();

    if (PanelManager == nullptr)
    {
        Shutdown();
        return false;
    }

    PanelManager->Init();
    return true;
}

void FImGuiRenderer::OnRender()
{
    if (!bContextCreated || !bWin32Initialized || !bDX11Initialized || PanelManager == nullptr)
    {
        return;
    }
    // ImGui update, render 
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    DrawDockHost();
    PanelManager->OnRender();
    
    // 이후 ImGui UI 컨트롤 추가는 ImGui::NewFrame()과 ImGui::Render() 사이인 여기에 위치합니다.
    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

void FImGuiRenderer::Shutdown()
{
    if (PanelManager != nullptr)
    {
        PanelManager->Shutdown();
        delete PanelManager;
        PanelManager = nullptr;
    }

    if (bDX11Initialized)
    {
        ImGui_ImplDX11_Shutdown();
        bDX11Initialized = false;
    }

    if (bWin32Initialized)
    {
        ImGui_ImplWin32_Shutdown();
        bWin32Initialized = false;
    }

    if (bContextCreated)
    {
        ImGui::DestroyPlatformWindows();
        ImGui::DestroyContext();
        bContextCreated = false;
    }
}

void FImGuiRenderer::DrawDockHost()
{
    const ImGuiViewport* viewport = ImGui::GetMainViewport();

    ImGuiID dockspace_id = 0;

    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGuiWindowFlags host_window_flags = 0;
    host_window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDocking;
    host_window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

        host_window_flags |= ImGuiWindowFlags_NoMouseInputs;

    char label[32];
    ImFormatString(label, IM_COUNTOF(label), "WindowOverViewport_%08X", viewport->ID);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin(label, NULL, host_window_flags);
    ImGui::PopStyleVar(3);

    if (dockspace_id == 0)
    {
        dockspace_id = ImGui::GetID("DockSpace");
    }

    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f));

    if (ImGui::DockBuilderGetNode(dockspace_id) == nullptr)
    {
        ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->WorkSize);

        ImGuiID DockMain = dockspace_id;
        ImGuiID DockLeft = 0;
        ImGuiID DockRight = 0;
        ImGuiID DockBottom = 0;

        ImGui::DockBuilderSplitNode(DockMain, ImGuiDir_Left, 0.22f, &DockLeft, &DockMain);
        ImGui::DockBuilderSplitNode(DockMain, ImGuiDir_Right, 0.25f, &DockRight, &DockMain);
        ImGui::DockBuilderSplitNode(DockMain, ImGuiDir_Down, 0.25f, &DockBottom, &DockMain);

        ImGui::DockBuilderDockWindow("World Outliner", DockLeft);
        ImGui::DockBuilderDockWindow("Details", DockRight);
        ImGui::DockBuilderDockWindow("Viewport", DockMain);
        ImGui::DockBuilderDockWindow("Content Browser", DockBottom);
        ImGui::DockBuilderDockWindow("Console", DockBottom);

        ImGui::DockBuilderFinish(dockspace_id);
    }

    ImGui::End();
}