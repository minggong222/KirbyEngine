#include "StatsPanel.h"

#include "imgui.h"

#include "Core/EngineStatics.h"
#include "UObject/Object.h"

bool FStatsPanel::Init()
{
    return FrameStats != nullptr && Context != nullptr;
}

void FStatsPanel::Tick(float DeltaTime)
{
}

void FStatsPanel::OnRender()
{
    if (!ImGui::Begin("Stats"))
    {
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
        ImGui::Text("Scene Objects: %d", Context->Objects.Num());
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
}