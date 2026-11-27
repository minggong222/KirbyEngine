#pragma once

#include "imgui.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

// Editor theme.
//   - Kirby  : pink theme (default)
//   - Unreal : dark, flat, blue-accent editor look (Window > Theme)
// Panels ask this namespace for accent colors, so switching the style at runtime
// re-colors custom-drawn widgets (selection bars, badges, splitters) as well.
namespace KirbyEditorTheme
{
    enum class EStyle : uint8_t
    {
        Unreal,
        Kirby
    };

    inline EStyle& CurrentStyleRef()
    {
        static EStyle Style = EStyle::Kirby;
        return Style;
    }

    inline EStyle GetStyle()
    {
        return CurrentStyleRef();
    }

    inline bool IsKirby()
    {
        return CurrentStyleRef() == EStyle::Kirby;
    }

    // Rounding for custom-drawn shapes: Kirby keeps its soft corners, Unreal stays nearly square.
    inline float Rounding(float KirbyRounding)
    {
        return IsKirby() ? KirbyRounding : (std::min)(KirbyRounding, 2.0f);
    }

    // ============================================================
    // Accent colors (style-aware)
    // ============================================================

    inline ImVec4 PinkText()
    {
        return IsKirby() ? ImVec4(0.96f, 0.30f, 0.56f, 1.0f) : ImVec4(0.90f, 0.90f, 0.90f, 1.0f);
    }

    inline ImVec4 CreamText()
    {
        return IsKirby() ? ImVec4(1.0f, 0.96f, 0.98f, 1.0f) : ImVec4(0.55f, 0.55f, 0.55f, 1.0f);
    }

    inline ImVec4 PrimaryButton()
    {
        return IsKirby() ? ImVec4(0.94f, 0.43f, 0.65f, 1.0f) : ImVec4(0.22f, 0.22f, 0.22f, 1.0f);
    }

    inline ImVec4 PrimaryButtonHovered()
    {
        return IsKirby() ? ImVec4(1.0f, 0.53f, 0.73f, 1.0f) : ImVec4(0.34f, 0.34f, 0.34f, 1.0f);
    }

    inline ImVec4 PrimaryButtonActive()
    {
        return IsKirby() ? ImVec4(0.84f, 0.25f, 0.49f, 1.0f) : ImVec4(0.0f, 0.44f, 0.88f, 1.0f);
    }

    inline ImVec4 BlueButton()
    {
        return IsKirby() ? ImVec4(0.48f, 0.72f, 0.89f, 1.0f) : ImVec4(0.22f, 0.22f, 0.22f, 1.0f);
    }

    inline ImVec4 BlueButtonHovered()
    {
        return IsKirby() ? ImVec4(0.58f, 0.81f, 0.96f, 1.0f) : ImVec4(0.34f, 0.34f, 0.34f, 1.0f);
    }

    inline ImVec4 BlueButtonActive()
    {
        return IsKirby() ? ImVec4(0.34f, 0.61f, 0.81f, 1.0f) : ImVec4(0.0f, 0.44f, 0.88f, 1.0f);
    }

    // Selection accent (selected rows, tiles)
    inline ImU32 PinkU32()
    {
        return IsKirby() ? IM_COL32(255, 91, 153, 255) : IM_COL32(0, 112, 224, 255);
    }

    // Secondary accent (active splitter, sparkles)
    inline ImU32 GoldU32()
    {
        return IsKirby() ? IM_COL32(255, 211, 79, 255) : IM_COL32(242, 163, 31, 255);
    }

    inline ImU32 CreamU32()
    {
        return IsKirby() ? IM_COL32(255, 245, 249, 255) : IM_COL32(210, 210, 210, 255);
    }

    inline ImU32 BorderU32()
    {
        return IsKirby() ? IM_COL32(225, 139, 176, 255) : IM_COL32(58, 58, 58, 255);
    }

    inline ImU32 BadgeBackgroundU32()
    {
        return IsKirby() ? IM_COL32(111, 54, 83, 235) : IM_COL32(16, 16, 16, 215);
    }

    // Idle viewport splitter
    inline ImU32 SplitterU32()
    {
        return IsKirby() ? PinkU32() : IM_COL32(10, 10, 10, 255);
    }

    // ============================================================
    // Kirby Star
    // ============================================================


    inline void DrawStar(ImDrawList* DrawList, const ImVec2& Center, float Radius, ImU32 Color)
    {
        if (DrawList == nullptr)
        {
            return;
        }

        constexpr float Pi = 3.14159265358979323846f;
        ImVec2 Points[10];

        for (int32_t Index = 0; Index < 10; ++Index)
        {
            const float Angle = -Pi * 0.5f + Pi * static_cast<float>(Index) / 5.0f;
            const float CurrentRadius = Index % 2 == 0 ? Radius : Radius * 0.43f;

            Points[Index] = ImVec2(Center.x + std::cos(Angle) * CurrentRadius, Center.y + std::sin(Angle) * CurrentRadius);
        }

        DrawList->AddConvexPolyFilled(Points, 10, Color);
    }

    // Decorative only: drawn in the Kirby style, skipped in the Unreal style.
    inline void DrawSparkle(ImDrawList* DrawList, const ImVec2& Center, float Radius, ImU32 Color)
    {
        if (DrawList == nullptr || !IsKirby())
        {
            return;
        }

        const ImVec2 VerticalDiamond[4] =
        {
            ImVec2(Center.x, Center.y - Radius),
            ImVec2(Center.x + Radius * 0.18f, Center.y),
            ImVec2(Center.x, Center.y + Radius),
            ImVec2(Center.x - Radius * 0.18f, Center.y)
        };

        const ImVec2 HorizontalDiamond[4] =
        {
            ImVec2(Center.x - Radius, Center.y),
            ImVec2(Center.x, Center.y - Radius * 0.18f),
            ImVec2(Center.x + Radius, Center.y),
            ImVec2(Center.x, Center.y + Radius * 0.18f)
        };

        DrawList->AddConvexPolyFilled(VerticalDiamond, 4, Color);
        DrawList->AddConvexPolyFilled(HorizontalDiamond, 4, Color);
    }

    // ============================================================
    // Kirby Face Badge
    // ============================================================

    inline void DrawPuffBadge(ImDrawList* DrawList, const ImVec2& Center, float Radius)
    {
        if (DrawList == nullptr)
        {
            return;
        }

        const ImU32 FaceColor = IM_COL32(255, 151, 187, 255);
        const ImU32 FaceOutline = IM_COL32(224, 91, 145, 255);
        const ImU32 EyeColor = IM_COL32(48, 57, 105, 255);
        const ImU32 EyeBlue = IM_COL32(68, 139, 205, 255);
        const ImU32 BlushColor = IM_COL32(244, 91, 145, 255);
        const ImU32 HighlightColor = IM_COL32(255, 220, 234, 255);

        DrawList->AddCircleFilled(Center, Radius, FaceColor, 32);
        DrawList->AddCircle(Center, Radius, FaceOutline, 32, 1.2f);

        const ImVec2 LeftEye(Center.x - Radius * 0.27f, Center.y - Radius * 0.14f);
        const ImVec2 RightEye(Center.x + Radius * 0.27f, Center.y - Radius * 0.14f);

        DrawList->AddCircleFilled(LeftEye, Radius * 0.13f, EyeColor, 16);
        DrawList->AddCircleFilled(RightEye, Radius * 0.13f, EyeColor, 16);

        DrawList->AddCircleFilled(ImVec2(LeftEye.x, LeftEye.y + Radius * 0.045f), Radius * 0.055f, EyeBlue, 12);
        DrawList->AddCircleFilled(ImVec2(RightEye.x, RightEye.y + Radius * 0.045f), Radius * 0.055f, EyeBlue, 12);

        DrawList->AddCircleFilled(ImVec2(LeftEye.x - Radius * 0.025f, LeftEye.y - Radius * 0.04f), Radius * 0.035f, CreamU32(), 8);
        DrawList->AddCircleFilled(ImVec2(RightEye.x - Radius * 0.025f, RightEye.y - Radius * 0.04f), Radius * 0.035f, CreamU32(), 8);

        DrawList->AddCircleFilled(ImVec2(Center.x - Radius * 0.49f, Center.y + Radius * 0.20f), Radius * 0.105f, BlushColor, 16);
        DrawList->AddCircleFilled(ImVec2(Center.x + Radius * 0.49f, Center.y + Radius * 0.20f), Radius * 0.105f, BlushColor, 16);

        DrawList->PathArcTo(ImVec2(Center.x, Center.y + Radius * 0.13f), Radius * 0.19f, 0.15f * 3.14159265f, 0.85f * 3.14159265f, 12);
        DrawList->PathStroke(EyeColor, false, (std::max)(1.0f, Radius * 0.045f));

        DrawList->AddCircleFilled(ImVec2(Center.x - Radius * 0.30f, Center.y - Radius * 0.49f), Radius * 0.14f, HighlightColor, 12);

        DrawStar(DrawList, ImVec2(Center.x + Radius * 1.02f, Center.y - Radius * 0.84f), Radius * 0.43f, GoldU32());
    }

    // ============================================================
    // Global Kirby Editor Theme
    // ============================================================
    inline void ApplyKirby()
    {
        ImGui::StyleColorsLight();

        ImGuiStyle& Style = ImGui::GetStyle();

        // Compact layout
        Style.WindowPadding = ImVec2(10.0f, 8.0f);
        Style.FramePadding = ImVec2(6.0f, 3.0f);
        Style.ItemSpacing = ImVec2(7.0f, 5.0f);
        Style.ItemInnerSpacing = ImVec2(5.0f, 4.0f);
        Style.IndentSpacing = 18.0f;

        // Rounded UI
        Style.WindowRounding = 12.0f;
        Style.ChildRounding = 10.0f;
        Style.FrameRounding = 7.0f;
        Style.PopupRounding = 10.0f;
        Style.ScrollbarRounding = 10.0f;
        Style.GrabRounding = 7.0f;
        Style.TabRounding = 9.0f;

        // Borders
        Style.WindowBorderSize = 1.0f;
        Style.ChildBorderSize = 1.0f;
        Style.FrameBorderSize = 1.0f;
        Style.PopupBorderSize = 1.0f;
        Style.DockingSeparatorSize = 3.0f;

        // Compact controls
        Style.ScrollbarSize = 12.0f;
        Style.GrabMinSize = 12.0f;
        Style.ButtonTextAlign = ImVec2(0.5f, 0.5f);

        // Main text
        const ImVec4 Text(0.38f, 0.20f, 0.29f, 1.0f);
        const ImVec4 TextDisabled(0.67f, 0.43f, 0.54f, 1.0f);

        // Strawberry milk background
        const ImVec4 Background(0.96f, 0.77f, 0.86f, 1.0f);
        const ImVec4 Panel(1.0f, 0.88f, 0.93f, 1.0f);
        const ImVec4 PanelLight(1.0f, 0.94f, 0.97f, 1.0f);
        const ImVec4 Popup(1.0f, 0.91f, 0.96f, 1.0f);

        // Pink input fields
        const ImVec4 Input(0.96f, 0.81f, 0.88f, 1.0f);
        const ImVec4 InputHovered(1.0f, 0.73f, 0.84f, 1.0f);
        const ImVec4 InputActive(1.0f, 0.66f, 0.80f, 1.0f);

        // Kirby signature pink
        const ImVec4 Pink(0.97f, 0.33f, 0.59f, 1.0f);
        const ImVec4 PinkDark(0.83f, 0.23f, 0.47f, 1.0f);
        const ImVec4 PinkHover(1.0f, 0.49f, 0.70f, 1.0f);
        const ImVec4 PinkActive(0.78f, 0.19f, 0.43f, 1.0f);

        // Small accent colors
        const ImVec4 SkyBlue(0.35f, 0.67f, 0.89f, 1.0f);
        const ImVec4 Gold(1.0f, 0.79f, 0.24f, 1.0f);
        const ImVec4 Mint(0.39f, 0.78f, 0.61f, 1.0f);

        // Pink borders
        const ImVec4 Border(0.87f, 0.57f, 0.70f, 1.0f);
        const ImVec4 BorderLight(0.93f, 0.68f, 0.79f, 1.0f);

        ImVec4* Colors = Style.Colors;

        Colors[ImGuiCol_Text] = Text;
        Colors[ImGuiCol_TextDisabled] = TextDisabled;

        Colors[ImGuiCol_WindowBg] = Background;
        Colors[ImGuiCol_ChildBg] = Panel;
        Colors[ImGuiCol_PopupBg] = Popup;

        Colors[ImGuiCol_Border] = Border;
        Colors[ImGuiCol_BorderShadow] = ImVec4(0.45f, 0.20f, 0.31f, 0.08f);

        Colors[ImGuiCol_FrameBg] = Input;
        Colors[ImGuiCol_FrameBgHovered] = InputHovered;
        Colors[ImGuiCol_FrameBgActive] = InputActive;

        Colors[ImGuiCol_TitleBg] = ImVec4(0.94f, 0.67f, 0.79f, 1.0f);
        Colors[ImGuiCol_TitleBgActive] = ImVec4(1.0f, 0.56f, 0.73f, 1.0f);
        Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.91f, 0.68f, 0.80f, 1.0f);
        Colors[ImGuiCol_MenuBarBg] = ImVec4(0.96f, 0.68f, 0.80f, 1.0f);

        Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.94f, 0.75f, 0.84f, 1.0f);
        Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.88f, 0.52f, 0.69f, 1.0f);
        Colors[ImGuiCol_ScrollbarGrabHovered] = Pink;
        Colors[ImGuiCol_ScrollbarGrabActive] = PinkDark;

        Colors[ImGuiCol_CheckMark] = PinkDark;
        Colors[ImGuiCol_CheckboxSelectedBg] = ImVec4(1.0f, 0.68f, 0.81f, 1.0f);
        Colors[ImGuiCol_SliderGrab] = Pink;
        Colors[ImGuiCol_SliderGrabActive] = Gold;

        Colors[ImGuiCol_Button] = ImVec4(0.96f, 0.68f, 0.81f, 1.0f);
        Colors[ImGuiCol_ButtonHovered] = PinkHover;
        Colors[ImGuiCol_ButtonActive] = PinkActive;

        Colors[ImGuiCol_Header] = ImVec4(0.96f, 0.69f, 0.82f, 1.0f);
        Colors[ImGuiCol_HeaderHovered] = ImVec4(1.0f, 0.57f, 0.76f, 1.0f);
        Colors[ImGuiCol_HeaderActive] = Pink;

        Colors[ImGuiCol_Separator] = Border;
        Colors[ImGuiCol_SeparatorHovered] = Pink;
        Colors[ImGuiCol_SeparatorActive] = Gold;

        Colors[ImGuiCol_ResizeGrip] = ImVec4(0.91f, 0.59f, 0.74f, 0.55f);
        Colors[ImGuiCol_ResizeGripHovered] = Pink;
        Colors[ImGuiCol_ResizeGripActive] = Gold;

        Colors[ImGuiCol_InputTextCursor] = PinkDark;

        // Tabs
        Colors[ImGuiCol_Tab] = ImVec4(0.94f, 0.70f, 0.82f, 1.0f);
        Colors[ImGuiCol_TabHovered] = PinkHover;
        Colors[ImGuiCol_TabSelected] = ImVec4(1.0f, 0.87f, 0.93f, 1.0f);
        Colors[ImGuiCol_TabSelectedOverline] = Pink;
        Colors[ImGuiCol_TabDimmed] = ImVec4(0.91f, 0.67f, 0.79f, 1.0f);
        Colors[ImGuiCol_TabDimmedSelected] = ImVec4(0.98f, 0.81f, 0.89f, 1.0f);
        Colors[ImGuiCol_TabDimmedSelectedOverline] = SkyBlue;

        // Docking
        Colors[ImGuiCol_DockingPreview] = ImVec4(0.97f, 0.33f, 0.59f, 0.38f);
        Colors[ImGuiCol_DockingEmptyBg] = Background;

        // Tables
        Colors[ImGuiCol_TableHeaderBg] = ImVec4(0.94f, 0.69f, 0.82f, 1.0f);
        Colors[ImGuiCol_TableBorderStrong] = Border;
        Colors[ImGuiCol_TableBorderLight] = BorderLight;
        Colors[ImGuiCol_TableRowBg] = Panel;
        Colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.0f, 0.92f, 0.96f, 1.0f);

        // Interaction
        Colors[ImGuiCol_TextLink] = SkyBlue;
        Colors[ImGuiCol_TextSelectedBg] = ImVec4(0.97f, 0.33f, 0.59f, 0.30f);
        Colors[ImGuiCol_TreeLines] = BorderLight;

        Colors[ImGuiCol_DragDropTarget] = Gold;
        Colors[ImGuiCol_DragDropTargetBg] = ImVec4(1.0f, 0.80f, 0.24f, 0.20f);
        Colors[ImGuiCol_UnsavedMarker] = Pink;

        Colors[ImGuiCol_NavCursor] = Gold;
        Colors[ImGuiCol_NavWindowingHighlight] = SkyBlue;
        Colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.55f, 0.29f, 0.40f, 0.18f);
        Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.45f, 0.20f, 0.32f, 0.32f);

        // Graph colors
        Colors[ImGuiCol_PlotLines] = SkyBlue;
        Colors[ImGuiCol_PlotLinesHovered] = Pink;
        Colors[ImGuiCol_PlotHistogram] = Mint;
        Colors[ImGuiCol_PlotHistogramHovered] = Gold;
    }

    // ============================================================
    // Unreal-style Editor Theme
    // ============================================================

    inline void ApplyUnreal()
    {
        ImGui::StyleColorsDark();

        ImGuiStyle& Style = ImGui::GetStyle();

        // Dense, flat layout
        Style.WindowPadding = ImVec2(8.0f, 6.0f);
        Style.FramePadding = ImVec2(6.0f, 4.0f);
        Style.ItemSpacing = ImVec2(6.0f, 4.0f);
        Style.ItemInnerSpacing = ImVec2(4.0f, 4.0f);
        Style.CellPadding = ImVec2(4.0f, 2.0f);
        Style.IndentSpacing = 14.0f;

        // Nearly square corners
        Style.WindowRounding = 0.0f;
        Style.ChildRounding = 2.0f;
        Style.FrameRounding = 3.0f;
        Style.PopupRounding = 3.0f;
        Style.ScrollbarRounding = 6.0f;
        Style.GrabRounding = 2.0f;
        Style.TabRounding = 3.0f;

        // Thin dark borders
        Style.WindowBorderSize = 1.0f;
        Style.ChildBorderSize = 1.0f;
        Style.FrameBorderSize = 0.0f;
        Style.PopupBorderSize = 1.0f;
        Style.TabBorderSize = 0.0f;
        Style.TabBarBorderSize = 1.0f;
        Style.TabBarOverlineSize = 2.0f;
        Style.DockingSeparatorSize = 2.0f;

        Style.ScrollbarSize = 12.0f;
        Style.GrabMinSize = 10.0f;
        Style.ButtonTextAlign = ImVec2(0.5f, 0.5f);
        Style.WindowMenuButtonPosition = ImGuiDir_None;
        Style.SeparatorTextBorderSize = 1.0f;

        const ImVec4 Text(0.78f, 0.78f, 0.78f, 1.0f);
        const ImVec4 TextBright(0.95f, 0.95f, 0.95f, 1.0f);
        const ImVec4 TextDisabled(0.44f, 0.44f, 0.44f, 1.0f);

        // Background layers, darkest to lightest
        const ImVec4 Darkest(0.059f, 0.059f, 0.059f, 1.0f);   // #0F0F0F  input fields, separators
        const ImVec4 Dark(0.082f, 0.082f, 0.082f, 1.0f);      // #151515  menu bar, tab strip
        const ImVec4 Panel(0.141f, 0.141f, 0.141f, 1.0f);     // #242424  panel body
        const ImVec4 PanelAlt(0.102f, 0.102f, 0.102f, 1.0f);  // #1A1A1A  child regions
        const ImVec4 Raised(0.188f, 0.188f, 0.188f, 1.0f);    // #303030  headers
        const ImVec4 Control(0.220f, 0.220f, 0.220f, 1.0f);   // #383838  buttons
        const ImVec4 ControlHover(0.341f, 0.341f, 0.341f, 1.0f);

        const ImVec4 Accent(0.0f, 0.44f, 0.88f, 1.0f);        // selection blue
        const ImVec4 AccentHover(0.13f, 0.55f, 0.98f, 1.0f);
        const ImVec4 AccentDim(0.0f, 0.44f, 0.88f, 0.55f);
        const ImVec4 Orange(0.95f, 0.64f, 0.12f, 1.0f);

        const ImVec4 Border(0.035f, 0.035f, 0.035f, 1.0f);

        ImVec4* Colors = Style.Colors;

        Colors[ImGuiCol_Text] = Text;
        Colors[ImGuiCol_TextDisabled] = TextDisabled;

        Colors[ImGuiCol_WindowBg] = Panel;
        Colors[ImGuiCol_ChildBg] = PanelAlt;
        Colors[ImGuiCol_PopupBg] = ImVec4(0.11f, 0.11f, 0.11f, 0.98f);

        Colors[ImGuiCol_Border] = Border;
        Colors[ImGuiCol_BorderShadow] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);

        Colors[ImGuiCol_FrameBg] = Darkest;
        Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.12f, 0.12f, 0.12f, 1.0f);
        Colors[ImGuiCol_FrameBgActive] = ImVec4(0.15f, 0.15f, 0.15f, 1.0f);

        Colors[ImGuiCol_TitleBg] = Dark;
        Colors[ImGuiCol_TitleBgActive] = Dark;
        Colors[ImGuiCol_TitleBgCollapsed] = Dark;
        Colors[ImGuiCol_MenuBarBg] = Dark;

        Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
        Colors[ImGuiCol_ScrollbarGrab] = Control;
        Colors[ImGuiCol_ScrollbarGrabHovered] = ControlHover;
        Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.45f, 0.45f, 0.45f, 1.0f);

        Colors[ImGuiCol_CheckMark] = TextBright;
        Colors[ImGuiCol_CheckboxSelectedBg] = Accent;
        Colors[ImGuiCol_SliderGrab] = ControlHover;
        Colors[ImGuiCol_SliderGrabActive] = Accent;

        Colors[ImGuiCol_Button] = Control;
        Colors[ImGuiCol_ButtonHovered] = ControlHover;
        Colors[ImGuiCol_ButtonActive] = Accent;

        // Selectable rows / tree nodes / collapsing headers
        Colors[ImGuiCol_Header] = AccentDim;
        Colors[ImGuiCol_HeaderHovered] = ImVec4(0.26f, 0.26f, 0.26f, 1.0f);
        Colors[ImGuiCol_HeaderActive] = Accent;

        Colors[ImGuiCol_Separator] = Border;
        Colors[ImGuiCol_SeparatorHovered] = Accent;
        Colors[ImGuiCol_SeparatorActive] = AccentHover;

        Colors[ImGuiCol_ResizeGrip] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
        Colors[ImGuiCol_ResizeGripHovered] = AccentDim;
        Colors[ImGuiCol_ResizeGripActive] = Accent;

        Colors[ImGuiCol_InputTextCursor] = TextBright;

        // Tabs: dark strip, the selected tab merges with the panel body
        Colors[ImGuiCol_Tab] = Dark;
        Colors[ImGuiCol_TabHovered] = Raised;
        Colors[ImGuiCol_TabSelected] = Panel;
        Colors[ImGuiCol_TabSelectedOverline] = Accent;
        Colors[ImGuiCol_TabDimmed] = Dark;
        Colors[ImGuiCol_TabDimmedSelected] = Panel;
        Colors[ImGuiCol_TabDimmedSelectedOverline] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);

        // Docking
        Colors[ImGuiCol_DockingPreview] = ImVec4(0.0f, 0.44f, 0.88f, 0.35f);
        Colors[ImGuiCol_DockingEmptyBg] = Darkest;

        // Tables
        Colors[ImGuiCol_TableHeaderBg] = Raised;
        Colors[ImGuiCol_TableBorderStrong] = Border;
        Colors[ImGuiCol_TableBorderLight] = ImVec4(0.09f, 0.09f, 0.09f, 1.0f);
        Colors[ImGuiCol_TableRowBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
        Colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.0f, 1.0f, 1.0f, 0.02f);

        // Interaction
        Colors[ImGuiCol_TextLink] = AccentHover;
        Colors[ImGuiCol_TextSelectedBg] = ImVec4(0.0f, 0.44f, 0.88f, 0.40f);
        Colors[ImGuiCol_TreeLines] = ImVec4(0.25f, 0.25f, 0.25f, 1.0f);

        Colors[ImGuiCol_DragDropTarget] = Orange;
        Colors[ImGuiCol_DragDropTargetBg] = ImVec4(0.95f, 0.64f, 0.12f, 0.15f);
        Colors[ImGuiCol_UnsavedMarker] = TextBright;

        Colors[ImGuiCol_NavCursor] = Accent;
        Colors[ImGuiCol_NavWindowingHighlight] = Accent;
        Colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.35f);
        Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.55f);

        // Graphs
        Colors[ImGuiCol_PlotLines] = AccentHover;
        Colors[ImGuiCol_PlotLinesHovered] = Orange;
        Colors[ImGuiCol_PlotHistogram] = AccentHover;
        Colors[ImGuiCol_PlotHistogramHovered] = Orange;
    }

    // ============================================================
    // Entry point
    // ============================================================

    inline void Apply(EStyle NewStyle)
    {
        CurrentStyleRef() = NewStyle;

        if (NewStyle == EStyle::Kirby)
        {
            ApplyKirby();
        }
        else
        {
            ApplyUnreal();
        }

        ImGui::GetStyle().WindowMinSize = ImVec2(230.0f, 180.0f);
    }

    inline void Apply()
    {
        Apply(CurrentStyleRef());
    }
}
