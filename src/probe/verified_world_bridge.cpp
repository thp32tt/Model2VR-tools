// Native Model2 stereo world->RGBA bridge. Explicit render adapter only:
// no automatic D3D9 draw replay and no second simulation/input/FFB call.
#include "verified_world_bridge.h"
#include "../vr/model2_stereo_raster.hpp"
#include "../vr/model2_head_recenter.hpp"
#include "../vr/model2_camera_source_contract.hpp"
#include "daytona_eye_pose_gate.hpp"
#include "daytona_source_pose_commit_gate.hpp"
#include "verified_world_present_gate.hpp"
#include "verified_world_source_epoch.hpp"
#include "original_d3d9_draw_result_gate.hpp"
#include "original_d3d9_clear_result_gate.hpp"
#include "original_d3d9_surface_result_gate.hpp"
#include "verified_world_source_result.hpp"
#include "verified_world_projection_epoch.hpp"
#include "original_d3d9_authenticated_draw.hpp"
#include "original_d3d9_stateblock_audit.hpp"
#include "daytona_native_stereo.hpp"
#include "../common/daytona_producer_health.hpp"
#include "logger.h"
#include <Windows.h>
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <vector>

extern "C" BOOL __stdcall Model2VR_OpenStereoWriter();
extern "C" BOOL __stdcall Model2VR_SubmitVerifiedCpuWorldRGBAWithPose(
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
    // Original CPU-world and GPU fallback share one EyeClones producer
    // status writer; a second Link would fail Create() on the same PID.
    m2vr::vr::Model2CameraCalibration calibration{};
    m2vr::vr::DrawEvidence evidence{};
    std::uint64_t frame=0,last_completed=0,pose_sequence=0;
    m2vr::vrbridge::OriginalSourceFrameEpoch source_epoch{};
    m2vr::vrbridge::OriginalDrawOneShot original_draw_one_shot{};
    std::atomic<bool> original_draw_armed{false};
    m2vr::vrbridge::OriginalProjectedCameraEpoch projected_camera{};
    m2vr::vrbridge::OriginalFixedCameraEpoch fixed_camera{};
    m2vr::vrbridge::OriginalCameraFamilyEpoch camera_family{};
    std::uint64_t pose_published_qpc=0;
    DWORD owner=0;
    unsigned draw_count=0,triangle_count=0;
    bool active=false,pending_present=false;
    void Reject(unsigned source_kind)noexcept{
        if((active || pending_present) && source_epoch.Revoke(frame)) {
            m2vr::LogEvent("model2_vr_cpu_world_source_rejected",
                frame,source_kind,draw_count,triangle_count);
            // Same frame/Draw metadata only; no private pixel/ROM payload.
            const auto detail=m2vr::vrbridge::CurrentOriginalSourceFailure();
            if(detail!=0u)
                m2vr::LogEvent("model2_vr_cpu_world_source_failure_detail",
                    frame,source_kind,detail,0u);
        }
        Abort();
    }
    void Abort()noexcept{
        active=false;pending_present=false;
        owner=0;frame=0;pose_sequence=0;pose_published_qpc=0;
        draw_count=0;triangle_count=0;
        projected_camera.Reset();fixed_camera.Reset();camera_family.Reset();
        original_draw_one_shot.Reset();
        original_draw_armed.store(false,std::memory_order_release);
        evidence={};calibration={};
    }
};
Bridge g;
m2vr::drawprobe::OriginalStateBlockAuditOncePerPresent g_stateblock_audit;
// Called under the only verified CPU-world bridge mutex. Source lifetime
// failures must revoke the original Present frame, not just discard its
// pixels; otherwise another Begin/End could retry the same invalid frame.
// The actual original Draw/Present, FFB and GPU/mono fallback are untouched.
void RejectWorldAtOriginalBoundary(
    m2vr::vrbridge::OriginalSourceFailure reason,
    std::uint64_t physical_frame,std::uint32_t boundary) noexcept {
    if(!(g.active||g.pending_present))return;
    const auto rejected_frame=g.frame;
    const auto draw_count=g.draw_count;
    const auto old_reason=m2vr::vrbridge::g_current_original_source_failure;
    m2vr::vrbridge::g_current_original_source_failure=reason;
    g.Reject(m2vr::vrbridge::CurrentOriginalSourceDrawKind());
    m2vr::vrbridge::g_current_original_source_failure=old_reason;
    // 1=unarmed at Present, 2=stale armed, 3=invalid source selector,
    // 4=physical Present rejected, 5=Reset, 6=wrong source frame at End.
    // No proprietary game pixels, addresses or matrices are exported.
    m2vr::LogEvent("model2_vr_cpu_world_physical_boundary_rejected",
        rejected_frame,physical_frame,boundary,draw_count);
}
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
// Internal original source adapters retain their own camera families.
// Native caller-supplied CPU-space triangles must not be silently mixed with
// a proven D3D9 camera inside the SAME eye-pose frame transaction.
bool NativeCameraFamilyAccepted() noexcept {
    if(m2vr::vrbridge::CurrentOriginalSourceDrawKind()!=0u)return true;
    if(g.camera_family.Admit(g.frame,
        m2vr::vrbridge::OriginalCameraSourceFamily::Native))return true;
    m2vr::vrbridge::RejectOriginalSourceMaterial(
        m2vr::vrbridge::OriginalSourceFailure::ProjectionPass);
    m2vr::LogEvent("model2_vr_cpu_world_native_camera_rejected",
        g.frame,0u,1u,g.draw_count);
    return false;
}
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
namespace m2vr::vrbridge {
bool AdmitVerifiedProjectedD3D9SourcePass(std::uint32_t caller,
    OriginalProjectionViewport viewport,
    const m2vr::drawprobe::PerspectiveCandidate& fit) noexcept {
    std::scoped_lock lock(g.mutex);
    if(!Owner())return false;
    const bool same_family=g.camera_family.Admit(
        g.frame,OriginalCameraSourceFamily::OriginalCPUProjected);
    const auto result=same_family
        ?g.projected_camera.Admit(g.frame,caller,viewport,fit)
        :OriginalCameraPassResult::Invalid;
    if(!CameraPassAccepted(result)){
        RejectOriginalSourceMaterial(OriginalSourceFailure::ProjectionPass);
        m2vr::LogEvent("model2_vr_cpu_world_camera_pass_rejected",
            g.frame,CurrentOriginalSourceDrawKind(),
            static_cast<std::uint32_t>(result),g.draw_count);
        return false;
    }
    return true;
}
bool AdmitVerifiedFixedD3D9SourcePass(
    OriginalProjectionViewport viewport,
    const std::array<float,16>& view,
    const std::array<float,16>& projection) noexcept {
    std::scoped_lock lock(g.mutex);
    if(!Owner())return false;
    const bool same_family=g.camera_family.Admit(
        g.frame,OriginalCameraSourceFamily::FixedFunctionD3D9);
    const auto result=same_family
        ?g.fixed_camera.Admit(g.frame,viewport,view,projection)
        :OriginalFixedCameraPassResult::Invalid;
    if(!FixedCameraPassAccepted(result)){
        RejectOriginalSourceMaterial(OriginalSourceFailure::ProjectionPass);
        m2vr::LogEvent("model2_vr_cpu_world_fixed_camera_rejected",
            g.frame,CurrentOriginalSourceDrawKind(),
            static_cast<std::uint32_t>(result),g.draw_count);
        return false;
    }
    return true;
}
} // namespace m2vr::vrbridge
namespace m2vr::vrbridge {
bool TryTakeAuthenticatedOriginalDraw(std::uint64_t frame,
    std::uint64_t ordinal,std::uint32_t family,std::uint32_t caller,std::uint32_t scene,
    bool genuine_scene_open,std::uint64_t observed_lineage_nonce,
    ArmedOriginalDraw& result) noexcept {
    if(!g.original_draw_armed.load(std::memory_order_acquire))return false;
    std::unique_lock lock(g.mutex);
    if(!Owner() || g.frame!=frame)return false;
    bool lineage_mismatch=false;
    if(!g.original_draw_one_shot.Take(frame,ordinal,family,caller,scene,
                                     genuine_scene_open,result,
                                     observed_lineage_nonce,&lineage_mismatch)){
        if(lineage_mismatch){
            g.original_draw_armed.store(false,std::memory_order_release);
            const auto prior=g_current_original_source_failure;
            g_current_original_source_failure=OriginalSourceFailure::OriginalLineageMismatch;
            // Revoke the entire physical Present epoch, even if earlier
            // authentic CPU-world triangles had already reached both eyes.
            g.Reject(CurrentOriginalSourceDrawKind());
            // Rejection of an armed exact original Draw is a PHYSICAL
            // Present failure, not just a transient CPU image abort.
            // A second Begin/arm in this same source Present must fail.
            g.source_epoch.Revoke(frame);
            g_current_original_source_failure=prior;
            m2vr::LogEvent("model2_vr_original_draw_lineage_rejected",
                frame,ordinal,(std::uint64_t{family}<<32)|caller,scene);
            // The private physical Draw auditor must reject this frame too:
            // a pre-COM one-shot mismatch has the same unsafe source
            // outcome as an intervening state change during original Draw.
            m2vr::LogEvent("model2_d3d9_original_lineage_drift",
                frame,family,caller,ordinal);
            lock.unlock();
            // A camera/target mutation after the adapter arm also
            // invalidates speculative GPU-eye replay, but NOT original mono.
            m2vr::daytona::MarkOriginalLineageMismatch();
        }
        return false;
    }
    g.original_draw_armed.store(false,std::memory_order_release);
    // Source proof is one-use. The actual original COM Draw HRESULT still
    // decides the physical Present. Never emit game geometry or matrices.
    m2vr::LogEvent("model2_vr_authenticated_original_draw_taken",
        frame,ordinal,(std::uint64_t{family}<<32)|caller,scene);
    return true;
}
} // namespace m2vr::vrbridge
extern "C" BOOL __stdcall Model2VR_ArmVerifiedOriginalD3D9Draw(
    unsigned long long frame,unsigned long long ordinal,
    unsigned int family,unsigned int caller,unsigned int scene,
    unsigned int cpu_projector,unsigned int alpha_cutoff){
    const auto source_frame=Model2VR_NextOriginalD3D9PresentFrameId();
    std::scoped_lock lock(g.mutex);
    if(!Owner() || g.frame!=frame || frame!=source_frame)return FALSE;
    m2vr::vrbridge::ArmedOriginalDraw rule{};
    rule.frame=frame;rule.ordinal=ordinal;
    rule.family=family;rule.caller=caller;rule.scene=scene;
    rule.cpu_projector=cpu_projector;rule.alpha_cutoff=alpha_cutoff;
    if(!g.original_draw_one_shot.Arm(rule))return FALSE;
    g.original_draw_armed.store(true,std::memory_order_release);
    return TRUE;
}
// An independently authenticated source adapter can pin genuine
// original EXE-owned camera/target/viewport revisions for ONE real Draw.
// The opaque nonce provides state continuity, NOT World3D proof.
extern "C" BOOL __stdcall Model2VR_ArmVerifiedOriginalD3D9DrawWithLineage(
    unsigned long long frame,unsigned long long ordinal,
    unsigned int family,unsigned int caller,unsigned int scene,
    unsigned int cpu_projector,unsigned int alpha_cutoff,
    unsigned long long expected_lineage_nonce){
    if(!expected_lineage_nonce ||
       Model2VR_CurrentOriginalD3D9LineageNonce()!=expected_lineage_nonce)
        return FALSE;
    const auto source_frame=Model2VR_NextOriginalD3D9PresentFrameId();
    std::scoped_lock lock(g.mutex);
    if(!Owner() || g.frame!=frame || frame!=source_frame)return FALSE;
    m2vr::vrbridge::ArmedOriginalDraw rule{};
    rule.frame=frame;rule.ordinal=ordinal;
    rule.family=family;rule.caller=caller;rule.scene=scene;
    rule.cpu_projector=cpu_projector;rule.alpha_cutoff=alpha_cutoff;
    rule.expected_lineage_nonce=expected_lineage_nonce;
    if(!g.original_draw_one_shot.Arm(rule))return FALSE;
    g.original_draw_armed.store(true,std::memory_order_release);
    return TRUE;
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
    if(g.active || g.pending_present ||
       !g.source_epoch.CanBegin(frame_id,g.last_completed) ||
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
        g.projected_camera.Reset();g.fixed_camera.Reset();g.camera_family.Reset();
        g.original_draw_one_shot.Reset();
        g.original_draw_armed.store(false,std::memory_order_release);
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
        if(Owner())g.Reject(m2vr::vrbridge::CurrentOriginalSourceDrawKind());
        return FALSE;
    }
    // Native camera-space triangles are not interchangeable with an
    // original CPU-projected or fixed D3D9 WVP source in this XR frame.
    // A mixed family poisons the entire in-flight binocular transaction.
    if(!NativeCameraFamilyAccepted()){
        g.Reject(m2vr::vrbridge::CurrentOriginalSourceDrawKind());
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
            g.Reject(m2vr::vrbridge::CurrentOriginalSourceDrawKind());return FALSE;
        }
        m2vr::vr::ClippedStereoBatch clipped{};
        if(!m2vr::vr::BuildClippedStereoBatch(
            vertices.data(),vertices.size(),safe_indices.data(),index_count,
            g.frame,g.latch,g.calibration,g.evidence,clipped) ||
           !g.raster->Draw(clipped,safe_colors.data(),index_count/3u)) {
            g.Reject(m2vr::vrbridge::CurrentOriginalSourceDrawKind());
            return FALSE;
        }
        ++g.draw_count;
        g.triangle_count+=index_count/3u;
        return TRUE;
    }catch(...){
        g.Reject(m2vr::vrbridge::CurrentOriginalSourceDrawKind());
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
        if(Owner())g.Reject(m2vr::vrbridge::CurrentOriginalSourceDrawKind());
        return FALSE;
    }
    if(!NativeCameraFamilyAccepted()){
        g.Reject(m2vr::vrbridge::CurrentOriginalSourceDrawKind());
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
            g.Reject(m2vr::vrbridge::CurrentOriginalSourceDrawKind());return FALSE;
        }
        m2vr::vr::ClippedTexturedStereoBatch clipped{};
        if(!m2vr::vr::BuildTexturedStereoBatch(
              positions.data(),safe_uv.data(),positions.size(),
              safe_indices.data(),safe_indices.size(),g.frame,g.latch,
              g.calibration,g.evidence,clipped,
              safe_diffuse.empty()?nullptr:safe_diffuse.data())) {
            g.Reject(m2vr::vrbridge::CurrentOriginalSourceDrawKind());return FALSE;
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
            g.Reject(m2vr::vrbridge::CurrentOriginalSourceDrawKind());return FALSE;
        }
        ++g.draw_count;
        g.triangle_count+=index_count/3u;
        return TRUE;
    }catch(...){
        g.Reject(m2vr::vrbridge::CurrentOriginalSourceDrawKind());return FALSE;
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
       g.original_draw_one_shot.Pending() ||
       !g.raster->StereoPairUsable() || !OriginalWorldPoseLive()){
        g.Reject(m2vr::vrbridge::CurrentOriginalSourceDrawKind());
        return FALSE;
    }
    // One game frame, two independently clipped/depth-tested images.
    // Only after both CPU render targets finish can the writer publish.
    const auto& a=g.raster->Eye(0);
    const auto& b=g.raster->Eye(1);
    const bool success=Model2VR_OpenStereoWriter() &&
        Model2VR_SubmitVerifiedCpuWorldRGBAWithPose(a.data(),b.data(),g.frame,
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
       g.original_draw_one_shot.Pending() ||
       !g.raster->StereoPairUsable() || !OriginalWorldPoseLive()){
        g.Reject(m2vr::vrbridge::CurrentOriginalSourceDrawKind());
        return FALSE;
    }
    // Explicit original-Present adapter ONLY: the frame ID must be the
    // exact physical source Present owned by the installed original hook,
    // not an emulator tick or a stale previous source-frame number.
    const auto source_present=Model2VR_NextOriginalD3D9PresentFrameId();
    if(!source_present || g.frame!=source_present){
        RejectWorldAtOriginalBoundary(
            m2vr::vrbridge::OriginalSourceFailure::PresentFrameMismatch,
            source_present,6u);
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
    // Reject any partially composed source that crosses its actual game
    // Present without End, or an older armed pair skipped by a Present.
    // Previously !pending_present returned early and leaked g.active into
    // later original game frames, blocking recovery and stale-eye safety.
    const auto expiry=m2vr::vrbridge::DecideOriginalWorldPresentExpiry(
        g.frame,original_frame,g.active,g.pending_present);
    if(expiry!=m2vr::vrbridge::OriginalWorldPresentExpiry::None){
        RejectWorldAtOriginalBoundary(
            expiry==m2vr::vrbridge::OriginalWorldPresentExpiry::Unarmed?
                m2vr::vrbridge::OriginalSourceFailure::PresentUnarmed:
                m2vr::vrbridge::OriginalSourceFailure::PresentStale,
            original_frame,static_cast<std::uint32_t>(expiry));
        return FALSE;
    }
    if(!g.pending_present)return FALSE;
    // 0 CPU-only, 1 GPU-only, 2 authenticated CPU preferred with GPU/mono
    // backup. Never allow an unknown internal source selector to publish.
    if(native_gpu_replay_enabled>2u){
        RejectWorldAtOriginalBoundary(
            m2vr::vrbridge::OriginalSourceFailure::PresentRejected,
            original_frame,3u);
        return FALSE;
    }
    const bool gpu_only=native_gpu_replay_enabled==1u;
    const bool gpu_fallback=native_gpu_replay_enabled==2u;
    const auto action=m2vr::vrbridge::DecidePendingWorldPresent(
        g.frame,original_frame,original_succeeded!=0u,
        gpu_only,g.owner==GetCurrentThreadId());
    if(action==m2vr::vrbridge::PendingWorldPresent::Wait)return FALSE;
    const bool pose_live=action==m2vr::vrbridge::PendingWorldPresent::Publish
        && OriginalWorldPoseLive();
    bool success=false;
    if(action==m2vr::vrbridge::PendingWorldPresent::Publish && pose_live){
        const auto& left=g.raster->Eye(0);
        const auto& right=g.raster->Eye(1);
        success=Model2VR_OpenStereoWriter() &&
            Model2VR_SubmitVerifiedCpuWorldRGBAWithPose(
                left.data(),right.data(),g.frame,
                m2vr::transport::kWidth,m2vr::transport::kHeight,
                m2vr::transport::kPitch,g.pose_sequence);
        if(success)g.last_completed=g.frame;
    }
    // Publish source health through the ONE original-device producer
    // shared-memory writer. If CPU failed with GPU fallback armed, let the
    // GPU/real-backbuffer path report the same frame AFTER this function.
    // Otherwise two writers/duplicate frame IDs would corrupt host health.
    if(!gpu_only && (!gpu_fallback || success)) {
        const auto reason=m2vr::vrbridge::PendingWorldHealthReason(
            action,original_succeeded!=0u,success,pose_live);
        m2vr::daytona::ReportVerifiedCpuWorldPresent(
            original_frame,reason,success?g.draw_count:0u);
    }
    if(action==m2vr::vrbridge::PendingWorldPresent::Publish && !pose_live)
        m2vr::LogEvent("model2_vr_cpu_world_source_pose_expired",
            g.frame,g.pose_sequence,g.pose_published_qpc,0u);
    m2vr::LogEvent("model2_vr_cpu_world_present_commit",g.frame,
        static_cast<unsigned int>(action),success?1u:0u,
        native_gpu_replay_enabled);
    if(action==m2vr::vrbridge::PendingWorldPresent::Drop){
        // Wrong render thread, failed physical original Present or a
        // disallowed owner must not be reopened for this same Present.
        RejectWorldAtOriginalBoundary(
            m2vr::vrbridge::OriginalSourceFailure::PresentRejected,
            original_frame,4u);
    }else{
        g.Abort();
    }
    return success?TRUE:FALSE;
}
// Called only on a rejected explicit original D3D9 source adapter.
// Abort the entire partially composed world frame (not the original game).
// An already armed frame from the same thread also becomes unusable.
extern "C" void __stdcall Model2VR_RejectVerifiedD3D9SourceDraw(
    unsigned int source_kind) {
    std::scoped_lock lock(g.mutex);
    if(g.owner==GetCurrentThreadId() && (g.active || g.pending_present)){
        // An original game Draw that cannot produce both verified eyes
        // must never be retried using another adapter call in this Present.
        const auto invalid_source_present=g.frame;
        g.Reject(source_kind);
        if(invalid_source_present)
            g.source_epoch.Revoke(invalid_source_present);
    }
}
// Any failed authentic original D3D9 Draw revokes the whole physical
// Present, including when the CPU-world adapter has not begun yet.
// Never execute the game's Draw, Present, simulation or FFB again.
extern "C" void __stdcall Model2VR_RejectFailedOriginalD3D9Draw(
    std::uint32_t original_family,std::uint32_t result_bits,
    std::uint32_t original_caller_rva) {
    using namespace m2vr::vrbridge;
    if(!IsFailedOriginalD3D9Draw(original_family,result_bits) ||
       !original_caller_rva)return;
    const auto failed_present=Model2VR_NextOriginalD3D9PresentFrameId();
    std::scoped_lock lock(g.mutex);
    const bool owned=(g.owner==GetCurrentThreadId()) &&
                     (g.active || g.pending_present);
    const auto rejected_frame=g.frame;
    if(owned){
        const auto old_reason=g_current_original_source_failure;
        g_current_original_source_failure=OriginalSourceFailure::OriginalDrawFailed;
        g.Reject(FailedOriginalD3D9SourceKind(original_family));
        g_current_original_source_failure=old_reason;
        // EXE-relative caller/HRESULT only; never private game pixels.
        m2vr::LogEvent("model2_vr_cpu_world_original_draw_failed",
            rejected_frame,original_family,result_bits,original_caller_rva);
    }
    // A late CPU-world Begin must not re-arm this incomplete source frame.
    if(failed_present)g.source_epoch.Revoke(failed_present);
}
// A failed authentic primary-device BeginScene or EndScene cannot be
// part of a fully composited original-world eye pair for this same thread.
// Preserve the original COM HRESULT; revoke any owned CPU pair AND prevent
// this physical source Present from being re-armed later in the frame.
extern "C" void __stdcall Model2VR_RejectFailedOriginalD3D9Scene(
    std::uint32_t transition,std::uint32_t result_bits,
    std::uint32_t original_caller_rva){
    using namespace m2vr::vrbridge;
    if((transition!=1u && transition!=2u) ||
       (result_bits&0x80000000u)==0u || !original_caller_rva)return;
    // Even when no CPU transaction is active yet, this failed genuine
    // original scene permanently rejects its physical Present epoch.
    // A subsequent Begin on the same source frame must not re-arm stereo
    // after a GPU scene HRESULT failure; the NEXT physical frame may recover.
    const auto failed_present=Model2VR_NextOriginalD3D9PresentFrameId();
    std::scoped_lock lock(g.mutex);
    const bool owned=(g.owner==GetCurrentThreadId()) &&
                     (g.active || g.pending_present);
    const auto rejected_frame=g.frame;
    if(owned){
        const auto old_reason=g_current_original_source_failure;
        g_current_original_source_failure=OriginalSourceFailure::OriginalSceneFailed;
        g.Reject(CurrentOriginalSourceDrawKind());
        g_current_original_source_failure=old_reason;
        // No game texture bytes or world transforms are recorded.
        m2vr::LogEvent("model2_vr_cpu_world_original_scene_failed",
            rejected_frame,transition,result_bits,original_caller_rva);
    }
    if(failed_present)g.source_epoch.Revoke(failed_present);
}
// A real original EXE-owned failed Clear poisons the entire physical
// Present even before CPU-world Begin or after the last successful Draw.
extern "C" void __stdcall Model2VR_RejectFailedOriginalD3D9Clear(
    std::uint32_t flags,std::uint32_t result_bits,
    std::uint32_t original_caller_rva){
    using namespace m2vr::vrbridge;
    if(!IsFailedOriginalD3D9Clear(result_bits) || !original_caller_rva)return;
    const auto failed_present=Model2VR_NextOriginalD3D9PresentFrameId();
    std::scoped_lock lock(g.mutex);
    const bool owned=g.owner==GetCurrentThreadId() &&
                     (g.active || g.pending_present);
    const auto rejected_frame=g.frame;
    if(owned){
        const auto previous=g_current_original_source_failure;
        g_current_original_source_failure=OriginalSourceFailure::OriginalClearFailed;
        g.Reject(FailedOriginalD3D9ClearSourceKind());
        g_current_original_source_failure=previous;
        m2vr::LogEvent("model2_vr_cpu_world_original_clear_failed",
            rejected_frame,flags,result_bits,original_caller_rva);
    }
    if(failed_present)g.source_epoch.Revoke(failed_present);
}
// Original EXE-owned UpdateSurface/StretchRect/ColorFill failure discards
// any incomplete binocular world frame for its single physical Present.
extern "C" void __stdcall Model2VR_RejectFailedOriginalD3D9Surface(
    std::uint32_t method,std::uint32_t result_bits,
    std::uint32_t original_caller_rva){
    using namespace m2vr::vrbridge;
    if(!IsFailedOriginalD3D9Surface(method,result_bits) ||
       !original_caller_rva)return;
    const auto failed_present=Model2VR_NextOriginalD3D9PresentFrameId();
    std::scoped_lock lock(g.mutex);
    const bool owned=g.owner==GetCurrentThreadId() &&
                     (g.active || g.pending_present);
    const auto rejected_frame=g.frame;
    if(owned){
        const auto old_reason=g_current_original_source_failure;
        g_current_original_source_failure=OriginalSourceFailure::OriginalSurfaceFailed;
        g.Reject(FailedOriginalD3D9SurfaceSourceKind(method));
        g_current_original_source_failure=old_reason;
        m2vr::LogEvent("model2_vr_cpu_world_original_surface_failed",
            rejected_frame,method,result_bits,original_caller_rva);
    }
    if(failed_present)g.source_epoch.Revoke(failed_present);
}
// UpdateTexture can rewrite already bound texels without SetTexture. A
// success before any CPU eye source begins is safe; an already active/pending
// eye source is stale, so revoke that one physical Present's partial eyes.
extern "C" void __stdcall Model2VR_ObserveOriginalD3D9TextureUpload(
    unsigned long long physical_present,unsigned int original_caller_rva,
    unsigned long long preceding_draw_ordinal){
    using namespace m2vr::vrbridge;
    if(!physical_present || !original_caller_rva ||
       preceding_draw_ordinal>10000000u ||
       Model2VR_NextOriginalD3D9PresentFrameId()!=physical_present)return;
    bool rejected=false;
    {
        std::scoped_lock lock(g.mutex);
        if(g.frame==physical_present && (g.active || g.pending_present)){
            const auto previous=g_current_original_source_failure;
            g_current_original_source_failure=
                OriginalSourceFailure::OriginalTextureUploadAfterStereo;
            g.Reject(CurrentOriginalSourceDrawKind());
            g_current_original_source_failure=previous;
            g.source_epoch.Revoke(physical_present);
            rejected=true;
        }
    }
    if(rejected)
        m2vr::LogEvent("model2_d3d9_original_texture_upload_rejected",
            physical_present,original_caller_rva,preceding_draw_ordinal,0u);
    m2vr::daytona::MarkOriginalTextureUploadAfterEyes();
}
// Actual primary EXE-owned GPU Surface commands (Clear, UpdateSurface,
// StretchRect, ColorFill) can change active render content without any
// LockRect/SetTexture. Revoke incomplete XR eyes BEFORE the original D3D9
// COM method runs. This is NOT evidence of world-pass ownership.
extern "C" void __stdcall Model2VR_ObserveOriginalD3D9GpuSurfaceMutation(
    unsigned long long physical_present,unsigned int original_caller_rva,
    unsigned long long preceding_draw_ordinal,unsigned int method){
    using namespace m2vr::vrbridge;
    if(!physical_present || !original_caller_rva ||
        method<1u || method>4u || preceding_draw_ordinal>10000000u ||
        Model2VR_NextOriginalD3D9PresentFrameId()!=physical_present)return;
    bool rejected=false;
    {
        std::scoped_lock lock(g.mutex);
        if(g.frame==physical_present && (g.active || g.pending_present)){
            const auto previous=g_current_original_source_failure;
            g_current_original_source_failure=
                OriginalSourceFailure::OriginalGpuSurfaceMutationAfterStereo;
            g.Reject(CurrentOriginalSourceDrawKind());
            g_current_original_source_failure=previous;
            g.source_epoch.Revoke(physical_present);
            rejected=true;
        }
    }
    if(rejected)
        m2vr::LogEvent("model2_d3d9_original_gpu_surface_mutation_rejected",
            physical_present,original_caller_rva,preceding_draw_ordinal,method);
    m2vr::daytona::MarkOriginalGpuSurfaceMutationAfterEyes(method);
}
// Authentic writable original VB/IB Lock may change already-bound geometry
// without another device stream/index setter. No buffer bytes cross into IPC.
// Existing eye frames are revoked before the game can modify the buffer.
extern "C" void __stdcall Model2VR_ObserveOriginalD3D9BufferMutation(
    unsigned long long physical_present,unsigned int original_caller_rva,
    unsigned long long preceding_draw_ordinal,unsigned int family){
    using namespace m2vr::vrbridge;
    if(!physical_present || !original_caller_rva ||
       (family!=1u && family!=2u) ||
       preceding_draw_ordinal>10000000u ||
       Model2VR_NextOriginalD3D9PresentFrameId()!=physical_present)return;
    bool rejected=false;
    {
        std::scoped_lock lock(g.mutex);
        if(g.frame==physical_present && (g.active || g.pending_present)){
            const auto prior=g_current_original_source_failure;
            g_current_original_source_failure=
                OriginalSourceFailure::OriginalBufferContentAfterStereo;
            g.Reject(CurrentOriginalSourceDrawKind());
            g_current_original_source_failure=prior;
            g.source_epoch.Revoke(physical_present);
            rejected=true;
        }
    }
    if(rejected)
        m2vr::LogEvent("model2_d3d9_original_buffer_mutation_rejected",
            physical_present,original_caller_rva,
            preceding_draw_ordinal,family);
    m2vr::daytona::MarkOriginalBufferMutationAfterEyes(family);
}
// The original resource COM runtime did not expose a complete Lock+Unlock
// hook pair. Unknown resource writes are unsafe for this AND future
// original source Presents, while the game's untouched mono remains valid.
extern "C" void __stdcall Model2VR_RejectOriginalD3D9BufferHookCoverage(
    unsigned long long physical_present,unsigned int original_caller_rva,
    unsigned int family){
    using namespace m2vr::vrbridge;
    if(!physical_present || family>2u ||
       Model2VR_NextOriginalD3D9PresentFrameId()!=physical_present)return;
    bool first=false;
    {
        std::scoped_lock lock(g.mutex);
        if(g.frame==physical_present && (g.active || g.pending_present)){
            const auto prior=g_current_original_source_failure;
            g_current_original_source_failure=
                OriginalSourceFailure::OriginalBufferHookCoverageUnknown;
            g.Reject(CurrentOriginalSourceDrawKind());
            g_current_original_source_failure=prior;
        }
        first=g.source_epoch.Revoke(physical_present);
    }
    if(first)
        m2vr::LogEvent("model2_d3d9_original_buffer_hook_unobserved",
            physical_present,original_caller_rva,family,0u);
    m2vr::daytona::MarkOriginalLineageMismatch();
}
// A real IDirect3DSwapChain9::Present is not the primary IDirect3DDevice9
// physical Present. There is no proven original Draw/eye ownership mapping,
// so discard both speculative paths before the original COM call and keep
// the original game's mono presentation intact. Reason 1=observed call,
// 2=persistent unknown alternate swap-chain / unobserved source coverage,
// 3=original SwapChain::GetFrontBufferData GPU-to-CPU Surface write.
extern "C" void __stdcall Model2VR_RejectOriginalD3D9SwapChainPresent(
    unsigned long long physical_present,unsigned int original_caller_rva,
    unsigned int reason){
    using namespace m2vr::vrbridge;
    if(!physical_present || reason<1u || reason>3u ||
       Model2VR_NextOriginalD3D9PresentFrameId()!=physical_present)return;
    bool first=false;
    {
        std::scoped_lock lock(g.mutex);
        if(g.frame==physical_present && (g.active || g.pending_present)){
            const auto prior=g_current_original_source_failure;
            g_current_original_source_failure=
                OriginalSourceFailure::OriginalSwapChainPresentUnverified;
            g.Reject(CurrentOriginalSourceDrawKind());
            g_current_original_source_failure=prior;
        }
        first=g.source_epoch.Revoke(physical_present);
    }
    if(first)
        m2vr::LogEvent("model2_d3d9_original_swap_chain_source_rejected",
            physical_present,original_caller_rva,reason,0u);
    m2vr::daytona::MarkOriginalLineageMismatch();
}
// Real original Device9::ValidateDevice COM 70 may require several
// hardware texture stages/passes. Without original per-pass camera and RT
// proof, staged Win64 OpenXR eyes cannot be declared equivalent to the
// original game's multi-pass output. Reasons 1=failed original HRESULT,
// 2=multiple passes, 3=unreadable/ambiguous count, 4=foreign COM caller,
// 5=unresolved state at physical Present. Real game COM and mono remain intact.
extern "C" void __stdcall Model2VR_RejectOriginalD3D9ValidationUse(
    unsigned long long physical_present,unsigned int original_caller_rva,
    unsigned long long preceding_draw_ordinal,unsigned int reason){
    using namespace m2vr::vrbridge;
    if(!physical_present || reason<1u || reason>5u ||
       preceding_draw_ordinal>10000000u ||
       (reason<=3u && !original_caller_rva) ||
       (reason>=4u && (original_caller_rva || preceding_draw_ordinal)) ||
       Model2VR_NextOriginalD3D9PresentFrameId()!=physical_present)return;
    bool first=false;
    {
        std::scoped_lock lock(g.mutex);
        if(g.frame==physical_present && (g.active || g.pending_present)){
            const auto prior=g_current_original_source_failure;
            g_current_original_source_failure=
                OriginalSourceFailure::OriginalDeviceValidationUnproven;
            g.Reject(CurrentOriginalSourceDrawKind());
            g_current_original_source_failure=prior;
        }
        first=g.source_epoch.Revoke(physical_present);
    }
    if(first)
        m2vr::LogEvent("model2_d3d9_original_validation_rejected",
            physical_present,original_caller_rva,preceding_draw_ordinal,reason);
    m2vr::daytona::MarkOriginalLineageMismatch();
}
// Original Win32 x86 D3D9 COM 5 managed-residency transition, not
// authentic World3D proof. Reason 1=game eviction, 2=foreign caller,
// 3=unknown provenance still active at the real physical Present.
extern "C" void __stdcall Model2VR_RejectOriginalD3D9ManagedResidency(
    unsigned long long physical_present,unsigned int original_caller_rva,
    unsigned long long preceding_draw_ordinal,unsigned int reason){
    using namespace m2vr::vrbridge;
    if(!physical_present || reason<1u || reason>3u ||
       preceding_draw_ordinal>10000000u ||
       (reason==1u && !original_caller_rva) ||
       (reason>=2u && (original_caller_rva || preceding_draw_ordinal)) ||
       Model2VR_NextOriginalD3D9PresentFrameId()!=physical_present)return;
    bool first=false;
    {
        std::scoped_lock lock(g.mutex);
        if(g.frame==physical_present && (g.active || g.pending_present)){
            const auto old=g_current_original_source_failure;
            g_current_original_source_failure=
                OriginalSourceFailure::OriginalManagedResidencyUnproven;
            g.Reject(CurrentOriginalSourceDrawKind());
            g_current_original_source_failure=old;
        }
        first=g.source_epoch.Revoke(physical_present);
    }
    if(first)
        m2vr::LogEvent("model2_d3d9_original_residency_rejected",
            physical_present,original_caller_rva,preceding_draw_ordinal,reason);
    m2vr::daytona::MarkOriginalLineageMismatch();
}
// Genuine Query9 Issue/GetData may change the CPU's occlusion-culling
// decisions without another original source camera write. Reason 1=Issue,
// 2=GetData, 3=unknown caller/owner/vtable coverage (persistent at Present).
// No query data, pointers, geometry or game pixel content crosses processes.
extern "C" void __stdcall Model2VR_RejectOriginalD3D9QueryUse(
    unsigned long long physical_present,unsigned int original_caller_rva,
    unsigned long long preceding_draw_ordinal,unsigned int reason){
    using namespace m2vr::vrbridge;
    if(!physical_present || reason<1u || reason>3u ||
       preceding_draw_ordinal>10000000u ||
       (reason<=2u && !original_caller_rva) ||
       (reason==3u && (original_caller_rva || preceding_draw_ordinal)) ||
       Model2VR_NextOriginalD3D9PresentFrameId()!=physical_present)return;
    bool first=false;
    {
        std::scoped_lock lock(g.mutex);
        if(g.frame==physical_present && (g.active || g.pending_present)){
            const auto before=g_current_original_source_failure;
            g_current_original_source_failure=OriginalSourceFailure::OriginalQuerySourceUnproven;
            g.Reject(CurrentOriginalSourceDrawKind());
            g_current_original_source_failure=before;
        }
        first=g.source_epoch.Revoke(physical_present);
    }
    if(first)
        m2vr::LogEvent("model2_d3d9_original_query_source_rejected",
            physical_present,original_caller_rva,preceding_draw_ordinal,reason);
    m2vr::daytona::MarkOriginalLineageMismatch();
}
// Authentic original D3D9 display-mode/GDI-dialog and gamma output are
// physical presentation state, not verified camera/texture/World3D proof.
// method 1=SetDialogBoxMode, 2=SetGammaRamp, 3=active original GDI-dialog
// fallback at physical Present, 4=uncalibrated gamma at physical Present,
// 5=non-EXE primary-device presentation mutation with unknown source owner.
// 6/7/8=EXE-owned cursor bitmap/position/visibility changes.
// 9=visible or unknown genuine D3D9 cursor at physical Present.
// Caller/ordinal are nonzero only for actual original EXE-owned Set* calls.
extern "C" void __stdcall Model2VR_ObserveOriginalD3D9PresentationMutation(
    unsigned long long physical_present,unsigned int original_caller_rva,
    unsigned long long preceding_draw_ordinal,unsigned int method){
    using namespace m2vr::vrbridge;
    if(!physical_present || method<1u || method>9u ||
       preceding_draw_ordinal>10000000u ||
       ((method<=2u || (method>=6u && method<=8u)) && !original_caller_rva) ||
       (((method>=3u && method<=5u) || method==9u) &&
        (original_caller_rva || preceding_draw_ordinal)) ||
       Model2VR_NextOriginalD3D9PresentFrameId()!=physical_present)return;
    bool first=false;
    {
        std::scoped_lock lock(g.mutex);
        if(g.frame==physical_present && (g.active || g.pending_present)){
            const auto prior=g_current_original_source_failure;
            g_current_original_source_failure=
                OriginalSourceFailure::OriginalPresentationStateUnproven;
            g.Reject(CurrentOriginalSourceDrawKind());
            g_current_original_source_failure=prior;
        }
        first=g.source_epoch.Revoke(physical_present);
    }
    // Local metadata only. A persistent display state is recorded once
    // per actual Present to prevent a sampled VB/UP source appearing clean.
    if(first)
        m2vr::LogEvent("model2_d3d9_original_presentation_control_rejected",
            physical_present,original_caller_rva,preceding_draw_ordinal,method);
    m2vr::daytona::MarkOriginalLineageMismatch();
}
// Palette writes may change already drawn eye texels without a binding call.
// They never alter the original emulator Draw or mono backbuffer.
extern "C" void __stdcall Model2VR_ObserveOriginalD3D9PaletteMutation(
    unsigned long long physical_present,unsigned int original_caller_rva,
    unsigned long long preceding_draw_ordinal,unsigned int method){
    using namespace m2vr::vrbridge;
    if(!physical_present || !original_caller_rva || method<1u || method>2u ||
       preceding_draw_ordinal>10000000u ||
       Model2VR_NextOriginalD3D9PresentFrameId()!=physical_present)return;
    bool rejected=false;
    {
        std::scoped_lock lock(g.mutex);
        if(g.frame==physical_present && (g.active || g.pending_present)){
            const auto previous=g_current_original_source_failure;
            g_current_original_source_failure=OriginalSourceFailure::OriginalPaletteMutationAfterStereo;
            g.Reject(CurrentOriginalSourceDrawKind());
            g_current_original_source_failure=previous;
            g.source_epoch.Revoke(physical_present);
            rejected=true;
        }
    }
    if(rejected)
        m2vr::LogEvent("model2_d3d9_original_palette_mutation_rejected",
            physical_present,original_caller_rva,preceding_draw_ordinal,method);
    m2vr::daytona::MarkOriginalPaletteMutationAfterEyes(method);
}
// Original software VP and N-Patch modes cannot be guaranteed equivalent
// to CPU independent-eye triangles. Never promote stale source geometry.
extern "C" void __stdcall Model2VR_RejectOriginalD3D9RasterMode(
    unsigned long long physical_present,unsigned int unsafe_mask,
    unsigned int caller_rva){
    using namespace m2vr::vrbridge;
    if(!physical_present || !unsafe_mask || (unsafe_mask&~3u) ||
       Model2VR_NextOriginalD3D9PresentFrameId()!=physical_present)return;
    {
        std::scoped_lock lock(g.mutex);
        if(g.owner==GetCurrentThreadId() && g.frame==physical_present &&
           (g.active || g.pending_present)){
            const auto old=g_current_original_source_failure;
            g_current_original_source_failure=OriginalSourceFailure::OriginalRasterModeUnknown;
            g.Reject(CurrentOriginalSourceDrawKind());
            g_current_original_source_failure=old;
        }
        g.source_epoch.Revoke(physical_present);
    }
    // Only bounded mode flags and caller RVA; never game vertex/clip values.
    if(caller_rva || physical_present==1u || physical_present%8u==0u)
        m2vr::LogEvent("model2_d3d9_original_raster_mode_unsupported",
            physical_present,unsafe_mask,caller_rva,0u);
    m2vr::daytona::MarkOriginalLineageMismatch();
}
// Incomplete original D3D9 tessellated patch draws/processed vertex data
// are never falsely promoted to independently rasterized OpenXR eyes.
extern "C" void __stdcall Model2VR_RejectOriginalD3D9UnmirroredGeometry(
    unsigned long long physical_present,unsigned int method,
    unsigned int caller_rva,unsigned long long ordinal,
    unsigned int original_hresult){
    using namespace m2vr::vrbridge;
    if(!physical_present || method<1u || method>5u ||
       (method==5u ? (caller_rva!=0u || ordinal!=0u) : caller_rva==0u) ||
       ordinal>10000000u ||
       Model2VR_NextOriginalD3D9PresentFrameId()!=physical_present)return;
    {
        std::scoped_lock lock(g.mutex);
        if(g.owner==GetCurrentThreadId() && g.frame==physical_present &&
           (g.active || g.pending_present)){
            const auto previous=g_current_original_source_failure;
            g_current_original_source_failure=OriginalSourceFailure::OriginalUnmirroredGeometry;
            g.Reject(CurrentOriginalSourceDrawKind());
            g_current_original_source_failure=previous;
        }
        g.source_epoch.Revoke(physical_present);
    }
    // All physical Presents with unmirrored geometry must be represented:
    // each real COM method is logged once per frame; persistent method 5
    // is logged once at Present even without a fresh ProcessVertices call.
    // Caller/ordinal remain opaque; never log patch handles or VB bytes.
    if(method==5u)
        m2vr::LogEvent("model2_d3d9_original_unmirrored_geometry",
            physical_present,0u,std::uint64_t{method}<<32u,0u);
    m2vr::daytona::MarkOriginalLineageMismatch();
}
// Complete original primary Scene/Draw provenance is a physical-epoch
// condition, not something a later sampled VB/UP camera can repair.
// The original hardware COM Draws and Present still execute exactly once;
// rejecting speculative eyes preserves original mono/HUD and input/FFB.
extern "C" void __stdcall Model2VR_RejectOriginalD3D9SceneDrawCoverage(
    unsigned long long physical_present,unsigned int flags,
    unsigned int untrusted_successful_draws,unsigned int failed_draws){
    using namespace m2vr::vrbridge;
    if(!physical_present ||
       !(flags || untrusted_successful_draws || failed_draws) ||
       Model2VR_NextOriginalD3D9PresentFrameId()!=physical_present)return;
    bool first=false;
    {
        std::scoped_lock lock(g.mutex);
        if(g.owner==GetCurrentThreadId() && g.frame==physical_present &&
           (g.active || g.pending_present)){
            const auto old=g_current_original_source_failure;
            g_current_original_source_failure=OriginalSourceFailure::OriginalSceneFailed;
            g.Reject(CurrentOriginalSourceDrawKind());
            g_current_original_source_failure=old;
        }
        first=g.source_epoch.Revoke(physical_present);
    }
    if(first)
        m2vr::LogEvent("model2_d3d9_original_scene_coverage_rejected",
            physical_present,flags,untrusted_successful_draws,failed_draws);
    m2vr::daytona::MarkOriginalLineageMismatch();
}
// The native primary RT0/depth seen by every original four-family Draw
// must remain attributable to the same physical Scene/Present. Inferred
// D3D9 bindings, depth mismatch, crossed COM epochs and missing Draws are
// negative evidence, NEVER an automatic 3D/HMD permission grant.
extern "C" void __stdcall Model2VR_RejectOriginalD3D9TargetDrawCoverage(
    unsigned long long physical_present,unsigned int flags,
    unsigned int unknown_target_draws,unsigned int invalid_depth_draws){
    using namespace m2vr::vrbridge;
    if(!physical_present ||
       !(flags || unknown_target_draws || invalid_depth_draws) ||
       Model2VR_NextOriginalD3D9PresentFrameId()!=physical_present)return;
    bool first=false;
    {
        std::scoped_lock lock(g.mutex);
        if(g.owner==GetCurrentThreadId() && g.frame==physical_present &&
           (g.active || g.pending_present)){
            const auto old=g_current_original_source_failure;
            g_current_original_source_failure=OriginalSourceFailure::OriginalLineageMismatch;
            g.Reject(CurrentOriginalSourceDrawKind());
            g_current_original_source_failure=old;
        }
        first=g.source_epoch.Revoke(physical_present);
    }
    if(first)
        m2vr::LogEvent("model2_d3d9_original_target_coverage_rejected",
            physical_present,flags,unknown_target_draws,invalid_depth_draws);
    // The original game device/mono/2D/menu/FFB continue unchanged.
    m2vr::daytona::MarkOriginalLineageMismatch();
}
// Actual primary x86 camera COM mutations must remain from same Draw
// through the original physical Present. This rejects speculative CPU/GPU
// OpenXR eye sources while preserving original mono/gameplay/input/FFB.
extern "C" void __stdcall Model2VR_RejectOriginalD3D9CameraComLifetime(
    unsigned long long physical_present,unsigned int flags,
    unsigned int source_draws,unsigned int original_camera_writes){
    using namespace m2vr::vrbridge;
    if(!physical_present || !flags ||
       Model2VR_NextOriginalD3D9PresentFrameId()!=physical_present)return;
    bool first=false;
    {
        std::scoped_lock lock(g.mutex);
        if(g.owner==GetCurrentThreadId() && g.frame==physical_present &&
           (g.active || g.pending_present)){
            const auto previous=g_current_original_source_failure;
            g_current_original_source_failure=
                OriginalSourceFailure::OriginalLineageMismatch;
            g.Reject(CurrentOriginalSourceDrawKind());
            g_current_original_source_failure=previous;
        }
        first=g.source_epoch.Revoke(physical_present);
    }
    if(first)m2vr::LogEvent("model2_d3d9_original_camera_com_rejected",
        physical_present,flags,source_draws,original_camera_writes);
    m2vr::daytona::MarkOriginalCameraComLifetimeUnsafe();
}
// Strict original physical-Draw post-call check: a state write concurrent
// with CPU eye raster or the genuine D3D9 Draw may change its source.
extern "C" void __stdcall Model2VR_RejectOriginalD3D9SourceLineageDrift(
    unsigned long long physical_frame,unsigned long long ordinal,
    unsigned int family,unsigned int original_caller_rva){
    using namespace m2vr::vrbridge;
    if(!physical_frame || !ordinal || ordinal>10000000u ||
       family<1u || family>4u || !original_caller_rva ||
       Model2VR_NextOriginalD3D9PresentFrameId()!=physical_frame)return;
    {
        std::scoped_lock lock(g.mutex);
        if(g.owner==GetCurrentThreadId() && g.frame==physical_frame &&
           (g.active || g.pending_present)){
            const auto old_reason=g_current_original_source_failure;
            g_current_original_source_failure=OriginalSourceFailure::OriginalLineageMismatch;
            g.Reject(CurrentOriginalSourceDrawKind());
            g_current_original_source_failure=old_reason;
        }
        g.source_epoch.Revoke(physical_frame);
    }
    m2vr::LogEvent("model2_d3d9_original_lineage_drift",
        physical_frame,family,original_caller_rva,ordinal);
    m2vr::daytona::MarkOriginalLineageMismatch();
}
// Unknown original D3D9 StateBlock::Apply restores camera/material state
// without the individual hooked Set* calls. Regardless of original HRESULT,
// reject BOTH speculative binocular paths for the physical source Present.
extern "C" void __stdcall Model2VR_RejectOriginalD3D9StateBlockMutation(
    unsigned long long physical_present,unsigned int stage,
    unsigned int original_caller_rva,unsigned int original_hresult){
    using namespace m2vr::vrbridge;
    if(!physical_present || stage<1u || stage>2u ||
       !original_caller_rva ||
       Model2VR_NextOriginalD3D9PresentFrameId()!=physical_present)return;
    {
        std::scoped_lock lock(g.mutex);
        if(g.owner==GetCurrentThreadId() && g.frame==physical_present &&
           (g.active || g.pending_present)){
            const auto old_reason=g_current_original_source_failure;
            g_current_original_source_failure=OriginalSourceFailure::OriginalStateBlockUnknown;
            g.Reject(CurrentOriginalSourceDrawKind());
            g_current_original_source_failure=old_reason;
        }
        g.source_epoch.Revoke(physical_present);
    }
    // Public-safe witness: physical frame, stage, original EXE caller RVA,
    // original HRESULT (zero means observer setup failed). No COM pointers.
    if(g_stateblock_audit.Claim(physical_present))
        m2vr::LogEvent("model2_d3d9_original_stateblock_mutation",
            physical_present,stage,original_caller_rva,original_hresult);
    m2vr::daytona::MarkOriginalLineageMismatch();
}
// The original game's non-unit D3D9 vertex stream divider cannot be
// represented by the independent-eye CPU topology raster. Preserve mono.
extern "C" void __stdcall Model2VR_RejectOriginalD3D9InstancedStream(
    unsigned long long physical_present,unsigned int stream,
    unsigned int original_caller_rva,unsigned int frequency){
    using namespace m2vr::vrbridge;
    if(!physical_present || !original_caller_rva || frequency==1u ||
       Model2VR_NextOriginalD3D9PresentFrameId()!=physical_present)return;
    {
        std::scoped_lock lock(g.mutex);
        if(g.owner==GetCurrentThreadId() && g.frame==physical_present &&
           (g.active||g.pending_present)){
            const auto previous=g_current_original_source_failure;
            g_current_original_source_failure=OriginalSourceFailure::OriginalInstancedStreamUnknown;
            g.Reject(CurrentOriginalSourceDrawKind());
            g_current_original_source_failure=previous;
        }
        g.source_epoch.Revoke(physical_present);
    }
    m2vr::LogEvent("model2_d3d9_original_instancing_unsupported",
        physical_present,stream,original_caller_rva,frequency);
    m2vr::daytona::MarkOriginalLineageMismatch();
}
extern "C" void __stdcall Model2VR_RejectOriginalD3D9ActiveInstancing(
    unsigned long long physical_present,unsigned int active_stream_mask){
    using namespace m2vr::vrbridge;
    if(!physical_present || !active_stream_mask ||
       Model2VR_NextOriginalD3D9PresentFrameId()!=physical_present)return;
    {
        std::scoped_lock lock(g.mutex);
        if(g.owner==GetCurrentThreadId() && g.frame==physical_present &&
           (g.active||g.pending_present)){
            const auto old=g_current_original_source_failure;
            g_current_original_source_failure=OriginalSourceFailure::OriginalInstancedStreamUnknown;
            g.Reject(CurrentOriginalSourceDrawKind());
            g_current_original_source_failure=old;
        }
        g.source_epoch.Revoke(physical_present);
    }
    // a1 is the currently active unsafe-stream bit mask, not a game pointer.
    m2vr::LogEvent("model2_d3d9_original_instancing_active",
        physical_present,active_stream_mask,0u,0u);
    m2vr::daytona::MarkOriginalLineageMismatch();
}
extern "C" void __stdcall Model2VR_DiscardPendingWorldOnReset(){
    std::scoped_lock lock(g.mutex);
    // A physical original device Reset invalidates all stored CPU-world
    // image, source camera, pose and Present ownership. Do not permit
    // rebuilding the same rejected frame on the new device generation.
    if(g.pending_present || g.active)
        RejectWorldAtOriginalBoundary(
            m2vr::vrbridge::OriginalSourceFailure::DeviceReset,0u,5u);
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
