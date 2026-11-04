// Experimental native Daytona GPU eye replay. The game executes only its
// original mono Draw once; this module renders original GPU commands into
// independent left/right D3D9 surfaces, shifting only the verified shader-UP
// scene caller. Unsupported sources remain flat/HUD-safe in both eyes.
// Source-space shift is not a calibrated physical IPD or head rotation.
#include "daytona_native_stereo.hpp"
#include "daytona_up_decl_witness.hpp"
#include "daytona_shader_scene_gate.hpp"
#include "daytona_eye_pose_gate.hpp"
#include "daytona_triangle_eye_shift.hpp"
#include "daytona_readback_scale.hpp"
#include "daytona_surface_transfer_gate.hpp"
#include "daytona_auxiliary_surface_gate.hpp"
#include "logger.h"
#include "../common/stereo_frame_ipc.h"
#include "../common/daytona_producer_health.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <mutex>
#include <vector>

extern "C" BOOL __stdcall Model2VR_OpenStereoWriter();
extern "C" BOOL __stdcall Model2VR_ReadPredictedEyeViews(
    m2vr::pose::Frame* output);
// Associate the full original x86 GPU-eye pair with the ONE OpenXR
// predicted view pair latched before the first game Draw of this frame.
extern "C" BOOL __stdcall Model2VR_SubmitStereoRGBAWithPose(
    const unsigned char*,const unsigned char*,unsigned long long,
    unsigned int,unsigned int,unsigned int,unsigned long long);

namespace {
// The game hooks D3D9::Clear. Initializing an eye target calls Clear
// through the device vtable while the eye mutex is held; do NOT recurse into
// MirrorClear or try to acquire the same mutex again on this render thread.
thread_local bool g_in_eye_replay=false;
struct ScopedEyeReplay final {
    ScopedEyeReplay()noexcept {g_in_eye_replay=true;}
    ~ScopedEyeReplay(){g_in_eye_replay=false;}
    ScopedEyeReplay(const ScopedEyeReplay&)=delete;
    ScopedEyeReplay& operator=(const ScopedEyeReplay&)=delete;
};
template<class T> void Drop(T*& p) noexcept {
    if(p){p->Release();p=nullptr;}
}
bool GuardedCopy(void* dst,const void* src,std::size_t size) noexcept {
    if(!dst||!src||!size||size>65536u)return false;
    __try {std::memcpy(dst,src,size);return true;}
    __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
struct Surface {
    IDirect3DSurface9* color=nullptr;
    IDirect3DSurface9* depth=nullptr;
    IDirect3DSurface9* resolve=nullptr;
    IDirect3DSurface9* staging=nullptr;
    void Clear()noexcept{Drop(color);Drop(depth);Drop(resolve);Drop(staging);}
};
class EyeClones {
public:
    void Configure(bool enable,std::uint32_t shift)noexcept{
        std::lock_guard lock(mu_);
        enabled_=enable;
        shift_=float((std::clamp)(shift,1u,250u))/1000.f;
        first_stereo_published_=false;
        diag_attempts_=diag_shape_=diag_caller_=diag_shader_=0u;
        diag_state_=diag_shift_=diag_pose_=diag_resources_=0u;
        diag_eye_=0u;diag_witness_count_=0u;diag_witness_.fill(0u);
        Reset();
    }
    void ResetExternal()noexcept{
        std::lock_guard lock(mu_);
        Reset();
    }
    template<class Replay>
    void DrawAny(IDirect3DDevice9* d,D3DPRIMITIVETYPE primitive,UINT count,
                 const void* source,UINT stride,std::uint32_t caller,
                 unsigned family,Replay&& replay) {
        // An ordinary source-backed VB/IB Draw is replayed without modifying
        // game-managed VB/IB data. Only the separately proven Daytona UP
        // shader scene may receive the experimental two-eye X adjustment.
        if(!enabled_||!d||count==0||count>8192u||g_in_eye_replay)return;
        std::lock_guard lock(mu_);
        ScopedEyeReplay recursion_guard;
        ++diag_attempts_;
        // Bounded callsite evidence; never store game vertices or textures.
        if(family==3u && source && diag_witness_count_<diag_witness_.size()){
            const std::uint64_t sig=(std::uint64_t{caller}<<32)|
                (std::uint64_t{stride}<<16)|static_cast<std::uint32_t>(primitive);
            const auto end=diag_witness_.begin()+diag_witness_count_;
            if(std::find(diag_witness_.begin(),end,sig)==end){
                diag_witness_[diag_witness_count_++]=sig;
                DWORD fvf=0xffffffffu;
                if(FAILED(d->GetFVF(&fvf)))fvf=0xffffffffu;
                m2vr::LogEvent("model2_vr_diag_up_source",caller,
                    (std::uint64_t{static_cast<std::uint32_t>(primitive)}<<32)|count,
                    (std::uint64_t{stride}<<32)|fvf,0u);
            }
        }
        // The OpenXR host supplies a coherent, fresh two-eye tracking pair.
        // Never allocate GPU resources, readback, or re-render when the Quest
        // host is absent, disconnected, stale or returning identical eyes.
        // Latch once per emulator frame, not every game Draw.
        if(!LivePoseForGameFrame()){++diag_pose_;return;}
        if(!Ensure(d)){++diag_resources_;return;}
        if(!LivePoseForGameFrame()){++diag_pose_;return;}
        IDirect3DSurface9* target=nullptr;
        if(FAILED(d->GetRenderTarget(0,&target))||!target)return;
        const bool backbuffer=(target==backbuffer_);
        // Device caps are immutable: Ensure() proves them once per device.
        // Recheck actual extra MRT binding for every original draw.
        for(DWORD slot=1u;slot<mrt_slots_;++slot){
            IDirect3DSurface9* other=nullptr;
            const HRESULT hr=d->GetRenderTarget(slot,&other);
            const bool bound=SUCCEEDED(hr)&&other;
            Drop(other);
            // GetRenderTarget returns D3DERR_NOTFOUND for a normal,
            // unbound optional MRT slot. Only reject genuinely unknown
            // errors or a second attached render target.
            if(bound || (FAILED(hr) && hr!=D3DERR_NOTFOUND)){
                Drop(target);return;
            }
        }
        D3DVIEWPORT9 original_vp{};
        if(FAILED(d->GetViewport(&original_vp))){Drop(target);return;}
        IDirect3DSurface9* original_depth=nullptr;
        const HRESULT depth_hr=d->GetDepthStencilSurface(&original_depth);
        const bool depth_bound=SUCCEEDED(depth_hr)&&original_depth;
        if(!depth_bound){
            // A real game HUD/menu pass can detach the depth buffer. Mirror
            // only if the original draw explicitly disabled Z testing.
            DWORD z_enabled=1u;
            if(depth_hr!=D3DERR_NOTFOUND || original_depth ||
               FAILED(d->GetRenderState(D3DRS_ZENABLE,&z_enabled)) ||
               z_enabled!=D3DZB_FALSE){
                Drop(original_depth);Drop(target);return;
            }
        }
        if(backbuffer && depth_bound && !MainDepthMatches(original_depth)){
            Drop(original_depth);Drop(target);Reset();return;
        }
        const bool aux=!backbuffer &&
            target==aux_original_ && aux_eyes_[0].color &&
            (!depth_bound || original_depth==aux_original_depth_);
        if(!backbuffer && (!aux || !aux_frame_started_)){
            Drop(original_depth);Drop(target);return;
        }
        auto& active_eyes=backbuffer?eyes_:aux_eyes_;
        bool& active_started=backbuffer?frame_started_:aux_frame_started_;
        bool& depth_ready=backbuffer?main_depth_ready_:aux_depth_ready_;
        // A prior color-only Clear may have initialized pixels but not Z.
        // Do not rasterize an uninitialized clone depth surface.
        if(depth_bound && active_started && !depth_ready){
            Drop(original_depth);Drop(target);return;
        }
        // A camera candidate requires an exact independently observed
        // source callsite and complete original shader-up declaration.
        bool scene=false;
        const bool bounded_up=source && stride>=16u && stride<=256u && count<=512u;
        const std::size_t bytes=bounded_up?std::size_t{count}*3u*stride:0u;
        if(bounded_up && primitive==D3DPT_TRIANGLELIST &&
           stride==32u && bytes<=49152u)++diag_shape_;
        if(bounded_up && caller==0x000BABADu && primitive==D3DPT_TRIANGLELIST &&
           stride==32u && bytes<=49152u) {
            ++diag_caller_;
            DWORD fvf=1,z=0,alpha=1;
            IDirect3DVertexShader9* vs=nullptr;
            IDirect3DPixelShader9* ps=nullptr;
            IDirect3DVertexDeclaration9* decl=nullptr;
            if(SUCCEEDED(d->GetFVF(&fvf))&&fvf==0 &&
               SUCCEEDED(d->GetRenderState(D3DRS_ZENABLE,&z))&&z &&
               SUCCEEDED(d->GetRenderState(D3DRS_ALPHABLENDENABLE,&alpha))&&!alpha&&
               SUCCEEDED(d->GetVertexShader(&vs))&&vs &&
               SUCCEEDED(d->GetPixelShader(&ps))&&ps &&
               SUCCEEDED(d->GetVertexDeclaration(&decl))&&decl) {
                std::array<D3DVERTEXELEMENT9,16> elements{};
                UINT n=static_cast<UINT>(elements.size());
                if(SUCCEEDED(decl->GetDeclaration(elements.data(),&n)) &&
                   m2vr::drawprobe::ExactDaytonaUPDeclaration(elements.data(),n,stride))
                    scene=true;
            }
            Drop(vs);Drop(ps);Drop(decl);
            if(scene){
                ++diag_shader_;
                ++scene_candidates_;
                const auto verdict=m2vr::daytona::CheckShaderUPCandidate(
                    d,original_vp,width_,height_);
                if(verdict!=m2vr::daytona::ShaderCandidateReject::None){
                    const auto index=static_cast<std::size_t>(verdict);
                    if(index<scene_rejections_.size())++scene_rejections_[index];
                    scene=false; // pass is copied flat rather than stereo
                }else ++diag_state_;
            }
        }
        if(scene) {
            if(scratch_[0].size()<bytes)scratch_[0].resize(bytes);
            if(scratch_[1].size()<bytes)scratch_[1].resize(bytes);
            scene=GuardedCopy(scratch_[0].data(),source,bytes);
            if(scene) {
                std::memcpy(scratch_[1].data(),scratch_[0].data(),bytes);
                // Preserve negative/mixed/near-zero W TRIANGLES flat,
                // but do not let a single behind-camera triangle cancel
                // stereo for other safe triangles in the same original draw.
                const float eye_offset=shift_*ipd_scale_;
                const auto stats=m2vr::daytona::ShiftSafeUPTriangles(
                    scratch_[0].data(),scratch_[1].data(),bytes,count,
                    eye_offset);
                scene=stats.shifted>0u;
                if(scene)++diag_shift_;
            }
        }
        const D3DVIEWPORT9 viewport=ClippedViewport(original_vp);
        bool both=true;
        for(unsigned eye=0;eye<2;++eye){
            // SetDepthStencilSurface(nullptr) first avoids surface size/MSAA
            // mismatch while switching the original render-target.
            if(FAILED(d->SetDepthStencilSurface(nullptr)) ||
               FAILED(d->SetRenderTarget(0,active_eyes[eye].color)) ||
               FAILED(d->SetDepthStencilSurface(depth_bound?active_eyes[eye].depth:nullptr)) ||
               FAILED(d->SetViewport(&viewport))) {
                both=false;break;
            }
            if(!active_started) {
                // Preserve an actual source Z-only Clear already mirrored
                // before the first color draw. Only seed Z if still unknown.
                const DWORD clear_flags=D3DCLEAR_TARGET|
                    (depth_bound && !depth_ready?D3DCLEAR_ZBUFFER:0u);
                if(FAILED(d->Clear(0,nullptr,clear_flags,
                                   D3DCOLOR_XRGB(0,0,0),1.f,0))){
                    both=false;break;
                }
            }
            const void* vertices=scene?scratch_[eye].data():source;
            if(FAILED(replay(eye,vertices)))
                both=false;
        }
        // Restore original game's RT, DS and exact viewport even if one
        // stereo eye fails. Never suppress its original successful draw.
        const HRESULT detach=d->SetDepthStencilSurface(nullptr);
        const HRESULT target_restore=d->SetRenderTarget(0,target);
        const HRESULT depth_restore=d->SetDepthStencilSurface(original_depth);
        const HRESULT viewport_restore=d->SetViewport(&original_vp);
        const bool restored=SUCCEEDED(detach)&&SUCCEEDED(target_restore)&&
            SUCCEEDED(depth_restore)&&SUCCEEDED(viewport_restore);
        Drop(original_depth);Drop(target);
        if(!both || !restored){++diag_eye_;Reset();return;}
        // The synthetic first-draw clear initializes both color and Z.
        if(depth_bound && !active_started)depth_ready=true;
        active_started=true;
        if(aux)++mirrored_aux_draws_;
        ++mirrored_draws_;
        if(family>=1u && family<=4u)++mirrored_family_[family-1u];
        if(scene)++scene_draws_;
    }
    void Draw(IDirect3DDevice9* d,D3DPRIMITIVETYPE p,UINT n,
              const void* v,UINT stride,std::uint32_t caller,
              m2vr::daytona::OriginalDrawUP original){
        if(!v||!original||!stride)return;
        DrawAny(d,p,n,v,stride,caller,3u,[=](unsigned,const void* source){
            return original(d,p,n,source,stride);
        });
    }
    void Primitive(IDirect3DDevice9* d,D3DPRIMITIVETYPE p,UINT start,
                   UINT n,m2vr::daytona::OriginalDrawPrimitive original){
        if(!original)return;
        DrawAny(d,p,n,nullptr,0u,0u,1u,[=](unsigned,const void*){
            return original(d,p,start,n);
        });
    }
    void Indexed(IDirect3DDevice9* d,D3DPRIMITIVETYPE p,INT base,
                 UINT min,UINT num,UINT start,UINT n,
                 m2vr::daytona::OriginalDrawIndexed original){
        if(!original)return;
        DrawAny(d,p,n,nullptr,0u,0u,2u,[=](unsigned,const void*){
            return original(d,p,base,min,num,start,n);
        });
    }
    void IndexedUP(IDirect3DDevice9* d,D3DPRIMITIVETYPE p,UINT min,
                   UINT num,UINT n,const void* indices,D3DFORMAT format,
                   const void* vertices,UINT stride,
                   m2vr::daytona::OriginalDrawIndexedUP original){
        if(!original||!indices||!vertices||!stride)return;
        // IndexedUP stream-relative source indexing is preserved verbatim;
        // no false per-vertex scene shift or unproven index remapping.
        DrawAny(d,p,n,nullptr,0u,0u,4u,[=](unsigned,const void*){
            return original(d,p,min,num,n,indices,format,vertices,stride);
        });
    }
    // Restore a successful original system-memory-to-backbuffer copy into
    // both eyes. Validate geometry before issuing either eye call.
    void UpdateSurface(IDirect3DDevice9* d,IDirect3DSurface9* src,
                       const RECT* src_rect,IDirect3DSurface9* dst,
                       const POINT* dst_point,
                       m2vr::daytona::OriginalUpdateSurface original){
        if(!enabled_||!d||!src||!dst||!original||g_in_eye_replay)return;
        std::lock_guard lock(mu_);
        ScopedEyeReplay recursion_guard;
        if(!LivePoseForGameFrame() || !Ensure(d) ||
           !LivePoseForGameFrame())return;
        const bool to_main=dst==backbuffer_;
        const bool to_aux=dst==aux_original_ && aux_eyes_[0].color;
        if((!to_main && !to_aux) || src==backbuffer_ ||
           src==aux_original_)return;
        D3DSURFACE_DESC source_desc{};
        if(FAILED(src->GetDesc(&source_desc)))return;
        bool full=false;
        auto& active_eyes=to_main?eyes_:aux_eyes_;
        bool& active_started=to_main?frame_started_:aux_frame_started_;
        if(!m2vr::daytona::UpdateSurfaceBounds(
               src_rect,source_desc.Width,source_desc.Height,dst_point,
               width_,height_,full) ||
           !m2vr::daytona::CanMirrorSurfaceTransfer(
               true,true,full,active_started))return;
        bool both=true;
        for(auto& eye:active_eyes){
            if(FAILED(original(d,src,src_rect,eye.color,dst_point))){
                both=false;break;
            }
        }
        if(!both){Reset();return;}
        active_started=true;
        ++mirrored_updates_;
    }
    // Mirror original offscreen-to-backbuffer GPU copies in exact call order.
    // Only full writes may seed a fresh eye; never read the original
    // backbuffer as if it were the earlier independent eye source.
    void StretchRect(IDirect3DDevice9* d,IDirect3DSurface9* src,
                     const RECT* src_rect,IDirect3DSurface9* dst,
                     const RECT* dst_rect,D3DTEXTUREFILTERTYPE filter,
                     m2vr::daytona::OriginalStretchRect original){
        if(!enabled_||!d||!src||!dst||!original||g_in_eye_replay)return;
        std::lock_guard lock(mu_);
        ScopedEyeReplay recursion_guard;
        if(!LivePoseForGameFrame() || !Ensure(d) ||
           !LivePoseForGameFrame())return;
        const bool to_main=dst==backbuffer_;
        const bool to_aux=dst==aux_original_ && aux_eyes_[0].color;
        const auto route=m2vr::daytona::RouteOriginalToMain(
            to_main,src==backbuffer_,src==aux_original_,
            aux_frame_started_,
            m2vr::daytona::FullSurfaceRect(dst_rect,width_,height_),
            frame_started_);
        if(to_main && route==m2vr::daytona::MainBlitSource::Skip)return;
        if(!to_main && (!to_aux || src==dst || src==backbuffer_))return;
        auto& active_eyes=to_main?eyes_:aux_eyes_;
        bool& active_started=to_main?frame_started_:aux_frame_started_;
        if(!to_main && !m2vr::daytona::CanMirrorSurfaceTransfer(
               true,true,
               m2vr::daytona::FullSurfaceRect(dst_rect,width_,height_),
               active_started))return;
        const bool stereo_source=to_main &&
            route==m2vr::daytona::MainBlitSource::StereoAuxiliary;
        bool both=true;
        for(unsigned eye=0;eye<2;++eye){
            auto* eye_source=stereo_source?aux_eyes_[eye].color:src;
            if(FAILED(original(d,eye_source,src_rect,
                               active_eyes[eye].color,dst_rect,filter))){
                both=false;break;
            }
        }
        if(!both){Reset();return;}
        if(stereo_source)++mirrored_aux_blits_;
        active_started=true;
        ++mirrored_stretches_;
    }
    void ColorFill(IDirect3DDevice9* d,IDirect3DSurface9* dst,
                   const RECT* rect,D3DCOLOR color,
                   m2vr::daytona::OriginalColorFill original){
        if(!enabled_||!d||!dst||!original||g_in_eye_replay)return;
        std::lock_guard lock(mu_);
        ScopedEyeReplay recursion_guard;
        if(!LivePoseForGameFrame() || !Ensure(d) ||
           !LivePoseForGameFrame())return;
        const bool to_main=dst==backbuffer_;
        const bool to_aux=dst==aux_original_ && aux_eyes_[0].color;
        if(!to_main && !to_aux)return;
        auto& active_eyes=to_main?eyes_:aux_eyes_;
        bool& active_started=to_main?frame_started_:aux_frame_started_;
        if(!m2vr::daytona::CanMirrorSurfaceTransfer(
               true,true,
               m2vr::daytona::FullSurfaceRect(rect,width_,height_),
               active_started))return;
        bool both=true;
        for(auto& eye:active_eyes){
            if(FAILED(original(d,eye.color,rect,color))){both=false;break;}
        }
        if(!both){Reset();return;}
        active_started=true;
        ++mirrored_fills_;
    }
    void Clear(IDirect3DDevice9* d,DWORD count,const D3DRECT* rects,
               DWORD flags,D3DCOLOR color,float z,DWORD stencil,
               m2vr::daytona::OriginalClear original){
        if(!enabled_||!d||!original||!flags || g_in_eye_replay ||
           (flags&~(D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER|D3DCLEAR_STENCIL)))return;
        std::lock_guard lock(mu_);
        ScopedEyeReplay recursion_guard;
        if(!LivePoseForGameFrame() || !Ensure(d) ||
           !LivePoseForGameFrame())return;
        IDirect3DSurface9* target=nullptr;
        if(FAILED(d->GetRenderTarget(0,&target))||!target)return;
        const bool backbuffer=target==backbuffer_;
        // Never reflect a multi-target clear into an unrelated stereo eye.
        for(DWORD slot=1;slot<mrt_slots_;++slot){
            IDirect3DSurface9* other=nullptr;
            const HRESULT hr=d->GetRenderTarget(slot,&other);
            const bool attached=SUCCEEDED(hr)&&other;
            Drop(other);
            if(attached || (FAILED(hr)&&hr!=D3DERR_NOTFOUND)){
                Drop(target);return;
            }
        }
        IDirect3DSurface9* original_depth=nullptr;
        const HRESULT depth_hr=d->GetDepthStencilSurface(&original_depth);
        const bool depth_bound=SUCCEEDED(depth_hr)&&original_depth;
        if(!depth_bound && (depth_hr!=D3DERR_NOTFOUND || original_depth ||
           (flags&(D3DCLEAR_ZBUFFER|D3DCLEAR_STENCIL))!=0u)){
            Drop(original_depth);Drop(target);return;
        }
        if(backbuffer && depth_bound && !MainDepthMatches(original_depth)){
            Drop(original_depth);Drop(target);Reset();return;
        }
        D3DVIEWPORT9 viewport{};
        if(FAILED(d->GetViewport(&viewport))){
            Drop(original_depth);Drop(target);return;
        }
        const bool full_color_clear=m2vr::daytona::FullOriginalColorClear(
            count,rects,flags,viewport,width_,height_);
        // A partial initial color clear cannot initialize the remainder of
        // a new eye surface. Depth-only clears remain independent of color.
        if(backbuffer && !frame_started_ &&
           (flags&D3DCLEAR_TARGET)!=0u && !full_color_clear){
            Drop(original_depth);Drop(target);return;
        }
        if(!backbuffer){
            const bool initialized=aux_frame_started_ || full_color_clear;
            if(!depth_bound || !initialized ||
               !EnsureAuxiliary(d,target,original_depth)){
                Drop(original_depth);Drop(target);return;
            }
        }
        auto& active_eyes=backbuffer?eyes_:aux_eyes_;
        bool& active_started=backbuffer?frame_started_:aux_frame_started_;
        bool& depth_ready=backbuffer?main_depth_ready_:aux_depth_ready_;
        const bool full_depth_clear=depth_bound &&
            m2vr::daytona::FullOriginalDepthClear(
                count,rects,flags,viewport,width_,height_);
        bool both=true;
        for(unsigned eye=0;eye<2;++eye){
            if(FAILED(d->SetDepthStencilSurface(nullptr)) ||
               FAILED(d->SetRenderTarget(0,active_eyes[eye].color)) ||
               FAILED(d->SetDepthStencilSurface(depth_bound?active_eyes[eye].depth:nullptr)) ||
               FAILED(d->SetViewport(&viewport)) ||
               FAILED(original(d,count,rects,flags,color,z,stencil))){
                both=false;break;
            }
        }
        const HRESULT detach=d->SetDepthStencilSurface(nullptr);
        const HRESULT target_restore=d->SetRenderTarget(0,target);
        const HRESULT depth_restore=d->SetDepthStencilSurface(original_depth);
        const HRESULT viewport_restore=d->SetViewport(&viewport);
        const bool restored=SUCCEEDED(detach)&&SUCCEEDED(target_restore)&&
            SUCCEEDED(depth_restore)&&SUCCEEDED(viewport_restore);
        Drop(original_depth);Drop(target);
        if(!both || !restored){Reset();return;}
        // A Z-only original Clear does not initialize the color plane;
        // a full-screen original color clear does. Preserve both separately.
        if(full_color_clear)active_started=true;
        if(full_depth_clear)depth_ready=true;
        ++mirrored_clears_;
    }
    void PreparePresent(IDirect3DDevice9* d,std::uint64_t frame){
        if(!enabled_ || !frame)return;
        std::lock_guard lock(mu_);
        // MSAA resolves call device->StretchRect while this mutex is held.
        // That COM method is also hooked: its synthetic internal call must
        // never re-enter MirrorStretchRect and deadlock the render thread.
        ScopedEyeReplay recursion_guard;
        pending_present_frame_=frame;
        pending_present_reason_=0u;
        // Read the current game frame before the original Present can flip
        // its backbuffer. No transport writer or health is touched here.
        const bool fresh_pose=pose_admitted_ && LivePoseNow();
        const std::uint32_t gate=
            (!frame_started_ ? 1u : 0u) |
            (!scene_draws_ ? 2u : 0u) |
            (!d || d!=device_ ? 4u : 0u) |
            (!pose_admitted_ ? 8u : 0u) |
            (!fresh_pose ? 16u : 0u);
        if(gate){
            pending_present_reason_=gate;
            return;
        }
        bool ready=true;
        for(unsigned eye=0;eye<2;++eye){
            IDirect3DSurface9* readback_source=eyes_[eye].color;
            if(eyes_[eye].resolve){
                if(FAILED(d->StretchRect(eyes_[eye].color,nullptr,
                                         eyes_[eye].resolve,nullptr,
                                         D3DTEXF_NONE))){
                    ready=false;break;
                }
                readback_source=eyes_[eye].resolve;
            }
            if(FAILED(d->GetRenderTargetData(readback_source,
                                             eyes_[eye].staging))){
                ready=false;break;
            }
            D3DLOCKED_RECT locked{};
            if(FAILED(eyes_[eye].staging->LockRect(
                      &locked,nullptr,D3DLOCK_READONLY))){
                ready=false;break;
            }
            const bool converted=ConvertBGRA(locked,rgba_[eye]);
            eyes_[eye].staging->UnlockRect();
            if(!converted){ready=false;break;}
        }
        std::uint32_t reason=ready?0u:32u;
        if(ready && rgba_[0]==rgba_[1]){
            ready=false;
            reason=64u;
        }
        pending_present_reason_=reason;
    }
    void FinishPresent(std::uint64_t frame,HRESULT original_result)noexcept{
        if(!enabled_ || !frame)return;
        std::lock_guard lock(mu_);
        // A missing/mismatched pre-Present stage cannot publish a previous
        // frame just because the game completed a later Present.
        const bool coherent=pending_present_frame_==frame;
        const std::uint32_t prepared=coherent?pending_present_reason_:4u;
        const bool original_ok=SUCCEEDED(original_result);
        const bool pose_live=coherent && prepared==0u && original_ok
            ? LivePoseNow() : true;
        std::uint32_t reason=m2vr::producerhealth::FinalPresentReason(
            prepared,original_ok,pose_live);
        if(!original_ok){
            // Log a transition and bounded heartbeat, not a 60fps error
            // flood on a lost D3D9 device.
            if(original_result!=last_failed_present_ ||
               frame==1u || frame%120u==0u)
                m2vr::LogEvent("model2_vr_original_present_failed",
                              frame,static_cast<std::uint32_t>(original_result),
                              prepared,0u);
            last_failed_present_=original_result;
        }else last_failed_present_=S_OK;
        if(m2vr::producerhealth::MayPublishGameStereo(reason)){
            if(!writer_ready_)writer_ready_=Model2VR_OpenStereoWriter()!=FALSE;
            if(!writer_ready_ || !pose_sequence_ ||
               !Model2VR_SubmitStereoRGBAWithPose(
                   rgba_[0].data(),rgba_[1].data(),frame,
                   m2vr::transport::kWidth,m2vr::transport::kHeight,
                   m2vr::transport::kPitch,pose_sequence_))reason=128u;
        }
        // Health is committed after the *actual original* Present outcome.
        // x64 may revoke a cached older eye frame on a newer game failure.
        ReportFrame(frame,reason);
        FinishFrame();
    }
private:
    void ReportFrame(std::uint64_t frame,std::uint32_t reason)noexcept{
        if(!frame)return;
        if(!health_.Valid())health_.Create(GetCurrentProcessId());
        if(health_.Valid())health_.Publish(frame,reason,scene_draws_,
                                          mirrored_draws_,mirrored_clears_,
                                          mirrored_family_);
        const bool first=reason==0u && !first_stereo_published_;
        // Every 120 original Presents (roughly 2s at 60 Hz), plus the
        // first successful game source. The launcher keeps the CSV locally.
        if(frame==1u || frame%120u==0u || first){
            const std::uint64_t replay_count=
                (std::uint64_t{mirrored_clears_}<<32)|mirrored_draws_;
            m2vr::LogEvent("model2_vr_producer_frame",frame,reason,
                           scene_draws_,replay_count);
            m2vr::LogEvent("model2_vr_surface_updates",frame,
                           mirrored_stretches_,mirrored_fills_,mirrored_updates_);
            m2vr::LogEvent("model2_vr_aux_surface_chain",frame,
                           mirrored_aux_draws_,mirrored_aux_blits_,0u);
            if(scene_candidates_){
                m2vr::LogEvent("model2_vr_scene_gate",frame,scene_candidates_,
                               scene_draws_,scene_rejections_[2]);
                for(std::size_t reason=1;reason<scene_rejections_.size();++reason)
                    if(scene_rejections_[reason])
                        m2vr::LogEvent("model2_vr_scene_reject_reason",frame,
                                       reason,scene_rejections_[reason],0u);
            }
            m2vr::LogEvent("model2_vr_diag_source_gates",frame,
                (std::uint64_t{diag_shape_}<<32)|diag_caller_,
                (std::uint64_t{diag_shader_}<<32)|diag_state_,
                (std::uint64_t{diag_shift_}<<32)|diag_attempts_);
            m2vr::LogEvent("model2_vr_diag_source_drops",frame,
                diag_pose_,diag_resources_,diag_eye_);
            if(first)m2vr::FlushLog();
        }
        if(first)first_stereo_published_=true;
    }
    bool LivePoseNow()noexcept {
        m2vr::pose::Frame frame{};
        if(!Model2VR_ReadPredictedEyeViews(&frame))return false;
        LARGE_INTEGER ticks{},frequency{};
        if(!QueryPerformanceCounter(&ticks) ||
           !QueryPerformanceFrequency(&frequency) ||
           ticks.QuadPart<=0 || frequency.QuadPart<=0)return false;
        return m2vr::daytona::AdmitLiveEyePose(
            frame,static_cast<std::uint64_t>(ticks.QuadPart),
            static_cast<std::uint64_t>(frequency.QuadPart)).valid;
    }
    bool LivePoseForGameFrame()noexcept {
        if(pose_checked_)return pose_admitted_;
        pose_checked_=true;
        pose_admitted_=false;
        pose_sequence_=0;
        m2vr::pose::Frame frame{};
        if(!Model2VR_ReadPredictedEyeViews(&frame))return false;
        LARGE_INTEGER ticks{},frequency{};
        if(!QueryPerformanceCounter(&ticks) ||
           !QueryPerformanceFrequency(&frequency) ||
           ticks.QuadPart<=0 || frequency.QuadPart<=0)return false;
        const auto admitted=m2vr::daytona::AdmitLiveEyePose(
            frame,static_cast<std::uint64_t>(ticks.QuadPart),
            static_cast<std::uint64_t>(frequency.QuadPart));
        pose_admitted_=admitted.valid &&
            m2vr::daytona::CanTagGameStereoPose(admitted.sequence);
        if(pose_admitted_){
            pose_sequence_=admitted.sequence;
            ipd_scale_=admitted.ipd_scale;
        }
        return pose_admitted_;
    }
    void FinishFrame()noexcept{
        pending_present_frame_=0u;pending_present_reason_=0u;
        frame_started_=false;scene_draws_=0;
        main_depth_ready_=false;aux_depth_ready_=false;
        mirrored_draws_=0;mirrored_clears_=0;
        mirrored_stretches_=0;mirrored_fills_=0;mirrored_updates_=0;
        mirrored_aux_draws_=0;mirrored_aux_blits_=0;
        aux_frame_started_=false;
        scene_candidates_=0;
        scene_rejections_.fill(0u);
        mirrored_family_.fill(0u);
        pose_checked_=false;pose_admitted_=false;pose_sequence_=0;
    }
    static D3DVIEWPORT9 Clip(D3DVIEWPORT9 vp,UINT w,UINT h)noexcept {
        if(vp.X>=w||vp.Y>=h){vp.X=0;vp.Y=0;vp.Width=w;vp.Height=h;}
        else{
            vp.Width=(std::min)(vp.Width,w-vp.X);
            vp.Height=(std::min)(vp.Height,h-vp.Y);
        }
        if(!vp.Width||!vp.Height){vp.X=0;vp.Y=0;vp.Width=w;vp.Height=h;}
        return vp;
    }
    D3DVIEWPORT9 ClippedViewport(D3DVIEWPORT9 vp)const noexcept{
        return Clip(vp,width_,height_);
    }
    // Admit the physical main depth only when the game actually binds it.
    // A previously started auxiliary eye frame cannot swap to unrelated Z.
    bool MainDepthMatches(IDirect3DSurface9* depth)noexcept{
        if(!depth || !eyes_[0].depth || !eyes_[1].depth)return false;
        D3DSURFACE_DESC actual{},left{},right{};
        if(FAILED(depth->GetDesc(&actual)) ||
           FAILED(eyes_[0].depth->GetDesc(&left)) ||
           FAILED(eyes_[1].depth->GetDesc(&right)) ||
           !m2vr::daytona::SameEyeDepthLayout(actual,left) ||
           !m2vr::daytona::SameEyeDepthLayout(actual,right))return false;
        if(main_depth_)return main_depth_==depth;
        if(aux_frame_started_ && aux_original_depth_ &&
           aux_original_depth_!=depth)return false;
        main_depth_=depth;
        main_depth_->AddRef();
        return true;
    }
    bool EnsureAuxiliary(IDirect3DDevice9* d,IDirect3DSurface9* target,
                         IDirect3DSurface9* depth){
        if(!target || !depth || target==backbuffer_ || !backbuffer_)
            return false;
        if(aux_original_)return aux_original_==target &&
            aux_original_depth_==depth && aux_eyes_[0].color &&
            aux_eyes_[1].color;
        D3DSURFACE_DESC color_desc{},depth_desc{},main_desc{};
        if(FAILED(target->GetDesc(&color_desc)) ||
           FAILED(depth->GetDesc(&depth_desc)) ||
           FAILED(backbuffer_->GetDesc(&main_desc)) ||
           !m2vr::daytona::SourceCompatibleWithMain(
                color_desc,depth_desc,main_desc))return false;
        std::array<Surface,2> created{};
        const bool shared_depth=depth==main_depth_ || !main_depth_;
        bool ok=true;
        for(unsigned eye=0;eye<2;++eye){
            if(FAILED(d->CreateRenderTarget(width_,height_,color_desc.Format,
                     color_desc.MultiSampleType,color_desc.MultiSampleQuality,
                     FALSE,&created[eye].color,nullptr))){
                ok=false;break;
            }
            if(shared_depth){
                created[eye].depth=eyes_[eye].depth;
                created[eye].depth->AddRef();
            }else if(FAILED(d->CreateDepthStencilSurface(width_,height_,
                  depth_desc.Format,depth_desc.MultiSampleType,
                  depth_desc.MultiSampleQuality,TRUE,
                  &created[eye].depth,nullptr))){
                ok=false;break;
            }
        }
        if(!ok){
            for(auto& eye:created)eye.Clear();
            return false;
        }
        for(unsigned eye=0;eye<2;++eye){
            aux_eyes_[eye].color=created[eye].color;
            aux_eyes_[eye].depth=created[eye].depth;
        }
        aux_original_=target;aux_original_->AddRef();
        aux_original_depth_=depth;aux_original_depth_->AddRef();
        return true;
    }
    bool Ensure(IDirect3DDevice9* d){
        if(d==device_ && backbuffer_ && eyes_[0].color&&eyes_[1].color)return true;
        Reset();
        IDirect3DSurface9* back=nullptr;
        if(FAILED(d->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&back)) ||
           !back)return false;
        D3DSURFACE_DESC desc{};
        if(FAILED(back->GetDesc(&desc)) ||
           desc.Width<320u||desc.Width>2048u ||
           desc.Height<200u||desc.Height>2048u ||
           (desc.Format!=D3DFMT_A8R8G8B8 && desc.Format!=D3DFMT_X8R8G8B8)){
            Drop(back);return false;
        }
        // Original Daytona rendering can start on a full-size auxiliary
        // render target. Accept its verified format/depth layout; never
        // mistake a small UI or mirror target for the main scene.
        IDirect3DSurface9* current=nullptr;
        IDirect3DSurface9* depth=nullptr;
        D3DSURFACE_DESC source_desc{},dd{};
        if(FAILED(d->GetRenderTarget(0,&current)) || !current ||
           FAILED(d->GetDepthStencilSurface(&depth)) || !depth ||
           FAILED(current->GetDesc(&source_desc)) ||
           FAILED(depth->GetDesc(&dd)) ||
           !m2vr::daytona::BootstrapTargetCompatible(
               source_desc,dd,desc)){
            Drop(depth);Drop(current);Drop(back);return false;
        }
        const bool source_is_main=current==back;
        const D3DFORMAT depth_format=dd.Format;
        Drop(current);
        D3DCAPS9 caps{};
        if(FAILED(d->GetDeviceCaps(&caps)) || caps.NumSimultaneousRTs<1u ||
           caps.NumSimultaneousRTs>4u){
            Drop(depth);Drop(back);return false;
        }
        mrt_slots_=caps.NumSimultaneousRTs;
        // If bootstrap began on auxiliary, postpone main DS ownership.
        // Both provisional eye depths are shared with that auxiliary.
        if(source_is_main){main_depth_=depth;main_depth_->AddRef();}
        Drop(depth);
        backbuffer_=back;device_=d;
        width_=desc.Width;height_=desc.Height;
        if(!readback_scale_.Configure(width_,height_)){
            Reset();return false;
        }
        for(auto& eye:eyes_){
            if(FAILED(d->CreateRenderTarget(width_,height_,desc.Format,
                       desc.MultiSampleType,desc.MultiSampleQuality,
                       FALSE,&eye.color,nullptr)) ||
               FAILED(d->CreateDepthStencilSurface(width_,height_,depth_format,
                       desc.MultiSampleType,desc.MultiSampleQuality,
                       TRUE,&eye.depth,nullptr))){
                Reset();return false;
            }
            if(desc.MultiSampleType!=D3DMULTISAMPLE_NONE &&
               FAILED(d->CreateRenderTarget(width_,height_,desc.Format,
                       D3DMULTISAMPLE_NONE,0,FALSE,&eye.resolve,nullptr))){
                Reset();return false;
            }
            if(FAILED(d->CreateOffscreenPlainSurface(width_,height_,desc.Format,
                       D3DPOOL_SYSTEMMEM,&eye.staging,nullptr))){
                Reset();return false;
            }
        }
        for(auto& pixels:rgba_)pixels.resize(m2vr::transport::kBytesPerEye);
        return true;
    }
    bool ConvertBGRA(const D3DLOCKED_RECT& source,
                     std::vector<std::uint8_t>& output)noexcept{
        // Original D3D9 staging surfaces are BGRA8 and may have padded rows.
        // Reject an invalid or negative driver pitch rather than treating
        // it as an enormous positive offset into an unmapped buffer.
        return readback_scale_.Convert(
            static_cast<const std::uint8_t*>(source.pBits),
            source.Pitch,output.data(),output.size());
    }
    void Reset()noexcept{
        for(auto& eye:aux_eyes_)eye.Clear();
        Drop(aux_original_);Drop(aux_original_depth_);
        for(auto& eye:eyes_)eye.Clear();
        Drop(main_depth_);Drop(backbuffer_);
        device_=nullptr;width_=height_=0u;mrt_slots_=0u;
        readback_scale_=m2vr::daytona::ReadbackScalePlan{};
        FinishFrame();
    }
    std::mutex mu_;
    m2vr::producerhealth::Link health_;
    bool enabled_=false,frame_started_=false,writer_ready_=false;
    std::uint64_t pending_present_frame_=0u;
    std::uint32_t pending_present_reason_=0u;
    HRESULT last_failed_present_=S_OK;
    bool pose_checked_=false,pose_admitted_=false;
    std::uint64_t pose_sequence_=0;
    bool first_stereo_published_=false;
    float ipd_scale_=1.f;
    UINT width_=0,height_=0;
    DWORD mrt_slots_=0u;
    std::uint32_t mirrored_draws_=0u,mirrored_clears_=0u;
    std::uint32_t mirrored_stretches_=0u,mirrored_fills_=0u;
    std::uint32_t mirrored_updates_=0u;
    std::uint32_t mirrored_aux_draws_=0u,mirrored_aux_blits_=0u;
    bool aux_frame_started_=false;
    bool main_depth_ready_=false,aux_depth_ready_=false;
    std::array<std::uint32_t,4> mirrored_family_{};
    std::uint32_t diag_attempts_=0u,diag_shape_=0u,diag_caller_=0u;
    std::uint32_t diag_shader_=0u,diag_state_=0u,diag_shift_=0u;
    std::uint32_t diag_pose_=0u,diag_resources_=0u,diag_eye_=0u;
    std::array<std::uint64_t,12> diag_witness_{};
    std::size_t diag_witness_count_=0u;
    float shift_=0.10f;
    unsigned scene_draws_=0,scene_candidates_=0;
    std::array<std::uint32_t,m2vr::daytona::kShaderCandidateReasons>
        scene_rejections_{};
    IDirect3DDevice9* device_=nullptr; // non-owning, Reset hook releases
    IDirect3DSurface9* backbuffer_=nullptr;
    IDirect3DSurface9* main_depth_=nullptr;
    // One full-size, source-proven auxiliary pass per device generation.
    IDirect3DSurface9* aux_original_=nullptr;
    IDirect3DSurface9* aux_original_depth_=nullptr;
    std::array<Surface,2> aux_eyes_{};
    std::array<Surface,2> eyes_{};
    std::array<std::vector<std::uint8_t>,2> scratch_{};
    std::array<std::vector<std::uint8_t>,2> rgba_{};
    m2vr::daytona::ReadbackScalePlan readback_scale_{};
};
EyeClones g_eye;
} // namespace

namespace m2vr::daytona {
void Configure(bool enabled,std::uint32_t shift)noexcept{
    g_eye.Configure(enabled,shift);
}
void MirrorDraw(IDirect3DDevice9* d,D3DPRIMITIVETYPE p,UINT n,
                const void* v,UINT stride,std::uint32_t caller,
                OriginalDrawUP original)noexcept{
    try {g_eye.Draw(d,p,n,v,stride,caller,original);}
    catch(...){g_eye.ResetExternal();}
}
void MirrorPrimitive(IDirect3DDevice9* d,D3DPRIMITIVETYPE p,UINT start,
                     UINT count,OriginalDrawPrimitive original)noexcept{
    try{g_eye.Primitive(d,p,start,count,original);}
    catch(...){g_eye.ResetExternal();}
}
void MirrorIndexed(IDirect3DDevice9* d,D3DPRIMITIVETYPE p,INT base,
                   UINT min,UINT num,UINT start,UINT count,
                   OriginalDrawIndexed original)noexcept{
    try{g_eye.Indexed(d,p,base,min,num,start,count,original);}
    catch(...){g_eye.ResetExternal();}
}
void MirrorIndexedUP(IDirect3DDevice9* d,D3DPRIMITIVETYPE p,UINT min,
                     UINT num,UINT count,const void* indices,D3DFORMAT format,
                     const void* vertices,UINT stride,
                     OriginalDrawIndexedUP original)noexcept{
    try{g_eye.IndexedUP(d,p,min,num,count,indices,format,vertices,stride,original);}
    catch(...){g_eye.ResetExternal();}
}
void MirrorClear(IDirect3DDevice9* d,DWORD count,const D3DRECT* rects,
                 DWORD flags,D3DCOLOR color,float z,DWORD stencil,
                 OriginalClear original)noexcept{
    try{g_eye.Clear(d,count,rects,flags,color,z,stencil,original);}
    catch(...){g_eye.ResetExternal();}
}
void MirrorUpdateSurface(IDirect3DDevice9* d,IDirect3DSurface9* src,
                         const RECT* src_rect,IDirect3DSurface9* dst,
                         const POINT* dst_point,
                         OriginalUpdateSurface original)noexcept{
    try{g_eye.UpdateSurface(d,src,src_rect,dst,dst_point,original);}
    catch(...){g_eye.ResetExternal();}
}
void MirrorStretchRect(IDirect3DDevice9* d,IDirect3DSurface9* src,
                       const RECT* src_rect,IDirect3DSurface9* dst,
                       const RECT* dst_rect,D3DTEXTUREFILTERTYPE filter,
                       OriginalStretchRect original)noexcept{
    try{g_eye.StretchRect(d,src,src_rect,dst,dst_rect,filter,original);}
    catch(...){g_eye.ResetExternal();}
}
void MirrorColorFill(IDirect3DDevice9* d,IDirect3DSurface9* dst,
                     const RECT* rect,D3DCOLOR color,
                     OriginalColorFill original)noexcept{
    try{g_eye.ColorFill(d,dst,rect,color,original);}
    catch(...){g_eye.ResetExternal();}
}
void PrepareFrameBeforePresent(IDirect3DDevice9* d,std::uint64_t frame)noexcept{
    try{g_eye.PreparePresent(d,frame);}
    catch(...){g_eye.ResetExternal();}
}
void FinishFrameAfterPresent(std::uint64_t frame,HRESULT hr)noexcept{
    try{g_eye.FinishPresent(frame,hr);}
    catch(...){g_eye.ResetExternal();}
}
void ResetResources()noexcept{g_eye.ResetExternal();}
}
