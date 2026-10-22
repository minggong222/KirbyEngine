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
    ImGui::Image((ImTextureID)SRV, ImGui::GetContentRegionAvail());
    ImGui::End();
}
