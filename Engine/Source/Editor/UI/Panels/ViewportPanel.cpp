#include "ViewportPanel.h"

#include "imgui.h"

bool FViewportPanel::Init()
{
    return true;
}

void FViewportPanel::Tick(float DeltaTime)
{
}

void FViewportPanel::OnRender()
{
    ImGui::Begin("Viewport", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    ImVec2 AvailableSize = ImGui::GetContentRegionAvail();
    if (SRV && AvailableSize.x > 0 && AvailableSize.y > 0)
    {
        ImGui::Image((ImTextureID)SRV, ImGui::GetContentRegionAvail());
    }
    ImGui::End();
}
