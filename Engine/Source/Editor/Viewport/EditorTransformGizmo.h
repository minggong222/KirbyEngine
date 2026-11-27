#pragma once

#include "Core/Math/Math.h"
#include "Core/Math/Transform.h"
#include "EditorObjectContext.h"
#include "imgui.h"
#include <cmath>

class USceneComponent;

enum class ETransformGizmoMode
{
    Translate,
    Rotate,
    Scale
};

enum class ETransformCoordinateSpace
{
    World,
    Local
};

enum class EGizmoHandle
{
    None,
    X,
    Y,
    Z,
    XY,
    XZ,
    YZ,
    XYZ
};

class FEditorTransformGizmo
{
public:
    bool Draw(FEditorObjectContext& Context, const FMatrix& View, const FMatrix& Projection, ImDrawList* DrawList, const ImVec2& ImageMin, const ImVec2& ImageSize, bool bImageHovered);
    
    void CancelDrag();
    
    void SetMode(ETransformGizmoMode InMode)
    {
        if (CurrentMode != InMode)
        {
            CancelDrag();
            CurrentMode = InMode;
        }
    }

    ETransformGizmoMode GetMode() const
    {
        return CurrentMode;
    }

    void SetCoordinateSpace(ETransformCoordinateSpace InSpace)
    {
        CoordinateSpace = InSpace;
    }

    ETransformCoordinateSpace GetCoordinateSpace() const
    {
        return CoordinateSpace;
    }
    
    void SetSnapEnabled(bool bEnabled) { bSnapEnabled = bEnabled; }
    bool IsSnapEnabled() const { return bSnapEnabled; }

    void SetTranslationSnap(float Value) { if (std::isfinite(Value) && Value > 0.0f) TranslationSnap = Value; }
    void SetRotationSnap(float Value) { if (std::isfinite(Value) && Value > 0.0f) RotationSnap = Value; }
    void SetScaleSnap(float Value) { if (std::isfinite(Value) && Value > 0.0f) ScaleSnap = Value; }

    float GetTranslationSnap() const { return TranslationSnap; }
    float GetRotationSnap() const { return RotationSnap; }
    float GetScaleSnap() const { return ScaleSnap; }
private:
    bool BeginDrag(USceneComponent* Component, EGizmoHandle Handle, const FTransform& Transform, const FVector Axes[3],
        const ImVec2& MousePosition, const ImVec2& ImageMin, const ImVec2& ImageSize, const FMatrix& View, const FMatrix& Projection, float GizmoWorldSize);
    void UpdateDrag(const ImVec2& MousePosition, const ImVec2& ImageMin, const ImVec2& ImageSize, const FMatrix& View, const FMatrix& Projection);
    void EndDrag();

    ETransformGizmoMode CurrentMode = ETransformGizmoMode::Translate;
    ETransformCoordinateSpace CoordinateSpace = ETransformCoordinateSpace::World;
    ETransformGizmoMode DragMode = ETransformGizmoMode::Translate;
    ETransformCoordinateSpace DragSpace = ETransformCoordinateSpace::World;
    bool bIsDragging = false;
    EGizmoHandle DragHandle = EGizmoHandle::None;
    USceneComponent* DraggedComponent = nullptr;
    FTransform DragStartTransform;
    FVector DragPivot = FVector::Zero;
    FVector DragAxisWorld = FVector::Zero;
    FVector DragRotationAxis = FVector::Zero;
    FVector DragPlaneNormal = FVector::Zero;
    FVector DragStartPlaneHit = FVector::Zero;
    FVector PreviousRotationVector = FVector::Zero;
    FVector DragRingU = FVector::Zero;
    FVector DragRingV = FVector::Zero;
    float DragStartAxisParameter = 0.0f;
    float DragWorldSize = 0.0f;
    float DragRingRadius = 0.0f;
    float PreviousRingAngle = 0.0f;
    float AccumulatedRotationAngle = 0.0f;
    ImVec2 DragStartMouse = ImVec2(0.0f, 0.0f);
    
    bool bSnapEnabled = false;
    float TranslationSnap = 1.0f;
    float RotationSnap = 15.0f;
    float ScaleSnap = 0.1f;

    FVector DragPlaneAxisA = FVector::Zero;
    FVector DragPlaneAxisB = FVector::Zero;
};