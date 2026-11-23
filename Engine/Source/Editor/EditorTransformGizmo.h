#pragma once

#include "Core/Math/Math.h"
#include "Core/Math/Transform.h"
#include "EditorObjectContext.h"
#include "imgui.h"

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
};