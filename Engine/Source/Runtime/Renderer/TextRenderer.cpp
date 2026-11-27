#include "Core/Precompiled.h"
#include "Renderer/TextRenderer.h"
#include "RHI/RHI.h"

#include <Windows.h>
#include <algorithm>
#include <vector>

bool FTextRenderer::Initialize(const wchar_t* FontFace, int32 InFontHeight)
{
    if (FontDC != nullptr || Font != nullptr || FontFace == nullptr || InFontHeight <= 0)
    {
        return false;
    }

    FontHeight = InFontHeight;

    FontDC = CreateCompatibleDC(nullptr);

    if (FontDC == nullptr)
    {
        UE_LOG(Error, "[TextRenderer] CreateCompatibleDC failed.");
        return false;
    }

    Font = CreateFontW(-FontHeight, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, FontFace);

    if (Font == nullptr)
    {
        UE_LOG(Error, "[TextRenderer] CreateFontW failed.");
        Shutdown();
        return false;
    }

    PreviousFont = SelectObject(FontDC, Font);

    if (PreviousFont == nullptr || PreviousFont == HGDI_ERROR)
    {
        UE_LOG(Error, "[TextRenderer] SelectObject failed.");
        Shutdown();
        return false;
    }

    AtlasPixels.SetNumZeroed(AtlasSize * AtlasSize * 4);

    for (int32 Index = 0; Index < AtlasPixels.Num(); Index += 4)
    {
        AtlasPixels[Index + 0] = 255;
        AtlasPixels[Index + 1] = 255;
        AtlasPixels[Index + 2] = 255;
        AtlasPixels[Index + 3] = 0;
    }

    CursorX = AtlasPadding;
    CursorY = AtlasPadding;
    CurrentRowHeight = 0;
    bAtlasDirty = true;

    for (uint32 Character = 32; Character <= 126; ++Character)
    {
        FTextGlyph Glyph;

        if (!GetGlyph(Character, Glyph))
        {
            Shutdown();
            return false;
        }
    }

    return true;
}

void FTextRenderer::Shutdown()
{
    AtlasResource.Release();

    if (FontDC != nullptr)
    {
        if (PreviousFont != nullptr && PreviousFont != HGDI_ERROR)
        {
            SelectObject(FontDC, PreviousFont);
        }

        if (Font != nullptr)
        {
            DeleteObject(Font);
            Font = nullptr;
        }

        DeleteDC(FontDC);
        FontDC = nullptr;
    }

    PreviousFont = nullptr;

    AtlasPixels.Reset();
    PendingVertices.Reset();
    GlyphCache.clear();

    CursorX = AtlasPadding;
    CursorY = AtlasPadding;
    CurrentRowHeight = 0;

    bAtlasDirty = true;
}

void FTextRenderer::BeginFrame()
{
    PendingVertices.Reset();
}

bool FTextRenderer::DecodeUTF8(const FString& Text, std::wstring& OutText) const
{
    OutText.clear();

    if (Text.IsEmpty())
    {
        return true;
    }

    const std::string& Source = Text.ToStd();

    const int32 RequiredLength = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, Source.c_str(), -1, nullptr, 0);

    if (RequiredLength <= 0)
    {
        return false;
    }

    OutText.resize(static_cast<std::size_t>(RequiredLength));

    const int32 ConvertedLength = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, Source.c_str(), -1, OutText.data(), RequiredLength);

    if (ConvertedLength <= 0)
    {
        OutText.clear();
        return false;
    }

    OutText.resize(static_cast<std::size_t>(ConvertedLength - 1));

    return true;
}

bool FTextRenderer::GetGlyph(uint32 Character, FTextGlyph& OutGlyph)
{
    const auto Found = GlyphCache.find(Character);

    if (Found != GlyphCache.end())
    {
        OutGlyph = Found->second;
        return true;
    }

    if (FontDC == nullptr)
    {
        return false;
    }

    GLYPHMETRICS GlyphMetrics = {};
    MAT2 Transform = {};

    Transform.eM11.value = 1;
    Transform.eM22.value = 1;

    const wchar_t WideCharacter = static_cast<wchar_t>(Character);

    const DWORD BitmapSize = GetGlyphOutlineW(FontDC, WideCharacter, GGO_GRAY8_BITMAP, &GlyphMetrics, 0, nullptr, &Transform);

    if (BitmapSize == GDI_ERROR)
    {
        if (Character != static_cast<uint32>(L'?'))
        {
            return GetGlyph(static_cast<uint32>(L'?'), OutGlyph);
        }

        UE_LOG(Error, "[TextRenderer] GetGlyphOutlineW failed for fallback glyph.");
        return false;
    }

    FTextGlyph Glyph;
    Glyph.Width = static_cast<int32>(GlyphMetrics.gmBlackBoxX);
    Glyph.Height = static_cast<int32>(GlyphMetrics.gmBlackBoxY);
    Glyph.BearingX = GlyphMetrics.gmptGlyphOrigin.x;
    Glyph.BearingY = GlyphMetrics.gmptGlyphOrigin.y;
    Glyph.AdvanceX = GlyphMetrics.gmCellIncX;

    if (BitmapSize == 0)
    {
        Glyph.Width = 0;
        Glyph.Height = 0;
    }
    else if (Glyph.Width > 0 && Glyph.Height > 0)
    {
        if (Glyph.Width + AtlasPadding * 2 > AtlasSize || Glyph.Height + AtlasPadding * 2 > AtlasSize)
        {
            return false;
        }

        if (CursorX + Glyph.Width + AtlasPadding > AtlasSize)
        {
            CursorX = AtlasPadding;
            CursorY += CurrentRowHeight + AtlasPadding;
            CurrentRowHeight = 0;
        }

        if (CursorY + Glyph.Height + AtlasPadding > AtlasSize)
        {
            UE_LOG(Error, "[TextRenderer] Glyph atlas is full. CharacterCode=%u", Character);
            return false;
        }

        std::vector<uint8> Bitmap(BitmapSize);

        const DWORD ReadSize = GetGlyphOutlineW(FontDC, WideCharacter, GGO_GRAY8_BITMAP, &GlyphMetrics, BitmapSize, Bitmap.data(), &Transform);

        if (ReadSize == GDI_ERROR)
        {
            UE_LOG(Error, "[TextRenderer] GetGlyphOutlineW bitmap read failed. CharacterCode=%u", Character);
            return false;
        }

        const int32 SourcePitch = (Glyph.Width + 3) & ~3;

        if (Bitmap.size() < static_cast<std::size_t>(SourcePitch * Glyph.Height))
        {
            UE_LOG(Error, "[TextRenderer] Glyph bitmap size mismatch. CharacterCode=%u BitmapSize=%d RequiredSize=%d", Character, static_cast<int32>(Bitmap.size()), SourcePitch * Glyph.Height);
            return false;
        }

        Glyph.AtlasX = CursorX;
        Glyph.AtlasY = CursorY;

        for (int32 Y = 0; Y < Glyph.Height; ++Y)
        {
            for (int32 X = 0; X < Glyph.Width; ++X)
            {
                const uint8 GrayValue = Bitmap[static_cast<std::size_t>(Y * SourcePitch + X)];
                const uint8 Alpha = static_cast<uint8>((static_cast<uint32>(GrayValue) * 255u) / 64u);

                const int32 DestinationIndex = ((Glyph.AtlasY + Y) * AtlasSize + Glyph.AtlasX + X) * 4;

                AtlasPixels[DestinationIndex + 0] = 255;
                AtlasPixels[DestinationIndex + 1] = 255;
                AtlasPixels[DestinationIndex + 2] = 255;
                AtlasPixels[DestinationIndex + 3] = Alpha;
            }
        }

        CursorX += Glyph.Width + AtlasPadding;
        CurrentRowHeight = (std::max)(CurrentRowHeight, Glyph.Height);

        bAtlasDirty = true;
    }

    GlyphCache.emplace(Character, Glyph);
    OutGlyph = Glyph;

    return true;
}

FVector2D FTextRenderer::MeasureText(const FString& Text)
{
    std::wstring WideText;

    if (!DecodeUTF8(Text, WideText))
    {
        return FVector2D::Zero;
    }

    float CurrentWidth = 0.0f;
    float MaximumWidth = 0.0f;
    int32 LineCount = 1;

    for (wchar_t Character : WideText)
    {
        if (Character == L'\n')
        {
            MaximumWidth = (std::max)(MaximumWidth, CurrentWidth);
            CurrentWidth = 0.0f;
            ++LineCount;
            continue;
        }

        FTextGlyph Glyph;

        if (GetGlyph(static_cast<uint32>(Character), Glyph))
        {
            CurrentWidth += static_cast<float>(Glyph.AdvanceX);
        }
    }

    MaximumWidth = (std::max)(MaximumWidth, CurrentWidth);

    return FVector2D(MaximumWidth, static_cast<float>(LineCount * FontHeight));
}

void FTextRenderer::AddText(const FString& Text, const FVector& Position, const FVector& CameraRight, const FVector& CameraUp, float WorldUnitsPerPixel, const FVector4& Color)
{
    if (FontDC == nullptr || WorldUnitsPerPixel <= 0.0f || !std::isfinite(WorldUnitsPerPixel))
    {
        return;
    }

    std::wstring WideText;

    if (!DecodeUTF8(Text, WideText))
    {
        return;
    }

    float CursorXOffset = 0.0f;
    float CursorYOffset = 0.0f;

    for (wchar_t Character : WideText)
    {
        if (Character == L'\n')
        {
            CursorXOffset = 0.0f;
            CursorYOffset -= static_cast<float>(FontHeight);
            continue;
        }

        FTextGlyph Glyph;

        if (!GetGlyph(static_cast<uint32>(Character), Glyph))
        {
            continue;
        }

        if (Glyph.Width <= 0 || Glyph.Height <= 0)
        {
            CursorXOffset += static_cast<float>(Glyph.AdvanceX);
            continue;
        }

        const float Left = CursorXOffset + static_cast<float>(Glyph.BearingX);
        const float Top = CursorYOffset + static_cast<float>(Glyph.BearingY);

        const FVector TopLeft = Position + CameraRight * (Left * WorldUnitsPerPixel) + CameraUp * (Top * WorldUnitsPerPixel);
        const FVector TopRight = TopLeft + CameraRight * (static_cast<float>(Glyph.Width) * WorldUnitsPerPixel);
        const FVector BottomLeft = TopLeft - CameraUp * (static_cast<float>(Glyph.Height) * WorldUnitsPerPixel);
        const FVector BottomRight = BottomLeft + CameraRight * (static_cast<float>(Glyph.Width) * WorldUnitsPerPixel);

        const float U0 = static_cast<float>(Glyph.AtlasX) / static_cast<float>(AtlasSize);
        const float V0 = static_cast<float>(Glyph.AtlasY) / static_cast<float>(AtlasSize);
        const float U1 = static_cast<float>(Glyph.AtlasX + Glyph.Width) / static_cast<float>(AtlasSize);
        const float V1 = static_cast<float>(Glyph.AtlasY + Glyph.Height) / static_cast<float>(AtlasSize);

        PendingVertices.Add({TopLeft.x, TopLeft.y, TopLeft.z, Color.x, Color.y, Color.z, Color.w, U0, V0});
        PendingVertices.Add({TopRight.x, TopRight.y, TopRight.z, Color.x, Color.y, Color.z, Color.w, U1, V0});
        PendingVertices.Add({BottomRight.x, BottomRight.y, BottomRight.z, Color.x, Color.y, Color.z, Color.w, U1, V1});

        PendingVertices.Add({TopLeft.x, TopLeft.y, TopLeft.z, Color.x, Color.y, Color.z, Color.w, U0, V0});
        PendingVertices.Add({BottomRight.x, BottomRight.y, BottomRight.z, Color.x, Color.y, Color.z, Color.w, U1, V1});
        PendingVertices.Add({BottomLeft.x, BottomLeft.y, BottomLeft.z, Color.x, Color.y, Color.z, Color.w, U0, V1});

        CursorXOffset += static_cast<float>(Glyph.AdvanceX);
    }
}

bool FTextRenderer::RebuildAtlas(FRHI& Renderer)
{
    if (!bAtlasDirty)
    {
        return AtlasResource.IsValid();
    }

    AtlasResource.Release();

    if (!Renderer.CreateTexture2D(AtlasPixels.GetData(), AtlasSize, AtlasSize, AtlasResource))
    {
        return false;
    }

    bAtlasDirty = false;

    return true;
}

bool FTextRenderer::Flush(FRHI& Renderer, const FMatrix& View, const FMatrix& Projection)
{
    if (PendingVertices.IsEmpty())
    {
        return true;
    }

    if (!RebuildAtlas(Renderer))
    {
        UE_LOG(Error, "[TextRenderer] Failed to rebuild glyph atlas.");
        return false;
    }

    const bool bDrawn = Renderer.DrawDynamicVertices(PendingVertices.GetData(), static_cast<UINT>(PendingVertices.Num()), ERHIPrimitiveTopology::TriangleList, FMatrix::Identity, View, Projection, AtlasResource.ShaderResourceView, true, true);

    if (!bDrawn)
    {
        UE_LOG(Error, "[TextRenderer] DrawDynamicVertices failed. VertexCount=%d", PendingVertices.Num());
    }

    PendingVertices.Reset();

    return bDrawn;
}
