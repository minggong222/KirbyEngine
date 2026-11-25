#include "ViewportPanel.h"

#include "imgui.h"

#include "EditorSettings.h"
#include "EditorUndoManager.h"
#include "TextureAssetImporter.h"

#include "Core/Math/MathUtility.h"

#include "Resources/Asset.h"
#include "Resources/AssetManager.h"
#include "Resources/StaticMesh.h"
#include "Resources/Material.h"
#include "Resources/Texture2D.h"

#include "Actors/StaticMeshActor.h"

#include "Components/ActorComponent.h"
#include "Components/StaticMeshComponent.h"

#include "World/World.h"
#include "World/Level.h"

#include <algorithm>
#include <cmath>

namespace
{
    UStaticMeshComponent* GetSelectedStaticMeshComponent(FEditorObjectContext* Context)
    {
        if (Context == nullptr)
        {
            return nullptr;
        }

        if (UStaticMeshComponent* SelectedComponent = Cast<UStaticMeshComponent>(Context->SelectedComponent))
        {
            return SelectedComponent;
        }

        if (AStaticMeshActor* SelectedMeshActor = Cast<AStaticMeshActor>(Context->SelectedActor))
        {
            return SelectedMeshActor->GetStaticMeshComponent();
        }

        if (Context->SelectedActor != nullptr)
        {
            for (UActorComponent* Component : Context->SelectedActor->GetComponents())
            {
                if (UStaticMeshComponent* MeshComponent = Cast<UStaticMeshComponent>(Component))
                {
                    return MeshComponent;
                }
            }
        }

        return nullptr;
    }

    void ApplyContentBrowserDrop(FEditorObjectContext* Context, const ImGuiPayload* Payload, FAssetManager* AssetManager, URenderer* Renderer)
    {
        if (Context == nullptr || Context->World == nullptr || Payload == nullptr || Payload->Data == nullptr || Payload->DataSize != sizeof(UAsset*))
        {
            return;
        }

        UAsset* DroppedAsset = *static_cast<UAsset**>(Payload->Data);

        if (DroppedAsset == nullptr)
        {
            return;
        }

        if (UStaticMesh* DroppedMesh = Cast<UStaticMesh>(DroppedAsset))
        {
            if (UStaticMeshComponent* TargetComponent = GetSelectedStaticMeshComponent(Context))
            {
                UStaticMesh* PreviousMesh = TargetComponent->GetStaticMesh();
                const TArray<UMaterial*> PreviousOverrides = TargetComponent->GetMaterialOverrides();
                const TArray<uint8> PreviousOverrideMask = TargetComponent->GetMaterialOverrideMask();

                TargetComponent->SetStaticMesh(DroppedMesh);

                if (Context->UndoManager != nullptr)
                {
                    Context->UndoManager->RecordStaticMeshChange(*Context, TargetComponent, PreviousMesh, DroppedMesh, PreviousOverrides, PreviousOverrideMask, TargetComponent->GetMaterialOverrides(), TargetComponent->GetMaterialOverrideMask());
                }

                Context->SelectComponent(TargetComponent);
                return;
            }

            ULevel* Level = Context->World->GetPersistentLevel();

            FTransform Transform;
            Transform.Location.y = Level != nullptr ? static_cast<float>(Level->GetActors().Num()) * 1.5f : 0.0f;

            const uint32 PreviousSelectedUUID = Context->GetSelectedObject() != nullptr ? Context->GetSelectedObject()->GetUUID() : 0;
            AStaticMeshActor* NewActor = Context->World->SpawnActor<AStaticMeshActor>(FName("StaticMeshActor"), Transform);

            if (NewActor == nullptr || NewActor->GetStaticMeshComponent() == nullptr)
            {
                if (NewActor != nullptr)
                {
                    Context->World->DestroyActor(NewActor);
                }

                return;
            }

            NewActor->GetStaticMeshComponent()->SetStaticMesh(DroppedMesh);
            Context->SelectActor(NewActor);

            if (Context->UndoManager != nullptr)
            {
                Context->UndoManager->RecordCreate(*Context, NewActor, PreviousSelectedUUID);
            }

            return;
        }

        UMaterial* DroppedMaterial = Cast<UMaterial>(DroppedAsset);

        if (UTexture2D* DroppedTexture = Cast<UTexture2D>(DroppedAsset))
        {
            if (AssetManager == nullptr || Renderer == nullptr)
            {
                return;
            }

            DroppedMaterial = TextureImporter::LoadTextureMaterial(DroppedTexture->GetAssetPath(), *AssetManager, *Renderer);
        }

        if (DroppedMaterial != nullptr)
        {
            UStaticMeshComponent* TargetComponent = GetSelectedStaticMeshComponent(Context);

            if (TargetComponent == nullptr)
            {
                return;
            }

            constexpr int32 MaterialIndex = 0;

            UMaterial* PreviousMaterial = TargetComponent->GetMaterial(MaterialIndex);
            const bool bPreviousOverride = TargetComponent->HasMaterialOverride(MaterialIndex);

            TargetComponent->SetMaterial(MaterialIndex, DroppedMaterial);

            if (Context->UndoManager != nullptr)
            {
                Context->UndoManager->RecordMaterialChange(*Context, TargetComponent, MaterialIndex, PreviousMaterial, bPreviousOverride, DroppedMaterial, true);
            }

            Context->SelectComponent(TargetComponent);
        }
    }

    void BuildPaneRects(const ImVec2& ImageMin, const ImVec2& ImageSize, bool bFourViewports, float SplitX, float SplitY, ImVec2 OutMins[4], ImVec2 OutSizes[4])
    {
        if (!bFourViewports)
        {
            OutMins[0] = ImageMin;
            OutSizes[0] = ImageSize;

            for (int32 Index = 1; Index < 4; ++Index)
            {
                OutMins[Index] = ImageMin;
                OutSizes[Index] = ImVec2(0.0f, 0.0f);
            }

            return;
        }

        const float LeftWidth = ImageSize.x * SplitX;
        const float RightWidth = ImageSize.x - LeftWidth;
        const float TopHeight = ImageSize.y * SplitY;
        const float BottomHeight = ImageSize.y - TopHeight;

        OutMins[0] = ImageMin;
        OutSizes[0] = ImVec2(LeftWidth, TopHeight);

        OutMins[1] = ImVec2(ImageMin.x + LeftWidth, ImageMin.y);
        OutSizes[1] = ImVec2(RightWidth, TopHeight);

        OutMins[2] = ImVec2(ImageMin.x, ImageMin.y + TopHeight);
        OutSizes[2] = ImVec2(LeftWidth, BottomHeight);

        OutMins[3] = ImVec2(ImageMin.x + LeftWidth, ImageMin.y + TopHeight);
        OutSizes[3] = ImVec2(RightWidth, BottomHeight);
    }

    int32 GetPaneIndexFromMouse(const ImVec2& MousePosition, const ImVec2& ImageMin, const ImVec2& ImageSize, float SplitX, float SplitY)
    {
        const float RelativeX = MousePosition.x - ImageMin.x;
        const float RelativeY = MousePosition.y - ImageMin.y;

        const bool bRight = RelativeX >= ImageSize.x * SplitX;
        const bool bBottom = RelativeY >= ImageSize.y * SplitY;

        if (!bBottom)
        {
            return bRight ? 1 : 0;
        }

        return bRight ? 3 : 2;
    }

    const char* GetViewportTypeName(EEditorViewportType Type)
    {
        switch (Type)
        {
        case EEditorViewportType::Perspective:
            return "Perspective";

        case EEditorViewportType::Front:
            return "Front";

        case EEditorViewportType::Right:
            return "Right";

        case EEditorViewportType::Top:
            return "Top";

        default:
            return "Unknown";
        }
    }
}

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
        bDraggingVerticalSplitter = false;
        bDraggingHorizontalSplitter = false;
        return;
    }

    bool bIsOpen = IsOpen();

    if (!ImGui::Begin("Viewport", &bIsOpen))
    {
        TransformGizmo.CancelDrag();
        bDraggingVerticalSplitter = false;
        bDraggingHorizontalSplitter = false;
        SetOpen(bIsOpen);
        ImGui::End();
        return;
    }

    const bool bFourViewports = EditorSettings != nullptr && EditorSettings->bFourViewports;

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

        if (ImGui::GetContentRegionAvail().x > 140.0f) ImGui::SameLine();

        if (ImGui::Button("Show"))
        {
            ImGui::OpenPopup("ViewportShowMenu");
        }

        if (ImGui::BeginPopup("ViewportShowMenu"))
        {
            bool bPrimitives = EditorSettings->ShowFlags.IsEnabled(EEngineShowFlag::Primitives);

            if (ImGui::Checkbox("Primitives", &bPrimitives))
            {
                EditorSettings->ShowFlags.SetEnabled(EEngineShowFlag::Primitives, bPrimitives);
            }

            bool bGrid = EditorSettings->ShowFlags.IsEnabled(EEngineShowFlag::Grid);

            if (ImGui::Checkbox("Grid", &bGrid))
            {
                EditorSettings->ShowFlags.SetEnabled(EEngineShowFlag::Grid, bGrid);
            }

            bool bAxes = EditorSettings->ShowFlags.IsEnabled(EEngineShowFlag::Axes);

            if (ImGui::Checkbox("Axes", &bAxes))
            {
                EditorSettings->ShowFlags.SetEnabled(EEngineShowFlag::Axes, bAxes);
            }

            bool bBounds = EditorSettings->ShowFlags.IsEnabled(EEngineShowFlag::Bounds);

            if (ImGui::Checkbox("Bounds", &bBounds))
            {
                EditorSettings->ShowFlags.SetEnabled(EEngineShowFlag::Bounds, bBounds);
            }

            bool bBillboardText = EditorSettings->ShowFlags.IsEnabled(EEngineShowFlag::BillboardText);

            if (ImGui::Checkbox("UUID Labels", &bBillboardText))
            {
                EditorSettings->ShowFlags.SetEnabled(EEngineShowFlag::BillboardText, bBillboardText);
            }

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

        if (ImGui::GetContentRegionAvail().x > 110.0f) ImGui::SameLine();

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

        if (ImGui::GetContentRegionAvail().x > 110.0f) ImGui::SameLine();

        ImGui::Checkbox("Four Views", &EditorSettings->bFourViewports);
    }

    ImGui::Separator();

    ETransformGizmoMode CurrentMode = TransformGizmo.GetMode();

    if (ImGui::RadioButton("Move", CurrentMode == ETransformGizmoMode::Translate))
    {
        TransformGizmo.SetMode(ETransformGizmoMode::Translate);
    }
    if (ImGui::GetContentRegionAvail().x > 90.0f) ImGui::SameLine();

    if (ImGui::RadioButton("Rotate", CurrentMode == ETransformGizmoMode::Rotate))
    {
        TransformGizmo.SetMode(ETransformGizmoMode::Rotate);
    }
    if (ImGui::GetContentRegionAvail().x > 90.0f) ImGui::SameLine();

    if (ImGui::RadioButton("Scale", CurrentMode == ETransformGizmoMode::Scale))
    {
        TransformGizmo.SetMode(ETransformGizmoMode::Scale);
    }
    if (ImGui::GetContentRegionAvail().x > 90.0f) ImGui::SameLine();

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

        if (ImGui::GetContentRegionAvail().x > 80.0f) ImGui::SameLine();

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
        ImGui::Separator();
        ImGui::SetNextItemWidth(-1.0f);

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
            const ImVec2 MousePosition = ImGui::GetIO().MousePos;

            float SplitX = EditorSettings != nullptr ? EditorSettings->ViewportSplitX : 0.5f;
            float SplitY = EditorSettings != nullptr ? EditorSettings->ViewportSplitY : 0.5f;

            SplitX = std::isfinite(SplitX) ? std::clamp(SplitX, 0.2f, 0.8f) : 0.5f;
            SplitY = std::isfinite(SplitY) ? std::clamp(SplitY, 0.2f, 0.8f) : 0.5f;

            const float SplitterX = ImageMin.x + ImageSize.x * SplitX;
            const float SplitterY = ImageMin.y + ImageSize.y * SplitY;

            bool bClickedSplitter = false;

            if (bFourViewports && bImageHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            {
                const bool bNearVerticalSplitter = std::fabs(MousePosition.x - SplitterX) <= 5.0f && MousePosition.y >= ImageMin.y && MousePosition.y <= ImageMax.y;
                const bool bNearHorizontalSplitter = std::fabs(MousePosition.y - SplitterY) <= 5.0f && MousePosition.x >= ImageMin.x && MousePosition.x <= ImageMax.x;

                if (bNearVerticalSplitter || bNearHorizontalSplitter)
                {
                    if (bNearVerticalSplitter && (!bNearHorizontalSplitter || std::fabs(MousePosition.x - SplitterX) <= std::fabs(MousePosition.y - SplitterY)))
                    {
                        bDraggingVerticalSplitter = true;
                        bDraggingHorizontalSplitter = false;
                    }
                    else
                    {
                        bDraggingHorizontalSplitter = true;
                        bDraggingVerticalSplitter = false;
                    }

                    bClickedSplitter = true;
                }
            }

            if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
            {
                bDraggingVerticalSplitter = false;
                bDraggingHorizontalSplitter = false;
            }

            if (EditorSettings != nullptr && bFourViewports)
            {
                if (bDraggingVerticalSplitter)
                {
                    EditorSettings->ViewportSplitX = std::clamp((MousePosition.x - ImageMin.x) / ImageSize.x, 0.2f, 0.8f);
                    SplitX = EditorSettings->ViewportSplitX;
                }

                if (bDraggingHorizontalSplitter)
                {
                    EditorSettings->ViewportSplitY = std::clamp((MousePosition.y - ImageMin.y) / ImageSize.y, 0.2f, 0.8f);
                    SplitY = EditorSettings->ViewportSplitY;
                }
            }

            ImVec2 PaneMins[4];
            ImVec2 PaneSizes[4];

            BuildPaneRects(ImageMin, ImageSize, bFourViewports, SplitX, SplitY, PaneMins, PaneSizes);

            if (bImageHovered)
            {
                ActiveViewportIndex = bFourViewports ? GetPaneIndexFromMouse(MousePosition, ImageMin, ImageSize, SplitX, SplitY) : 0;
            }

            ActiveViewportIndex = std::clamp(ActiveViewportIndex, 0, bFourViewports ? 3 : 0);

            const ImVec2 ActivePaneMin = PaneMins[ActiveViewportIndex];
            const ImVec2 ActivePaneSize = PaneSizes[ActiveViewportIndex];

            const bool bActivePaneHovered =
                bImageHovered &&
                MousePosition.x >= ActivePaneMin.x &&
                MousePosition.x <= ActivePaneMin.x + ActivePaneSize.x &&
                MousePosition.y >= ActivePaneMin.y &&
                MousePosition.y <= ActivePaneMin.y + ActivePaneSize.y;

            bViewportHovered = bImageHovered || bDraggingVerticalSplitter || bDraggingHorizontalSplitter;

            ImDrawList* DrawList = ImGui::GetWindowDrawList();

            if (bFourViewports)
            {
                const ImU32 SplitterColor = IM_COL32(110, 110, 110, 255);
                const ImU32 SplitterActiveColor = IM_COL32(230, 180, 70, 255);

                const ImU32 VerticalColor = bDraggingVerticalSplitter ? SplitterActiveColor : SplitterColor;
                const ImU32 HorizontalColor = bDraggingHorizontalSplitter ? SplitterActiveColor : SplitterColor;

                DrawList->AddRectFilled(ImVec2(SplitterX - 2.0f, ImageMin.y), ImVec2(SplitterX + 2.0f, ImageMax.y), VerticalColor);
                DrawList->AddRectFilled(ImVec2(ImageMin.x, SplitterY - 2.0f), ImVec2(ImageMax.x, SplitterY + 2.0f), HorizontalColor);
            }

            if (Viewports != nullptr)
            {
                const char* ActivePaneName = GetViewportTypeName(Viewports[ActiveViewportIndex].Type);

                DrawList->AddText(ImVec2(ActivePaneMin.x + 8.0f, ActivePaneMin.y + 8.0f), IM_COL32(255, 255, 255, 220), ActivePaneName);
            }

            if (bActivePaneHovered && !bClickedSplitter && !bDraggingVerticalSplitter && !bDraggingHorizontalSplitter && ObjectContext != nullptr && Viewports != nullptr)
            {
                DrawList->PushClipRect(ActivePaneMin, ImVec2(ActivePaneMin.x + ActivePaneSize.x, ActivePaneMin.y + ActivePaneSize.y), true);

                const FEditorViewportPane& ActiveViewport = Viewports[ActiveViewportIndex];

                const bool bGizmoConsumed = TransformGizmo.Draw(*ObjectContext, ActiveViewport.View, ActiveViewport.Projection, DrawList, ActivePaneMin, ActivePaneSize, true);

                DrawList->PopClipRect();

                if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !bGizmoConsumed)
                {
                    ClickU = (MousePosition.x - ActivePaneMin.x) / ActivePaneSize.x;
                    ClickV = (MousePosition.y - ActivePaneMin.y) / ActivePaneSize.y;
                    ClickPaneIndex = ActiveViewportIndex;
                    bHasPendingClick = true;
                }
            }

            if (ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload* Payload = ImGui::AcceptDragDropPayload("KIRBY_ASSET"))
                {
                    ApplyContentBrowserDrop(ObjectContext, Payload, AssetManager, Renderer);
                }

                ImGui::EndDragDropTarget();
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

bool FViewportPanel::ConsumeClick(float& OutU, float& OutV, int32& OutPaneIndex)
{
    if (!bHasPendingClick)
    {
        return false;
    }

    OutU = ClickU;
    OutV = ClickV;
    OutPaneIndex = ClickPaneIndex;
    bHasPendingClick = false;

    return true;
}