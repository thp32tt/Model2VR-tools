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
#include "../src/common/stereo_frame_ipc.h"
#include "../src/common/stereo_frame_policy.hpp"
#include "../src/common/stereo_pose_ipc.h"
#include "../src/common/daytona_wizard_health.hpp"

namespace m2vr::host {
using Microsoft::WRL::ComPtr;

struct EyeChain {
    XrSwapchain handle=XR_NULL_HANDLE;
    std::vector<XrSwapchainImageD3D11KHR> images;
    std::vector<ComPtr<ID3D11RenderTargetView>> rtvs;
    std::vector<std::uint64_t> uploaded_frame_ids; // per XR image, per eye
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
    void set_source_pid(std::uint32_t pid) { source_pid_=pid; }
private:
    void create_swapchains();
    void draw_frame(XrTime predicted_time);
    void report_wizard_health(bool tracking_valid, bool paired_frame_fresh);
    XrInstance instance_=XR_NULL_HANDLE;
    XrSystemId system_=XR_NULL_SYSTEM_ID;
    XrSession session_=XR_NULL_HANDLE;
    XrSpace local_space_=XR_NULL_HANDLE;
    ComPtr<ID3D11Device> device_;
    ComPtr<ID3D11DeviceContext> context_;
    std::vector<XrViewConfigurationView> view_configs_;
    std::array<EyeChain,2> chains_;
    std::uint32_t source_pid_=0;
    transport::SharedFrames shared_frames_;
    pose::Link pose_writer_;
    std::uint64_t predicted_pose_sequence_=0;
    transport::Snapshot eye_frame_;
    transport::StereoFrameGate frame_gate_;
    bool saw_diagnostic_=false;
    bool wizard_diag_enabled_=true;
    wizard::RuntimeGate wizard_gate_;
};
}  // namespace m2vr::host
