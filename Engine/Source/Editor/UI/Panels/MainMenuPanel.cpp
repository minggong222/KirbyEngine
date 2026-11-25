#include "MainMenuPanel.h"

#include "EditorSceneSerializer.h"
#include "EditorObjectContext.h"
#include "Resources/AssetManager.h"
#include "Core/Logging/Log.h"
#include "Components/CameraComponent.h"
#include "EditorUndoManager.h"
#include "UI/PanelManager.h"
#include "UI/KirbyEditorTheme.h"

#include "imgui.h"

#include <commdlg.h>
#include <cwchar>

#include "imgui_internal.h"

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
        ImDrawList* DrawList = ImGui::GetWindowDrawList();

        ImGui::PushStyleColor(ImGuiCol_Text, KirbyEditorTheme::PinkText());
        ImGui::TextUnformatted("KIRBY");

        const ImVec2 BrandMin = ImGui::GetItemRectMin();
        const ImVec2 BrandMax = ImGui::GetItemRectMax();

        KirbyEditorTheme::DrawPuffBadge(DrawList, ImVec2(BrandMax.x + 11.0f, (BrandMin.y + BrandMax.y) * 0.5f), 8.0f);
        ImGui::PopStyleColor();

        ImGui::SameLine(0.0f, 22.0f);

        ImGui::PushStyleColor(ImGuiCol_Text, KirbyEditorTheme::CreamText());
        ImGui::TextUnformatted("ENGINE");
        ImGui::PopStyleColor();

        ImGui::SameLine(0.0f, 20.0f);

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
    
    if (ImGui::BeginViewportSideBar("##MainToolbar", ImGui::GetMainViewport(), ImGuiDir_Up, 36.0f, ImGuiWindowFlags_NoSavedSettings))
    {
        FEditorUndoManager* UndoManager = Context != nullptr ? Context->UndoManager : nullptr;

        ImGui::PushStyleColor(ImGuiCol_Button, KirbyEditorTheme::PrimaryButton());
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, KirbyEditorTheme::PrimaryButtonHovered());
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, KirbyEditorTheme::PrimaryButtonActive());

        if (ImGui::Button("Save"))
        {
            SaveScene(false);
        }

        ImGui::PopStyleColor(3);

        ImGui::SameLine();

        ImGui::PushStyleColor(ImGuiCol_Button, KirbyEditorTheme::BlueButton());
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, KirbyEditorTheme::BlueButtonHovered());
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, KirbyEditorTheme::BlueButtonActive());

        ImGui::BeginDisabled(UndoManager == nullptr || !UndoManager->CanUndo());

        if (ImGui::Button("Undo"))
        {
            UndoManager->Undo(*Context);
        }

        ImGui::EndDisabled();

        ImGui::SameLine();

        ImGui::BeginDisabled(UndoManager == nullptr || !UndoManager->CanRedo());

        if (ImGui::Button("Redo"))
        {
            UndoManager->Redo(*Context);
        }

        ImGui::EndDisabled();

        ImGui::PopStyleColor(3);

        ImGui::End();
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