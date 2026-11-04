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
#include "../src/common/stereo_pose_history.hpp"
#include "../src/common/daytona_wizard_health.hpp"
#include "../src/common/daytona_host_ready.hpp"
#include "../src/common/daytona_producer_health.hpp"

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
    void invalidate_source_session() noexcept;
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
    producerhealth::Link producer_health_;
    producerhealth::Snapshot producer_health_status_{};
    std::uint32_t producer_health_last_reason_=0xffffffffu;
    bool producer_health_fresh_=false;
    pose::Link pose_writer_;
    std::uint64_t predicted_pose_sequence_=0;
    transport::StereoPoseHistory<std::array<XrView,2>,32> pose_history_{};
    std::array<XrView,2> accepted_render_pose_{};
    bool accepted_exact_pose_=false;
    transport::Snapshot eye_frame_;
    // Recycled x86->x64 8-MiB IPC input. Never expose a torn staging copy
    // as the last valid OpenXR game frame.
    transport::Snapshot staging_frame_;
    transport::StereoFrameGate frame_gate_;
    bool saw_diagnostic_=false;
    bool wizard_diag_enabled_=true;
    bool startup_ready_signaled_=false;
    wizard::RuntimeGate wizard_gate_;
};
}  // namespace m2vr::host
