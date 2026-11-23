#include "ViewportPanel.h"

#include "imgui.h"
#include "Core/Math/MathUtility.h"

#include <cmath>

namespace
{
    bool ProjectWorldToViewport(const FVector& WorldPosition, const FViewport& Viewport, const FMatrix& View, const FMatrix& Projection, const ImVec2& ImageMin, ImVec2& OutScreenPosition)
    {
        const FVector ScreenPosition = Viewport.Project(WorldPosition, Projection, View, FMatrix::Identity);

        if (!std::isfinite(ScreenPosition.x) || !std::isfinite(ScreenPosition.y) || !std::isfinite(ScreenPosition.z))
        {
            return false;
        }

        if (ScreenPosition.z < 0.0f || ScreenPosition.z > 1.0f)
        {
            return false;
        }

        OutScreenPosition = ImVec2(ImageMin.x + ScreenPosition.x, ImageMin.y + ScreenPosition.y);
        return true;
    }

    void DrawProjectedWorldLine(ImDrawList* DrawList, const FVector& Start, const FVector& End, ImU32 Color, const FViewport& Viewport, const FMatrix& View, const FMatrix& Projection, const ImVec2& ImageMin)
    {
        constexpr int32 SegmentCount = 24;

        FVector PreviousWorld = Start;
        ImVec2 PreviousScreen;
        bool bPreviousValid = ProjectWorldToViewport(PreviousWorld, Viewport, View, Projection, ImageMin, PreviousScreen);

        for (int32 Index = 1; Index <= SegmentCount; ++Index)
        {
            const float Alpha = static_cast<float>(Index) / static_cast<float>(SegmentCount);
            const FVector CurrentWorld = Start + (End - Start) * Alpha;

            ImVec2 CurrentScreen;
            const bool bCurrentValid = ProjectWorldToViewport(CurrentWorld, Viewport, View, Projection, ImageMin, CurrentScreen);

            if (bPreviousValid && bCurrentValid)
            {
                DrawList->AddLine(PreviousScreen, CurrentScreen, Color, 1.0f);
            }

            PreviousWorld = CurrentWorld;
            PreviousScreen = CurrentScreen;
            bPreviousValid = bCurrentValid;
        }
    }

    void DrawWorldGridAndAxes(ImDrawList* DrawList, const ImVec2& ImageMin, const ImVec2& ImageMax, const ImVec2& ImageSize, const FMatrix& View, const FMatrix& Projection)
    {
        if (DrawList == nullptr || ImageSize.x <= 0.0f || ImageSize.y <= 0.0f)
        {
            return;
        }

        const FViewport Viewport(0.0f, 0.0f, ImageSize.x, ImageSize.y, 0.0f, 1.0f);

        const ImU32 MinorGridColor = IM_COL32(150, 160, 175, 45);
        const ImU32 MajorGridColor = IM_COL32(170, 180, 195, 85);
        const ImU32 XAxisColor = IM_COL32(255, 70, 70, 220);
        const ImU32 YAxisColor = IM_COL32(70, 230, 100, 220);
        const ImU32 ZAxisColor = IM_COL32(80, 140, 255, 230);

        DrawList->PushClipRect(ImageMin, ImageMax, true);

        constexpr float GridExtent = 40.0f;
        constexpr float GridSpacing = 2.0f;
        constexpr int32 GridIndexLimit = 20;

        for (int32 Index = -GridIndexLimit; Index <= GridIndexLimit; ++Index)
        {
            if (Index == 0)
            {
                continue;
            }

            const float Offset = static_cast<float>(Index) * GridSpacing;
            const bool bMajorLine = Index % 5 == 0;
            const ImU32 GridColor = bMajorLine ? MajorGridColor : MinorGridColor;

            DrawProjectedWorldLine(DrawList, FVector(-GridExtent, Offset, 0.0f), FVector(GridExtent, Offset, 0.0f), GridColor, Viewport, View, Projection, ImageMin);
            DrawProjectedWorldLine(DrawList, FVector(Offset, -GridExtent, 0.0f), FVector(Offset, GridExtent, 0.0f), GridColor, Viewport, View, Projection, ImageMin);
        }

        DrawProjectedWorldLine(DrawList, FVector(-GridExtent, 0.0f, 0.0f), FVector(GridExtent, 0.0f, 0.0f), XAxisColor, Viewport, View, Projection, ImageMin);
        DrawProjectedWorldLine(DrawList, FVector(0.0f, -GridExtent, 0.0f), FVector(0.0f, GridExtent, 0.0f), YAxisColor, Viewport, View, Projection, ImageMin);
        DrawProjectedWorldLine(DrawList, FVector(0.0f, 0.0f, -10.0f), FVector(0.0f, 0.0f, 20.0f), ZAxisColor, Viewport, View, Projection, ImageMin);

        DrawList->PopClipRect();
    }
}

bool FViewportPanel::Init()
{
    return true;
}

void FViewportPanel::Tick(float DeltaTime)
{
}

void FViewportPanel::OnRender()
{
    bViewportHovered = false;

    if (!ImGui::Begin("Viewport"))
    {
        ImGui::End();
        return;
    }

    const ETransformGizmoMode CurrentMode = TransformGizmo.GetMode();

    if (ImGui::RadioButton("Translate (W)", CurrentMode == ETransformGizmoMode::Translate))
    {
        TransformGizmo.SetMode(ETransformGizmoMode::Translate);
    }

    ImGui::SameLine();

    if (ImGui::RadioButton("Rotate (E)", CurrentMode == ETransformGizmoMode::Rotate))
    {
        TransformGizmo.SetMode(ETransformGizmoMode::Rotate);
    }

    ImGui::SameLine();

    if (ImGui::RadioButton("Scale (R)", CurrentMode == ETransformGizmoMode::Scale))
    {
        TransformGizmo.SetMode(ETransformGizmoMode::Scale);
    }

    ImGui::SameLine();

    if (CurrentMode != ETransformGizmoMode::Scale)
    {
        const ETransformCoordinateSpace CurrentSpace = TransformGizmo.GetCoordinateSpace();

        if (ImGui::RadioButton("World", CurrentSpace == ETransformCoordinateSpace::World))
        {
            TransformGizmo.SetCoordinateSpace(ETransformCoordinateSpace::World);
        }

        ImGui::SameLine();

        if (ImGui::RadioButton("Local", CurrentSpace == ETransformCoordinateSpace::Local))
        {
            TransformGizmo.SetCoordinateSpace(ETransformCoordinateSpace::Local);
        }
    }
    else
    {
        ImGui::TextDisabled("Scale: Local");
    }
    
    bool bSnapEnabled = TransformGizmo.IsSnapEnabled();

    if (ImGui::Checkbox("Snap", &bSnapEnabled))
    {
        TransformGizmo.SetSnapEnabled(bSnapEnabled);
    }

    if (bSnapEnabled)
    {
        if (CurrentMode == ETransformGizmoMode::Translate)
        {
            float SnapValue = TransformGizmo.GetTranslationSnap();

            ImGui::SameLine();

            if (ImGui::DragFloat("Move Step", &SnapValue, 0.1f, 0.1f, 100.0f, "%.1f"))
            {
                TransformGizmo.SetTranslationSnap(SnapValue);
            }
        }
        else if (CurrentMode == ETransformGizmoMode::Rotate)
        {
            float SnapValue = TransformGizmo.GetRotationSnap();

            ImGui::SameLine();

            if (ImGui::DragFloat("Rotation Step", &SnapValue, 1.0f, 1.0f, 90.0f, "%.0f"))
            {
                TransformGizmo.SetRotationSnap(SnapValue);
            }
        }
        else if (CurrentMode == ETransformGizmoMode::Scale)
        {
            float SnapValue = TransformGizmo.GetScaleSnap();

            ImGui::SameLine();

            if (ImGui::DragFloat("Scale Step", &SnapValue, 0.01f, 0.01f, 10.0f, "%.2f"))
            {
                TransformGizmo.SetScaleSnap(SnapValue);
            }
        }
    }
    
    const ImVec2 AvailableSize = ImGui::GetContentRegionAvail();

    if (AvailableSize.x > 0.0f && AvailableSize.y > 0.0f)
    {
        const ImVec2 FramebufferScale = ImGui::GetIO().DisplayFramebufferScale;

        const float ScaleX = FramebufferScale.x > 0.0f ? FramebufferScale.x : 1.0f;
        const float ScaleY = FramebufferScale.y > 0.0f ? FramebufferScale.y : 1.0f;

        const uint32 Width = static_cast<uint32>(AvailableSize.x * ScaleX + 0.5f);
        const uint32 Height = static_cast<uint32>(AvailableSize.y * ScaleY + 0.5f);

        SetDesiredSize(Width, Height);

        if (SRV != nullptr)
        {
            ImGui::Image((ImTextureID)SRV, AvailableSize);

            const ImVec2 ImageMin = ImGui::GetItemRectMin();
            const ImVec2 ImageMax = ImGui::GetItemRectMax();

            const ImVec2 ImageSize(ImageMax.x - ImageMin.x, ImageMax.y - ImageMin.y);

            const bool bImageHovered = ImGui::IsItemHovered();
            bViewportHovered = bImageHovered;

            DrawWorldGridAndAxes(ImGui::GetWindowDrawList(), ImageMin, ImageMax, ImageSize, ViewMatrix, ProjectionMatrix);
            
            bool bGizmoConsumed = false;

            if (ObjectContext != nullptr)
            {
                ImDrawList* DrawList = ImGui::GetWindowDrawList();
                DrawList->PushClipRect(ImageMin, ImageMax, true);
                bGizmoConsumed = TransformGizmo.Draw(*ObjectContext, ViewMatrix, ProjectionMatrix, DrawList, ImageMin, ImageSize, bImageHovered);
                DrawList->PopClipRect();
            }

            if (bImageHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !bGizmoConsumed)
            {
                const ImVec2 MousePos = ImGui::GetIO().MousePos;

                if (ImageSize.x > 0.0f && ImageSize.y > 0.0f)
                {
                    ClickU = (MousePos.x - ImageMin.x) / ImageSize.x;
                    ClickV = (MousePos.y - ImageMin.y) / ImageSize.y;

                    bHasPendingClick = true;
                }
            }
        }
    }

    ImGui::End();
}

void FViewportPanel::SetDesiredSize(uint32 Width, uint32 Height)
{
    if (Width == 0 || Height == 0)
    {
        return;
    }
    if (DesiredWidth == Width && DesiredHeight == Height)
    {
        return;
    }
    DesiredWidth = Width;
    DesiredHeight = Height;
    bHasPendingSize = true;
}

bool FViewportPanel::ConsumeDesiredSize(uint32& OutWidth, uint32& OutHeight)
{
    if (!bHasPendingSize)
    {
        return false;
    }
    OutWidth = DesiredWidth;
    OutHeight = DesiredHeight;
    bHasPendingSize = false;
    return true;
}

bool FViewportPanel::ConsumeClick(float& OutU, float& OutV)
{
    if (!bHasPendingClick)
    {
        return false;
    }
    OutU = ClickU;
    OutV = ClickV;
    bHasPendingClick = false;
    return true;
}
