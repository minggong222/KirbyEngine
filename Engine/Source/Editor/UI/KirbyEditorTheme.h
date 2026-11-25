
#pragma once

#include "imgui.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace KirbyEditorTheme
{
    // ============================================================
    // Kirby Pink Palette
    // ============================================================

    inline ImVec4 PinkText()
    {
        return ImVec4(0.96f, 0.30f, 0.56f, 1.0f);
    }

    inline ImVec4 CreamText()
    {
        return ImVec4(1.0f, 0.96f, 0.98f, 1.0f);
    }

    inline ImVec4 PrimaryButton()
    {
        return ImVec4(0.94f, 0.43f, 0.65f, 1.0f);
    }

    inline ImVec4 PrimaryButtonHovered()
    {
        return ImVec4(1.0f, 0.53f, 0.73f, 1.0f);
    }

    inline ImVec4 PrimaryButtonActive()
    {
        return ImVec4(0.84f, 0.25f, 0.49f, 1.0f);
    }

    inline ImVec4 BlueButton()
    {
        return ImVec4(0.48f, 0.72f, 0.89f, 1.0f);
    }

    inline ImVec4 BlueButtonHovered()
    {
        return ImVec4(0.58f, 0.81f, 0.96f, 1.0f);
    }

    inline ImVec4 BlueButtonActive()
    {
        return ImVec4(0.34f, 0.61f, 0.81f, 1.0f);
    }

    inline ImU32 PinkU32()
    {
        return IM_COL32(255, 91, 153, 255);
    }

    inline ImU32 GoldU32()
    {
        return IM_COL32(255, 211, 79, 255);
    }

    inline ImU32 CreamU32()
    {
        return IM_COL32(255, 245, 249, 255);
    }

    inline ImU32 BorderU32()
    {
        return IM_COL32(225, 139, 176, 255);
    }

    inline ImU32 BadgeBackgroundU32()
    {
        return IM_COL32(111, 54, 83, 235);
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

    inline void DrawSparkle(ImDrawList* DrawList, const ImVec2& Center, float Radius, ImU32 Color)
    {
        if (DrawList == nullptr)
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

    inline void Apply()
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
}
