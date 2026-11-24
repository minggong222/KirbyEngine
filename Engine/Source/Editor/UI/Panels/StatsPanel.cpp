#include "StatsPanel.h"

#include "imgui.h"

#include "Core/EngineStatics.h"
#include "UObject/Object.h"
#include "World/Level.h"
#include "World/World.h"

bool FStatsPanel::Init()
{
    return FrameStats != nullptr && Context != nullptr;
}

void FStatsPanel::Tick(float DeltaTime)
{
}

void FStatsPanel::OnRender()
{
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

    ImGui::Text("Tracked C++ Heap: %.2f MB", AllocationMB);
    ImGui::Text("Tracked Allocations: %llu", static_cast<unsigned long long>(AllocationCount));
    ImGui::Text("Tracked Bytes: %llu", static_cast<unsigned long long>(AllocationBytes));

    ImGui::End();

    SetOpen(bIsOpen);
}