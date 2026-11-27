#include "ConsolePanel.h"
#include "UI/KirbyEditorTheme.h"

#include "imgui.h"
#include "Core/Logging/Log.h"
#include "StatsPanel.h"

#include <cctype>
#include <cstdarg>
#include <cstdio>
#include <cstring>

// Log colors per theme: deep, saturated tones on the light pink Kirby background,
// light tones on the dark Unreal background.
static ImVec4 GetLogColor(ELogVerbosity Verbosity)
{
    const bool bLightBackground = KirbyEditorTheme::IsKirby();

    switch (Verbosity)
    {
    case ELogVerbosity::Error:
        return bLightBackground ? ImVec4(0.78f, 0.06f, 0.20f, 1.0f) : ImVec4(1.0f, 0.39f, 0.45f, 1.0f);

    case ELogVerbosity::Warning:
        return bLightBackground ? ImVec4(0.66f, 0.38f, 0.0f, 1.0f) : ImVec4(1.0f, 0.79f, 0.38f, 1.0f);

    case ELogVerbosity::Input:
        return bLightBackground ? ImVec4(0.10f, 0.38f, 0.70f, 1.0f) : ImVec4(0.52f, 0.80f, 0.98f, 1.0f);

    case ELogVerbosity::Info:
    default:
        // Same as the normal panel text so info lines read like body text
        return ImGui::GetStyleColorVec4(ImGuiCol_Text);
    }
}

static int TextEditCallbackStub(ImGuiInputTextCallbackData* Data)
{
    FConsolePanel* OutputLog = static_cast<FConsolePanel*>(Data->UserData);
    return OutputLog->TextEditCallback(Data);
}

static int Stricmp(const char* S1, const char* S2)
{
    int Difference = 0;

    while ((Difference = std::toupper(static_cast<unsigned char>(*S2)) - std::toupper(static_cast<unsigned char>(*S1))) == 0 && *S1 != '\0')
    {
        ++S1;
        ++S2;
    }

    return Difference;
}

static int Strnicmp(const char* S1, const char* S2, int Count)
{
    int Difference = 0;

    while (Count > 0 && (Difference = std::toupper(static_cast<unsigned char>(*S2)) - std::toupper(static_cast<unsigned char>(*S1))) == 0 && *S1 != '\0')
    {
        ++S1;
        ++S2;
        --Count;
    }

    return Difference;
}

static void Strtrim(char* Text)
{
    char* End = Text + std::strlen(Text);

    while (End > Text && End[-1] == ' ')
    {
        --End;
    }

    *End = '\0';
}

static char* Strdup(const char* Text)
{
    IM_ASSERT(Text);

    const std::size_t Length = std::strlen(Text) + 1;
    void* Buffer = ImGui::MemAlloc(Length);

    IM_ASSERT(Buffer);

    return static_cast<char*>(std::memcpy(Buffer, Text, Length));
}

bool FConsolePanel::Init()
{
    FLog::Init(&FConsolePanel::HandleLogOutput, this);

    ClearLog();

    Commands.Add("HELP");
    Commands.Add("HISTORY");
    Commands.Add("CLEAR");
    Commands.Add("STAT");
    Commands.Add("STAT ON");
    Commands.Add("STAT OFF");
    Commands.Add("STAT FPS");
    Commands.Add("STAT MEMORY");
    Commands.Add("STAT NONE");

    HistoryPos = -1;
    InputBuf[0] = '\0';

    AutoScroll = true;
    ScrollToBottom = false;

    return true;
}

void FConsolePanel::Tick(float DeltaTime)
{
}

void FConsolePanel::OnRender()
{
    if (!IsOpen())
    {
        return;
    }

    bool bIsOpen = IsOpen();

    ImGui::SetNextWindowSize(ImVec2(520.0f, 600.0f), ImGuiCond_FirstUseEver);

    if (!ImGui::Begin(GetPanelName(), &bIsOpen))
    {
        SetOpen(bIsOpen);
        ImGui::End();
        return;
    }

    if (ImGui::SmallButton("Clear"))
    {
        ClearLog();
    }

    if (ImGui::GetContentRegionAvail().x > 60.0f) ImGui::SameLine();

    const bool bCopyToClipboard = ImGui::SmallButton("Copy");

    ImGui::Separator();

    if (ImGui::BeginPopup("Options"))
    {
        ImGui::Checkbox("Auto-scroll", &AutoScroll);
        ImGui::EndPopup();
    }

    ImGui::SetNextItemShortcut(ImGuiMod_Ctrl | ImGuiKey_O, ImGuiInputFlags_Tooltip);

    if (ImGui::Button("Options"))
    {
        ImGui::OpenPopup("Options");
    }

    ImGui::Separator();
    const float FilterLabelWidth = ImGui::CalcTextSize("Filter").x + ImGui::GetStyle().ItemInnerSpacing.x;
    const float FilterAvailableWidth = ImGui::GetContentRegionAvail().x;

    if (FilterAvailableWidth >= FilterLabelWidth + 120.0f)
    {
        Filter.Draw("Filter", FilterAvailableWidth - FilterLabelWidth);
    }
    else
    {
        ImGui::TextUnformatted("Filter");
        Filter.Draw("##ConsoleFilter", ImGui::GetContentRegionAvail().x);
    }
    ImGui::Separator();

    ImGuiStyle& Style = ImGui::GetStyle();
    const float FooterHeightToReserve = Style.SeparatorSize + Style.ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();

    if (ImGui::BeginChild("ScrollingRegion", ImVec2(0.0f, -FooterHeightToReserve), ImGuiChildFlags_NavFlattened))
    {
        if (ImGui::BeginPopupContextWindow())
        {
            if (ImGui::Selectable("Clear"))
            {
                ClearLog();
            }

            ImGui::EndPopup();
        }

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f, 1.0f));

        if (bCopyToClipboard)
        {
            ImGui::LogToClipboard();
        }

        for (const FLogData& Item : Items)
        {
            if (!Filter.PassFilter(Item.Massage.c_str()))
            {
                continue;
            }

            ImGui::PushStyleColor(ImGuiCol_Text, GetLogColor(Item.Verbosity));
            ImGui::TextWrapped("%s", Item.Massage.c_str());
            ImGui::PopStyleColor();
        }

        if (bCopyToClipboard)
        {
            ImGui::LogFinish();
        }

        if (ScrollToBottom || (AutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()))
        {
            ImGui::SetScrollHereY(1.0f);
        }

        ScrollToBottom = false;

        ImGui::PopStyleVar();
    }

    ImGui::EndChild();
    ImGui::Separator();

    bool bReclaimFocus = false;

    const ImGuiInputTextFlags InputTextFlags =
        ImGuiInputTextFlags_EnterReturnsTrue |
        ImGuiInputTextFlags_EscapeClearsAll |
        ImGuiInputTextFlags_CallbackCompletion |
        ImGuiInputTextFlags_CallbackHistory;

    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);

    if (ImGui::InputTextWithHint("##ConsoleInput", "Enter command...", InputBuf, IM_COUNTOF(InputBuf), InputTextFlags, &TextEditCallbackStub, this))
    {
        Strtrim(InputBuf);

        if (InputBuf[0] != '\0')
        {
            ExecCommand(InputBuf);
        }

        strcpy_s(InputBuf, sizeof(InputBuf), "");
        bReclaimFocus = true;
    }

    ImGui::SetItemDefaultFocus();

    if (bReclaimFocus)
    {
        ImGui::SetKeyboardFocusHere(-1);
    }

    ImGui::End();

    SetOpen(bIsOpen);
}

void FConsolePanel::ClearLog()
{
    Items.Reset();
}

int FConsolePanel::TextEditCallback(ImGuiInputTextCallbackData* Data)
{
    switch (Data->EventFlag)
    {
    case ImGuiInputTextFlags_CallbackCompletion:
    {
        const char* WordEnd = Data->Buf + Data->CursorPos;
        const char* WordStart = WordEnd;

        while (WordStart > Data->Buf)
        {
            const char Character = WordStart[-1];

            if (Character == ' ' || Character == '\t' || Character == ',' || Character == ';')
            {
                break;
            }

            --WordStart;
        }

        ImVector<const char*> Candidates;

        for (int32 Index = 0; Index < Commands.Num(); ++Index)
        {
            if (Strnicmp(Commands[Index].c_str(), WordStart, static_cast<int>(WordEnd - WordStart)) == 0)
            {
                Candidates.push_back(Commands[Index].c_str());
            }
        }

        if (Candidates.Size == 0)
        {
            AddLog(ELogVerbosity::Error, "No match for \"%.*s\"!", static_cast<int>(WordEnd - WordStart), WordStart);
        }
        else if (Candidates.Size == 1)
        {
            Data->DeleteChars(static_cast<int>(WordStart - Data->Buf), static_cast<int>(WordEnd - WordStart));
            Data->InsertChars(Data->CursorPos, Candidates[0]);
            Data->InsertChars(Data->CursorPos, " ");
        }
        else
        {
            int MatchLength = static_cast<int>(WordEnd - WordStart);

            for (;;)
            {
                int Character = 0;
                bool bAllCandidatesMatch = true;

                for (int32 Index = 0; Index < Candidates.Size && bAllCandidatesMatch; ++Index)
                {
                    if (Index == 0)
                    {
                        Character = std::toupper(static_cast<unsigned char>(Candidates[Index][MatchLength]));
                    }
                    else if (Character == 0 || Character != std::toupper(static_cast<unsigned char>(Candidates[Index][MatchLength])))
                    {
                        bAllCandidatesMatch = false;
                    }
                }

                if (!bAllCandidatesMatch)
                {
                    break;
                }

                ++MatchLength;
            }

            if (MatchLength > 0)
            {
                Data->DeleteChars(static_cast<int>(WordStart - Data->Buf), static_cast<int>(WordEnd - WordStart));
                Data->InsertChars(Data->CursorPos, Candidates[0], Candidates[0] + MatchLength);
            }

            AddLog(ELogVerbosity::Info, "Possible matches:");

            for (int32 Index = 0; Index < Candidates.Size; ++Index)
            {
                AddLog(ELogVerbosity::Info, "- %s", Candidates[Index]);
            }
        }

        break;
    }

    case ImGuiInputTextFlags_CallbackHistory:
    {
        const int PreviousHistoryPosition = HistoryPos;

        if (Data->EventKey == ImGuiKey_UpArrow)
        {
            if (HistoryPos == -1)
            {
                HistoryPos = History.Num() - 1;
            }
            else if (HistoryPos > 0)
            {
                --HistoryPos;
            }
        }
        else if (Data->EventKey == ImGuiKey_DownArrow)
        {
            if (HistoryPos != -1 && ++HistoryPos >= History.Num())
            {
                HistoryPos = -1;
            }
        }

        if (PreviousHistoryPosition != HistoryPos)
        {
            const char* HistoryText = HistoryPos >= 0 ? History[HistoryPos].c_str() : "";

            Data->DeleteChars(0, Data->BufTextLen);
            Data->InsertChars(0, HistoryText);
        }

        break;
    }
    }

    return 0;
}

void FConsolePanel::ExecCommand(const char* CommandLine)
{
    AddLog(ELogVerbosity::Input, "# %s", CommandLine);

    HistoryPos = -1;

    for (int32 Index = History.Num() - 1; Index >= 0; --Index)
    {
        if (Stricmp(History[Index].c_str(), CommandLine) == 0)
        {
            History.RemoveAt(Index, 1);
            break;
        }
    }

    History.Add(Strdup(CommandLine));

    if (Stricmp(CommandLine, "CLEAR") == 0)
    {
        ClearLog();
    }
    else if (Stricmp(CommandLine, "HELP") == 0)
    {
        AddLog(ELogVerbosity::Info, "Commands:");

        for (int32 Index = 0; Index < Commands.Num(); ++Index)
        {
            AddLog(ELogVerbosity::Info, "- %s", Commands[Index].c_str());
        }
    }
    else if (Stricmp(CommandLine, "HISTORY") == 0)
    {
        const int32 FirstIndex = History.Num() - 10;

        for (int32 Index = FirstIndex > 0 ? FirstIndex : 0; Index < History.Num(); ++Index)
        {
            AddLog(ELogVerbosity::Info, "%3d: %s", Index, History[Index].c_str());
        }
    }
    else if (Stricmp(CommandLine, "STAT") == 0)
    {
        if (StatsPanel == nullptr)
        {
            AddLog(ELogVerbosity::Error, "StatsPanel is not available.");
        }
        else
        {
            const bool bOpen = !StatsPanel->IsOpen();
            StatsPanel->SetOpen(bOpen);
            AddLog(ELogVerbosity::Info, "Stats window %s.", bOpen ? "opened" : "closed");
        }
    }
    else if (Stricmp(CommandLine, "STAT ON") == 0)
    {
        if (StatsPanel == nullptr)
        {
            AddLog(ELogVerbosity::Error, "StatsPanel is not available.");
        }
        else
        {
            StatsPanel->SetOpen(true);
            AddLog(ELogVerbosity::Info, "Stats window opened.");
        }
    }
    else if (Stricmp(CommandLine, "STAT OFF") == 0)
    {
        if (StatsPanel == nullptr)
        {
            AddLog(ELogVerbosity::Error, "StatsPanel is not available.");
        }
        else
        {
            StatsPanel->SetOpen(false);
            AddLog(ELogVerbosity::Info, "Stats window closed.");
        }
    }
    else if (Stricmp(CommandLine, "STAT FPS") == 0)
    {
        if (StatsPanel == nullptr)
        {
            AddLog(ELogVerbosity::Error, "StatsPanel is not available.");
        }
        else
        {
            StatsPanel->SetOverlayMode(EStatOverlayMode::FPS);
            AddLog(ELogVerbosity::Info, "FPS overlay enabled.");
        }
    }
    else if (Stricmp(CommandLine, "STAT MEMORY") == 0)
    {
        if (StatsPanel == nullptr)
        {
            AddLog(ELogVerbosity::Error, "StatsPanel is not available.");
        }
        else
        {
            StatsPanel->SetOverlayMode(EStatOverlayMode::Memory);
            AddLog(ELogVerbosity::Info, "Memory overlay enabled.");
        }
    }
    else if (Stricmp(CommandLine, "STAT NONE") == 0)
    {
        if (StatsPanel == nullptr)
        {
            AddLog(ELogVerbosity::Error, "StatsPanel is not available.");
        }
        else
        {
            StatsPanel->SetOverlayMode(EStatOverlayMode::None);
            AddLog(ELogVerbosity::Info, "Stats overlay disabled.");
        }
    }
    else
    {
        AddLog(ELogVerbosity::Error, "Unknown command: '%s'", CommandLine);
    }

    ScrollToBottom = true;
}

void FConsolePanel::AddLog(ELogVerbosity Verbosity, const char* Format, ...)
{
    char Buffer[1024] = {};

    va_list Arguments;
    va_start(Arguments, Format);
    std::vsnprintf(Buffer, IM_COUNTOF(Buffer), Format, Arguments);
    Buffer[IM_COUNTOF(Buffer) - 1] = '\0';
    va_end(Arguments);

    constexpr int32 MaxConsoleLogEntries = 5000;

    if (Items.Num() >= MaxConsoleLogEntries)
    {
        Items.RemoveAt(0, 1);
    }

    Items.Add({FString(Buffer), Verbosity});
}

void FConsolePanel::HandleLogOutput(void* UserData, ELogVerbosity Verbosity, const char* Message)
{
    FConsolePanel* Panel = static_cast<FConsolePanel*>(UserData);

    if (Panel != nullptr)
    {
        Panel->AddLog(Verbosity, "%s", Message);
    }
}