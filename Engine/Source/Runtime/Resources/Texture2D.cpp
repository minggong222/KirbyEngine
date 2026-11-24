#include "Texture2D.h"

#include "Renderer/Renderer.h"

#include <Windows.h>
#include <objbase.h>
#include <wincodec.h>
#include <wrl/client.h>

#include <limits>

#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "ole32.lib")

namespace
{
    class FScopedCOMInitialization
    {
    public:
        FScopedCOMInitialization()
        {
            Result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
            bShouldUninitialize = SUCCEEDED(Result);
        }

        ~FScopedCOMInitialization()
        {
            if (bShouldUninitialize)
            {
                CoUninitialize();
            }
        }

        bool IsValid() const
        {
            return SUCCEEDED(Result) || Result == RPC_E_CHANGED_MODE;
        }

    private:
        HRESULT Result = E_FAIL;
        bool bShouldUninitialize = false;
    };

    bool DecodeImage(const std::filesystem::path& FilePath, TArray<uint8>& OutPixels, uint32& OutWidth, uint32& OutHeight)
    {
        OutPixels.Reset();
        OutWidth = 0;
        OutHeight = 0;

        if (FilePath.empty())
        {
            return false;
        }

        FScopedCOMInitialization COMInitialization;

        if (!COMInitialization.IsValid())
        {
            return false;
        }

        using Microsoft::WRL::ComPtr;

        ComPtr<IWICImagingFactory> Factory;

        HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&Factory));

        if (FAILED(hr))
        {
            return false;
        }

        ComPtr<IWICBitmapDecoder> Decoder;

        hr = Factory->CreateDecoderFromFilename(FilePath.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &Decoder);

        if (FAILED(hr))
        {
            return false;
        }

        ComPtr<IWICBitmapFrameDecode> Frame;

        hr = Decoder->GetFrame(0, &Frame);

        if (FAILED(hr))
        {
            return false;
        }

        UINT Width = 0;
        UINT Height = 0;

        hr = Frame->GetSize(&Width, &Height);

        if (FAILED(hr) || Width == 0 || Height == 0 || Width > D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION || Height > D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION)
        {
            return false;
        }

        const uint64 RowPitch = static_cast<uint64>(Width) * 4;
        const uint64 PixelByteWidth = RowPitch * Height;

        if (RowPitch > (std::numeric_limits<UINT>::max)() || PixelByteWidth > static_cast<uint64>((std::numeric_limits<int32>::max)()) || PixelByteWidth > (std::numeric_limits<UINT>::max)())
        {
            return false;
        }

        ComPtr<IWICFormatConverter> Converter;

        hr = Factory->CreateFormatConverter(&Converter);

        if (FAILED(hr))
        {
            return false;
        }

        hr = Converter->Initialize(Frame.Get(), GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom);

        if (FAILED(hr))
        {
            return false;
        }

        TArray<uint8> Pixels;
        Pixels.SetNum(static_cast<int32>(PixelByteWidth));

        hr = Converter->CopyPixels(nullptr, static_cast<UINT>(RowPitch), static_cast<UINT>(PixelByteWidth), Pixels.GetData());

        if (FAILED(hr))
        {
            return false;
        }

        OutPixels = std::move(Pixels);
        OutWidth = Width;
        OutHeight = Height;

        return true;
    }
}

bool UTexture2D::Initialize(URenderer& Renderer, const std::filesystem::path& SourcePath)
{
    if (Resource.IsValid() || Resource.Texture != nullptr || Resource.ShaderResourceView != nullptr)
    {
        return false;
    }

    TArray<uint8> Pixels;

    uint32 Width = 0;
    uint32 Height = 0;

    if (!DecodeImage(SourcePath, Pixels, Width, Height))
    {
        return false;
    }

    if (!Renderer.CreateTexture2D(Pixels.GetData(), Width, Height, Resource))
    {
        return false;
    }

    return true;
}

UTexture2D::~UTexture2D()
{
    Resource.Release();
}