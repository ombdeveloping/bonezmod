#pragma once
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>
#include <cstdint>
#include <vector>

namespace bonez {

// Zero-injection screen source. Uses DXGI Output Duplication on the monitor
// currently displaying the RocketLeague window. Frames are provided as BGRA8.
class DxgiCapture {
public:
    struct Frame {
        std::vector<uint8_t> bgra;
        int w = 0;
        int h = 0;
        int stride = 0;
        uint64_t seq = 0;
    };

    bool init(HMONITOR mon);
    void shutdown();

    // Returns false if no new frame is available within timeout_ms; true on
    // success. `out` is only mutated when true.
    bool acquire(uint32_t timeout_ms, Frame& out);

private:
    Microsoft::WRL::ComPtr<ID3D11Device>         dev_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext>  ctx_;
    Microsoft::WRL::ComPtr<IDXGIOutputDuplication> dup_;
    Microsoft::WRL::ComPtr<ID3D11Texture2D>      staging_;
    DXGI_OUTPUT_DESC out_desc_{};
    uint64_t seq_ = 0;

    bool ensure_staging(int w, int h);
};

} // namespace bonez
