#include "ViewportPanel.h"

#include "imgui.h"
#include "EditorSettings.h"
#include "Core/Math/MathUtility.h"

#include <cmath>

bool FViewportPanel::Init()
{
    return true;
}

void FViewportPanel::SetEditorSettings(FEditorSettings* InSettings)
{
    EditorSettings = InSettings;

    if (EditorSettings != nullptr)
    {
        TransformGizmo.SetSnapEnabled(EditorSettings->bSnapEnabled);
        TransformGizmo.SetTranslationSnap(EditorSettings->TranslationSnap);
        TransformGizmo.SetRotationSnap(EditorSettings->RotationSnap);
        TransformGizmo.SetScaleSnap(EditorSettings->ScaleSnap);
    }
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

    if (ImGui::Button("View Options"))
    {
        ImGui::OpenPopup("ViewportOptions");
    }

    if (ImGui::BeginPopup("ViewportOptions"))
    {
        if (EditorSettings != nullptr)
        {
            int32 ViewMode = static_cast<int32>(EditorSettings->ViewMode);
            const char* ViewModes[] = {"Lit", "Wireframe"};

            if (ImGui::Combo("View Mode", &ViewMode, ViewModes, IM_ARRAYSIZE(ViewModes)))
            {
                EditorSettings->ViewMode = static_cast<EViewMode>(ViewMode);
            }

            ImGui::Separator();
            ImGui::Text("Show Flags");

            ImGui::Checkbox("Static Meshes", &EditorSettings->ShowFlags.StaticMeshes);
            ImGui::Checkbox("Grid", &EditorSettings->ShowFlags.Grid);
            ImGui::Checkbox("Axes", &EditorSettings->ShowFlags.Axes);
            ImGui::Checkbox("Bounds", &EditorSettings->ShowFlags.Bounds);
            ImGui::Checkbox("Billboards", &EditorSettings->ShowFlags.Billboards);
            ImGui::Checkbox("Actor Names", &EditorSettings->ShowFlags.ActorNames);

            ImGui::Separator();
            ImGui::Text("Grid Settings");

            ImGui::DragFloat("Grid Extent", &EditorSettings->GridExtent, 1.0f, 1.0f, 10000.0f);
            ImGui::DragFloat("Grid Spacing", &EditorSettings->GridSpacing, 0.1f, 0.1f, 1000.0f);
            ImGui::DragInt("Major Interval", &EditorSettings->MajorGridInterval, 1.0f, 1, 100);
        }

        ImGui::EndPopup();
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

    if (EditorSettings != nullptr)
    {
        EditorSettings->bSnapEnabled = TransformGizmo.IsSnapEnabled();
        EditorSettings->TranslationSnap = TransformGizmo.GetTranslationSnap();
        EditorSettings->RotationSnap = TransformGizmo.GetRotationSnap();
        EditorSettings->ScaleSnap = TransformGizmo.GetScaleSnap();
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
