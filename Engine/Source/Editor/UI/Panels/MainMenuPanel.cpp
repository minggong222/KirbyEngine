#include "MainMenuPanel.h"

#include "EditorSceneSerializer.h"
#include "EditorObjectContext.h"
#include "Resources/AssetManager.h"
#include "Core/Logging/Log.h"
#include "Components/CameraComponent.h"
#include "EditorUndoManager.h"
#include "UI/PanelManager.h"

#include "imgui.h"

#include <commdlg.h>
#include <cwchar>

#pragma comment(lib, "Comdlg32.lib")

bool FMainMenuPanel::Init()
{
    return Context != nullptr && AssetManager != nullptr && OwnerWindow != nullptr && ViewportCamera != nullptr;
}

void FMainMenuPanel::Tick(float DeltaTime)
{
}

bool FMainMenuPanel::SelectSceneFile(bool bSaveDialog, std::filesystem::path& OutPath) const
{
    wchar_t FileName[32768] = {};

    if (bSaveDialog)
    {
        wcscpy_s(FileName, std::size(FileName), L"Untitled.Scene");
    }

    static constexpr wchar_t SceneFilter[] = L"KirbyEngine Scene (*.Scene)\0*.Scene\0All Files (*.*)\0*.*\0";

    OPENFILENAMEW Dialog = {};
    Dialog.lStructSize = sizeof(Dialog);
    Dialog.hwndOwner = OwnerWindow;
    Dialog.lpstrFilter = SceneFilter;
    Dialog.lpstrFile = FileName;
    Dialog.nMaxFile = static_cast<DWORD>(std::size(FileName));
    Dialog.lpstrDefExt = L"Scene";
    Dialog.lpstrTitle = bSaveDialog ? L"Save Scene" : L"Open Scene";
    Dialog.Flags = OFN_EXPLORER | OFN_NOCHANGEDIR | OFN_PATHMUSTEXIST | (bSaveDialog ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST);

    const BOOL bSelected = bSaveDialog ? GetSaveFileNameW(&Dialog) : GetOpenFileNameW(&Dialog);

    if (!bSelected)
    {
        const DWORD DialogError = CommDlgExtendedError();

        if (DialogError != 0)
        {
            UE_LOG(Error, "Scene file dialog failed: %u", static_cast<uint32>(DialogError));
        }

        return false;
    }

    OutPath = std::filesystem::path(FileName);
    return true;
}

void FMainMenuPanel::SaveScene(bool bSaveAs)
{
    std::filesystem::path TargetPath = CurrentScenePath;

    if (bSaveAs || TargetPath.empty())
    {
        if (!SelectSceneFile(true, TargetPath))
        {
            return;
        }
    }

    if (!FSceneSerializer::Save(TargetPath, *Context, *ViewportCamera))
    {
        return;
    }

    CurrentScenePath = TargetPath;
}

void FMainMenuPanel::LoadScene()
{
    std::filesystem::path SelectedPath;

    if (!SelectSceneFile(false, SelectedPath))
    {
        return;
    }

    if (!FSceneSerializer::Load(SelectedPath, *Context, *AssetManager, *ViewportCamera, *Renderer))
    {
        return;
    }

    if (Context->UndoManager != nullptr)
    {
        Context->UndoManager->Clear();
    }

    CurrentScenePath = SelectedPath;
}

void FMainMenuPanel::OnRender()
{
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Open Scene...", "Ctrl+O"))
            {
                LoadScene();
            }

            ImGui::Separator();

            if (ImGui::MenuItem("Save Scene", "Ctrl+S"))
            {
                SaveScene(false);
            }

            if (ImGui::MenuItem("Save Scene As...", "Ctrl+Shift+S"))
            {
                SaveScene(true);
            }

            ImGui::EndMenu();
        }
        
        if (PanelManager != nullptr && ImGui::BeginMenu("Window"))
        {
            const char* PanelNames[] =
            {
                "Viewport",
                "World Outliner",
                "Details",
                "Content Browser",
                "Console",
                "Stats"
            };

            for (const char* PanelName : PanelNames)
            {
                IEditorPanel* Panel = PanelManager->FindPanel(PanelName);

                if (Panel == nullptr)
                {
                    continue;
                }

                bool bOpen = Panel->IsOpen();

                if (ImGui::MenuItem(PanelName, nullptr, &bOpen))
                {
                    Panel->SetOpen(bOpen);
                }
            }

            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }

    const ImGuiIO& IO = ImGui::GetIO();

    if (IO.WantTextInput || IO.WantCaptureKeyboard)
    {
        return;
    }

    if (IO.KeyCtrl && IO.KeyShift && ImGui::IsKeyPressed(ImGuiKey_S, false))
    {
        SaveScene(true);
    }
    else if (IO.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false))
    {
        SaveScene(false);
    }
    else if (IO.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_O, false))
    {
        LoadScene();
    }
}