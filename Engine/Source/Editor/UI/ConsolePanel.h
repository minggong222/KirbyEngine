#pragma once
#include "EditorPanel.h"
#include "imgui.h"
#include "Core/STL.h"
#include "Core/Logging/Log.h"

struct FLogData
{
    FString Massage;
    ImVec4 Color;
};

class FConsolePanel : public IEditorPanel
{
public:
    bool Init() override;
    void Tick(float DeltaTime) override;
    void OnRender() override;
    
    void ClearLog();
    int TextEditCallback(ImGuiInputTextCallbackData* data);
    void ExecCommand(const char* command_line);
    
    void AddLog(ELogVerbosity Verbosity, const char* fmt, ...) IM_FMTARGS(2);
private:
    static void HandleLogOutput(void* UserData, ELogVerbosity Verbosity, const char* Message);
    
    void AddLogInternal(ELogVerbosity Verbosity, const char* Massage);
    
    char                  InputBuf[256];
    TArray<FLogData>      Items;
    TArray<FString>		  Commands;
    TArray<FString>       History;
    int                   HistoryPos;
    ImGuiTextFilter       Filter;
    bool                  AutoScroll;
    bool                  ScrollToBottom;
};
