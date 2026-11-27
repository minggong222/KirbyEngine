#include "ImGuiRenderer.h"
#include "UI/KirbyEditorTheme.h"

#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"
#include "imgui_internal.h"

#include <d3d11.h>

#include "Windows/WindowsWindow.h"

#include <filesystem>
#include <string>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace
{
    // Installed into the Runtime window so it never has to know about ImGui.
    bool ImGuiWindowMessageHook(HWND hWnd, uint32 Message, WPARAM wParam, LPARAM lParam)
    {
        return ImGui_ImplWin32_WndProcHandler(hWnd, Message, wParam, lParam) != 0;
    }

    constexpr float EditorFontSize = 15.0f;

    std::string GetSystemFontPath(const char* FileName)
    {
        char WindowsDirectory[MAX_PATH] = {};
        const UINT Length = GetWindowsDirectoryA(WindowsDirectory, MAX_PATH);

        if (Length == 0 || Length >= MAX_PATH)
        {
            return std::string();
        }

        const std::filesystem::path FontPath = std::filesystem::path(WindowsDirectory) / "Fonts" / FileName;
        std::error_code Error;
        return std::filesystem::exists(FontPath, Error) ? FontPath.string() : std::string();
    }

    // Segoe UI for a clean editor look, with Malgun Gothic merged in so Korean text renders.
    // Falls back to the built-in ImGui font when the system fonts are missing.
    void LoadEditorFonts(ImGuiIO& IO)
    {
        const std::string MainFont = GetSystemFontPath("segoeui.ttf");

        if (MainFont.empty() || IO.Fonts->AddFontFromFileTTF(MainFont.c_str(), EditorFontSize) == nullptr)
        {
            IO.Fonts->AddFontDefault();
            return;
        }

        const std::string KoreanFont = GetSystemFontPath("malgun.ttf");

        if (!KoreanFont.empty())
        {
            ImFontConfig MergeConfig;
            MergeConfig.MergeMode = true;
            IO.Fonts->AddFontFromFileTTF(KoreanFont.c_str(), EditorFontSize, &MergeConfig);
        }
    }

    // Default layout: viewport in the middle, Outliner/Details column on the right,
    // Content Browser / Output Log / Stats tabbed under the viewport.
    void BuildDefaultDockLayout(ImGuiID DockSpaceId, const ImVec2& Size)
    {
        ImGui::DockBuilderRemoveNode(DockSpaceId);
        ImGui::DockBuilderAddNode(DockSpaceId, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(DockSpaceId, Size);

        ImGuiID DockLeft = 0;
        ImGuiID DockRight = 0;
        ImGuiID DockOutliner = 0;
        ImGuiID DockDetails = 0;
        ImGuiID DockViewport = 0;
        ImGuiID DockBottom = 0;

        ImGui::DockBuilderSplitNode(DockSpaceId, ImGuiDir_Right, 0.22f, &DockRight, &DockLeft);
        ImGui::DockBuilderSplitNode(DockRight, ImGuiDir_Down, 0.58f, &DockDetails, &DockOutliner);
        ImGui::DockBuilderSplitNode(DockLeft, ImGuiDir_Down, 0.28f, &DockBottom, &DockViewport);

        ImGui::DockBuilderDockWindow("Viewport", DockViewport);
        ImGui::DockBuilderDockWindow("Outliner", DockOutliner);
        ImGui::DockBuilderDockWindow("Details", DockDetails);
        ImGui::DockBuilderDockWindow("Content Browser", DockBottom);
        ImGui::DockBuilderDockWindow("Output Log", DockBottom);
        ImGui::DockBuilderDockWindow("Stats", DockBottom);

        ImGui::DockBuilderFinish(DockSpaceId);
    }
}

bool FImGuiRenderer::Init(HWND Hwnd, void* NativeDevice, void* NativeDeviceContext)
{
    ID3D11Device* Device = static_cast<ID3D11Device*>(NativeDevice);
    ID3D11DeviceContext* DeviceContext = static_cast<ID3D11DeviceContext*>(NativeDeviceContext);

    if (Hwnd == nullptr || Device == nullptr || DeviceContext == nullptr)
    {
        return false;
    }

    IMGUI_CHECKVERSION();

    ImGui::CreateContext();
    bContextCreated = true;

    KirbyEditorTheme::Apply(KirbyEditorTheme::EStyle::Kirby);

    ImGuiIO& IO = ImGui::GetIO();
    IO.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    IO.ConfigWindowsMoveFromTitleBarOnly = true;
    IO.IniFilename = "imgui.ini";

    LoadEditorFonts(IO);

    if (!ImGui_ImplWin32_Init(reinterpret_cast<void*>(Hwnd)))
    {
        Shutdown();
        return false;
    }

    bWin32Initialized = true;
    FWindowsWindow::SetMessageHook(&ImGuiWindowMessageHook);

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

void FImGuiRenderer::BuildFrame()
{
    if (!bContextCreated || !bWin32Initialized || !bDX11Initialized || PanelManager == nullptr)
    {
        return;
    }
    // ImGui update, render 
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    PanelManager->OnRenderMainMenu();
    DrawDockHost();
    PanelManager->OnRender();
    
    // 이후 ImGui UI 컨트롤 추가는 ImGui::NewFrame()과 ImGui::Render() 사이인 여기에 위치합니다.
    ImGui::Render();
}

void FImGuiRenderer::RenderDrawData()
{
    if (!bDX11Initialized)
    {
        return;
    }

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
        FWindowsWindow::SetMessageHook(nullptr);
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

    // New dockspace name so layouts saved in imgui.ini by the old layout are not reused.
    dockspace_id = ImGui::GetID("KirbyEditorDockSpace_Unreal");

    const bool bResetRequested = PanelManager != nullptr && PanelManager->ConsumeLayoutReset();

    if (bResetRequested || ImGui::DockBuilderGetNode(dockspace_id) == nullptr)
    {
        BuildDefaultDockLayout(dockspace_id, viewport->WorkSize);
    }

    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f));

    ImGui::End();
}