// Native Model2 stereo world->RGBA bridge. Explicit render adapter only:
// no automatic D3D9 draw replay and no second simulation/input/FFB call.
#include "verified_world_bridge.h"
#include "../vr/model2_stereo_raster.hpp"
#include "../vr/model2_head_recenter.hpp"
#include "../vr/model2_camera_source_contract.hpp"
#include "daytona_eye_pose_gate.hpp"
#include "daytona_source_pose_commit_gate.hpp"
#include "verified_world_present_gate.hpp"
#include "../common/daytona_producer_health.hpp"
#include "logger.h"
#include <Windows.h>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <vector>

extern "C" BOOL __stdcall Model2VR_OpenStereoWriter();
extern "C" BOOL __stdcall Model2VR_SubmitStereoRGBAWithPose(
    const unsigned char*,const unsigned char*,unsigned long long,
    unsigned int,unsigned int,unsigned int,unsigned long long);
extern "C" BOOL __stdcall Model2VR_ReadPredictedEyeViews(
    m2vr::pose::Frame*);

namespace {
struct Bridge {
    std::mutex mutex;
    std::unique_ptr<m2vr::vr::StereoWorldRaster> raster;
    m2vr::vr::Model2FramePoseLatch latch;
    m2vr::vr::Model2HeadRecenter head_origin;
    m2vr::vr::Model2CameraEpochContract camera_epoch;
    m2vr::producerhealth::Link producer_health;
    m2vr::vr::Model2CameraCalibration calibration{};
    m2vr::vr::DrawEvidence evidence{};
    std::uint64_t frame=0,last_completed=0,pose_sequence=0;
    std::uint64_t pose_published_qpc=0;
    DWORD owner=0;
    unsigned draw_count=0,triangle_count=0;
    bool active=false,pending_present=false;
    void Abort()noexcept{
        active=false;pending_present=false;
        owner=0;frame=0;pose_sequence=0;pose_published_qpc=0;
        draw_count=0;triangle_count=0;
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
// A newer HMD pose cannot revive geometry drawn using an expired source.
bool OriginalWorldPoseLive()noexcept {
    if(!g.pose_sequence || !g.pose_published_qpc)return false;
    m2vr::pose::Frame latest{};
    if(!Model2VR_ReadPredictedEyeViews(&latest))return false;
    LARGE_INTEGER now{},frequency{};
    if(!QueryPerformanceCounter(&now) ||
       !QueryPerformanceFrequency(&frequency) ||
       now.QuadPart<=0 || frequency.QuadPart<=0)return false;
    return m2vr::daytona::OriginalStereoPoseStillLiveAtPresent(
        {g.pose_sequence,g.pose_published_qpc},latest,
        static_cast<std::uint64_t>(now.QuadPart),
        static_cast<std::uint64_t>(frequency.QuadPart));
}
}
extern "C" BOOL __stdcall Model2VR_VerifiedWorldFrameOwnedByCurrentThread(){
    std::scoped_lock lock(g.mutex);
    return Owner()?TRUE:FALSE;
}
// Read-only handoff claim. Only an authenticated Begin/Append/End can arm
// this exact same-thread original-Present source; no automatic world proof.
extern "C" BOOL __stdcall Model2VR_HasVerifiedWorldPendingForOriginalPresent(
    unsigned long long original_frame){
    std::scoped_lock lock(g.mutex);
    return m2vr::vrbridge::PreferPendingCpuWorld(
        g.frame,original_frame,g.pending_present,
        g.owner==GetCurrentThreadId())?TRUE:FALSE;
}

extern "C" BOOL __stdcall Model2VR_BeginVerifiedWorldFrame(
    unsigned long long frame_id,const float* axis,float units,
    float near_plane,unsigned int proof) {
    std::scoped_lock lock(g.mutex);
    if(g.active || g.pending_present || !frame_id ||
       frame_id<=g.last_completed ||
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
        if(!Model2VR_ReadPredictedEyeViews(&predicted))return FALSE;
        LARGE_INTEGER now{},frequency{};
        if(!QueryPerformanceCounter(&now) ||
           !QueryPerformanceFrequency(&frequency) ||
           now.QuadPart<=0 || frequency.QuadPart<=0 ||
           !m2vr::daytona::AdmitTrackedWorldEyePose(
               predicted,static_cast<std::uint64_t>(now.QuadPart),
               static_cast<std::uint64_t>(frequency.QuadPart)).valid)
            return FALSE;
        // Source game vertices are in camera view space, not XR LOCAL.
        // Stage new origin until proof, fresh pose and latch all succeed.
        auto next_origin=g.head_origin;
        auto next_camera=g.camera_epoch;
        m2vr::pose::Frame camera_relative{};
        if(!next_camera.Admit(cal) ||
           !next_origin.Rebase(predicted,camera_relative) ||
           !g.latch.Begin(frame_id,camera_relative))return FALSE;
        if(!g.raster)
            g.raster=std::make_unique<m2vr::vr::StereoWorldRaster>();
        g.raster->Clear();
        g.head_origin=next_origin;
        g.camera_epoch=next_camera;
        g.frame=frame_id;
        g.pose_sequence=predicted.sequence;
        g.pose_published_qpc=predicted.published_qpc;
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
// Optional texture source path for a trusted Model2 CPU-world integration.
// This does not query game textures, rerun any D3D9 draw, or alter FFB.
extern "C" BOOL __stdcall Model2VR_AppendVerifiedTexturedTrianglesWithMaterialColorMaskSeparateAlphaRenderStates(
    const float* xyz,const float* uv,unsigned int vertex_count,
    const std::uint32_t* indices,unsigned int index_count,
    const unsigned char* texture_rgba,unsigned int width,
    unsigned int height,unsigned int pitch,unsigned int alpha_cutoff,
    unsigned int filter_mode,unsigned int address_u,unsigned int address_v,
    unsigned int cull_mode,unsigned int depth_mode,unsigned int depth_write,
    const std::uint32_t* diffuse_argb,unsigned int rgb_source,
    unsigned int alpha_source,unsigned int blend_mode,
    unsigned int write_rgba_mask,unsigned int alpha_compare,
    unsigned int alpha_reference,unsigned int separate_alpha_enabled,
    unsigned int separate_alpha_src,unsigned int separate_alpha_dst) {
    std::scoped_lock lock(g.mutex);
    // Only stage-zero DIFFUSE/CURRENT on BOTH channels is textureless.
    // Zero-byte source texture is safe for this explicit world-only path.
    const bool diffuse_only=rgb_source==1u && alpha_source==1u;
    const bool source_texture=texture_rgba!=nullptr;
    if(!Owner() || !xyz || !uv || !indices ||
       !vertex_count || vertex_count>4096u ||
       !index_count || index_count%3u!=0 ||
       index_count>3u*2048u ||
       index_count/3u>2048u-g.triangle_count ||
       g.draw_count>=2048u ||
       (source_texture ?
            (!width || !height || width>1024u || height>1024u ||
             pitch!=width*4u) :
            (!diffuse_only || width || height || pitch)) ||
       alpha_cutoff>255u || alpha_compare>8u || alpha_reference>255u ||
       separate_alpha_enabled>1u || separate_alpha_src>5u ||
       separate_alpha_dst>5u ||
       (separate_alpha_enabled==0u &&
        (separate_alpha_src!=1u || separate_alpha_dst!=0u)) ||
       (separate_alpha_enabled==1u && blend_mode==0u) ||
       (alpha_compare!=0u && alpha_cutoff!=0u) ||
       (alpha_compare==0u && alpha_reference!=0u) ||
       filter_mode>1u || address_u>3u || address_v>3u ||
       cull_mode>2u || depth_mode>6u || depth_write>1u ||
       rgb_source>2u || alpha_source>2u || blend_mode>8u ||
       write_rgba_mask>15u ||
       ((rgb_source!=0u || alpha_source!=0u) && !diffuse_argb)) {
        if(Owner())g.Abort();
        return FALSE;
    }
    try {
        std::vector<m2vr::vr::CpuPoint> positions(vertex_count);
        std::vector<float> safe_uv(std::size_t(vertex_count)*2u);
        std::vector<std::uint32_t> safe_indices(index_count);
        std::vector<std::uint8_t> safe_texture;
        if(source_texture)safe_texture.resize(std::size_t(pitch)*height);
        std::vector<std::uint32_t> safe_diffuse;
        if(diffuse_argb)safe_diffuse.resize(vertex_count);
        if(!GuardedCopy(positions.data(),xyz,
                        std::size_t(vertex_count)*3u*sizeof(float)) ||
           !GuardedCopy(safe_uv.data(),uv,
                        safe_uv.size()*sizeof(float)) ||
           !GuardedCopy(safe_indices.data(),indices,
                        safe_indices.size()*sizeof(std::uint32_t)) ||
           (source_texture && !GuardedCopy(safe_texture.data(),texture_rgba,
                                           safe_texture.size())) ||
           (diffuse_argb && !GuardedCopy(safe_diffuse.data(),diffuse_argb,
                            safe_diffuse.size()*sizeof(std::uint32_t)))) {
            g.Abort();return FALSE;
        }
        m2vr::vr::ClippedTexturedStereoBatch clipped{};
        if(!m2vr::vr::BuildTexturedStereoBatch(
              positions.data(),safe_uv.data(),positions.size(),
              safe_indices.data(),safe_indices.size(),g.frame,g.latch,
              g.calibration,g.evidence,clipped,
              safe_diffuse.empty()?nullptr:safe_diffuse.data())) {
            g.Abort();return FALSE;
        }
        m2vr::vr::StereoWorldRaster::TextureRGBA texture{
            source_texture?safe_texture.data():nullptr,
            width,height,pitch,
            static_cast<std::uint8_t>(alpha_cutoff),
            {
                static_cast<m2vr::vr::TextureAddress>(address_u),
                static_cast<m2vr::vr::TextureAddress>(address_v),
                static_cast<m2vr::vr::TextureFilter>(filter_mode)
            },
            {
                static_cast<m2vr::vr::FaceCull>(cull_mode),
                static_cast<m2vr::vr::DepthCompare>(depth_mode),
                depth_write!=0u,
                static_cast<std::uint8_t>(write_rgba_mask)
            },
            rgb_source==2u,alpha_source==2u,
            static_cast<m2vr::vr::BlendMode>(blend_mode),
            rgb_source==1u,alpha_source==1u,
            static_cast<m2vr::vr::AlphaCompare>(alpha_compare),
            static_cast<std::uint8_t>(alpha_reference),
            {separate_alpha_enabled!=0u,
             static_cast<m2vr::vr::AlphaBlendFactor>(separate_alpha_src),
             static_cast<m2vr::vr::AlphaBlendFactor>(separate_alpha_dst)}
        };
        if(!g.raster->DrawTextured(clipped,texture)) {
            g.Abort();return FALSE;
        }
        ++g.draw_count;
        g.triangle_count+=index_count/3u;
        return TRUE;
    }catch(...){
        g.Abort();return FALSE;
    }
}
// Preserve the exact previous alpha-compare/material ABI for all already
// compiled world adapters. Only the new source path opts into separately
// selected original D3D9 alpha ADD factors.
extern "C" BOOL __stdcall Model2VR_AppendVerifiedTexturedTrianglesWithMaterialColorMaskAlphaCompareRenderStates(
    const float* xyz,const float* uv,unsigned int vertex_count,
    const std::uint32_t* indices,unsigned int index_count,
    const unsigned char* texture_rgba,unsigned int width,
    unsigned int height,unsigned int pitch,unsigned int alpha_cutoff,
    unsigned int filter_mode,unsigned int address_u,unsigned int address_v,
    unsigned int cull_mode,unsigned int depth_mode,unsigned int depth_write,
    const std::uint32_t* diffuse_argb,unsigned int rgb_source,
    unsigned int alpha_source,unsigned int blend_mode,
    unsigned int write_rgba_mask,unsigned int alpha_compare,
    unsigned int alpha_reference) {
    return Model2VR_AppendVerifiedTexturedTrianglesWithMaterialColorMaskSeparateAlphaRenderStates(
        xyz,uv,vertex_count,indices,index_count,texture_rgba,width,height,
        pitch,alpha_cutoff,filter_mode,address_u,address_v,cull_mode,
        depth_mode,depth_write,diffuse_argb,rgb_source,alpha_source,
        blend_mode,write_rgba_mask,alpha_compare,alpha_reference,0u,1u,0u);
}
// Stable legacy material/color-mask ABI. Previous producers call this
// unchanged and retain the 8-bit >= cutoff semantics. Only the additional
// opt-in export below/above may request exact original D3D9 alpha compares.
extern "C" BOOL __stdcall Model2VR_AppendVerifiedTexturedTrianglesWithMaterialColorMaskRenderStates(
    const float* xyz,const float* uv,unsigned int vertex_count,
    const std::uint32_t* indices,unsigned int index_count,
    const unsigned char* texture_rgba,unsigned int width,
    unsigned int height,unsigned int pitch,unsigned int alpha_cutoff,
    unsigned int filter_mode,unsigned int address_u,unsigned int address_v,
    unsigned int cull_mode,unsigned int depth_mode,unsigned int depth_write,
    const std::uint32_t* diffuse_argb,unsigned int rgb_source,
    unsigned int alpha_source,unsigned int blend_mode,
    unsigned int write_rgba_mask) {
    return Model2VR_AppendVerifiedTexturedTrianglesWithMaterialColorMaskAlphaCompareRenderStates(
        xyz,uv,vertex_count,indices,index_count,texture_rgba,width,
        height,pitch,alpha_cutoff,filter_mode,address_u,address_v,cull_mode,
        depth_mode,depth_write,diffuse_argb,rgb_source,alpha_source,
        blend_mode,write_rgba_mask,0u,0u);
}
// Preserve the established source-material export with its original
// signature: existing clients implicitly write all RGBA channels.
extern "C" BOOL __stdcall Model2VR_AppendVerifiedTexturedTrianglesWithMaterialSourceRenderStates(
    const float* xyz,const float* uv,unsigned int vertex_count,
    const std::uint32_t* indices,unsigned int index_count,
    const unsigned char* texture_rgba,unsigned int width,
    unsigned int height,unsigned int pitch,unsigned int alpha_cutoff,
    unsigned int filter_mode,unsigned int address_u,unsigned int address_v,
    unsigned int cull_mode,unsigned int depth_mode,unsigned int depth_write,
    const std::uint32_t* diffuse_argb,unsigned int rgb_source,
    unsigned int alpha_source,unsigned int blend_mode) {
    return Model2VR_AppendVerifiedTexturedTrianglesWithMaterialColorMaskRenderStates(
        xyz,uv,vertex_count,indices,index_count,
        texture_rgba,width,height,pitch,alpha_cutoff,
        filter_mode,address_u,address_v,cull_mode,depth_mode,depth_write,
        diffuse_argb,rgb_source,alpha_source,blend_mode,0x0fu);
}
// Preserve the established public ABI exactly: 0 selected texel and
// 1 texel*diffuse. Invalid old boolean arguments are still rejected
// by the same guarded owner/abort transaction, never coerced.
extern "C" BOOL __stdcall Model2VR_AppendVerifiedTexturedTrianglesWithDiffuseBlendRenderStates(
    const float* xyz,const float* uv,unsigned int vertex_count,
    const std::uint32_t* indices,unsigned int index_count,
    const unsigned char* texture_rgba,unsigned int width,
    unsigned int height,unsigned int pitch,unsigned int alpha_cutoff,
    unsigned int filter_mode,unsigned int address_u,unsigned int address_v,
    unsigned int cull_mode,unsigned int depth_mode,unsigned int depth_write,
    const std::uint32_t* diffuse_argb,unsigned int modulate_rgb,
    unsigned int modulate_alpha,unsigned int blend_mode) {
    return Model2VR_AppendVerifiedTexturedTrianglesWithMaterialSourceRenderStates(
        xyz,uv,vertex_count,indices,index_count,texture_rgba,width,height,
        pitch,alpha_cutoff,filter_mode,address_u,address_v,cull_mode,
        depth_mode,depth_write,diffuse_argb,
        modulate_rgb>1u?3u:(modulate_rgb?2u:0u),
        modulate_alpha>1u?3u:(modulate_alpha?2u:0u),blend_mode);
}
// Do not change existing clients: the original diffuse export remains opaque.
extern "C" BOOL __stdcall Model2VR_AppendVerifiedTexturedTrianglesWithDiffuseRenderStates(
    const float* xyz,const float* uv,unsigned int vertex_count,
    const std::uint32_t* indices,unsigned int index_count,
    const unsigned char* texture_rgba,unsigned int width,
    unsigned int height,unsigned int pitch,unsigned int alpha_cutoff,
    unsigned int filter_mode,unsigned int address_u,unsigned int address_v,
    unsigned int cull_mode,unsigned int depth_mode,unsigned int depth_write,
    const std::uint32_t* diffuse_argb,unsigned int modulate_rgb,
    unsigned int modulate_alpha) {
    return Model2VR_AppendVerifiedTexturedTrianglesWithDiffuseBlendRenderStates(
        xyz,uv,vertex_count,indices,index_count,texture_rgba,
        width,height,pitch,alpha_cutoff,filter_mode,address_u,address_v,
        cull_mode,depth_mode,depth_write,diffuse_argb,
        modulate_rgb,modulate_alpha,0u);
}
// Earlier ABI remains unchanged: no diffuse stream and D3D9
// SELECTARG1(TEXTURE) remains exact, not implicitly modulated.
extern "C" BOOL __stdcall Model2VR_AppendVerifiedTexturedTrianglesWithRenderStates(
    const float* xyz,const float* uv,unsigned int vertex_count,
    const std::uint32_t* indices,unsigned int index_count,
    const unsigned char* texture_rgba,unsigned int width,
    unsigned int height,unsigned int pitch,unsigned int alpha_cutoff,
    unsigned int filter_mode,unsigned int address_u,unsigned int address_v,
    unsigned int cull_mode,unsigned int depth_mode,unsigned int depth_write) {
    return Model2VR_AppendVerifiedTexturedTrianglesWithDiffuseRenderStates(
        xyz,uv,vertex_count,indices,index_count,
        texture_rgba,width,height,pitch,alpha_cutoff,filter_mode,
        address_u,address_v,cull_mode,depth_mode,depth_write,
        nullptr,0u,0u);
}
// The earlier point/linear + address ABI remains valid with its historical
// no-culling, strict-less and depth-writing defaults.
extern "C" BOOL __stdcall Model2VR_AppendVerifiedTexturedTrianglesWithSampler(
    const float* xyz,const float* uv,unsigned int vertex_count,
    const std::uint32_t* indices,unsigned int index_count,
    const unsigned char* texture_rgba,unsigned int width,
    unsigned int height,unsigned int pitch,unsigned int alpha_cutoff,
    unsigned int filter_mode,unsigned int address_u,unsigned int address_v) {
    return Model2VR_AppendVerifiedTexturedTrianglesWithRenderStates(
        xyz,uv,vertex_count,indices,index_count,texture_rgba,width,
        height,pitch,alpha_cutoff,filter_mode,address_u,address_v,
        0u,0u,1u);
}
// Preserve the original public ABI: old callers keep point/clamp.
extern "C" BOOL __stdcall Model2VR_AppendVerifiedTexturedTriangles(
    const float* xyz,const float* uv,unsigned int vertex_count,
    const std::uint32_t* indices,unsigned int index_count,
    const unsigned char* texture_rgba,unsigned int width,
    unsigned int height,unsigned int pitch,unsigned int alpha_cutoff) {
    return Model2VR_AppendVerifiedTexturedTrianglesWithSampler(
        xyz,uv,vertex_count,indices,index_count,texture_rgba,width,height,
        pitch,alpha_cutoff,0u,0u,0u);
}
extern "C" BOOL __stdcall Model2VR_EndVerifiedWorldFrame(){
    std::scoped_lock lock(g.mutex);
    if(!Owner())return FALSE;
    if(!g.draw_count || !g.raster ||
       !g.raster->StereoPairUsable() || !OriginalWorldPoseLive()){
        g.Abort();
        return FALSE;
    }
    // One game frame, two independently clipped/depth-tested images.
    // Only after both CPU render targets finish can the writer publish.
    const auto& a=g.raster->Eye(0);
    const auto& b=g.raster->Eye(1);
    const bool success=Model2VR_OpenStereoWriter() &&
        Model2VR_SubmitStereoRGBAWithPose(a.data(),b.data(),g.frame,
            m2vr::transport::kWidth,m2vr::transport::kHeight,
            m2vr::transport::kPitch,g.pose_sequence);
    if(success)g.last_completed=g.frame;
    g.Abort();
    return success?TRUE:FALSE;
}
// Optional atomic original-Present-boundary commit. The adapter must first
// prove gameplay world ownership, obtain the next original Present frame ID,
// call Begin, then Append. End here only ARMS two completed eye images.
extern "C" BOOL __stdcall Model2VR_EndVerifiedWorldFrameOnOriginalPresent(){
    std::scoped_lock lock(g.mutex);
    if(!Owner())return FALSE;
    if(!g.draw_count || !g.raster ||
       !g.raster->StereoPairUsable() || !OriginalWorldPoseLive()){
        g.Abort();
        return FALSE;
    }
    g.active=false;
    g.pending_present=true;
    return TRUE;
}
// Called exclusively from the hooked ORIGINAL D3D9 Present path, after its
// actual HRESULT. No stereo IPC publish on a wrong frame, lost device,
// different render thread or simultaneous experimental GPU replay.
extern "C" BOOL __stdcall Model2VR_CompleteVerifiedWorldOriginalPresent(
    unsigned long long original_frame,unsigned int original_succeeded,
    unsigned int native_gpu_replay_enabled) {
    std::scoped_lock lock(g.mutex);
    if(!g.pending_present)return FALSE;
    const auto action=m2vr::vrbridge::DecidePendingWorldPresent(
        g.frame,original_frame,original_succeeded!=0u,
        native_gpu_replay_enabled!=0u,
        g.owner==GetCurrentThreadId());
    if(action==m2vr::vrbridge::PendingWorldPresent::Wait)return FALSE;
    const bool pose_live=action==m2vr::vrbridge::PendingWorldPresent::Publish
        && OriginalWorldPoseLive();
    bool success=false;
    if(action==m2vr::vrbridge::PendingWorldPresent::Publish && pose_live){
        const auto& left=g.raster->Eye(0);
        const auto& right=g.raster->Eye(1);
        success=Model2VR_OpenStereoWriter() &&
            Model2VR_SubmitStereoRGBAWithPose(
                left.data(),right.data(),g.frame,
                m2vr::transport::kWidth,m2vr::transport::kHeight,
                m2vr::transport::kPitch,g.pose_sequence);
        if(success)g.last_completed=g.frame;
    }
    // GPU native replay already owns the process health channel; an
    // unverified overlapping CPU frame must not stomp its diagnosis.
    if(!native_gpu_replay_enabled) {
        if(!g.producer_health.Valid())
            g.producer_health.Create(GetCurrentProcessId());
        if(g.producer_health.Valid()){
            const auto reason=m2vr::vrbridge::PendingWorldHealthReason(
                action,original_succeeded!=0u,success,pose_live);
            // This describes proven CPU-world draws, never fabricated GPU
            // DP/DIP/UP family counts. The host can distinguish a failed
            // original Present from stale or disconnected HMD tracking.
            const std::array<std::uint32_t,4> none{};
            g.producer_health.Publish(original_frame,reason,
                success?g.draw_count:0u,0u,0u,none);
        }
    }
    if(action==m2vr::vrbridge::PendingWorldPresent::Publish && !pose_live)
        m2vr::LogEvent("model2_vr_cpu_world_source_pose_expired",
            g.frame,g.pose_sequence,g.pose_published_qpc,0u);
    m2vr::LogEvent("model2_vr_cpu_world_present_commit",g.frame,
        static_cast<unsigned int>(action),success?1u:0u,
        native_gpu_replay_enabled);
    g.Abort();
    return success?TRUE:FALSE;
}
extern "C" void __stdcall Model2VR_DiscardPendingWorldOnReset(){
    std::scoped_lock lock(g.mutex);
    if(g.pending_present || g.active)g.Abort();
}
extern "C" void __stdcall Model2VR_AbortVerifiedWorldFrame() {
    std::scoped_lock lock(g.mutex);
    if(Owner() || (g.pending_present &&
       g.owner==GetCurrentThreadId()))g.Abort();
}

// An explicit verified game-adapter scene/device restart may establish a
// fresh tracking origin. Never recenter on a failed Draw/pose sample.
extern "C" BOOL __stdcall Model2VR_ResetVerifiedWorldTrackingOrigin() {
    std::scoped_lock lock(g.mutex);
    if(g.active || g.pending_present)return FALSE;
    g.head_origin.Reset();
    g.camera_epoch.Reset();
    g.latch.Reset();
    // Keep game-frame IDs monotonic: the shared stereo writer cannot
    // rewind its own frame ID in this process. Recenter only the pose.
    return TRUE;
}
