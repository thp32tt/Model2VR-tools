// Model2VR public native-stereo frame submission ABI.
// Only use this callback AFTER a Model 2 render integration produced TWO
// independently rendered game-eye images. It is not a 2D -> 3D converter.
#include "../common/stereo_frame_ipc.h"
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
