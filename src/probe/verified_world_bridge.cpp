// Native Model2 stereo world->RGBA bridge. Explicit render adapter only:
// no automatic D3D9 draw replay and no second simulation/input/FFB call.
#include "verified_world_bridge.h"
#include "../vr/model2_stereo_raster.hpp"
#include <Windows.h>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <vector>

extern "C" BOOL __stdcall Model2VR_OpenStereoWriter();
extern "C" BOOL __stdcall Model2VR_SubmitStereoRGBA(
    const unsigned char*,const unsigned char*,unsigned long long,
    unsigned int,unsigned int,unsigned int);
extern "C" BOOL __stdcall Model2VR_ReadPredictedEyeViews(
    m2vr::pose::Frame*);

namespace {
struct Bridge {
    std::mutex mutex;
    std::unique_ptr<m2vr::vr::StereoWorldRaster> raster;
    m2vr::vr::Model2FramePoseLatch latch;
    m2vr::vr::Model2CameraCalibration calibration{};
    m2vr::vr::DrawEvidence evidence{};
    std::uint64_t frame=0,last_completed=0;
    DWORD owner=0;
    unsigned draw_count=0,triangle_count=0;
    bool active=false;
    void Abort()noexcept{
        active=false;owner=0;frame=0;draw_count=0;triangle_count=0;
        evidence={};calibration={};
    }
};
Bridge g;
// Fault-safe boundary for pointers supplied by an external emulator module.
// No C++ objects with destructors live inside this SEH block.
bool GuardedCopy(void* destination,const void* source,std::size_t bytes)noexcept{
    if(!destination || !source || !bytes)return false;
    __try {
        std::memcpy(destination,source,bytes);
        return true;
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}
m2vr::vr::DrawEvidence EvidenceFromProof(std::uint32_t flags)noexcept{
    using namespace m2vr::vrbridge;
    using namespace m2vr::vr;
    DrawEvidence e{};
    if((flags&kRequiredWorldProof)!=kRequiredWorldProof ||
       (flags&~kRequiredWorldProof)!=0)return e;
    e.verified_cpu_pass=RenderPass::World3D;
    e.scene_is_gameplay=true;
    e.current_render_target_is_backbuffer=true;
    e.render_target_known=true;
    e.depth_test_enabled=true;
    e.frame_pose_valid=true;
    e.pixel_projection_calibrated=true;
    return e;
}
bool Owner()noexcept{return g.active && g.owner==GetCurrentThreadId();}
}

extern "C" BOOL __stdcall Model2VR_BeginVerifiedWorldFrame(
    unsigned long long frame_id,const float* axis,float units,
    float near_plane,unsigned int proof) {
    std::scoped_lock lock(g.mutex);
    if(g.active || !frame_id || frame_id<=g.last_completed ||
       !axis || proof!=m2vr::vrbridge::kRequiredWorldProof)
        return FALSE;
    try {
        m2vr::vr::Model2CameraCalibration cal{};
        if(!GuardedCopy(cal.view_to_xr.data(),axis,
                        sizeof(float)*9))return FALSE;
        cal.units_per_meter=units;
        cal.min_depth_m=near_plane;
        cal.runtime_confirmed=true;
        if(!m2vr::vr::ValidCalibration(cal))return FALSE;
        const auto evidence=EvidenceFromProof(proof);
        if(m2vr::vr::ChooseEyeAction(evidence)!=
           m2vr::vr::EyeAction::StereoGeometry)return FALSE;
        m2vr::pose::Frame predicted{};
        if(!Model2VR_ReadPredictedEyeViews(&predicted) ||
           !g.latch.Begin(frame_id,predicted))return FALSE;
        if(!g.raster)
            g.raster=std::make_unique<m2vr::vr::StereoWorldRaster>();
        g.raster->Clear();
        g.frame=frame_id;
        g.owner=GetCurrentThreadId();
        g.calibration=cal;
        g.evidence=evidence;
        g.draw_count=0;
        g.triangle_count=0;
        g.active=true;
        return TRUE;
    }catch(...){
        g.Abort();
        return FALSE;
    }
}
extern "C" BOOL __stdcall Model2VR_AppendVerifiedWorldTriangles(
    const float* xyz,unsigned int vertex_count,
    const std::uint32_t* indices,unsigned int index_count,
    const std::uint32_t* colors) {
    std::scoped_lock lock(g.mutex);
    if(!Owner() || !xyz || !indices || !colors ||
       !vertex_count || vertex_count>4096 ||
       !index_count || index_count%3!=0 ||
       index_count>3u*2048u ||
       index_count/3u>2048u-g.triangle_count ||
       g.draw_count>=2048u) {
        if(Owner())g.Abort();
        return FALSE;
    }
    try {
        std::vector<m2vr::vr::CpuPoint> vertices(vertex_count);
        std::vector<std::uint32_t> safe_indices(index_count);
        std::vector<std::uint32_t> safe_colors(index_count/3u);
        if(!GuardedCopy(vertices.data(),xyz,
                        std::size_t(vertex_count)*3u*sizeof(float)) ||
           !GuardedCopy(safe_indices.data(),indices,
                        std::size_t(index_count)*sizeof(std::uint32_t)) ||
           !GuardedCopy(safe_colors.data(),colors,
                        std::size_t(index_count/3u)*sizeof(std::uint32_t))){
            g.Abort();return FALSE;
        }
        m2vr::vr::ClippedStereoBatch clipped{};
        if(!m2vr::vr::BuildClippedStereoBatch(
            vertices.data(),vertices.size(),safe_indices.data(),index_count,
            g.frame,g.latch,g.calibration,g.evidence,clipped) ||
           !g.raster->Draw(clipped,safe_colors.data(),index_count/3u)) {
            g.Abort();
            return FALSE;
        }
        ++g.draw_count;
        g.triangle_count+=index_count/3u;
        return TRUE;
    }catch(...){
        g.Abort();
        return FALSE;
    }
}
extern "C" BOOL __stdcall Model2VR_EndVerifiedWorldFrame(){
    std::scoped_lock lock(g.mutex);
    if(!Owner())return FALSE;
    if(!g.draw_count || !g.raster || !g.raster->DrawnPixels()){
        g.Abort();
        return FALSE;
    }
    // One game frame, two independently clipped/depth-tested images.
    // Only after both CPU render targets finish can the writer publish.
    const auto& a=g.raster->Eye(0);
    const auto& b=g.raster->Eye(1);
    const bool success=Model2VR_OpenStereoWriter() &&
        Model2VR_SubmitStereoRGBA(a.data(),b.data(),g.frame,
            m2vr::transport::kWidth,m2vr::transport::kHeight,
            m2vr::transport::kPitch);
    if(success)g.last_completed=g.frame;
    g.Abort();
    return success?TRUE:FALSE;
}
extern "C" void __stdcall Model2VR_AbortVerifiedWorldFrame() {
    std::scoped_lock lock(g.mutex);
    if(Owner())g.Abort();
}
