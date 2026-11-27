#pragma once

#include "EditorPanel.h"
#include "imgui.h"
#include "Core/STL.h"
#include "Core/Logging/Log.h"

class FStatsPanel;

struct FLogData
{
    FString Massage;
    ELogVerbosity Verbosity = ELogVerbosity::Info;  // color is resolved from the active theme when drawn
};

class FConsolePanel : public IEditorPanel
{
public:
    bool Init() override;
    void Tick(float DeltaTime) override;
    const char* GetPanelName() const override { return "Output Log"; }
    void OnRender() override;

    void ClearLog();
    int TextEditCallback(ImGuiInputTextCallbackData* data);
    void ExecCommand(const char* command_line);
    void SetStatsPanel(FStatsPanel* InStatsPanel) { StatsPanel = InStatsPanel; }

    void AddLog(ELogVerbosity Verbosity, const char* fmt, ...) IM_FMTARGS(2);

private:
    static void HandleLogOutput(void* UserData, ELogVerbosity Verbosity, const char* Message);

    void AddLogInternal(ELogVerbosity Verbosity, const char* Massage);

    char InputBuf[256];
    TArray<FLogData> Items;
    TArray<FString> Commands;
    TArray<FString> History;
    int HistoryPos = -1;
    ImGuiTextFilter Filter;
    bool AutoScroll = true;
    bool ScrollToBottom = false;

    FStatsPanel* StatsPanel = nullptr;
};