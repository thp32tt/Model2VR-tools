// Model2VR public native-stereo frame submission ABI.
// Only use this callback AFTER a Model 2 render integration produced TWO
// independently rendered game-eye images. It is not a 2D -> 3D converter.
#include "../common/stereo_frame_ipc.h"
#include "../common/stereo_pose_ipc.h"
#include <Windows.h>
#include <cstdint>
#include <mutex>

namespace {
std::mutex g_writer_mutex;
m2vr::transport::SharedFrames g_writer;
bool g_initialized=false;
}
// Called from the emulator's rendering integration, NOT the game simulation
// or force-feedback callback. The main emulator remains single-step/frame.
extern "C" __declspec(dllexport) BOOL __stdcall Model2VR_OpenStereoWriter() {
    std::scoped_lock lock(g_writer_mutex);
    if(g_initialized)return TRUE;
    g_initialized=g_writer.Create(GetCurrentProcessId());
    return g_initialized?TRUE:FALSE;
}
// RGBA8 full-frame, tightly packed, 1024x1024 per eye.
// The caller retains ownership of the input buffers. Non-reentrant.
// Never pass a cloned mono frame as a native stereo pair.
extern "C" __declspec(dllexport) BOOL __stdcall Model2VR_SubmitStereoRGBA(
    const unsigned char* left_rgba,
    const unsigned char* right_rgba,
    unsigned long long emulated_frame_id,
    unsigned int width,
    unsigned int height,
    unsigned int stride_bytes) {
    if(width!=m2vr::transport::kWidth ||
       height!=m2vr::transport::kHeight ||
       stride_bytes!=m2vr::transport::kPitch ||
       !left_rgba || !right_rgba || emulated_frame_id==0)return FALSE;
    std::scoped_lock lock(g_writer_mutex);
    if(!g_initialized)return FALSE;
    return g_writer.Publish(left_rgba,right_rgba,emulated_frame_id,
                            m2vr::transport::FrameKind::GameStereo)?TRUE:FALSE;
}

// Present-bound original game backbuffer fallback. Only a real D3D9 capture
// calls this after the original Present succeeds. Sending one image twice is
// explicitly marked mono, never mislabeled as GameStereo.
extern "C" __declspec(dllexport) BOOL __stdcall Model2VR_SubmitOriginalGameMonoRGBA(
    const unsigned char* original_rgba,unsigned long long original_frame,
    unsigned int width,unsigned int height,unsigned int stride_bytes) {
    if(width!=m2vr::transport::kWidth ||
       height!=m2vr::transport::kHeight ||
       stride_bytes!=m2vr::transport::kPitch ||
       !original_rgba || !original_frame)return FALSE;
    std::scoped_lock lock(g_writer_mutex);
    if(!g_initialized)return FALSE;
    // 4-MiB single-eye packet, not a cloned 8-MiB binocular payload.
    // The wire remains v1; pre-compact hosts reject kind=4 fail-closed.
    return g_writer.Publish(original_rgba,nullptr,original_frame,
               m2vr::transport::FrameKind::CompactMono)?TRUE:FALSE;
}

// The proven CPU-world renderer can include the exact predicted pose used
// for independent eye projection. The legacy API stays untagged.
extern "C" __declspec(dllexport) BOOL __stdcall Model2VR_SubmitStereoRGBAWithPose(
    const unsigned char* left_rgba,const unsigned char* right_rgba,
    unsigned long long emulated_frame_id,unsigned int width,
    unsigned int height,unsigned int stride_bytes,
    unsigned long long pose_sequence) {
    if(width!=m2vr::transport::kWidth ||
       height!=m2vr::transport::kHeight ||
       stride_bytes!=m2vr::transport::kPitch ||
       !left_rgba || !right_rgba || !emulated_frame_id ||
       !pose_sequence || pose_sequence>0xffffffffull)return FALSE;
    std::scoped_lock lock(g_writer_mutex);
    if(!g_initialized)return FALSE;
    return g_writer.Publish(left_rgba,right_rgba,emulated_frame_id,
               m2vr::transport::FrameKind::GameStereo,
               static_cast<std::uint32_t>(pose_sequence))?TRUE:FALSE;
}

// Separate real CPU-world provenance, still not a Quest HMD validation.
extern "C" __declspec(dllexport) BOOL __stdcall
Model2VR_SubmitVerifiedCpuWorldRGBAWithPose(
    const unsigned char* left,const unsigned char* right,
    unsigned long long frame,unsigned int width,unsigned int height,
    unsigned int pitch,unsigned long long pose) {
    if(width!=m2vr::transport::kWidth ||
       height!=m2vr::transport::kHeight ||
       pitch!=m2vr::transport::kPitch || !left || !right || !frame ||
       !pose || pose>0xffffffffull)return FALSE;
    std::scoped_lock lock(g_writer_mutex);
    if(!g_initialized)return FALSE;
    return g_writer.Publish(left,right,frame,
         m2vr::transport::FrameKind::VerifiedCpuWorldStereo,
         static_cast<std::uint32_t>(pose))?TRUE:FALSE;
}
// Returns an atomic, coherent *pair* of predicted OpenXR eye views, not two
// independently sampled poses. Sample once per emulated render frame.
namespace {
std::mutex g_pose_mutex;
m2vr::pose::Link g_pose_reader;
}
extern "C" __declspec(dllexport) BOOL __stdcall Model2VR_ReadPredictedEyeViews(
    m2vr::pose::Frame* output) {
    if(!output)return FALSE;
    std::scoped_lock lock(g_pose_mutex);
    if(!g_pose_reader.Valid() &&
       !g_pose_reader.Open(GetCurrentProcessId()))return FALSE;
    m2vr::pose::Frame snapshot{};
    if(!g_pose_reader.Read(snapshot))return FALSE;
    *output=snapshot;
    return TRUE;
}
