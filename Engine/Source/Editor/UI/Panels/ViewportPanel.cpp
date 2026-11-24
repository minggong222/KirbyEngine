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

    if (!IsOpen())
    {
        TransformGizmo.CancelDrag();
        bViewportHovered = false;
        return;
    }

    bool bIsOpen = IsOpen();

    if (!ImGui::Begin("Viewport", &bIsOpen))
    {
        TransformGizmo.CancelDrag();
        SetOpen(bIsOpen);
        ImGui::End();
        return;
    }

    
    if (EditorSettings != nullptr)
    {
        int32 ViewMode = static_cast<int32>(EditorSettings->ViewMode);
        const char* ViewModes[] = {"Lit", "Unlit", "Wireframe"};

        if (ViewMode < 0 || ViewMode >= IM_ARRAYSIZE(ViewModes))
        {
            ViewMode = 0;
        }

        if (ImGui::BeginCombo("##ViewMode", ViewModes[ViewMode]))
        {
            for (int32 Index = 0; Index < IM_ARRAYSIZE(ViewModes); ++Index)
            {
                const bool bSelected = ViewMode == Index;

                if (ImGui::Selectable(ViewModes[Index], bSelected))
                {
                    EditorSettings->ViewMode = static_cast<EViewMode>(Index);
                }

                if (bSelected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }

            ImGui::EndCombo();
        }

        ImGui::SameLine();

        if (ImGui::Button("Show"))
        {
            ImGui::OpenPopup("ViewportShowMenu");
        }

        if (ImGui::BeginPopup("ViewportShowMenu"))
        {
            bool bPrimitives = EditorSettings->ShowFlags.IsEnabled(EEngineShowFlag::Primitives);
            if (ImGui::Checkbox("Primitives", &bPrimitives)) EditorSettings->ShowFlags.SetEnabled(EEngineShowFlag::Primitives, bPrimitives);

            bool bGrid = EditorSettings->ShowFlags.IsEnabled(EEngineShowFlag::Grid);
            if (ImGui::Checkbox("Grid", &bGrid)) EditorSettings->ShowFlags.SetEnabled(EEngineShowFlag::Grid, bGrid);

            bool bAxes = EditorSettings->ShowFlags.IsEnabled(EEngineShowFlag::Axes);
            if (ImGui::Checkbox("Axes", &bAxes)) EditorSettings->ShowFlags.SetEnabled(EEngineShowFlag::Axes, bAxes);

            bool bBounds = EditorSettings->ShowFlags.IsEnabled(EEngineShowFlag::Bounds);
            if (ImGui::Checkbox("Bounds", &bBounds)) EditorSettings->ShowFlags.SetEnabled(EEngineShowFlag::Bounds, bBounds);

            bool bBillboardText = EditorSettings->ShowFlags.IsEnabled(EEngineShowFlag::BillboardText);
            if (ImGui::Checkbox("UUID Labels", &bBillboardText)) EditorSettings->ShowFlags.SetEnabled(EEngineShowFlag::BillboardText, bBillboardText);

            ImGui::Separator();

            if (ImGui::BeginMenu("Grid Settings"))
            {
                ImGui::DragFloat("Grid Extent", &EditorSettings->GridExtent, 1.0f, 1.0f, 10000.0f);
                ImGui::DragFloat("Grid Spacing", &EditorSettings->GridSpacing, 0.1f, 0.1f, 1000.0f);
                ImGui::DragInt("Major Interval", &EditorSettings->MajorGridInterval, 1.0f, 1, 100);
                ImGui::EndMenu();
            }

            ImGui::EndPopup();
        }

        ImGui::SameLine();

        if (ImGui::Button("Camera"))
        {
            ImGui::OpenPopup("ViewportCameraMenu");
        }

        if (ImGui::BeginPopup("ViewportCameraMenu"))
        {
            ImGui::Text("Camera Settings");
            ImGui::Separator();
            ImGui::SliderFloat("Mouse Sensitivity", &EditorSettings->CameraSensitivity, 0.001f, 0.2f, "%.3f");
            ImGui::EndPopup();
        }
    }

    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();

    ETransformGizmoMode CurrentMode = TransformGizmo.GetMode();

    if (ImGui::RadioButton("Move", CurrentMode == ETransformGizmoMode::Translate))
    {
        TransformGizmo.SetMode(ETransformGizmoMode::Translate);
    }

    ImGui::SameLine();

    if (ImGui::RadioButton("Rotate", CurrentMode == ETransformGizmoMode::Rotate))
    {
        TransformGizmo.SetMode(ETransformGizmoMode::Rotate);
    }

    ImGui::SameLine();

    if (ImGui::RadioButton("Scale", CurrentMode == ETransformGizmoMode::Scale))
    {
        TransformGizmo.SetMode(ETransformGizmoMode::Scale);
    }

    CurrentMode = TransformGizmo.GetMode();

    ImGui::SameLine();
    ImGui::TextDisabled("|");
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
        ImGui::TextDisabled("Local");
    }

    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();

    bool bSnapEnabled = TransformGizmo.IsSnapEnabled();

    if (ImGui::Checkbox("Snap", &bSnapEnabled))
    {
        TransformGizmo.SetSnapEnabled(bSnapEnabled);
    }

    if (bSnapEnabled)
    {
        ImGui::SameLine();

        if (CurrentMode == ETransformGizmoMode::Translate)
        {
            float SnapValue = TransformGizmo.GetTranslationSnap();

            if (ImGui::DragFloat("Move Step", &SnapValue, 0.1f, 0.1f, 100.0f, "%.1f"))
            {
                TransformGizmo.SetTranslationSnap(SnapValue);
            }
        }
        else if (CurrentMode == ETransformGizmoMode::Rotate)
        {
            float SnapValue = TransformGizmo.GetRotationSnap();

            if (ImGui::DragFloat("Rotation Step", &SnapValue, 1.0f, 1.0f, 90.0f, "%.0f"))
            {
                TransformGizmo.SetRotationSnap(SnapValue);
            }
        }
        else if (CurrentMode == ETransformGizmoMode::Scale)
        {
            float SnapValue = TransformGizmo.GetScaleSnap();

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

    if (!bIsOpen)
    {
        TransformGizmo.CancelDrag();
    }

    SetOpen(bIsOpen);
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
