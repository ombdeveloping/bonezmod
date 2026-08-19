#include "capture/dxgi_capture.h"

#include <dxgi1_2.h>
#include <dxgi1_6.h>
#include <cstring>

using Microsoft::WRL::ComPtr;

namespace bonez {

namespace {
ComPtr<IDXGIOutput1> output_for_monitor(ID3D11Device* dev, HMONITOR mon) {
    ComPtr<IDXGIDevice> dxdev;
    if (FAILED(dev->QueryInterface(IID_PPV_ARGS(&dxdev)))) return {};
    ComPtr<IDXGIAdapter> adapter;
    if (FAILED(dxdev->GetAdapter(&adapter))) return {};
    for (UINT i = 0;; ++i) {
        ComPtr<IDXGIOutput> out;
        if (adapter->EnumOutputs(i, &out) == DXGI_ERROR_NOT_FOUND) break;
        DXGI_OUTPUT_DESC d{};
        out->GetDesc(&d);
        if (d.Monitor == mon) {
            ComPtr<IDXGIOutput1> out1;
            if (SUCCEEDED(out.As(&out1))) return out1;
        }
    }
    return {};
}
} // namespace

bool DxgiCapture::init(HMONITOR mon) {
    UINT flags = 0;
#ifdef _DEBUG
    // flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
    D3D_FEATURE_LEVEL fl;
    HRESULT hr = D3D11CreateDevice(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
        nullptr, 0, D3D11_SDK_VERSION,
        dev_.GetAddressOf(), &fl, ctx_.GetAddressOf());
    if (FAILED(hr)) return false;

    auto out = output_for_monitor(dev_.Get(), mon);
    if (!out) return false;
    out->GetDesc(&out_desc_);

    hr = out->DuplicateOutput(dev_.Get(), dup_.GetAddressOf());
    if (FAILED(hr)) return false;
    return true;
}

void DxgiCapture::shutdown() {
    dup_.Reset(); staging_.Reset(); ctx_.Reset(); dev_.Reset();
}

bool DxgiCapture::ensure_staging(int w, int h) {
    if (staging_) {
        D3D11_TEXTURE2D_DESC d{};
        staging_->GetDesc(&d);
        if ((int)d.Width == w && (int)d.Height == h) return true;
        staging_.Reset();
    }
    D3D11_TEXTURE2D_DESC d{};
    d.Width = w; d.Height = h;
    d.MipLevels = 1; d.ArraySize = 1;
    d.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    d.SampleDesc.Count = 1;
    d.Usage = D3D11_USAGE_STAGING;
    d.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    return SUCCEEDED(dev_->CreateTexture2D(&d, nullptr, staging_.GetAddressOf()));
}

bool DxgiCapture::acquire(uint32_t timeout_ms, Frame& out) {
    if (!dup_) return false;

    DXGI_OUTDUPL_FRAME_INFO info{};
    ComPtr<IDXGIResource> res;
    HRESULT hr = dup_->AcquireNextFrame(timeout_ms, &info, res.GetAddressOf());
    if (hr == DXGI_ERROR_WAIT_TIMEOUT) return false;
    if (FAILED(hr)) return false;

    ComPtr<ID3D11Texture2D> tex;
    if (FAILED(res.As(&tex))) { dup_->ReleaseFrame(); return false; }

    D3D11_TEXTURE2D_DESC td{};
    tex->GetDesc(&td);
    if (!ensure_staging((int)td.Width, (int)td.Height)) {
        dup_->ReleaseFrame(); return false;
    }
    ctx_->CopyResource(staging_.Get(), tex.Get());

    D3D11_MAPPED_SUBRESOURCE map{};
    hr = ctx_->Map(staging_.Get(), 0, D3D11_MAP_READ, 0, &map);
    if (FAILED(hr)) { dup_->ReleaseFrame(); return false; }

    out.w = (int)td.Width;
    out.h = (int)td.Height;
    out.stride = (int)map.RowPitch;
    out.bgra.resize((size_t)map.RowPitch * td.Height);
    std::memcpy(out.bgra.data(), map.pData, out.bgra.size());
    out.seq = ++seq_;

    ctx_->Unmap(staging_.Get(), 0);
    dup_->ReleaseFrame();
    return true;
}

} // namespace bonez
