#pragma once
#include <Windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <array>
#include <cstdint>
#include <vector>

namespace m2vr::host {
using Microsoft::WRL::ComPtr;

struct EyeChain {
    XrSwapchain handle=XR_NULL_HANDLE;
    std::vector<XrSwapchainImageD3D11KHR> images;
    std::vector<ComPtr<ID3D11RenderTargetView>> rtvs;
    std::uint32_t width=0;
    std::uint32_t height=0;
};
void check(XrResult result,const char* operation);
void checkhr(HRESULT result,const char* operation);

class XrApp {
public:
    ~XrApp();
    void initialize();
    void run(int max_frames);
private:
    void create_swapchains();
    void draw_frame(XrTime predicted_time);
    XrInstance instance_=XR_NULL_HANDLE;
    XrSystemId system_=XR_NULL_SYSTEM_ID;
    XrSession session_=XR_NULL_HANDLE;
    XrSpace local_space_=XR_NULL_HANDLE;
    ComPtr<ID3D11Device> device_;
    ComPtr<ID3D11DeviceContext> context_;
    std::vector<XrViewConfigurationView> view_configs_;
    std::array<EyeChain,2> chains_;
};
}  // namespace m2vr::host
