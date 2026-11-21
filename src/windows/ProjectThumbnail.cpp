// Explorer reads the embedded composite without loading Qt or the editable layers.
#define NOMINMAX
#include <windows.h>

#include "../storage/ProjectBinaryHeader.h"
#include <algorithm>
#include <array>
#include <new>
#include <propsys.h>
#include <shlwapi.h>
#include <thumbcache.h>
#include <vector>
#include <wincodec.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;
using PhotoCrap::Storage::BinaryHeader;

namespace
{
const CLSID thumbnailClass = {0xda388cad,
                              0x0a58,
                              0x4c18,
                              {0xbe, 0x98, 0x35, 0xc2, 0xaf, 0xa1, 0x8b, 0x01}};
LONG        activeObjects  = 0;

HRESULT readExactly(IStream *stream, void *data, ULONG count)
{
    ULONG   read = 0;
    HRESULT hr   = stream->Read(data, count, &read);
    if (FAILED(hr))
        return hr;
    return read == count ? S_OK : STG_E_READFAULT;
}

HRESULT readThumbnail(IStream *project, UINT limit, HBITMAP *bitmap)
{
    HRESULT hr = project->Seek({}, STREAM_SEEK_SET, nullptr);
    if (FAILED(hr))
        return hr;
    std::array<char, BinaryHeader::Size> bytes{};
    BinaryHeader                         header;
    hr = readExactly(project, bytes.data(), static_cast<ULONG>(bytes.size()));
    if (FAILED(hr) || !header.read(bytes.data()) || !header.preview)
        return WINCODEC_ERR_BADHEADER;
    std::vector<BYTE> jpeg(static_cast<size_t>(header.preview));
    hr = readExactly(project, jpeg.data(), static_cast<ULONG>(jpeg.size()));
    if (FAILED(hr))
        return hr;
    ComPtr<IStream> stream;
    stream.Attach(SHCreateMemStream(jpeg.data(), static_cast<UINT>(jpeg.size())));
    if (!stream)
        return E_OUTOFMEMORY;
    ComPtr<IWICImagingFactory> factory;
    hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                          IID_PPV_ARGS(factory.GetAddressOf()));
    if (FAILED(hr))
        return hr;
    ComPtr<IWICBitmapDecoder> decoder;
    hr = factory->CreateDecoder(GUID_ContainerFormatJpeg, &GUID_VendorMicrosoft, &decoder);
    if (FAILED(hr))
        return hr;
    hr = decoder->Initialize(stream.Get(), WICDecodeMetadataCacheOnDemand);
    if (FAILED(hr))
        return hr;
    ComPtr<IWICBitmapFrameDecode> frame;
    hr = decoder->GetFrame(0, &frame);
    if (FAILED(hr))
        return hr;
    UINT width, height;
    hr = frame->GetSize(&width, &height);
    if (FAILED(hr) || !width || !height || width > 1280 || height > 1280)
        return WINCODEC_ERR_BADIMAGE;

    ComPtr<IWICBitmapSource> source  = frame;
    const UINT               longest = std::max(width, height);
    if (longest > limit) {
        width  = std::max(1u, width * limit / longest);
        height = std::max(1u, height * limit / longest);
        ComPtr<IWICBitmapScaler> scaler;
        hr = factory->CreateBitmapScaler(&scaler);
        if (FAILED(hr))
            return hr;
        hr = scaler->Initialize(frame.Get(), width, height, WICBitmapInterpolationModeFant);
        if (FAILED(hr))
            return hr;
        source = scaler;
    }
    ComPtr<IWICBitmapSource> pixels;
    hr = WICConvertBitmapSource(GUID_WICPixelFormat32bppBGR, source.Get(), &pixels);
    if (FAILED(hr))
        return hr;
    BITMAPINFO info{};
    info.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth       = static_cast<LONG>(width);
    info.bmiHeader.biHeight      = -static_cast<LONG>(height);
    info.bmiHeader.biPlanes      = 1;
    info.bmiHeader.biBitCount    = 32;
    info.bmiHeader.biCompression = BI_RGB;
    void   *data                 = nullptr;
    HBITMAP image = CreateDIBSection(nullptr, &info, DIB_RGB_COLORS, &data, nullptr, 0);
    if (!image)
        return E_OUTOFMEMORY;
    hr = pixels->CopyPixels(nullptr, width * 4, width * height * 4, static_cast<BYTE *>(data));
    if (FAILED(hr))
        DeleteObject(image);
    else
        *bitmap = image;
    return hr;
}

template <class Derived>
class Lifetime
{
public:
    ULONG retain()
    {
        return InterlockedIncrement(&references);
    }
    ULONG release()
    {
        ULONG remaining = InterlockedDecrement(&references);
        if (!remaining)
            delete static_cast<Derived *>(this);
        return remaining;
    }

protected:
    Lifetime()
    {
        InterlockedIncrement(&activeObjects);
    }
    ~Lifetime()
    {
        InterlockedDecrement(&activeObjects);
    }

private:
    LONG references = 1;
};

class Thumbnail final : public Lifetime<Thumbnail>,
                        public IThumbnailProvider,
                        public IInitializeWithStream
{
public:
    IFACEMETHODIMP QueryInterface(REFIID iid, void **out) override
    {
        if (!out)
            return E_POINTER;
        if (iid == IID_IUnknown || iid == __uuidof(IThumbnailProvider))
            *out = static_cast<IThumbnailProvider *>(this);
        else if (iid == __uuidof(IInitializeWithStream))
            *out = static_cast<IInitializeWithStream *>(this);
        else {
            *out = nullptr;
            return E_NOINTERFACE;
        }
        AddRef();
        return S_OK;
    }
    IFACEMETHODIMP_(ULONG) AddRef() override
    {
        return retain();
    }
    IFACEMETHODIMP_(ULONG) Release() override
    {
        return release();
    }
    IFACEMETHODIMP Initialize(IStream *source, DWORD) override
    {
        if (!source)
            return E_INVALIDARG;
        if (stream)
            return HRESULT_FROM_WIN32(ERROR_ALREADY_INITIALIZED);
        stream = source;
        return S_OK;
    }
    IFACEMETHODIMP GetThumbnail(UINT size, HBITMAP *bitmap, WTS_ALPHATYPE *alpha) override
    {
        if (!bitmap || !alpha)
            return E_POINTER;
        *bitmap = nullptr;
        *alpha  = WTSAT_RGB;
        if (!size || !stream)
            return E_INVALIDARG;
        try {
            return readThumbnail(stream.Get(), size, bitmap);
        } catch (const std::bad_alloc &) {
            return E_OUTOFMEMORY;
        }
    }

private:
    ComPtr<IStream> stream;
};

class Factory final : public Lifetime<Factory>, public IClassFactory
{
public:
    IFACEMETHODIMP QueryInterface(REFIID iid, void **out) override
    {
        if (!out)
            return E_POINTER;
        if (iid == IID_IUnknown || iid == __uuidof(IClassFactory))
            *out = static_cast<IClassFactory *>(this);
        else {
            *out = nullptr;
            return E_NOINTERFACE;
        }
        AddRef();
        return S_OK;
    }
    IFACEMETHODIMP_(ULONG) AddRef() override
    {
        return retain();
    }
    IFACEMETHODIMP_(ULONG) Release() override
    {
        return release();
    }
    IFACEMETHODIMP CreateInstance(IUnknown *outer, REFIID iid, void **out) override
    {
        if (!out)
            return E_POINTER;
        *out = nullptr;
        if (outer)
            return CLASS_E_NOAGGREGATION;
        auto *provider = new (std::nothrow) Thumbnail;
        if (!provider)
            return E_OUTOFMEMORY;
        HRESULT hr = provider->QueryInterface(iid, out);
        provider->Release();
        return hr;
    }
    IFACEMETHODIMP LockServer(BOOL lock) override
    {
        if (lock)
            InterlockedIncrement(&activeObjects);
        else
            InterlockedDecrement(&activeObjects);
        return S_OK;
    }
};
} // namespace

STDAPI DllGetClassObject(REFCLSID clsid, REFIID iid, void **out)
{
    if (!out)
        return E_POINTER;
    *out = nullptr;
    if (clsid != thumbnailClass)
        return CLASS_E_CLASSNOTAVAILABLE;
    auto *factory = new (std::nothrow) Factory;
    if (!factory)
        return E_OUTOFMEMORY;
    HRESULT hr = factory->QueryInterface(iid, out);
    factory->Release();
    return hr;
}

STDAPI DllCanUnloadNow()
{
    return InterlockedCompareExchange(&activeObjects, 0, 0) == 0 ? S_OK : S_FALSE;
}
