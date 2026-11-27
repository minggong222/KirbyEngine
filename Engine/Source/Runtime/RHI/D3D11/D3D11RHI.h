#pragma once

#include <d3d11.h>

#include "RHI/RHI.h"

// Shader constant buffer layout (matches shaderW0.hlsl)
struct FConstants
{
    FMatrix World;
    FMatrix View;
    FMatrix Projection;
    FMatrix NormalMatrix;
    FVector4 MaterialColor;
    FVector2D UVOffset;
    uint32 MaterialPadding[2] = {};
    uint32 IsOutline = 0;
    uint32 IsTextured = 0;
    uint32 IsUnlit = 0;
    uint32 UseVertexNormals = 0;
};

static_assert(sizeof(FConstants) % 16u == 0u);

// Direct3D 11 implementation of the RHI. The only place in the engine that touches D3D11.
// RHI handles (FRHIBuffer*, FRHITexture*, FRHIShaderResourceView*) are the native
// D3D11 interface pointers, converted with the To*/FromNative helpers below.
class FD3D11RHI final : public FRHI
{
public:
    // FRHI
    void Create(HWND hWindow) override;
    bool IsReady() const override;
    void Shutdown() override;
    void SwapBuffer() override;
    void ResizeBackBuffer(uint32 Width, uint32 Height) override;

    void CreateShader() override;
    void CreateConstantBuffer() override;
    void UpdateConstant(FMatrix World, FMatrix View, FMatrix Projection, bool bIsOutline, bool bIsTextured = false, bool bForceUnlit = false, const FVector4& InMaterialColor = FVector4(1.0f, 1.0f, 1.0f, 1.0f), const FVector2D& InUVOffset = FVector2D(0.0f, 0.0f), bool bUseVertexNormals = false) override;
    void SetViewMode(EViewMode InViewMode) override;

    void Prepare() override;
    void PrepareShader() override;

    void RenderPrimitive(FRHIBuffer* VertexBuffer, FRHIBuffer* IndexBuffer, uint32 IndexCount, bool bRenderOutline = false, FRHIShaderResourceView* TextureSRV = nullptr, uint32 StartIndexLocation = 0) override;
    bool DrawDynamicVertices(const FVertexSimple* Vertices, uint32 VertexCount, ERHIPrimitiveTopology Topology, const FMatrix& World, const FMatrix& View, const FMatrix& Projection, FRHIShaderResourceView* TextureSRV = nullptr, bool bIsTextured = false, bool bAlphaBlend = false) override;
    bool DrawDynamicIndexedLines(const FVertexSimple* Vertices, uint32 VertexCount, const uint32* Indices, uint32 IndexCount, const FMatrix& View, const FMatrix& Projection) override;

    FRHIBuffer* CreateVertexBuffer(FVertexSimple* Vertices, uint32 ByteWidth) override;
    void ReleaseVertexBuffer(FRHIBuffer* VertexBuffer) override;
    FRHIBuffer* CreateIndexBuffer(const uint32* Indices, uint32 IndexCount) override;
    void ReleaseIndexBuffer(FRHIBuffer* IndexBuffer) override;
    bool CreateTexture2D(const uint8* Pixels, uint32 Width, uint32 Height, FTexture2DResource& OutResource) override;
    void ReleaseTexture2D(FTexture2DResource& Resource) override;

    void CreateViewportRenderTarget(uint32 Width, uint32 Height) override;
    void ResizeViewportRenderTarget(uint32 Width, uint32 Height) override;
    void BeginViewportRender() override;
    void EndViewportRender() override;
    void SetViewportRegion(float X, float Y, float Width, float Height) override;
    uint32 GetViewportWidth() const override { return ViewportWidth; }
    uint32 GetViewportHeight() const override { return ViewportHeight; }
    FRHIShaderResourceView* GetViewportSRV() const override { return reinterpret_cast<FRHIShaderResourceView*>(ViewportSRV); }

    void CreateAssetPreviewRenderTarget(uint32 Width, uint32 Height) override;
    void BeginAssetPreviewRender() override;
    void EndAssetPreviewRender() override;
    FRHIShaderResourceView* GetAssetPreviewSRV() const override { return reinterpret_cast<FRHIShaderResourceView*>(AssetPreviewSRV); }
    bool CopyAssetPreviewToTexture(FTexture2DResource& OutResource) override;

    void* GetNativeDevice() const override { return mDevice; }
    void* GetNativeDeviceContext() const override { return mDeviceContext; }

private:
    static ID3D11Buffer* ToD3D(FRHIBuffer* Buffer) { return reinterpret_cast<ID3D11Buffer*>(Buffer); }
    static ID3D11ShaderResourceView* ToD3D(FRHIShaderResourceView* View) { return reinterpret_cast<ID3D11ShaderResourceView*>(View); }
    static ID3D11Texture2D* ToD3D(FRHITexture* Texture) { return reinterpret_cast<ID3D11Texture2D*>(Texture); }

    // Native implementations (D3D11 types)
    void RenderPrimitiveNative(ID3D11Buffer* VertexBuffer, ID3D11Buffer* IndexBuffer, UINT IndexCount, bool bRenderOutline, ID3D11ShaderResourceView* TextureSRV, UINT StartIndexLocation);
    bool DrawDynamicVerticesNative(const FVertexSimple* Vertices, UINT VertexCount, D3D11_PRIMITIVE_TOPOLOGY Topology, const FMatrix& World, const FMatrix& View, const FMatrix& Projection, ID3D11ShaderResourceView* TextureSRV, bool bIsTextured, bool bAlphaBlend);
    ID3D11Buffer* CreateVertexBufferNative(FVertexSimple* vertices, UINT byteWidth);
    void ReleaseVertexBufferNative(ID3D11Buffer* VertexBuffer);
    ID3D11Buffer* CreateIndexBufferNative(const uint32* Indices, UINT IndexCount);
    void ReleaseIndexBufferNative(ID3D11Buffer* IndexBuffer);
    ID3D11Device* GetDeviceNative();
    ID3D11DeviceContext* GetDeviceContextNative();

    void CreateDeviceAndSwapChain(HWND hWindow);
    void ReleaseDeviceAndSwapChain();
    void CreateFrameBuffer();
    void ReleaseFrameBuffer();
    void CreateRasterizerState();
    void ReleaseRasterizerState();
    void Release();
    void ReleaseShader();
    void ReleaseConstantBuffer();
    void ReleaseViewportRenderTarget();
    void ReleaseAssetPreviewRenderTarget();
    void ReleaseDynamicVertexBuffer();
    void ReleaseDynamicIndexBuffer();

    ID3D11Device* mDevice = nullptr;
    ID3D11DeviceContext* mDeviceContext = nullptr;
    IDXGISwapChain* mSwapChain = nullptr;

    ID3D11Texture2D* mFrameBuffer = nullptr;
    ID3D11RenderTargetView* mFrameBufferRTV = nullptr;
    ID3D11RasterizerState* mRasterizerState = nullptr;
    ID3D11RasterizerState* mOutlineRasterizerState = nullptr;
    ID3D11Buffer* mConstantBuffer = nullptr;

    FLOAT mClearColor[4] = {0.025f, 0.025f, 0.025f, 1.0f};
    D3D11_VIEWPORT mViewportInfo = {};

    ID3D11VertexShader* mSimpleVertexShader = nullptr;
    ID3D11PixelShader* mSimplePixelShader = nullptr;
    ID3D11InputLayout* mSimpleInputLayout = nullptr;
    ID3D11SamplerState* mTextureSamplerState = nullptr;

    unsigned int mStride = 0;
    bool bInitializationFailed = false;

    ID3D11Texture2D* ViewportTexture = nullptr;
    ID3D11RenderTargetView* ViewportRTV = nullptr;
    ID3D11ShaderResourceView* ViewportSRV = nullptr;
    ID3D11Texture2D* ViewportDepthTexture = nullptr;
    ID3D11DepthStencilView* ViewportDSV = nullptr;
    D3D11_VIEWPORT ViewportRenderInfo = {};
    ID3D11DepthStencilState* ViewportDepthState = nullptr;
    uint32 ViewportWidth = 0;
    uint32 ViewportHeight = 0;

    ID3D11Texture2D* AssetPreviewTexture = nullptr;
    ID3D11RenderTargetView* AssetPreviewRTV = nullptr;
    ID3D11ShaderResourceView* AssetPreviewSRV = nullptr;
    ID3D11Texture2D* AssetPreviewDepthTexture = nullptr;
    ID3D11DepthStencilView* AssetPreviewDSV = nullptr;
    D3D11_VIEWPORT AssetPreviewRenderInfo = {};
    uint32 AssetPreviewWidth = 0;
    uint32 AssetPreviewHeight = 0;

    ID3D11Buffer* mDynamicVertexBuffer = nullptr;
    UINT mDynamicVertexCapacity = 0;
    ID3D11Buffer* mDynamicIndexBuffer = nullptr;
    UINT mDynamicIndexCapacity = 0;

    ID3D11RasterizerState* mWireframeRasterizerState = nullptr;
    ID3D11BlendState* mAlphaBlendState = nullptr;
    ID3D11DepthStencilState* ViewportOverlayDepthState = nullptr;

    bool bWireframeEnabled = false;
    bool bUnlitEnabled = false;
};
