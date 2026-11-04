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
    ImGui::Begin("Viewport");
    const ImVec2 AvailableSize = ImGui::GetContentRegionAvail();
    if (AvailableSize.x > 0 && AvailableSize.y > 0)
    {
        const ImVec2 FramebufferScale = ImGui::GetIO().DisplayFramebufferScale;
        const float ScaleX = FramebufferScale.x > 0.0f ? FramebufferScale.x : 1.0f;
        const float ScaleY = FramebufferScale.y > 0.0f ? FramebufferScale.y : 1.0f;
        const uint32 Width = static_cast<uint32>(AvailableSize.x * ScaleX + 0.5f);
        const uint32 Height = static_cast<uint32>(AvailableSize.y * ScaleY + 0.5f);
        SetDesiredSize(Width, Height);
        if (SRV)
        {
            ImGui::Image((ImTextureID)SRV, AvailableSize);
        }
    }
    ImGui::End();
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
