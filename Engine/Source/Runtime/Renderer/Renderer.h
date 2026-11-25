#pragma once

#include <d3d11.h>
#include "FVertexSimple.h"
#include "Core/STL.h"
#include "Core/Math/Math.h"
#include "Renderer/RenderSettings.h"

struct FTexture2DResource;

struct FConstants
{
    FMatrix World;
    FMatrix View;
    FMatrix Projection;
    uint32 IsOutline = 0;
    uint32 IsTextured = 0;
    uint32 IsUnlit = 0;
    uint32 Padding = 0;
};

class URenderer
{
public:
    void Create(HWND hWindow);
    void CreateDeviceAndSwapChain(HWND hWindow);
    void ReleaseDeviceAndSwapChain();
    void CreateFrameBuffer();
    void ReleaseFrameBuffer();
    void CreateRasterizerState();
    void ReleaseRasterizerState();
    void Release();
    void SwapBuffer();

    void CreateShader();
    void ReleaseShader();

    void Prepare();
    void PrepareShader();
    void RenderPrimitive(ID3D11Buffer* VertexBuffer, ID3D11Buffer* IndexBuffer, UINT IndexCount, bool bRenderOutline = false, ID3D11ShaderResourceView* TextureSRV = nullptr);
    bool DrawDynamicVertices(const FVertexSimple* Vertices, UINT VertexCount, D3D11_PRIMITIVE_TOPOLOGY Topology, const FMatrix& World, const FMatrix& View, const FMatrix& Projection, ID3D11ShaderResourceView* TextureSRV = nullptr, bool bIsTextured = false, bool bAlphaBlend = false);
    bool DrawDynamicIndexedLines(const FVertexSimple* Vertices, UINT VertexCount, const uint32* Indices, UINT IndexCount, const FMatrix& View, const FMatrix& Projection);
    void SetViewMode(EViewMode InViewMode);

    ID3D11Buffer* CreateVertexBuffer(FVertexSimple* vertices, UINT byteWidth);
    void ReleaseVertexBuffer(ID3D11Buffer* VertexBuffer);
    ID3D11Buffer* CreateIndexBuffer(const uint32* Indices, UINT IndexCount);
    void ReleaseIndexBuffer(ID3D11Buffer* IndexBuffer);
    bool CreateTexture2D(const uint8* Pixels, uint32 Width, uint32 Height, FTexture2DResource& OutResource);

    void CreateConstantBuffer();
    void ReleaseConstantBuffer();
    void UpdateConstant(FMatrix World, FMatrix View, FMatrix Projection, bool bIsOutline, bool bIsTextured = false, bool bForceUnlit = false);

    ID3D11Device* GetDevice();
    ID3D11DeviceContext* GetDeviceContext();

    void ResizeBackBuffer(uint32 Width, uint32 Height);
    void CreateViewportRenderTarget(uint32 Width, uint32 Height);
    void ResizeViewportRenderTarget(uint32 Width, uint32 Height);
    void BeginViewportRender();
    void EndViewportRender();
    void ReleaseViewportRenderTarget();

    void CreateAssetPreviewRenderTarget(uint32 Width, uint32 Height);
    void BeginAssetPreviewRender();
    void EndAssetPreviewRender();
    void ReleaseAssetPreviewRenderTarget();

    uint32 GetViewportWidth() const { return ViewportWidth; }
    uint32 GetViewportHeight() const { return ViewportHeight; }
    ID3D11ShaderResourceView* GetViewportSRV() const { return ViewportSRV; }
    ID3D11ShaderResourceView* GetAssetPreviewSRV() const { return AssetPreviewSRV; }
    ID3D11Texture2D* GetAssetPreviewTexture() const { return AssetPreviewTexture; }

    bool IsReady() const;
    void Shutdown();

private:
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

    FLOAT mClearColor[4] = { 0.025f, 0.025f, 0.025f, 1.0f };
    D3D11_VIEWPORT mViewportInfo;

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
