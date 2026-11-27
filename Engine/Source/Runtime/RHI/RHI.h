#pragma once

#include "Core/STL.h"
#include "Core/Math/Math.h"
#include "Renderer/FVertexSimple.h"
#include "Renderer/RenderSettings.h"
#include "RHI/RHIResources.h"

struct FTexture2DResource;

// Stencil usage for the selection outline.
//  None          : regular depth test, stencil untouched.
//  WriteMask     : regular depth test, and every covered pixel (even depth-failed) is marked in the stencil.
//  MaskedOutline : no depth test, draws only where the stencil is NOT marked, so the outline
//                  appears outside the selected silhouette only.
enum class EOutlineStencilMode : uint8
{
    None,
    WriteMask,
    MaskedOutline,
};

// Rendering Hardware Interface: everything the engine needs from the graphics API.
// Engine and editor code talk only to this interface; the graphics API lives in
// one backend (RHI/D3D11). Counterpart of Unreal's FDynamicRHI.
//
// Threading: calls that record GPU commands (Prepare, Draw*, Begin/End*, UpdateConstant,
// SwapBuffer, Resize*) belong on the rendering thread, inside ENQUEUE_RENDER_COMMAND.
// Resource creation and release (Create*Buffer, CreateTexture2D, Release*) are safe
// from the game thread, like RHICreate* in Unreal.
class FRHI
{
public:
    virtual ~FRHI() = default;

    // Device / swap chain
    virtual void Create(HWND hWindow) = 0;
    virtual bool IsReady() const = 0;
    virtual void Shutdown() = 0;
    virtual void SwapBuffer() = 0;
    virtual void ResizeBackBuffer(uint32 Width, uint32 Height) = 0;

    // Shaders and per-draw constants
    virtual void CreateShader() = 0;
    virtual void CreateConstantBuffer() = 0;
    virtual void UpdateConstant(FMatrix World, FMatrix View, FMatrix Projection, bool bIsOutline, bool bIsTextured = false, bool bForceUnlit = false, const FVector4& InMaterialColor = FVector4(1.0f, 1.0f, 1.0f, 1.0f), const FVector2D& InUVOffset = FVector2D(0.0f, 0.0f), bool bUseVertexNormals = false) = 0;
    virtual void SetViewMode(EViewMode InViewMode) = 0;

    // Frame
    virtual void Prepare() = 0;
    virtual void PrepareShader() = 0;

    // Drawing
    virtual void SetOutlineStencilMode(EOutlineStencilMode Mode) = 0;
    virtual void RenderPrimitive(FRHIBuffer* VertexBuffer, FRHIBuffer* IndexBuffer, uint32 IndexCount, bool bRenderOutline = false, FRHIShaderResourceView* TextureSRV = nullptr, uint32 StartIndexLocation = 0) = 0;
    virtual bool DrawDynamicVertices(const FVertexSimple* Vertices, uint32 VertexCount, ERHIPrimitiveTopology Topology, const FMatrix& World, const FMatrix& View, const FMatrix& Projection, FRHIShaderResourceView* TextureSRV = nullptr, bool bIsTextured = false, bool bAlphaBlend = false) = 0;
    virtual bool DrawDynamicIndexedLines(const FVertexSimple* Vertices, uint32 VertexCount, const uint32* Indices, uint32 IndexCount, const FMatrix& View, const FMatrix& Projection) = 0;

    // Resources
    virtual FRHIBuffer* CreateVertexBuffer(FVertexSimple* Vertices, uint32 ByteWidth) = 0;
    virtual void ReleaseVertexBuffer(FRHIBuffer* VertexBuffer) = 0;
    virtual FRHIBuffer* CreateIndexBuffer(const uint32* Indices, uint32 IndexCount) = 0;
    virtual void ReleaseIndexBuffer(FRHIBuffer* IndexBuffer) = 0;
    virtual bool CreateTexture2D(const uint8* Pixels, uint32 Width, uint32 Height, FTexture2DResource& OutResource) = 0;
    virtual void ReleaseTexture2D(FTexture2DResource& Resource) = 0;

    // Editor viewport render target (shown inside an ImGui panel)
    virtual void CreateViewportRenderTarget(uint32 Width, uint32 Height) = 0;
    virtual void ResizeViewportRenderTarget(uint32 Width, uint32 Height) = 0;
    virtual void BeginViewportRender() = 0;
    virtual void EndViewportRender() = 0;
    virtual void SetViewportRegion(float X, float Y, float Width, float Height) = 0;
    virtual uint32 GetViewportWidth() const = 0;
    virtual uint32 GetViewportHeight() const = 0;
    virtual FRHIShaderResourceView* GetViewportSRV() const = 0;

    // Asset thumbnail render target
    virtual void CreateAssetPreviewRenderTarget(uint32 Width, uint32 Height) = 0;
    virtual void BeginAssetPreviewRender() = 0;
    virtual void EndAssetPreviewRender() = 0;
    virtual FRHIShaderResourceView* GetAssetPreviewSRV() const = 0;
    // Copies the current asset preview image into a new texture the caller owns.
    virtual bool CopyAssetPreviewToTexture(FTexture2DResource& OutResource) = 0;

    // Native API objects, only for third-party code that needs them (ImGui backend).
    virtual void* GetNativeDevice() const = 0;
    virtual void* GetNativeDeviceContext() const = 0;
};

// The active RHI, created by RHICreate and destroyed by RHIDestroy.
extern FRHI* GRHI;

// Creates the platform's RHI backend (D3D11) and sets GRHI.
FRHI* RHICreate();
void RHIDestroy();
