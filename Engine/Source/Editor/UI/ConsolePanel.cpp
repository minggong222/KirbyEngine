#include "ConsolePanel.h"

#include "imgui.h"
#include "Core/Logging/Log.h"

static int TextEditCallbackStub(ImGuiInputTextCallbackData* data)
{
    FConsolePanel* OutputLog = (FConsolePanel*)data->UserData;
    return OutputLog->TextEditCallback(data);
}
static int Stricmp(const char* s1, const char* s2)
{
    int d;
    while ((d = toupper(*s2) - toupper(*s1)) == 0 && *s1)
    {
        s1++; s2++;
    } 
    return d;
}

static int Strnicmp(const char* s1, const char* s2, int n)
{
    int d = 0; 
    while (n > 0 && (d = toupper(*s2) - toupper(*s1)) == 0 && *s1)
    {
        s1++; s2++; n--;
    } 
    return d;
}

static void Strtrim(char* s)
{
    char* str_end = s + strlen(s);
    while (str_end > s && str_end[-1] == ' ') 
        str_end--;
    *str_end = 0;
}

static char* Strdup(const char* s)
{
    IM_ASSERT(s);
    size_t len = strlen(s) + 1;
    void* buf = ImGui::MemAlloc(len);
    IM_ASSERT(buf);
    return (char*)memcpy(buf, (const void*)s, len);
}


bool FConsolePanel::Init()
{
    FLog::Init(&FConsolePanel::HandleLogOutput, this);
    
    ClearLog();

    Commands.Add("HELP");
    Commands.Add("HISTORY");
    Commands.Add("CLEAR");

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
    ImGui::SetNextWindowSize(ImVec2(520, 600), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Console"))
    {
        ImGui::End();
        return;
    }
    
    ImGui::TextWrapped(
    "This example implements a console with basic coloring, completion (TAB key) and history (Up/Down keys). A more elaborate "
    "implementation may want to store entries along with extra data such as timestamp, emitter, etc.");
    ImGui::TextWrapped("Enter 'HELP' for help.");
    
    if (ImGui::SmallButton("Add Debug Text"))
    {
        AddLog(ELogVerbosity::Info, "%d some text", Items.Num());
        AddLog(ELogVerbosity::Info, "some more text");
        AddLog(ELogVerbosity::Info, "display very important message here!");
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Add Debug Error"))
    {
        AddLog(ELogVerbosity::Error, "[error] something went wrong");
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Clear"))
    {
        ClearLog();
    }
    ImGui::SameLine();
    bool copy_to_clipboard = ImGui::SmallButton("Copy");
    //static float t = 0.0f; if (ImGui::GetTime() - t > 0.02f) { t = ImGui::GetTime(); AddLog("Spam %f", t); }
    
    ImGui::Separator();
    
    // Options menu
    if (ImGui::BeginPopup("Options"))
    {
        ImGui::Checkbox("Auto-scroll", &AutoScroll);
        ImGui::EndPopup();
    }
    
    // Options, Filter
    ImGui::SetNextItemShortcut(ImGuiMod_Ctrl | ImGuiKey_O, ImGuiInputFlags_Tooltip);
    if (ImGui::Button("Options"))
        ImGui::OpenPopup("Options");
    ImGui::SameLine();
    Filter.Draw("Filter (\"incl,-excl\") (\"error\")", 180);
    ImGui::Separator();
    
    // Reserve enough left-over height for 1 separator + 1 input text
    ImGuiStyle& style = ImGui::GetStyle();
    const float footer_height_to_reserve = style.SeparatorSize + style.ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
    if (ImGui::BeginChild("ScrollingRegion", ImVec2(0, -footer_height_to_reserve), ImGuiChildFlags_NavFlattened, ImGuiWindowFlags_HorizontalScrollbar))
    {
        if (ImGui::BeginPopupContextWindow())
        {
            if (ImGui::Selectable("Clear")) ClearLog();
            ImGui::EndPopup();
        }
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1)); // Tighten spacing
        if (copy_to_clipboard)
            ImGui::LogToClipboard();
        for (FLogData item : Items)
        {
            if (!Filter.PassFilter(item.Massage.c_str()))
                continue;
            
            
            ImGui::PushStyleColor(ImGuiCol_Text, item.Color);
            ImGui::TextUnformatted(item.Massage.c_str());
            ImGui::PopStyleColor();
        }
            if (copy_to_clipboard)
                ImGui::LogFinish();
        
        // Keep up at the bottom of the scroll region if we were already at the bottom at the beginning of the frame.
        // Using a scrollbar or mouse-wheel will take away from the bottom edge.
        if (ScrollToBottom || (AutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()))
            ImGui::SetScrollHereY(1.0f);
        ScrollToBottom = false;
        
        ImGui::PopStyleVar();
    }
    ImGui::EndChild();
    ImGui::Separator();
    
    // Command-line
    bool reclaim_focus = false;
    ImGuiInputTextFlags input_text_flags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_EscapeClearsAll | ImGuiInputTextFlags_CallbackCompletion | ImGuiInputTextFlags_CallbackHistory;
    if (ImGui::InputText("Input", InputBuf, IM_COUNTOF(InputBuf), input_text_flags, &TextEditCallbackStub, (void*)this))
    {
        char* s = InputBuf;
        Strtrim(s);
        if (s[0])
            ExecCommand(s);
        strcpy_s(s, sizeof(s), "");
        reclaim_focus = true;
    }
    
    // Auto-focus on window apparition
    ImGui::SetItemDefaultFocus();
    if (reclaim_focus)
        ImGui::SetKeyboardFocusHere(-1); // Auto focus previous widget
    
    ImGui::End();
}

void FConsolePanel::ClearLog()
{
    Items.clear();
}

int FConsolePanel::TextEditCallback(ImGuiInputTextCallbackData* data)
{
    //AddLog("cursor: %d, selection: %d-%d", data->CursorPos, data->SelectionStart, data->SelectionEnd);
    switch (data->EventFlag)
    {
    
    case ImGuiInputTextFlags_CallbackCompletion:
        {
            // Example of TEXT COMPLETION
            
            // Locate beginning of current word
            const char* word_end = data->Buf + data->CursorPos;
            const char* word_start = word_end;
            while (word_start > data->Buf)
            {
                const char c = word_start[-1];
                if (c == ' ' || c == '\t' || c == ',' || c == ';')
                    break;
                word_start--;
            }
            
            // Build a list of candidates
            ImVector<const char*> candidates;
            for (int i = 0; i < Commands.Num(); i++)
                if (Strnicmp(Commands[i].c_str(), word_start, (int)(word_end - word_start)) == 0)
                    candidates.push_back(Commands[i].c_str());
            
            if (candidates.Size == 0)
            {
                // No match
                AddLog(ELogVerbosity::Error, "No match for \"%.*s\"!\n", (int)(word_end - word_start), word_start);
            }
            else if (candidates.Size == 1)
            {
                // Single match. Delete the beginning of the word and replace it entirely so we've got nice casing.
                data->DeleteChars((int)(word_start - data->Buf), (int)(word_end - word_start));
                data->InsertChars(data->CursorPos, candidates[0]);
                data->InsertChars(data->CursorPos, " ");
            }
            else
            {
                // Multiple matches. Complete as much as we can..
                // So inputting "C"+Tab will complete to "CL" then display "CLEAR" and "CLASSIFY" as matches.
                int match_len = (int)(word_end - word_start);
                for (;;)
                {
                    int c = 0;
                    bool all_candidates_matches = true;
                    for (int i = 0; i < candidates.Size && all_candidates_matches; i++)
                        if (i == 0)
                            c = toupper(candidates[i][match_len]);
                        else if (c == 0 || c != toupper(candidates[i][match_len]))
                            all_candidates_matches = false;
                    if (!all_candidates_matches)
                        break;
                    match_len++;
                }

                if (match_len > 0)
                {
                    data->DeleteChars((int)(word_start - data->Buf), (int)(word_end - word_start));
                    data->InsertChars(data->CursorPos, candidates[0], candidates[0] + match_len);
                }

                // List matches
                AddLog(ELogVerbosity::Info, "Possible matches:\n");
                for (int i = 0; i < candidates.Size; i++)
                    AddLog(ELogVerbosity::Info, "- %s\n", candidates[i]);
            }

            break;
        }
    case ImGuiInputTextFlags_CallbackHistory:
        {
            // Example of HISTORY
            const int prev_history_pos = HistoryPos;
            if (data->EventKey == ImGuiKey_UpArrow)
            {
                if (HistoryPos == -1)
                    HistoryPos = History.Num() - 1;
                else if (HistoryPos > 0)
                    HistoryPos--;
                }
            else if (data->EventKey == ImGuiKey_DownArrow)
            {
                if (HistoryPos != -1)
                    if (++HistoryPos >= History.Num())
                        HistoryPos = -1;
            }

            // A better implementation would preserve the data on the current input line along with cursor position.
            if (prev_history_pos != HistoryPos)
            {
                const char* history_str = (HistoryPos >= 0) ? History[HistoryPos].c_str() : "";
                data->DeleteChars(0, data->BufTextLen);
                data->InsertChars(0, history_str);
            }
        }
    }
    return 0;
}

void FConsolePanel::ExecCommand(const char* command_line)
{
    AddLog(ELogVerbosity::Input, "# %s\n", command_line);

    // Insert into history. First find match and delete it so it can be pushed to the back.
    // This isn't trying to be smart or optimal.
    HistoryPos = -1;
    
    for (int i = History.Num() - 1; i >= 0; i--)
        if (Stricmp(History[i].c_str(), command_line) == 0)
        {
            History.RemoveAt(i, 1);
            break;
        }
    History.Add(Strdup(command_line));

    // Process command
    if (Stricmp(command_line, "CLEAR") == 0)
    {
        ClearLog();
    }
    else if (Stricmp(command_line, "HELP") == 0)
    {
        AddLog(ELogVerbosity::Info, "Commands:");
        for (int i = 0; i < Commands.Num(); i++)
            AddLog(ELogVerbosity::Info, "- %s", Commands[i].c_str());
    }
    else if (Stricmp(command_line, "HISTORY") == 0)
    {
        int first = History.Num() - 10;
        for (int i = first > 0 ? first : 0; i < History.Num(); i++)
            AddLog(ELogVerbosity::Info, "%3d: %s\n", i, History[i].c_str());
    }
    else
    {
        AddLog(ELogVerbosity::Error ,"Unknown command: '%s'\n", command_line);
    }

    // On command input, we scroll to bottom even if AutoScroll==false
    ScrollToBottom = true;
}

void FConsolePanel::AddLog(ELogVerbosity Verbosity, const char* fmt, ...)
{
    ImVec4 Color;
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, IM_COUNTOF(buf), fmt, args);
    buf[IM_COUNTOF(buf)-1] = 0;
    va_end(args);
    switch (Verbosity)
    {
    case ELogVerbosity::Info:
        Color = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);
        break;
    case ELogVerbosity::Error:
        Color = ImVec4(1.0f, 1.0f, 0.0f, 1.0f);
        break;
    case ELogVerbosity::Warning:
        Color = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
        break;
    case ELogVerbosity::Input:
        Color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
        break;
    }
    Items.Add({Strdup(buf), Color});
}

void FConsolePanel::HandleLogOutput(void* UserData, ELogVerbosity Verbosity, const char* Message)
{
    FConsolePanel* Panel = static_cast<FConsolePanel*>(UserData);
    
    if (Panel != nullptr)
    {
        Panel->AddLog(Verbosity, "%s", Message);
    }
}
