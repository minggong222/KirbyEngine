#include "EditorTransformGizmo.h"

#include "Components/SceneComponent.h"
#include "Core/Math/MathUtility.h"
#include "UObject/Object.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <limits>

namespace
{
    constexpr float GIZMO_PI = 3.14159265358979323846f;
    constexpr float GIZMO_TWO_PI = GIZMO_PI * 2.0f;

    constexpr float GIZMO_LENGTH_PIXELS = 90.0f;
    constexpr float GIZMO_HIT_TOLERANCE = 9.0f;
    constexpr float GIZMO_RING_RADIUS_SCALE = 0.8f;

    constexpr int32 GIZMO_RING_SEGMENTS = 96;

    constexpr float GIZMO_MIN_SCALE = 0.001f;

    const ImU32 GIZMO_COLOR_X = IM_COL32(235, 65, 65, 255);
    const ImU32 GIZMO_COLOR_Y = IM_COL32(75, 220, 100, 255);
    const ImU32 GIZMO_COLOR_Z = IM_COL32(70, 145, 255, 255);
    const ImU32 GIZMO_COLOR_ACTIVE = IM_COL32(255, 220, 80, 255);

    const ImU32 GIZMO_COLOR_PLANE_XY = IM_COL32(220, 100, 100, 85);
    const ImU32 GIZMO_COLOR_PLANE_XZ = IM_COL32(100, 220, 100, 85);
    const ImU32 GIZMO_COLOR_PLANE_YZ = IM_COL32(100, 130, 255, 85);

    FVector GetCanonicalAxis(int32 AxisIndex)
    {
        switch (AxisIndex)
        {
        case 0:
            return EngineAxis::Forward;
        case 1:
            return EngineAxis::Right;
        case 2:
            return EngineAxis::Up;
        default:
            return FVector::Zero;
        }
    }

    FVector GetTransformAxis(const FTransform& Transform, ETransformCoordinateSpace Space, int32 AxisIndex)
    {
        if (Space == ETransformCoordinateSpace::World)
        {
            return GetCanonicalAxis(AxisIndex);
        }

        switch (AxisIndex)
        {
        case 0:
            return Transform.GetForward();
        case 1:
            return Transform.GetRight();
        case 2:
            return Transform.GetUp();
        default:
            return FVector::Zero;
        }
    }

    int32 GetAxisIndex(EGizmoHandle Handle)
    {
        switch (Handle)
        {
        case EGizmoHandle::X:
            return 0;
        case EGizmoHandle::Y:
            return 1;
        case EGizmoHandle::Z:
            return 2;
        case EGizmoHandle::XY:
            return 0;
        case EGizmoHandle::XZ:
            return 0;
        case EGizmoHandle::YZ:
            return 1;
        default:
            return -1;
        }
    }

    ImU32 GetAxisColor(int32 AxisIndex)
    {
        switch (AxisIndex)
        {
        case 0:
            return GIZMO_COLOR_X;
        case 1:
            return GIZMO_COLOR_Y;
        case 2:
            return GIZMO_COLOR_Z;
        default:
            return GIZMO_COLOR_ACTIVE;
        }
    }

    void SetVectorComponent(FVector& Vector, int32 Index, float Value)
    {
        switch (Index)
        {
        case 0:
            Vector.x = Value;
            break;
        case 1:
            Vector.y = Value;
            break;
        case 2:
            Vector.z = Value;
            break;
        }
    }

    float GetVectorComponent(const FVector& Vector, int32 Index)
    {
        switch (Index)
        {
        case 0:
            return Vector.x;
        case 1:
            return Vector.y;
        case 2:
            return Vector.z;
        default:
            return 0.0f;
        }
    }

    bool IsFiniteVector(const FVector& Vector)
    {
        return std::isfinite(Vector.x) && std::isfinite(Vector.y) && std::isfinite(Vector.z);
    }

    bool ProjectWorldPosition(const FVector& WorldPosition, const FViewport& Viewport, const FMatrix& View, const FMatrix& Projection, const ImVec2& ImageMin, ImVec2& OutScreenPosition, float& OutDepth)
    {
        const FVector ScreenPosition = Viewport.Project(WorldPosition, Projection, View, FMatrix::Identity);

        if (!IsFiniteVector(ScreenPosition))
        {
            return false;
        }

        if (ScreenPosition.z < 0.0f || ScreenPosition.z > 1.0f)
        {
            return false;
        }

        OutScreenPosition = ImVec2(ImageMin.x + ScreenPosition.x, ImageMin.y + ScreenPosition.y);
        OutDepth = ScreenPosition.z;

        return true;
    }

    bool CreateWorldRay(const ImVec2& MousePosition, const ImVec2& ImageMin, const ImVec2& ImageSize, const FMatrix& View, const FMatrix& Projection, FRay& OutRay)
    {
        if (ImageSize.x <= 0.0f || ImageSize.y <= 0.0f)
        {
            return false;
        }

        const FViewport Viewport(0.0f, 0.0f, ImageSize.x, ImageSize.y, 0.0f, 1.0f);

        const float MouseX = MousePosition.x - ImageMin.x;
        const float MouseY = MousePosition.y - ImageMin.y;

        const FVector NearPoint = Viewport.Unproject(FVector(MouseX, MouseY, 0.0f), Projection, View, FMatrix::Identity);
        const FVector FarPoint = Viewport.Unproject(FVector(MouseX, MouseY, 1.0f), Projection, View, FMatrix::Identity);

        FVector Direction = FarPoint - NearPoint;

        const float LengthSquared = Direction.LengthSquared();

        if (!std::isfinite(LengthSquared) || LengthSquared <= EPSILON_1E_12)
        {
            return false;
        }

        Direction.Normalize();

        OutRay = FRay(NearPoint, Direction);

        return true;
    }
    float PointToSegmentDistance(const ImVec2& Point, const ImVec2& Start, const ImVec2& End, float& OutParameter)
    {
        const float ABX = End.x - Start.x;
        const float ABY = End.y - Start.y;

        const float APX = Point.x - Start.x;
        const float APY = Point.y - Start.y;

        const float LengthSquared = ABX * ABX + ABY * ABY;

        if (LengthSquared <= EPSILON_1E_8)
        {
            OutParameter = 0.0f;
            return std::sqrt(APX * APX + APY * APY);
        }

        OutParameter = std::clamp((APX * ABX + APY * ABY) / LengthSquared, 0.0f, 1.0f);

        const float ClosestX = Start.x + ABX * OutParameter;
        const float ClosestY = Start.y + ABY * OutParameter;

        const float DX = Point.x - ClosestX;
        const float DY = Point.y - ClosestY;

        return std::sqrt(DX * DX + DY * DY);
    }

    bool GetRayAxisParameter(const FRay& Ray, const FVector& AxisOrigin, const FVector& AxisDirection, float& OutParameter)
    {
        const FVector W = Ray.position - AxisOrigin;

        const float A = Ray.direction.Dot(Ray.direction);
        const float B = Ray.direction.Dot(AxisDirection);
        const float C = AxisDirection.Dot(AxisDirection);
        const float D = Ray.direction.Dot(W);
        const float E = AxisDirection.Dot(W);

        const float Denominator = A * C - B * B;

        if (!std::isfinite(Denominator) || std::fabs(Denominator) <= EPSILON_1E_6)
        {
            return false;
        }

        OutParameter = (A * E - B * D) / Denominator;

        return std::isfinite(OutParameter);
    }

    bool GetRayPlaneIntersection(const FRay& Ray, const FVector& PlanePosition, const FVector& PlaneNormal, FVector& OutIntersection)
    {
        const float Denominator = Ray.direction.Dot(PlaneNormal);

        if (!std::isfinite(Denominator) || std::fabs(Denominator) <= EPSILON_1E_6)
        {
            return false;
        }

        const float T = (PlanePosition - Ray.position).Dot(PlaneNormal) / Denominator;

        if (!std::isfinite(T) || T < 0.0f)
        {
            return false;
        }

        OutIntersection = Ray.position + Ray.direction * T;

        return IsFiniteVector(OutIntersection);
    }

    void MakeRingBasis(const FVector& Axis, FVector& OutU, FVector& OutV)
    {
        FVector ReferenceAxis = EngineAxis::Up;

        if (std::fabs(Axis.Dot(ReferenceAxis)) > 0.95f)
        {
            ReferenceAxis = EngineAxis::Forward;
        }

        OutU = Axis.Cross(ReferenceAxis);

        if (OutU.LengthSquared() <= EPSILON_1E_8)
        {
            ReferenceAxis = EngineAxis::Right;
            OutU = Axis.Cross(ReferenceAxis);
        }

        OutU.Normalize();

        OutV = Axis.Cross(OutU);
        OutV.Normalize();
    }

    bool BuildRingScreenPoints(const FVector& Pivot, const FVector& U, const FVector& V, float Radius, const FViewport& Viewport, const FMatrix& View, const FMatrix& Projection, const ImVec2& ImageMin, std::array<ImVec2, GIZMO_RING_SEGMENTS + 1>& OutPoints)
    {
        for (int32 Index = 0; Index <= GIZMO_RING_SEGMENTS; ++Index)
        {
            const float Angle = GIZMO_TWO_PI * static_cast<float>(Index) / static_cast<float>(GIZMO_RING_SEGMENTS);

            const FVector WorldPosition = Pivot + (U * std::cos(Angle) + V * std::sin(Angle)) * Radius;

            float Depth = 0.0f;

            if (!ProjectWorldPosition(WorldPosition, Viewport, View, Projection, ImageMin, OutPoints[Index], Depth))
            {
                return false;
            }
        }

        return true;
    }

    bool FindClosestRingAngle(const std::array<ImVec2, GIZMO_RING_SEGMENTS + 1>& RingPoints, const ImVec2& MousePosition, float& OutAngle, float& OutDistance)
    {
        OutDistance = (std::numeric_limits<float>::max)();
        OutAngle = 0.0f;

        bool bFound = false;

        for (int32 Index = 0; Index < GIZMO_RING_SEGMENTS; ++Index)
        {
            float SegmentParameter = 0.0f;

            const float Distance = PointToSegmentDistance(MousePosition, RingPoints[Index], RingPoints[Index + 1], SegmentParameter);

            if (Distance < OutDistance)
            {
                OutDistance = Distance;

                const float RingParameter = (static_cast<float>(Index) + SegmentParameter) / static_cast<float>(GIZMO_RING_SEGMENTS);

                OutAngle = RingParameter * GIZMO_TWO_PI;

                bFound = true;
            }
        }

        return bFound;
    }

    bool IsPointInsideRect(const ImVec2& Point, const ImVec2& Center, float HalfSize)
    {
        return std::fabs(Point.x - Center.x) <= HalfSize && std::fabs(Point.y - Center.y) <= HalfSize;
    }

    float GetWrappedAngleDelta(float PreviousAngle, float CurrentAngle)
    {
        float Delta = CurrentAngle - PreviousAngle;

        while (Delta > GIZMO_PI)
        {
            Delta -= GIZMO_TWO_PI;
        }

        while (Delta < -GIZMO_PI)
        {
            Delta += GIZMO_TWO_PI;
        }

        return Delta;
    }

    float GetSafeScaleFactor(float Exponent)
    {
        Exponent = std::clamp(Exponent, -6.9f, 6.9f);
        return std::exp(Exponent);
    }

    float ApplyScaleFactor(float StartScale, float Factor)
    {
        const float Sign = StartScale < 0.0f ? -1.0f : 1.0f;
        const float Magnitude = max(std::fabs(StartScale) * Factor, GIZMO_MIN_SCALE);

        return Sign * Magnitude;
    }
    
    bool IsPointInTriangle2D(const ImVec2& P, const ImVec2& A, const ImVec2& B, const ImVec2& C)
    {
        const float D1 = (P.x - B.x) * (A.y - B.y) - (A.x - B.x) * (P.y - B.y);
        const float D2 = (P.x - C.x) * (B.y - C.y) - (B.x - C.x) * (P.y - C.y);
        const float D3 = (P.x - A.x) * (C.y - A.y) - (C.x - A.x) * (P.y - A.y);

        const bool bNegative = D1 < 0.0f || D2 < 0.0f || D3 < 0.0f;
        const bool bPositive = D1 > 0.0f || D2 > 0.0f || D3 > 0.0f;

        return !(bNegative && bPositive);
    }

    bool IsPointInQuad2D(const ImVec2& P, const ImVec2 Points[4])
    {
        return IsPointInTriangle2D(P, Points[0], Points[1], Points[2]) || IsPointInTriangle2D(P, Points[0], Points[2], Points[3]);
    }

    void DrawGizmoArrow(ImDrawList* DrawList, const ImVec2& Start, const ImVec2& End, ImU32 Color, float Thickness)
    {
        const float DX = End.x - Start.x;
        const float DY = End.y - Start.y;
        const float Length = std::sqrt(DX * DX + DY * DY);

        if (Length <= EPSILON_1E_6)
        {
            return;
        }

        const float DirectionX = DX / Length;
        const float DirectionY = DY / Length;

        const ImVec2 Perpendicular(-DirectionY, DirectionX);

        const ImVec2 ArrowBase(End.x - DirectionX * 13.0f, End.y - DirectionY * 13.0f);
        const ImVec2 ArrowLeft(ArrowBase.x + Perpendicular.x * 6.0f, ArrowBase.y + Perpendicular.y * 6.0f);
        const ImVec2 ArrowRight(ArrowBase.x - Perpendicular.x * 6.0f, ArrowBase.y - Perpendicular.y * 6.0f);

        DrawList->AddLine(Start, ArrowBase, Color, Thickness);
        DrawList->AddTriangleFilled(End, ArrowLeft, ArrowRight, Color);
    }

    void DrawGizmoSquare(ImDrawList* DrawList, const ImVec2& Center, ImU32 Color, float HalfSize)
    {
        DrawList->AddRectFilled(ImVec2(Center.x - HalfSize, Center.y - HalfSize), ImVec2(Center.x + HalfSize, Center.y + HalfSize), Color, 2.0f);
    }

    bool GetGizmoWorldSize(const FTransform& Transform, const FViewport& Viewport, const FMatrix& View, const FMatrix& Projection, const ImVec2& ImageMin, ImVec2& OutCenter, float& OutWorldSize)
    {
        float Depth = 0.0f;

        if (!ProjectWorldPosition(Transform.Location, Viewport, View, Projection, ImageMin, OutCenter, Depth))
        {
            return false;
        }

        const FVector ScreenOffsetWorld = Viewport.Unproject(FVector(OutCenter.x - ImageMin.x + GIZMO_LENGTH_PIXELS, OutCenter.y - ImageMin.y, Depth), Projection, View, FMatrix::Identity);

        OutWorldSize = FVector::Distance(Transform.Location, ScreenOffsetWorld);

        return std::isfinite(OutWorldSize) && OutWorldSize > EPSILON_1E_6;
    }

    bool ProjectPlaneHandle(const FVector& Pivot, const FVector& AxisA, const FVector& AxisB, float WorldSize, const FViewport& Viewport, const FMatrix& View, const FMatrix& Projection, const ImVec2& ImageMin, ImVec2 OutPoints[4])
    {
        const float Offset = WorldSize * 0.18f;
        const float Size = WorldSize * 0.18f;

        const FVector WorldPoints[4] =
        {
            Pivot + AxisA * Offset + AxisB * Offset,
            Pivot + AxisA * (Offset + Size) + AxisB * Offset,
            Pivot + AxisA * (Offset + Size) + AxisB * (Offset + Size),
            Pivot + AxisA * Offset + AxisB * (Offset + Size)
        };

        for (int32 Index = 0; Index < 4; ++Index)
        {
            float Depth = 0.0f;

            if (!ProjectWorldPosition(WorldPoints[Index], Viewport, View, Projection, ImageMin, OutPoints[Index], Depth))
            {
                return false;
            }
        }

        return true;
    }

    void DrawGizmoPlane(ImDrawList* DrawList, const ImVec2 Points[4], ImU32 Color)
    {
        DrawList->AddTriangleFilled(Points[0], Points[1], Points[2], Color);
        DrawList->AddTriangleFilled(Points[0], Points[2], Points[3], Color);
    }
}

bool FEditorTransformGizmo::Draw(FEditorObjectContext& Context, const FMatrix& View, const FMatrix& Projection, ImDrawList* DrawList, const ImVec2& ImageMin, const ImVec2& ImageSize, bool bImageHovered)
{
    USceneComponent* SelectedComponent = Cast<USceneComponent>(Context.SelectedObject);

    const bool bWasDragging = bIsDragging;
    const EGizmoHandle PreviousDragHandle = DragHandle;

    bool bConsumedInput = bWasDragging;
    bool bDragStillTargetsSelection = bWasDragging && SelectedComponent == DraggedComponent;

    // ============================================================
    // DRAG UPDATE
    // ============================================================

    if (bIsDragging)
    {
        if (!bDragStillTargetsSelection)
        {
            EndDrag();
        }
        else
        {
            if (ImGui::IsMouseDown(ImGuiMouseButton_Left) || ImGui::IsMouseReleased(ImGuiMouseButton_Left))
            {
                UpdateDrag(ImGui::GetIO().MousePos, ImageMin, ImageSize, View, Projection);
            }

            if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
            {
                EndDrag();
            }
        }
    }

    if (SelectedComponent == nullptr)
    {
        CancelDrag();
        return bConsumedInput;
    }

    if (DrawList == nullptr || ImageSize.x <= 0.0f || ImageSize.y <= 0.0f)
    {
        return bConsumedInput;
    }

    // ============================================================
    // INPUT MODE
    // ============================================================

    if (!bWasDragging && bImageHovered)
    {
        ImGuiIO& IO = ImGui::GetIO();

        if (!IO.WantTextInput && !ImGui::IsMouseDown(ImGuiMouseButton_Right))
        {
            if (ImGui::IsKeyPressed(ImGuiKey_W, false))
            {
                CurrentMode = ETransformGizmoMode::Translate;
            }
            else if (ImGui::IsKeyPressed(ImGuiKey_E, false))
            {
                CurrentMode = ETransformGizmoMode::Rotate;
            }
            else if (ImGui::IsKeyPressed(ImGuiKey_R, false))
            {
                CurrentMode = ETransformGizmoMode::Scale;
            }

            if (ImGui::IsKeyPressed(ImGuiKey_GraveAccent, false))
            {
                CoordinateSpace = CoordinateSpace == ETransformCoordinateSpace::World ? ETransformCoordinateSpace::Local : ETransformCoordinateSpace::World;
            }
        }
    }

    const ETransformGizmoMode DrawMode = bDragStillTargetsSelection ? DragMode : CurrentMode;
    const ETransformCoordinateSpace DrawSpace = bDragStillTargetsSelection ? DragSpace : CoordinateSpace;

    const EGizmoHandle ActiveDragHandle = bDragStillTargetsSelection ? PreviousDragHandle : EGizmoHandle::None;

    // ============================================================
    // TRANSFORM / AXES
    // ============================================================

    const FTransform Transform = SelectedComponent->GetTransform();

    FVector Axes[3] =
    {
        GetTransformAxis(Transform, DrawSpace, 0),
        GetTransformAxis(Transform, DrawSpace, 1),
        GetTransformAxis(Transform, DrawSpace, 2)
    };

    const FViewport Viewport(0.0f, 0.0f, ImageSize.x, ImageSize.y, 0.0f, 1.0f);

    ImVec2 Center;

    float GizmoWorldSize = 0.0f;

    if (!GetGizmoWorldSize(Transform, Viewport, View, Projection, ImageMin, Center, GizmoWorldSize))
    {
        return bConsumedInput;
    }

    const ImVec2 MousePosition = ImGui::GetIO().MousePos;

    ImVec2 AxisEnds[3];

    bool bValidAxis[3] = {false, false, false};

    for (int32 AxisIndex = 0; AxisIndex < 3; ++AxisIndex)
    {
        float Depth = 0.0f;

        const FVector WorldEnd = Transform.Location + Axes[AxisIndex] * GizmoWorldSize;

        bValidAxis[AxisIndex] = ProjectWorldPosition(WorldEnd, Viewport, View, Projection, ImageMin, AxisEnds[AxisIndex], Depth);
    }

    // ============================================================
    // HANDLE HIT TEST
    // 한 번만 선택하고, 이후 렌더링은 선택 결과만 사용
    // ============================================================

    EGizmoHandle HoverHandle = EGizmoHandle::None;

    float BestDistance = GIZMO_HIT_TOLERANCE;

    const bool bCanHover = bImageHovered && !bWasDragging;

    // ------------------------------------------------------------
    // Plane candidates
    // ------------------------------------------------------------

    const EGizmoHandle PlaneHandles[3] =
    {
        EGizmoHandle::XY,
        EGizmoHandle::XZ,
        EGizmoHandle::YZ
    };

    const int32 PlaneAxes[3][2] =
    {
        {0, 1},
        {0, 2},
        {1, 2}
    };

    const ImU32 PlaneColors[3] =
    {
        GIZMO_COLOR_PLANE_XY,
        GIZMO_COLOR_PLANE_XZ,
        GIZMO_COLOR_PLANE_YZ
    };

    ImVec2 PlanePoints[3][4];

    bool bValidPlane[3] = {false, false, false};

    int32 BestPlaneIndex = INDEX_NONE;

    float BestPlaneCenterDistance = (std::numeric_limits<float>::max)();

    if (DrawMode == ETransformGizmoMode::Translate)
    {
        for (int32 Index = 0; Index < 3; ++Index)
        {
            bValidPlane[Index] = ProjectPlaneHandle(Transform.Location, Axes[PlaneAxes[Index][0]], Axes[PlaneAxes[Index][1]], GizmoWorldSize, Viewport, View, Projection, ImageMin, PlanePoints[Index]);

            if (!bCanHover || !bValidPlane[Index] || !IsPointInQuad2D(MousePosition, PlanePoints[Index]))
            {
                continue;
            }

            const ImVec2 PlaneCenter(
                (PlanePoints[Index][0].x + PlanePoints[Index][1].x + PlanePoints[Index][2].x + PlanePoints[Index][3].x) * 0.25f,
                (PlanePoints[Index][0].y + PlanePoints[Index][1].y + PlanePoints[Index][2].y + PlanePoints[Index][3].y) * 0.25f
            );

            const float DX = MousePosition.x - PlaneCenter.x;
            const float DY = MousePosition.y - PlaneCenter.y;

            const float CenterDistance = DX * DX + DY * DY;

            if (CenterDistance < BestPlaneCenterDistance)
            {
                BestPlaneCenterDistance = CenterDistance;
                BestPlaneIndex = Index;
            }
        }

        if (BestPlaneIndex != INDEX_NONE)
        {
            HoverHandle = PlaneHandles[BestPlaneIndex];
            BestDistance = 0.0f;
        }
    }

    // ------------------------------------------------------------
    // Uniform scale center
    // ------------------------------------------------------------

    if (DrawMode == ETransformGizmoMode::Scale && bCanHover && IsPointInsideRect(MousePosition, Center, 8.0f))
    {
        HoverHandle = EGizmoHandle::XYZ;
        BestDistance = 0.0f;
    }

    // ------------------------------------------------------------
    // Axis candidates
    // ------------------------------------------------------------

    if ((DrawMode == ETransformGizmoMode::Translate || DrawMode == ETransformGizmoMode::Scale) && HoverHandle == EGizmoHandle::None && bCanHover)
    {
        for (int32 AxisIndex = 0; AxisIndex < 3; ++AxisIndex)
        {
            if (!bValidAxis[AxisIndex])
            {
                continue;
            }

            float SegmentParameter = 0.0f;

            const float Distance = PointToSegmentDistance(MousePosition, Center, AxisEnds[AxisIndex], SegmentParameter);

            if (Distance > GIZMO_HIT_TOLERANCE || Distance >= BestDistance)
            {
                continue;
            }

            BestDistance = Distance;

            HoverHandle = AxisIndex == 0 ? EGizmoHandle::X : AxisIndex == 1 ? EGizmoHandle::Y : EGizmoHandle::Z;
        }
    }

    // ------------------------------------------------------------
    // Rotation ring candidates
    // ------------------------------------------------------------

    if (DrawMode == ETransformGizmoMode::Rotate && bCanHover)
    {
        for (int32 AxisIndex = 0; AxisIndex < 3; ++AxisIndex)
        {
            FVector RingU;
            FVector RingV;

            MakeRingBasis(Axes[AxisIndex], RingU, RingV);

            std::array<ImVec2, GIZMO_RING_SEGMENTS + 1> RingPoints;

            const float RingRadius = GizmoWorldSize * GIZMO_RING_RADIUS_SCALE;

            if (!BuildRingScreenPoints(Transform.Location, RingU, RingV, RingRadius, Viewport, View, Projection, ImageMin, RingPoints))
            {
                continue;
            }

            float RingAngle = 0.0f;
            float RingDistance = 0.0f;

            if (!FindClosestRingAngle(RingPoints, MousePosition, RingAngle, RingDistance))
            {
                continue;
            }

            if (RingDistance > GIZMO_HIT_TOLERANCE || RingDistance >= BestDistance)
            {
                continue;
            }

            BestDistance = RingDistance;

            HoverHandle = AxisIndex == 0 ? EGizmoHandle::X : AxisIndex == 1 ? EGizmoHandle::Y : EGizmoHandle::Z;
        }
    }

    // ============================================================
    // DRAW PLANE HANDLES
    // ============================================================

    if (DrawMode == ETransformGizmoMode::Translate)
    {
        for (int32 Index = 0; Index < 3; ++Index)
        {
            if (!bValidPlane[Index])
            {
                continue;
            }

            const bool bActive = HoverHandle == PlaneHandles[Index] || ActiveDragHandle == PlaneHandles[Index];

            DrawGizmoPlane(DrawList, PlanePoints[Index], bActive ? GIZMO_COLOR_ACTIVE : PlaneColors[Index]);
        }
    }

    // ============================================================
    // DRAW AXIS HANDLES
    // ============================================================

    if (DrawMode == ETransformGizmoMode::Translate || DrawMode == ETransformGizmoMode::Scale)
    {
        for (int32 AxisIndex = 0; AxisIndex < 3; ++AxisIndex)
        {
            if (!bValidAxis[AxisIndex])
            {
                continue;
            }

            const EGizmoHandle AxisHandle = AxisIndex == 0 ? EGizmoHandle::X : AxisIndex == 1 ? EGizmoHandle::Y : EGizmoHandle::Z;

            const bool bActive = HoverHandle == AxisHandle || ActiveDragHandle == AxisHandle;

            const ImU32 Color = bActive ? GIZMO_COLOR_ACTIVE : GetAxisColor(AxisIndex);

            if (DrawMode == ETransformGizmoMode::Translate)
            {
                DrawGizmoArrow(DrawList, Center, AxisEnds[AxisIndex], Color, 3.0f);
            }
            else
            {
                DrawList->AddLine(Center, AxisEnds[AxisIndex], Color, 3.0f);
                DrawGizmoSquare(DrawList, AxisEnds[AxisIndex], Color, 6.0f);
            }
        }
    }

    // ============================================================
    // DRAW UNIFORM SCALE HANDLE
    // ============================================================

    if (DrawMode == ETransformGizmoMode::Scale)
    {
        const bool bUniformActive = HoverHandle == EGizmoHandle::XYZ || ActiveDragHandle == EGizmoHandle::XYZ;

        DrawGizmoSquare(DrawList, Center, bUniformActive ? GIZMO_COLOR_ACTIVE : IM_COL32(235, 235, 235, 255), 6.0f);
    }

    // ============================================================
    // DRAW ROTATION RINGS
    // ============================================================

    if (DrawMode == ETransformGizmoMode::Rotate)
    {
        for (int32 AxisIndex = 0; AxisIndex < 3; ++AxisIndex)
        {
            FVector RingU;
            FVector RingV;

            MakeRingBasis(Axes[AxisIndex], RingU, RingV);

            std::array<ImVec2, GIZMO_RING_SEGMENTS + 1> RingPoints;

            const float RingRadius = GizmoWorldSize * GIZMO_RING_RADIUS_SCALE;

            if (!BuildRingScreenPoints(Transform.Location, RingU, RingV, RingRadius, Viewport, View, Projection, ImageMin, RingPoints))
            {
                continue;
            }

            const EGizmoHandle AxisHandle = AxisIndex == 0 ? EGizmoHandle::X : AxisIndex == 1 ? EGizmoHandle::Y : EGizmoHandle::Z;

            const bool bActive = HoverHandle == AxisHandle || ActiveDragHandle == AxisHandle;

            const ImU32 Color = bActive ? GIZMO_COLOR_ACTIVE : GetAxisColor(AxisIndex);

            for (int32 Segment = 0; Segment < GIZMO_RING_SEGMENTS; ++Segment)
            {
                DrawList->AddLine(RingPoints[Segment], RingPoints[Segment + 1], Color, 2.5f);
            }
        }
    }

    // ============================================================
    // PIVOT
    // ============================================================

    DrawList->AddCircleFilled(Center, 4.0f, IM_COL32(255, 255, 255, 255));

    // ============================================================
    // MODE INDICATOR
    // ============================================================

    const char* ModeText = DrawMode == ETransformGizmoMode::Translate ? "Translate" : DrawMode == ETransformGizmoMode::Rotate ? "Rotate" : "Scale";

    const char* SpaceText = DrawSpace == ETransformCoordinateSpace::World ? "World" : "Local";

    char Indicator[64] = {};

    std::snprintf(Indicator, sizeof(Indicator), "%s | %s", ModeText, SpaceText);

    DrawList->AddText(ImVec2(ImageMin.x + 12.0f, ImageMin.y + 12.0f), IM_COL32(255, 255, 255, 220), Indicator);

    // ============================================================
    // BEGIN DRAG
    // ============================================================

    if (!bWasDragging && bImageHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && HoverHandle != EGizmoHandle::None)
    {
        BeginDrag(SelectedComponent, HoverHandle, Transform, Axes, MousePosition, ImageMin, ImageSize, View, Projection, GizmoWorldSize);
        bConsumedInput = true;
    }

    return bConsumedInput;
}

bool FEditorTransformGizmo::BeginDrag(USceneComponent* Component, EGizmoHandle Handle, const FTransform& Transform, const FVector Axes[3],
                                      const ImVec2& MousePosition, const ImVec2& ImageMin, const ImVec2& ImageSize, const FMatrix& View, const FMatrix& Projection, float GizmoWorldSize)
{
    if (Component == nullptr || Handle == EGizmoHandle::None)
    {
        return false;
    }

    DragHandle = Handle;
    DraggedComponent = Component;

    DragStartTransform = Transform;
    DragPivot = Transform.Location;

    DragWorldSize = GizmoWorldSize;

    DragStartMouse = MousePosition;

    DragMode = CurrentMode;
    DragSpace = CoordinateSpace;

    const int32 AxisIndex = GetAxisIndex(Handle);

    if (AxisIndex >= 0)
    {
        DragAxisWorld = Axes[AxisIndex];
        DragRotationAxis = GetCanonicalAxis(AxisIndex);
    }

    FRay MouseRay;

    if (DragMode != ETransformGizmoMode::Scale || Handle != EGizmoHandle::XYZ)
    {
        if (!CreateWorldRay(MousePosition, ImageMin, ImageSize, View, Projection, MouseRay))
        {
            return false;
        }
    }

    if (DragMode == ETransformGizmoMode::Translate)
    {
        if (Handle == EGizmoHandle::XY)
        {
            DragPlaneNormal = Axes[2];
        }
        else if (Handle == EGizmoHandle::XZ)
        {
            DragPlaneNormal = Axes[1];
        }
        else if (Handle == EGizmoHandle::YZ)
        {
            DragPlaneNormal = Axes[0];
        }
        else
        {
            if (!GetRayAxisParameter(MouseRay, DragPivot, DragAxisWorld, DragStartAxisParameter))
            {
                return false;
            }
        }

        if (Handle == EGizmoHandle::XY || Handle == EGizmoHandle::XZ || Handle == EGizmoHandle::YZ)
        {
            if (!GetRayPlaneIntersection(MouseRay, DragPivot, DragPlaneNormal, DragStartPlaneHit))
            {
                return false;
            }
        }
    }
    else if (DragMode == ETransformGizmoMode::Rotate)
    {
        DragPlaneNormal = DragAxisWorld;

        if (!GetRayPlaneIntersection(MouseRay, DragPivot, DragPlaneNormal, DragStartPlaneHit))
        {
            return false;
        }

        PreviousRotationVector = DragStartPlaneHit - DragPivot;

        const float RotationVectorLengthSquared = PreviousRotationVector.LengthSquared();

        if (RotationVectorLengthSquared <= EPSILON_1E_8)
        {
            return false;
        }

        PreviousRotationVector.Normalize();

        AccumulatedRotationAngle = 0.0f;
    }
    else if (DragMode == ETransformGizmoMode::Scale)
    {
        if (Handle != EGizmoHandle::XYZ)
        {
            if (!GetRayAxisParameter(MouseRay, DragPivot, DragAxisWorld, DragStartAxisParameter))
            {
                return false;
            }
        }
    }

    bIsDragging = true;

    return true;
}

void FEditorTransformGizmo::UpdateDrag(const ImVec2& MousePosition, const ImVec2& ImageMin, const ImVec2& ImageSize, const FMatrix& View, const FMatrix& Projection)
{
    if (!bIsDragging || DraggedComponent == nullptr)
    {
        return;
    }

    FTransform NewTransform = DragStartTransform;

    FRay MouseRay;

    if (DragMode != ETransformGizmoMode::Scale || DragHandle != EGizmoHandle::XYZ)
    {
        if (!CreateWorldRay(MousePosition, ImageMin, ImageSize, View, Projection, MouseRay))
        {
            return;
        }
    }

    // ============================================================
    // TRANSLATE
    // ============================================================

    if (DragMode == ETransformGizmoMode::Translate)
    {
        if (DragHandle == EGizmoHandle::XY || DragHandle == EGizmoHandle::XZ || DragHandle == EGizmoHandle::YZ)
        {
            FVector CurrentPlaneHit;

            if (!GetRayPlaneIntersection(MouseRay, DragPivot, DragPlaneNormal, CurrentPlaneHit))
            {
                return;
            }

            const FVector Delta = CurrentPlaneHit - DragStartPlaneHit;

            NewTransform.Location = DragStartTransform.Location + Delta;
        }
        else
        {
            float CurrentAxisParameter = 0.0f;

            if (!GetRayAxisParameter(MouseRay, DragPivot, DragAxisWorld, CurrentAxisParameter))
            {
                return;
            }

            const float Delta = CurrentAxisParameter - DragStartAxisParameter;

            NewTransform.Location = DragStartTransform.Location + DragAxisWorld * Delta;
        }
    }

    // ============================================================
    // ROTATE
    // ============================================================

    else if (DragMode == ETransformGizmoMode::Rotate)
    {
        FVector CurrentPlaneHit;

        if (!GetRayPlaneIntersection(MouseRay, DragPivot, DragPlaneNormal, CurrentPlaneHit))
        {
            return;
        }

        FVector CurrentRotationVector = CurrentPlaneHit - DragPivot;

        const float CurrentVectorLengthSquared = CurrentRotationVector.LengthSquared();

        if (!std::isfinite(CurrentVectorLengthSquared) || CurrentVectorLengthSquared <= EPSILON_1E_8)
        {
            return;
        }

        CurrentRotationVector.Normalize();

        const float SinAngle = DragAxisWorld.Dot(PreviousRotationVector.Cross(CurrentRotationVector));
        const float CosAngle = PreviousRotationVector.Dot(CurrentRotationVector);

        const float AngleDelta = std::atan2(SinAngle, CosAngle);

        AccumulatedRotationAngle += AngleDelta;

        PreviousRotationVector = CurrentRotationVector;

        const FMatrix StartRotationMatrix = DragStartTransform.GetOrientation().ToMatrix();
        const FMatrix DeltaRotationMatrix = FMatrix::CreateFromAxisAngle(DragRotationAxis, AccumulatedRotationAngle);

        FMatrix NewRotationMatrix;

        if (DragSpace == ETransformCoordinateSpace::World)
        {
            NewRotationMatrix = StartRotationMatrix * DeltaRotationMatrix;
        }
        else
        {
            NewRotationMatrix = DeltaRotationMatrix * StartRotationMatrix;
        }

        FQuat NewRotation = FQuat::CreateFromRotationMatrix(NewRotationMatrix);

        NewRotation.Normalize();

        NewTransform.Rotation = FRotator(NewRotation);
    }

    // ============================================================
    // SCALE
    // ============================================================

    else if (DragMode == ETransformGizmoMode::Scale)
    {
        if (DragHandle == EGizmoHandle::XYZ)
        {
            const float MouseDeltaY = DragStartMouse.y - MousePosition.y;

            const float ScaleFactor = GetSafeScaleFactor(MouseDeltaY / 100.0f);

            NewTransform.Scale3D.x = ApplyScaleFactor(DragStartTransform.Scale3D.x, ScaleFactor);
            NewTransform.Scale3D.y = ApplyScaleFactor(DragStartTransform.Scale3D.y, ScaleFactor);
            NewTransform.Scale3D.z = ApplyScaleFactor(DragStartTransform.Scale3D.z, ScaleFactor);
        }
        else
        {
            float CurrentAxisParameter = 0.0f;

            if (!GetRayAxisParameter(MouseRay, DragPivot, DragAxisWorld, CurrentAxisParameter))
            {
                return;
            }

            const float AxisDelta = CurrentAxisParameter - DragStartAxisParameter;

            const float ScaleFactor = GetSafeScaleFactor(AxisDelta / (DragWorldSize * 0.75f));

            const int32 AxisIndex = GetAxisIndex(DragHandle);

            if (AxisIndex < 0 || AxisIndex > 2)
            {
                return;
            }

            const float StartScale = GetVectorComponent(DragStartTransform.Scale3D, AxisIndex);

            SetVectorComponent(NewTransform.Scale3D, AxisIndex, ApplyScaleFactor(StartScale, ScaleFactor));
        }
    }

    // ============================================================
    // APPLY TRANSFORM
    // ============================================================

    DraggedComponent->SetTransform(NewTransform);
}

void FEditorTransformGizmo::EndDrag()
{
    bIsDragging = false;

    DragHandle = EGizmoHandle::None;

    DraggedComponent = nullptr;

    DragAxisWorld = FVector::Zero;

    DragRotationAxis = FVector::Zero;

    DragPlaneNormal = FVector::Zero;

    DragStartPlaneHit = FVector::Zero;

    PreviousRotationVector = FVector::Zero;
    
    DragRingU = FVector::Zero;

    DragRingV = FVector::Zero;

    DragStartAxisParameter = 0.0f;

    DragWorldSize = 0.0f;

    DragRingRadius = 0.0f;

    PreviousRingAngle = 0.0f;

    AccumulatedRotationAngle = 0.0f;
}

void FEditorTransformGizmo::CancelDrag()
{
    EndDrag();
}