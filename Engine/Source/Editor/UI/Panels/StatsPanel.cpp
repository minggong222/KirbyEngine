#include "StatsPanel.h"

#include "imgui.h"

#include "Core/EngineStatics.h"
#include "UObject/Object.h"
#include "Actors/Actor.h"
#include "World/Level.h"
#include "World/World.h"

#include <cstdio>

bool FStatsPanel::Init()
{
    return FrameStats != nullptr && Context != nullptr;
}

void FStatsPanel::Tick(float DeltaTime)
{
}

void FStatsPanel::RenderOverlay()
{
    if (OverlayMode == EStatOverlayMode::None)
    {
        return;
    }

    char Buffer[256] = {};

    if (OverlayMode == EStatOverlayMode::FPS)
    {
        if (FrameStats == nullptr)
        {
            return;
        }

        std::snprintf(Buffer, sizeof(Buffer), "FPS: %.1f | Frame: %.3f ms", FrameStats->GetFPS(), FrameStats->GetFrameTimeMs());
    }
    else if (OverlayMode == EStatOverlayMode::Memory)
    {
        const uint64 AllocationBytes = FEngineStatics::GetTotalAllocationBytes();
        const uint64 AllocationCount = FEngineStatics::GetTotalAllocationCount();
        const double AllocationMB = static_cast<double>(AllocationBytes) / (1024.0 * 1024.0);

        std::snprintf(Buffer, sizeof(Buffer), "Tracked Heap: %.2f MB | Allocations: %llu", AllocationMB, static_cast<unsigned long long>(AllocationCount));
    }

    if (Buffer[0] == '\0')
    {
        return;
    }

    ImDrawList* DrawList = ImGui::GetForegroundDrawList();

    if (DrawList == nullptr)
    {
        return;
    }

    const ImVec2 Position(12.0f, 12.0f);
    const ImVec2 TextSize = ImGui::CalcTextSize(Buffer);
    const ImVec2 Padding(8.0f, 6.0f);

    const ImVec2 BackgroundMax(Position.x + TextSize.x + Padding.x * 2.0f, Position.y + TextSize.y + Padding.y * 2.0f);
    const ImVec2 TextPosition(Position.x + Padding.x, Position.y + Padding.y);

    DrawList->AddRectFilled(Position, BackgroundMax, IM_COL32(20, 20, 20, 190), 5.0f);
    DrawList->AddText(TextPosition, IM_COL32(255, 255, 255, 255), Buffer);
}

void FStatsPanel::OnRender()
{
    RenderOverlay();

    if (!IsOpen())
    {
        return;
    }

    bool bIsOpen = IsOpen();

    if (!ImGui::Begin("Stats", &bIsOpen))
    {
        SetOpen(bIsOpen);
        ImGui::End();
        return;
    }

    if (FrameStats != nullptr)
    {
        ImGui::Text("FPS: %.1f", FrameStats->GetFPS());
        ImGui::Text("Frame Time: %.3f ms", FrameStats->GetFrameTimeMs());
        ImGui::Text("Average Frame Time: %.3f ms", FrameStats->GetAverageFrameTimeMs());
    }

    ImGui::Separator();

    if (Context != nullptr)
    {
        ULevel* Level = Context->World != nullptr ? Context->World->GetPersistentLevel() : nullptr;

        const int32 ActorCount = Level != nullptr ? Level->GetActors().Num() : 0;
        int32 ComponentCount = 0;

        if (Level != nullptr)
        {
            for (AActor* Actor : Level->GetActors())
            {
                if (Actor != nullptr)
                {
                    ComponentCount += Actor->GetComponents().Num();
                }
            }
        }

        ImGui::Text("Actors: %d", ActorCount);
        ImGui::Text("Components: %d", ComponentCount);
    }

    ImGui::Text("Live UObjects: %d", GUObjectArray.Num());

    ImGui::Separator();

    const uint64 AllocationBytes = FEngineStatics::GetTotalAllocationBytes();
    const uint64 AllocationCount = FEngineStatics::GetTotalAllocationCount();
    const double AllocationMB = static_cast<double>(AllocationBytes) / (1024.0 * 1024.0);

    ImGui::TextWrapped("Tracked C++ Heap: %.2f MB", AllocationMB);
    ImGui::TextWrapped("Tracked Allocations: %llu", static_cast<unsigned long long>(AllocationCount));
    ImGui::TextWrapped("Tracked Bytes: %llu", static_cast<unsigned long long>(AllocationBytes));

    ImGui::Separator();

    const char* OverlayModeText = "None";

    switch (OverlayMode)
    {
    case EStatOverlayMode::FPS:
        OverlayModeText = "FPS";
        break;

    case EStatOverlayMode::Memory:
        OverlayModeText = "Memory";
        break;

    default:
        break;
    }

    ImGui::Text("Viewport Overlay: %s", OverlayModeText);

    ImGui::End();

    SetOpen(bIsOpen);
}