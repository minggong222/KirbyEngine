#pragma once

#include "Core/Math/Math.h"
#include "FVertexSimple.h"
#include "Resources/TextureResource.h"

#include <unordered_map>
#include <Windows.h>

class URenderer;

class FTextRenderer
{
public:
    bool Initialize(const wchar_t* FontFace = L"Malgun Gothic", int32 InFontHeight = 32);
    void Shutdown();

    void BeginFrame();

    FVector2D MeasureText(const FString& Text);
    void AddText(const FString& Text, const FVector& Position, const FVector& CameraRight, const FVector& CameraUp, float WorldUnitsPerPixel, const FVector4& Color);

    bool Flush(URenderer& Renderer, const FMatrix& View, const FMatrix& Projection);

private:
    struct FTextGlyph
    {
        int32 Width = 0;
        int32 Height = 0;
        int32 BearingX = 0;
        int32 BearingY = 0;
        int32 AdvanceX = 0;

        int32 AtlasX = 0;
        int32 AtlasY = 0;
    };

    bool GetGlyph(uint32 Character, FTextGlyph& OutGlyph);
    bool RebuildAtlas(URenderer& Renderer);
    bool DecodeUTF8(const FString& Text, std::wstring& OutText) const;

    static constexpr int32 AtlasSize = 2048;
    static constexpr int32 AtlasPadding = 2;

    HDC FontDC = nullptr;
    HFONT Font = nullptr;
    HGDIOBJ PreviousFont = nullptr;

    int32 FontHeight = 32;

    int32 CursorX = AtlasPadding;
    int32 CursorY = AtlasPadding;
    int32 CurrentRowHeight = 0;

    bool bAtlasDirty = true;

    TArray<uint8> AtlasPixels;
    TArray<FVertexSimple> PendingVertices;

    std::unordered_map<uint32, FTextGlyph> GlyphCache;

    FTexture2DResource AtlasResource;
};
