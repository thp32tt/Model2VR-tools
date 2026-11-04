#include "logger.h"
#include "draw_up_witness.hpp"
#include "daytona_native_stereo.hpp"
#include "daytona_up_decl_witness.hpp"
#include "draw_origin_gate.hpp"
#include "project_draw_correlator.hpp"
#include "d3d9_source_pass_context.hpp"
#include "d3d9_sampler_states.hpp"
#include "model2_camera_projection_fit.hpp"
#include "d3d9_primary_device_owner.hpp"
#include "verified_world_present_gate.hpp"
#include "original_d3d9_draw_result_gate.hpp"
#include "d3d9_world_source.hpp"
#include "d3d9_projected_vb_observer.hpp"
#include "d3d9_indexed_up_window.hpp"
#include "d3d9_primitive_topology.hpp"
#include "projected_up_world_source.hpp"
#include <Windows.h>
#include <d3d9.h>
#include <MinHook.h>
#include <intrin.h>
#include <atomic>
#include <array>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <vector>

// Passive Direct3D9 command-boundary instrumentation. This deliberately DOES
// NOT duplicate draw commands or change render targets. All per-eye replay
// remains disabled until the exact CPU world-pass and GPU state agree.
// API discovery is based on documented COM method order, not static addresses.
namespace {
using Create9Fn=IDirect3D9* (WINAPI*)(UINT);
using CreateDeviceFn=HRESULT (STDMETHODCALLTYPE*)(
    IDirect3D9*,UINT,D3DDEVTYPE,HWND,DWORD,D3DPRESENT_PARAMETERS*,IDirect3DDevice9**);
using PresentFn=HRESULT (STDMETHODCALLTYPE*)(
    IDirect3DDevice9*,CONST RECT*,CONST RECT*,HWND,CONST RGNDATA*);
using ResetFn=HRESULT (STDMETHODCALLTYPE*)(IDirect3DDevice9*,D3DPRESENT_PARAMETERS*);
using UpdateSurfaceFn=HRESULT (STDMETHODCALLTYPE*)(
    IDirect3DDevice9*,IDirect3DSurface9*,const RECT*,IDirect3DSurface9*,const POINT*);
using StretchRectFn=HRESULT (STDMETHODCALLTYPE*)(
    IDirect3DDevice9*,IDirect3DSurface9*,const RECT*,
    IDirect3DSurface9*,const RECT*,D3DTEXTUREFILTERTYPE);
using ColorFillFn=HRESULT (STDMETHODCALLTYPE*)(
    IDirect3DDevice9*,IDirect3DSurface9*,const RECT*,D3DCOLOR);
using ClearFn=HRESULT (STDMETHODCALLTYPE*)(
    IDirect3DDevice9*,DWORD,const D3DRECT*,DWORD,D3DCOLOR,float,DWORD);
using PrimitiveFn=HRESULT (STDMETHODCALLTYPE*)(IDirect3DDevice9*,D3DPRIMITIVETYPE,UINT,UINT);
using IndexedFn=HRESULT (STDMETHODCALLTYPE*)(
    IDirect3DDevice9*,D3DPRIMITIVETYPE,INT,UINT,UINT,UINT,UINT);
using PrimitiveUPFn=HRESULT (STDMETHODCALLTYPE*)(
    IDirect3DDevice9*,D3DPRIMITIVETYPE,UINT,const void*,UINT);
using IndexedUPFn=HRESULT (STDMETHODCALLTYPE*)(
    IDirect3DDevice9*,D3DPRIMITIVETYPE,UINT,UINT,UINT,const void*,D3DFORMAT,const void*,UINT);

Create9Fn g_create9=nullptr;
CreateDeviceFn g_create_device=nullptr;
PresentFn g_present=nullptr;
ResetFn g_reset=nullptr;
ClearFn g_clear=nullptr;
UpdateSurfaceFn g_update_surface=nullptr;
StretchRectFn g_stretch_rect=nullptr;
ColorFillFn g_color_fill=nullptr;
PrimitiveFn g_draw=nullptr;
IndexedFn g_draw_indexed=nullptr;
PrimitiveUPFn g_draw_up=nullptr;
IndexedUPFn g_draw_indexed_up=nullptr;
std::mutex g_setup_mutex;
std::uintptr_t g_exe_base=0;
std::uint32_t g_exe_size=0;
std::atomic<unsigned> g_draws{0},g_indexed{0},g_up{0},g_indexed_up{0};
// Independent draw-family sampling; rare UP paths are not hidden by
// thousands of indexed world draws. All counters reset each Present.
std::array<std::atomic<unsigned>,4> g_family_samples{};
m2vr::drawprobe::DrawOriginGate g_draw_origin_gate{};
std::atomic<unsigned> g_present_count{0};
// Physical original Draw order across all four API families, including
// unsampled commands. Never inferred from per-family sample cadence.
std::atomic<std::uint64_t> g_original_frame_draw_ordinal{0u};
std::atomic<std::uint64_t> g_witness_serial{0};
// Only frames containing a real sampled original VB/IB Draw receive a
// matching physical original Present outcome. No per-Draw GPU readback.
std::atomic<unsigned> g_original_source_result_samples{0u};
std::uint32_t g_draw_sample_every=1024u;
std::atomic<bool> g_device_installed{false};
m2vr::drawprobe::OriginalD3D9DeviceOwner g_primary_device{};
std::atomic<unsigned> g_secondary_presents{0u};
bool g_native_stereo=false;
// Internal callback into the independently proven CPU-world renderer.
// No verified-world frame is generated or self-authorized by this probe.
extern "C" BOOL __stdcall Model2VR_CompleteVerifiedWorldOriginalPresent(
    unsigned long long,unsigned int,unsigned int);
extern "C" BOOL __stdcall Model2VR_HasVerifiedWorldPendingForOriginalPresent(
    unsigned long long);
extern "C" void __stdcall Model2VR_DiscardPendingWorldOnReset();
extern "C" void __stdcall Model2VR_RejectFailedOriginalD3D9Draw(
    std::uint32_t family,std::uint32_t result_bits,std::uint32_t caller_rva);

template<class T>
bool Hook(void* target,void* detour,T& original){
    if(!target)return false;
    const MH_STATUS status=MH_CreateHook(target,detour,
        reinterpret_cast<LPVOID*>(&original));
    if(status!=MH_OK)return false;
    if(MH_EnableHook(target)!=MH_OK){
        MH_RemoveHook(target);
        original=nullptr;
        return false;
    }
    return true;
}
std::uint32_t CallerRva(void* ret) {
    const auto raw=reinterpret_cast<std::uintptr_t>(ret);
    if(raw<g_exe_base || raw-g_exe_base>=g_exe_size)return 0;
    return static_cast<std::uint32_t>(raw-g_exe_base);
}
// Forward the REAL original COM return before any GPU-eye replay. Only
// failed original Draws notify an independently authenticated CPU-world
// transaction, so normal successful Draws incur no extra bridge lock.
HRESULT TrackOriginalD3D9DrawResult(HRESULT hr,std::uint32_t family,
                                    void* caller) noexcept {
    if(m2vr::vrbridge::IsFailedOriginalD3D9Draw(
        family,static_cast<std::uint32_t>(hr)))
        Model2VR_RejectFailedOriginalD3D9Draw(
            family,static_cast<std::uint32_t>(hr),CallerRva(caller));
    return hr;
}
bool Sample(IDirect3DDevice9* dev,unsigned family,
            D3DPRIMITIVETYPE topology,UINT primitive_count,void* caller){
    if(family<1u || family>4u)return false;
    // Preserve the first/1024th draw and up to 64 novel in-EXE sources,
    // while explicitly sampling mid-frame Draws with the bounded opt-in
    // cadence. The first-only Daytona log exposed a 2D blit but missed
    // up to hundreds of later DrawPrimitiveUP calls within each frame.
    // This exposes rare actual world draw callsites without GPU readbacks.
    const unsigned ordinal=g_family_samples[family-1].fetch_add(
        1,std::memory_order_relaxed);
    const auto caller_rva=CallerRva(caller);
    if(!g_draw_origin_gate.ShouldObserve(family,caller_rva,ordinal) &&
       (ordinal%g_draw_sample_every)!=0u)return false;
    const std::uint64_t topology_and_count=
        (std::uint64_t{primitive_count}<<32)|
        static_cast<std::uint32_t>(topology);
    // a0=frame about to Present, a1=draw API family (1..4),
    // a2=caller RVA (0 means unverified/outside EXE), a3=(count<<32)|type.
    // It is a caller observation, not world/HUD/mirror classification.
    const auto upcoming_frame=g_present_count.load(
        std::memory_order_relaxed)+1u;
    m2vr::LogEvent("model2_d3d9_draw_origin",
        upcoming_frame,family,caller_rva,topology_and_count);
    // Relative Draw position lets offline analysis distinguish early
    // full-screen blits from late UI/HUD overlays at the same caller RVA.
    m2vr::LogEvent("model2_d3d9_draw_ordinal",
        upcoming_frame,family,ordinal,caller_rva);
    DWORD fvf=0,z=0,alpha=0;
    const bool ok=SUCCEEDED(dev->GetFVF(&fvf)) &&
                  SUCCEEDED(dev->GetRenderState(D3DRS_ZENABLE,&z)) &&
                  SUCCEEDED(dev->GetRenderState(D3DRS_ALPHABLENDENABLE,&alpha));
    if(!ok){m2vr::LogEvent("model2_d3d9_sample_failed",family);return true;}
    // A sample shows whether pretransformed (XYZRHW) data dominates.
    // It is NEVER sufficient by itself to classify world geometry.
    // Port0r's separate secondary projection regions motivate recording
    // viewport, scissor and target provenance; none alone proves a world pass.
    DWORD scissor_enabled=0;
    const bool scissor_state_ok=SUCCEEDED(
        dev->GetRenderState(D3DRS_SCISSORTESTENABLE,&scissor_enabled));
    D3DVIEWPORT9 viewport{};
    const bool viewport_ok=SUCCEEDED(dev->GetViewport(&viewport));
    RECT scissor{};
    const bool scissor_ok=scissor_state_ok && scissor_enabled &&
        SUCCEEDED(dev->GetScissorRect(&scissor));

    IDirect3DSurface9* target=nullptr;
    IDirect3DSurface9* backbuffer=nullptr;
    const bool target_ok=SUCCEEDED(dev->GetRenderTarget(0,&target)) && target;
    const bool backbuffer_ok=SUCCEEDED(dev->GetBackBuffer(
        0,0,D3DBACKBUFFER_TYPE_MONO,&backbuffer)) && backbuffer;
    D3DSURFACE_DESC desc{};
    const bool desc_ok=target_ok && SUCCEEDED(target->GetDesc(&desc));
    // Unknown if either COM query fails; never claim auxiliary without a
    // known main backbuffer comparison. Release all acquired COM references.
    const std::uint32_t target_class=target_ok && backbuffer_ok ?
        (target==backbuffer ? 1u : 2u) : 0u; // unknown/main/aux
    if(target)target->Release();
    if(backbuffer)backbuffer->Release();

    const std::uint32_t packed=(z?1u:0u)|(alpha?2u:0u)|
        ((fvf&D3DFVF_POSITION_MASK)==D3DFVF_XYZRHW?4u:0u)|
        (scissor_state_ok && scissor_enabled?8u:0u)|
        (viewport_ok?16u:0u)|(scissor_ok?32u:0u)|
        (target_class!=0u?64u:0u)|(target_class==1u?128u:0u);
    m2vr::LogEvent("model2_d3d9_draw_sample",family,fvf,packed,caller_rva);
    if(viewport_ok){
        const std::uint64_t size=(std::uint64_t{viewport.Width}<<32)|
            std::uint64_t{viewport.Height};
        m2vr::LogEvent("model2_d3d9_viewport_sample",caller_rva,
            viewport.X,viewport.Y,size);
    }
    if(scissor_ok){
        const std::uint64_t edges=
            (std::uint64_t{static_cast<std::uint32_t>(scissor.right)}<<32)|
            static_cast<std::uint32_t>(scissor.bottom);
        m2vr::LogEvent("model2_d3d9_scissor_sample",caller_rva,
            static_cast<std::uint32_t>(scissor.left),
            static_cast<std::uint32_t>(scissor.top),edges);
    }
    m2vr::LogEvent("model2_d3d9_target_sample",caller_rva,target_class,
        desc_ok?desc.Width:0u,desc_ok?desc.Height:0u);
    return true;
}

// Each independently sampled original Draw witness retains the original
// source RT/viewport/depth context and the *upcoming* original Present id.
// This records evidence only: unknown/aux/HUD never becomes world proof.
void LogOriginalSourcePassContext(IDirect3DDevice9* dev,
                                  std::uint64_t witness) {
    if(!dev || !witness)return;
    using namespace m2vr::drawprobe;
    OriginalDrawSourceContext c{};
    IDirect3DSurface9* source=nullptr;
    IDirect3DSurface9* backbuffer=nullptr;
    const bool have_source=SUCCEEDED(dev->GetRenderTarget(0,&source))&&source;
    const bool have_backbuffer=SUCCEEDED(dev->GetBackBuffer(
        0,0,D3DBACKBUFFER_TYPE_MONO,&backbuffer))&&backbuffer;
    D3DSURFACE_DESC source_desc{};
    D3DSURFACE_DESC backbuffer_desc{};
    c.target_desc_known=have_source &&
        SUCCEEDED(source->GetDesc(&source_desc));
    const bool backbuffer_desc_known=have_backbuffer &&
        SUCCEEDED(backbuffer->GetDesc(&backbuffer_desc));
    if(c.target_desc_known){
        c.source_width=source_desc.Width;
        c.source_height=source_desc.Height;
    }
    if(have_source && have_backbuffer)
        c.target=source==backbuffer?
            OriginalTargetClass::MainBackbuffer:
            OriginalTargetClass::Auxiliary;
    if(source)source->Release();
    if(backbuffer)backbuffer->Release();
    D3DVIEWPORT9 vp{};
    c.viewport_known=SUCCEEDED(dev->GetViewport(&vp));
    c.viewport_full=c.viewport_known && backbuffer_desc_known &&
        vp.X==0u && vp.Y==0u &&
        vp.Width==backbuffer_desc.Width &&
        vp.Height==backbuffer_desc.Height &&
        vp.MinZ==0.f && vp.MaxZ==1.f;
    DWORD z=0u,write=0u,scissor=0u;
    c.depth_known=SUCCEEDED(dev->GetRenderState(D3DRS_ZENABLE,&z)) &&
        SUCCEEDED(dev->GetRenderState(D3DRS_ZWRITEENABLE,&write));
    c.depth_enabled=c.depth_known && z==D3DZB_TRUE;
    c.depth_write=c.depth_known && write!=0u;
    c.scissor_known=SUCCEEDED(
        dev->GetRenderState(D3DRS_SCISSORTESTENABLE,&scissor));
    // Treat any nonzero original value as enabled for provenance.
    // Unknown values must fail closed, never masquerade as disabled.
    c.scissor_enabled=c.scissor_known && scissor!=0u;
    if(c.scissor_known && scissor==1u){
        RECT source_scissor{};
        c.scissor_rect_known=SUCCEEDED(dev->GetScissorRect(&source_scissor));
        c.scissor_full=c.scissor_rect_known && c.target_desc_known &&
            c.target==OriginalTargetClass::MainBackbuffer &&
            FullBackbufferScissor(scissor,&source_scissor,source_desc);
    }
    IDirect3DSurface9* depth=nullptr;
    if(SUCCEEDED(dev->GetDepthStencilSurface(&depth)) && depth){
        D3DSURFACE_DESC depth_desc{};
        c.depth_surface_known=SUCCEEDED(depth->GetDesc(&depth_desc));
        c.depth_surface_compatible=c.depth_surface_known &&
            c.target_desc_known &&
            CompatibleOriginalDepthSurface(source_desc,depth_desc);
    }
    if(depth)depth->Release();
    const auto original_frame=g_present_count.load(
        std::memory_order_relaxed)+1u;
    m2vr::LogEvent("model2_d3d9_source_pass_context",witness,
        original_frame,EncodeOriginalDrawSourceContext(c),
        (std::uint64_t{c.source_width}<<32)|c.source_height);
    // Same original Draw/Present ZFUNC and CULLMODE provenance, never
    // automatic world-pass authorization or a duplicate game Draw.
    DWORD zfunc=0u,cull=0u;
    if(c.depth_known &&
       SUCCEEDED(dev->GetRenderState(D3DRS_ZFUNC,&zfunc)) &&
       SUCCEEDED(dev->GetRenderState(D3DRS_CULLMODE,&cull)))
        m2vr::LogEvent("model2_d3d9_source_depth_cull",witness,
            (std::uint64_t{z}<<32)|zfunc,
            (std::uint64_t{write}<<32)|cull,original_frame);
}

// Read-only fixed-function material state belongs to its exact original
// Draw witness, not a later Present or an inferred gameplay-world pass.
// No original D3D9 state or game/FFB tick is modified.
void LogOriginalFixedStageEvidence(IDirect3DDevice9* dev,
                                   std::uint64_t witness,DWORD fvf,UINT stride) {
    if(!dev || !witness)return;
    DWORD co=0,c1=0,c2=0,ao=0,a1=0,a2=0;
    DWORD next_color=0,next_alpha=0,specular=0,fog=0;
    DWORD current_result=0,next_result=0;
    if(FAILED(dev->GetTextureStageState(0,D3DTSS_COLOROP,&co)) ||
       FAILED(dev->GetTextureStageState(0,D3DTSS_COLORARG1,&c1)) ||
       FAILED(dev->GetTextureStageState(0,D3DTSS_COLORARG2,&c2)) ||
       FAILED(dev->GetTextureStageState(0,D3DTSS_ALPHAOP,&ao)) ||
       FAILED(dev->GetTextureStageState(0,D3DTSS_ALPHAARG1,&a1)) ||
       FAILED(dev->GetTextureStageState(0,D3DTSS_ALPHAARG2,&a2)) ||
       FAILED(dev->GetTextureStageState(1,D3DTSS_COLOROP,&next_color)) ||
       FAILED(dev->GetTextureStageState(1,D3DTSS_ALPHAOP,&next_alpha)) ||
       FAILED(dev->GetTextureStageState(0,D3DTSS_RESULTARG,&current_result)) ||
       FAILED(dev->GetTextureStageState(1,D3DTSS_RESULTARG,&next_result)) ||
       FAILED(dev->GetRenderState(D3DRS_SPECULARENABLE,&specular)) ||
       FAILED(dev->GetRenderState(D3DRS_FOGENABLE,&fog)))
        return; // Unavailable states are unknown, never guessed.
    m2vr::LogEvent("model2_d3d9_source_stage0_color",witness,
        (std::uint64_t{co}<<32)|c1,c2,
        (std::uint64_t{fvf}<<32)|stride);
    m2vr::LogEvent("model2_d3d9_source_stage0_alpha",witness,
        (std::uint64_t{ao}<<32)|a1,a2,
        (std::uint64_t{fvf}<<32)|stride);
    m2vr::LogEvent("model2_d3d9_source_stage1_specular",witness,
        next_color,next_alpha,(std::uint64_t{specular}<<32)|fog);
    // D3D9 stage0 RESULTARG=TEMP can make plausible diffuse/texel
    // operations non-equivalent to the final pixel. Preserve actual
    // source output register and declaration without enabling world proof.
    m2vr::LogEvent("model2_d3d9_source_stage_result",witness,
        current_result,next_result,(std::uint64_t{fvf}<<32)|stride);
    // Source lighting has no effect on original pretransformed XYZRHW,
    // but may change XYZ color; record it under the exact same Draw witness
    // without treating either original state as gameplay-world proof.
    DWORD lighting=0,color_vertex=0;
    if(SUCCEEDED(dev->GetRenderState(D3DRS_LIGHTING,&lighting)) &&
       SUCCEEDED(dev->GetRenderState(D3DRS_COLORVERTEX,&color_vertex)))
        m2vr::LogEvent("model2_d3d9_source_lighting",witness,
            lighting,color_vertex,(std::uint64_t{fvf}<<32)|stride);
    // Capture original alpha compare on the SAME genuine Draw witness.
    // Preserve exact FVF/stride so a later or unrelated material sample
    // cannot be joined to this source or accidentally authorize a world.
    DWORD alpha_enabled=0u,alpha_function=0u,alpha_reference=0u;
    if(SUCCEEDED(dev->GetRenderState(D3DRS_ALPHATESTENABLE,&alpha_enabled)) &&
       SUCCEEDED(dev->GetRenderState(D3DRS_ALPHAFUNC,&alpha_function)) &&
       SUCCEEDED(dev->GetRenderState(D3DRS_ALPHAREF,&alpha_reference)))
        m2vr::LogEvent("model2_d3d9_source_alpha_compare",witness,
            (std::uint64_t{alpha_enabled}<<32)|alpha_function,
            alpha_reference,(std::uint64_t{fvf}<<32)|stride);
    // Read original D3D9 blend factors under the SAME Draw witness.
    // Passive capture only; no input, FFB or original render modification.
    DWORD blend_enable=0,blend_src=0,blend_dst=0,blend_op=0;
    DWORD separate_alpha=0;
    if(SUCCEEDED(dev->GetRenderState(D3DRS_ALPHABLENDENABLE,&blend_enable)) &&
       SUCCEEDED(dev->GetRenderState(D3DRS_SRCBLEND,&blend_src)) &&
       SUCCEEDED(dev->GetRenderState(D3DRS_DESTBLEND,&blend_dst)) &&
       SUCCEEDED(dev->GetRenderState(D3DRS_BLENDOP,&blend_op)) &&
       SUCCEEDED(dev->GetRenderState(
           D3DRS_SEPARATEALPHABLENDENABLE,&separate_alpha))) {
        m2vr::LogEvent("model2_d3d9_source_blend",witness,
            (std::uint64_t{blend_enable}<<32)|blend_src,
            (std::uint64_t{blend_dst}<<32)|blend_op,
            separate_alpha);
        // Independent ADD alpha factors MUST belong to this SAME original
        // Draw witness and FVF/stride, not a later Present or another pass.
        if(blend_enable==1u && separate_alpha==1u) {
            DWORD source_write_mask=0u;
            // The original alpha blend registers are semantically inert
            // if the genuine source COLORWRITEENABLE masks ALPHA. Avoid
            // unnecessary COM reads; the SAME-witness original color-mask
            // event still certifies this exact RGB-only path separately.
            if(SUCCEEDED(dev->GetRenderState(
                    D3DRS_COLORWRITEENABLE,&source_write_mask)) &&
               (source_write_mask&D3DCOLORWRITEENABLE_ALPHA)!=0u) {
                DWORD asrc=0u,adst=0u,aop=0u;
                if(SUCCEEDED(dev->GetRenderState(
                               D3DRS_SRCBLENDALPHA,&asrc)) &&
                   SUCCEEDED(dev->GetRenderState(
                               D3DRS_DESTBLENDALPHA,&adst)) &&
                   SUCCEEDED(dev->GetRenderState(
                               D3DRS_BLENDOPALPHA,&aop)))
                    m2vr::LogEvent("model2_d3d9_source_separate_alpha_blend",
                        witness,(std::uint64_t{asrc}<<32)|adst,aop,
                        (std::uint64_t{fvf}<<32)|stride);
            }
        }
    }
    // Original fixed-function vertex stage for the SAME sampled Draw.
    // Neither source FVF nor a CPU projector match establishes the
    // unobserved shade/clip/vertex-blend state or race-world ownership.
    DWORD shade=0,clipping=0,clip_planes=0,vertex_blend=0,indexed_blend=0;
    if(SUCCEEDED(dev->GetRenderState(D3DRS_SHADEMODE,&shade)) &&
       SUCCEEDED(dev->GetRenderState(D3DRS_CLIPPING,&clipping)) &&
       SUCCEEDED(dev->GetRenderState(D3DRS_CLIPPLANEENABLE,&clip_planes)) &&
       SUCCEEDED(dev->GetRenderState(D3DRS_VERTEXBLEND,&vertex_blend)) &&
       SUCCEEDED(dev->GetRenderState(
           D3DRS_INDEXEDVERTEXBLENDENABLE,&indexed_blend)))
        m2vr::LogEvent("model2_d3d9_source_vertex_pipeline",witness,
            (std::uint64_t{shade}<<32)|clipping,
            (std::uint64_t{clip_planes}<<32)|vertex_blend,
            (std::uint64_t{indexed_blend}<<32)|
             (g_present_count.load(std::memory_order_relaxed)+1u));
    // D3D9 TRIANGLELIST wireframe/point rendering changes pixel coverage.
    // The binocular world raster is filled-only: preserve the genuine
    // FILLMODE together with this very Draw's FVF and Present provenance.
    DWORD fill_mode=0;
    if(SUCCEEDED(dev->GetRenderState(D3DRS_FILLMODE,&fill_mode)))
        m2vr::LogEvent("model2_d3d9_source_fill",witness,fill_mode,
            g_present_count.load(std::memory_order_relaxed)+1u,
            (std::uint64_t{fvf}<<32)|stride);
    // Source mip capability is read from the ACTUAL bound 2D texture at
    // this Draw witness. It cannot authorize an otherwise unverified Draw.
    DWORD source_mip_filter=0u;
    if(SUCCEEDED(dev->GetSamplerState(
           0,D3DSAMP_MIPFILTER,&source_mip_filter))){
        IDirect3DBaseTexture9* bound=nullptr;
        unsigned source_levels=0u;
        if(SUCCEEDED(dev->GetTexture(0,&bound)) && bound){
            if(bound->GetType()==D3DRTYPE_TEXTURE)
                source_levels=bound->GetLevelCount();
        }
        if(bound)bound->Release();
        m2vr::LogEvent("model2_d3d9_source_sampler_mip",witness,
            source_mip_filter,source_levels,
            g_present_count.load(std::memory_order_relaxed)+1u);
    }
    DWORD color_mask=0;
    if(SUCCEEDED(dev->GetRenderState(D3DRS_COLORWRITEENABLE,&color_mask)))
        m2vr::LogEvent("model2_d3d9_source_color_mask",witness,
            color_mask,(std::uint64_t{fvf}<<32)|stride,
            g_present_count.load(std::memory_order_relaxed)+1u);
}

// Read at most 16 guarded bytes at a time from game-owned UP pointers.
// The small SEH helper contains no C++ objects requiring unwinding.
bool SafeReadBytes(const void* source,std::size_t offset,
                   void* destination,std::size_t size) noexcept {
    if(!source||!destination||!size||size>16u||
       offset>65536u-size)return false;
    const auto* p=static_cast<const volatile std::uint8_t*>(source);
    auto* out=static_cast<std::uint8_t*>(destination);
    __try {
        for(std::size_t j=0;j<size;++j)out[j]=p[offset+j];
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
    return true;
}

// Candidate only: the actual fixed-function preprojection UP vertex,
 // stage-zero UV and original World/View matrices. Even D3DFVF_XYZ can be
 // HUD/UI or non-gameplay; this function NEVER asserts world proof or
 // publishes a GameStereo frame. Logs three bounded vertices at most.
std::uint64_t ObserveWorldUP(IDirect3DDevice9* dev,unsigned family,
                    D3DPRIMITIVETYPE type,UINT primitive_count,void* caller,
                    const void* vertices,UINT stride,const void* indices,
                    D3DFORMAT index_format,UINT first,UINT vertex_count) {
    using namespace m2vr::drawprobe;
    if(!dev || !vertices ||
       (type!=D3DPT_TRIANGLELIST &&
        type!=D3DPT_TRIANGLESTRIP &&
        type!=D3DPT_TRIANGLEFAN))return 0u;
    DWORD fvf=0;
    if(FAILED(dev->GetFVF(&fvf)))return 0u;
    const auto layout=DecodeWorldUpFVF(fvf,stride);
    if(!layout.eligible)return 0u;
    if(family==4u && (!indices||!vertex_count||
       vertex_count>m2vr::drawprobe::kMaxIndexedUPDeclaredVertices ||
       (index_format!=D3DFMT_INDEX16 &&
        index_format!=D3DFMT_INDEX32)))return 0u;
    D3DMATRIX world{},view{};
    if(FAILED(dev->GetTransform(D3DTS_WORLD,&world)) ||
       FAILED(dev->GetTransform(D3DTS_VIEW,&view)) ||
       !AffineMatrix(world)||!AffineMatrix(view))return 0u;
    const auto elements=PrimitiveElements(
        static_cast<std::uint32_t>(type),primitive_count);
    if(!elements)return 0u;
    constexpr unsigned max_sample=3u;
    std::array<WorldVertex,max_sample> samples{};
    std::array<std::uint32_t,max_sample> ids{};
    const unsigned count=elements<max_sample?elements:max_sample;
    for(unsigned i=0;i<count;++i) {
        std::uint32_t index=i;
        std::uint32_t absolute_index=index;
        if(family==4u) {
            const unsigned bytes=index_format==D3DFMT_INDEX16?2u:4u;
            std::array<std::uint8_t,4> raw{};
            if(!SafeReadBytes(indices,std::size_t(i)*bytes,
                              raw.data(),bytes))return 0u;
            if(bytes==2u){std::uint16_t x=0;std::memcpy(&x,raw.data(),2);index=x;}
            else std::memcpy(&index,raw.data(),4);
            std::uint32_t relative=0u;
            if(!IndexedUPRelativeIndex(index,first,vertex_count,relative))
                return 0u;
            absolute_index=index;
            index=relative; // UP vertex pointer begins at first
        }
        // Read only fixed layout fields. Never dereference beyond 65536
        // source bytes, and never ask the caller to provide hidden memory.
        std::array<std::uint8_t,256> source{};
        const std::size_t vertex_base=std::size_t(index)*stride;
        if(!SafeReadBytes(vertices,vertex_base,source.data(),12u) ||
           (layout.has_diffuse &&
            !SafeReadBytes(vertices,vertex_base+layout.diffuse_offset,
                           source.data()+layout.diffuse_offset,4u)) ||
           (layout.has_uv &&
            !SafeReadBytes(vertices,vertex_base+layout.uv_offset,
                           source.data()+layout.uv_offset,8u)) ||
           !DecodeWorldVertex(source.data(),stride,layout,
                              world,view,samples[i]))return 0u;
        ids[i]=absolute_index;
    }
    const auto id=g_witness_serial.fetch_add(
        1u,std::memory_order_relaxed)+1u;
    LogOriginalSourcePassContext(dev,id);
    LogOriginalFixedStageEvidence(dev,id,fvf,stride);
    // Source FVF, stride and upcoming original Present are pinned to
    // this SAME original world-UP sample, never inferred from other Draws.
    m2vr::LogEvent("model2_d3d9_up_source_fvf",id,fvf,stride,
        g_present_count.load(std::memory_order_relaxed)+1u);
    m2vr::LogEvent("model2_world_up_candidate",id,
         CallerRva(caller),fvf,
         (std::uint64_t{family}<<32)|stride);
    if(family==4u)
        m2vr::LogEvent("model2_d3d9_indexed_up_window",id,
            first,vertex_count,index_format);
    if(family==4u)
        m2vr::LogEvent("model2_d3d9_indexed_up_topology",id,
            static_cast<unsigned int>(type),primitive_count,
            g_present_count.load(std::memory_order_relaxed)+1u);
    for(unsigned i=0;i<count;++i){
        const auto& v=samples[i];
        m2vr::LogEvent("model2_world_up_view_xyz",id,ids[i],
            PackPair(v.view_xyz[0],v.view_xyz[1]),
            PackPair(v.view_xyz[2],v.uv[0]));
        m2vr::LogEvent("model2_world_up_source_uv",id,ids[i],
            PackPair(v.uv[0],v.uv[1]),0u);
    }
    IDirect3DBaseTexture9* tex=nullptr;
    if(SUCCEEDED(dev->GetTexture(0,&tex)) && tex) {
        if(tex->GetType()==D3DRTYPE_TEXTURE) {
            auto* t=static_cast<IDirect3DTexture9*>(tex);
            D3DSURFACE_DESC desc{};
            if(SUCCEEDED(t->GetLevelDesc(0,&desc)))
                m2vr::LogEvent("model2_world_up_texture_meta",id,
                    desc.Format,desc.Width,desc.Height);
        }
        tex->Release();
    }
    return id;
}
// Candidate camera fit is implemented after ObserveUP below.
void ObserveProjectionFit(IDirect3DDevice9* dev,std::uint64_t witness,
        const m2vr::drawprobe::ProjectedTriangleBatchJoin& batch);
// Defined below: bounded original FVF=0 declaration observation.
std::uint64_t ObserveUnknownUpLayout(IDirect3DDevice9* dev,unsigned family,
                            void* caller,UINT stride,
                            D3DPRIMITIVETYPE topology,
                            UINT primitive_count,const void* vertices);
std::uint64_t ObserveUP(IDirect3DDevice9* dev,unsigned family,
               D3DPRIMITIVETYPE type,UINT primitive_count,void* caller,
               const void* vertices,UINT stride,const void* indices=nullptr,
               D3DFORMAT index_format=D3DFMT_UNKNOWN,UINT first=0,
               UINT vertex_count=0) {
    using namespace m2vr::drawprobe;
    const auto elements=PrimitiveElements(
        static_cast<std::uint32_t>(type),primitive_count);
    if(!dev||!vertices||!ValidStride(stride)||!elements)return 0u;
    DWORD fvf=0;
    if(FAILED(dev->GetFVF(&fvf)))return 0u;
    if(fvf==0u) {
        return ObserveUnknownUpLayout(dev,family,caller,stride,type,
                                      primitive_count,vertices);
    }
    if((fvf&D3DFVF_POSITION_MASK)==D3DFVF_XYZ){
        return ObserveWorldUP(dev,family,type,primitive_count,caller,
                              vertices,stride,indices,index_format,first,vertex_count);
    }
    if((fvf&D3DFVF_POSITION_MASK)!=D3DFVF_XYZRHW)return 0u;

    // Nonzero MinVertexIndex is legal: resolve original absolute index
    // against the declared UP span before reading its local memory.
    if(family==4u && (!indices||!vertex_count||
       vertex_count>m2vr::drawprobe::kMaxIndexedUPDeclaredVertices ||
       (index_format!=D3DFMT_INDEX16 &&
        index_format!=D3DFMT_INDEX32)))return 0u;

    struct Observation {std::uint32_t index=0; VertexWitness vertex{};};
    // Capture the first eight original TRIANGLELIST, TRIANGLESTRIP or
    // TRIANGLEFAN triangles with source winding preserved, without another
    // original Draw call. Unknown topology stays at three source vertices.
    constexpr unsigned kJoinTriangles=
        static_cast<unsigned>(m2vr::drawprobe::ProjectDrawCorrelator::kMaxDiagnosticJoinTriangles);
    constexpr unsigned kJoinSampleVertices=3u*kJoinTriangles;
    std::array<Observation,kJoinSampleVertices> observations{};
    const bool triangle_topology=
        type==D3DPT_TRIANGLELIST ||
        type==D3DPT_TRIANGLESTRIP ||
        type==D3DPT_TRIANGLEFAN;
    const auto sampled_triangles=
        primitive_count<kJoinTriangles?primitive_count:kJoinTriangles;
    const auto triangle_elements=triangle_topology?
        PrimitiveSourceElementCount(type,sampled_triangles):0u;
    const unsigned limit=triangle_elements?triangle_elements:MaxVertices;
    const unsigned count=elements<limit?elements:limit;
    for(unsigned i=0;i<count;++i){
        std::uint32_t index=i;
        std::uint32_t absolute_index=index;
        if(family==4u){
            std::array<std::uint8_t,4> bytes{};
            const unsigned width=index_format==D3DFMT_INDEX16?2u:4u;
            if(!SafeReadBytes(indices,std::size_t{i}*width,
                              bytes.data(),width))return 0u;
            if(width==2u){
                std::uint16_t short_index=0;
                std::memcpy(&short_index,bytes.data(),2u);
                index=short_index;
            }else{
                std::memcpy(&index,bytes.data(),4u);
            }
            std::uint32_t relative=0u;
            if(!IndexedUPRelativeIndex(index,first,vertex_count,relative))
                return 0u;
            absolute_index=index;
            index=relative; // UP vertex pointer begins at first
        }
        std::array<std::uint32_t,4> bits{};
        if(!SafeReadBytes(vertices,std::size_t{index}*stride,
                          bits.data(),sizeof(bits)) ||
           !DecodeXYZRHW(bits,observations[i].vertex))return 0u;
        observations[i].index=absolute_index;
    }

    const auto sample_id=g_witness_serial.fetch_add(
        1u,std::memory_order_relaxed)+1u;
    LogOriginalSourcePassContext(dev,sample_id);
    LogOriginalFixedStageEvidence(dev,sample_id,fvf,stride);
    m2vr::LogEvent("model2_d3d9_up_source_fvf",sample_id,fvf,stride,
        g_present_count.load(std::memory_order_relaxed)+1u);
    m2vr::LogEvent("model2_d3d9_up_meta",sample_id,
        g_present_count.load(std::memory_order_relaxed)+1u,
        CallerRva(caller),(std::uint64_t{family}<<32)|stride);
    if(family==4u)
        m2vr::LogEvent("model2_d3d9_indexed_up_window",sample_id,
            first,vertex_count,index_format);
    if(family==4u)
        m2vr::LogEvent("model2_d3d9_indexed_up_topology",sample_id,
            static_cast<unsigned int>(type),primitive_count,
            g_present_count.load(std::memory_order_relaxed)+1u);
    // Preserve the original three-vertex per-witness log volume/schema.
    // Extra source triangles are correlated only via aggregate metadata.
    for(unsigned i=0;i<count && i<MaxVertices;++i){
        const auto& v=observations[i].vertex;
        m2vr::LogEvent("model2_d3d9_up_vertex",sample_id,
            observations[i].index,PackPair(v.x,v.y),PackPair(v.z,v.rhw));
    }
    // The sampled original UP triangle can now be associated with
    // immediately preceding CPU projector outputs on this SAME thread and
    // Present epoch. Even a full match is a candidate, never world proof.
    if(triangle_elements && count==triangle_elements){
        std::vector<std::uint32_t> source_ids(count);
        for(unsigned i=0;i<count;++i)source_ids[i]=i;
        std::vector<std::uint32_t> triangle_ids;
        if(!ExpandOriginalTriangles(type,sampled_triangles,
                                    source_ids,triangle_ids))
            return sample_id;
        std::array<ProjectedDrawVertex,kJoinSampleVertices> screen{};
        for(std::size_t i=0;i<triangle_ids.size();++i){
            const auto& original=observations[triangle_ids[i]].vertex;
            screen[i]={original.x,original.y};
        }
        const auto epoch=CurrentProjectDrawEpoch();
        const auto& correlator=ThreadProjectDrawCorrelator();
        const auto join=correlator.Match(epoch,screen.data(),3u);
        m2vr::LogEvent("model2_d3d9_cpu_up_join",sample_id,
            join.matched,join.ambiguous,join.source_caller_rva);
        const auto batch=correlator.MatchTriangleList(
            epoch,screen.data(),triangle_ids.size());
        m2vr::LogEvent("model2_d3d9_cpu_up_join_batch",sample_id,
            (std::uint64_t{batch.inspected}<<32)|batch.complete,
            (std::uint64_t{batch.partial}<<32)|batch.ambiguous,
            batch.common_source_caller_rva);
        m2vr::LogEvent("model2_d3d9_cpu_up_matrix_identity",sample_id,
            batch.common_matrix_identity,
            batch.mixed_matrix_identity?1u:0u,
            batch.common_source_caller_rva);
        m2vr::LogEvent("model2_d3d9_cpu_up_matrix_content",sample_id,
            batch.common_matrix_fingerprint,
            batch.mixed_matrix_fingerprint?1u:0u,
            batch.common_matrix_identity);
        m2vr::LogEvent("model2_d3d9_cpu_up_geometry_command",sample_id,
            batch.common_geometry_command,
            batch.mixed_geometry_command?1u:0u,
            batch.common_matrix_fingerprint);
        // Evidence-only. A fitted original world camera remains unknown
        // until separately authenticated by private, real gameplay proof.
        ObserveProjectionFit(dev,sample_id,batch);
    }
    return sample_id;
}

// No original D3D9 draw mutation. A candidate affine perspective
// relationship between SAME source caller's source-XYZ and original screen
// coordinates provides camera-topology evidence but never world proof.
void ObserveProjectionFit(IDirect3DDevice9* dev,std::uint64_t witness,
        const m2vr::drawprobe::ProjectedTriangleBatchJoin& batch){
    if(!dev || !batch.CompleteSameSource())return;
    D3DVIEWPORT9 vp{};
    if(FAILED(dev->GetViewport(&vp)))return;
    const auto fit=m2vr::drawprobe::FitSourcePerspective(
        batch.camera_samples.data(),batch.camera_sample_count,
        vp.X,vp.Y,vp.Width,vp.Height);
    if(!fit.plausible)return;
    m2vr::LogEvent("model2_d3d9_cpu_projection_fit",witness,
        batch.common_source_caller_rva,
        m2vr::drawprobe::PackPair(fit.center_x,fit.center_y),
        m2vr::drawprobe::PackPair(fit.scale_x,fit.scale_y));
    m2vr::LogEvent("model2_d3d9_cpu_projection_fit_quality",witness,
        fit.accepted_samples,
        m2vr::drawprobe::PackPair(fit.rms_pixels,
            fit.depth_positive?1.f:-1.f),0u);
}

// Observe the original stream-zero D3D9 vertex declaration when GetFVF
// reports 0 (shader/declaration path). The actual Daytona world candidate
// uses this path for >8k mid-frame samples. Never treat FVF=0 as world
// proof, never read unknown UP source bytes, and never mutate D3D9 state.
// A declaration query is bounded to 16 elements including D3DDECL_END.
std::uint64_t ObserveUnknownUpLayout(IDirect3DDevice9* dev,unsigned family,
                            void* caller,UINT stride,
                            D3DPRIMITIVETYPE topology,
                            UINT primitive_count,const void* vertices) {
    if(!dev || family<3u || family>4u)return 0u;
    const auto id=g_witness_serial.fetch_add(
        1u,std::memory_order_relaxed)+1u;
    // 0=unknown/error, 1=shader absent, 2=shader present.
    std::uint32_t vs_status=0u,ps_status=0u;
    IDirect3DVertexShader9* vs=nullptr;
    if(SUCCEEDED(dev->GetVertexShader(&vs)))
        vs_status=vs?2u:1u;
    if(vs)vs->Release();
    IDirect3DPixelShader9* ps=nullptr;
    if(SUCCEEDED(dev->GetPixelShader(&ps)))
        ps_status=ps?2u:1u;
    if(ps)ps->Release();
    IDirect3DVertexDeclaration9* declaration=nullptr;
    const HRESULT hr=dev->GetVertexDeclaration(&declaration);
    std::array<D3DVERTEXELEMENT9,16> elements{};
    UINT count=0u;
    if(SUCCEEDED(hr)&&declaration){
        count=static_cast<UINT>(elements.size());
        if(FAILED(declaration->GetDeclaration(elements.data(),&count)) ||
           count==0u || count>elements.size() ||
           elements[count-1u].Stream!=0xffu)count=0u;
    }
    if(declaration)declaration->Release();
    LogOriginalSourcePassContext(dev,id);
    m2vr::LogEvent("model2_d3d9_up_layout",id,CallerRva(caller),
        (std::uint64_t{family}<<32)|stride,
        (std::uint64_t{vs_status}<<40)|
        (std::uint64_t{ps_status}<<32)|count);
    // Only the original declarative byte offsets and D3D9 semantics
    // are logged. No game vertex data or shader bytecode is copied.
    for(UINT k=0u;k<count;++k) {
        const auto& e=elements[k];
        m2vr::LogEvent("model2_d3d9_up_decl_element",id,k,
            (std::uint64_t{e.Stream}<<32)|e.Offset,
            (std::uint64_t{e.Type}<<24)|
            (std::uint64_t{e.Method}<<16)|
            (std::uint64_t{e.Usage}<<8)|e.UsageIndex);
    }
    // After v3 confirmed exactly this 32-byte declaration on *real*
    // Daytona DrawPrimitiveUP, safely witness only its first triangle.
    // FVF=0 alone is insufficient: reject any other declaration,
    // stride, topology, missing shaders or indexed-source mapping.
    // No GPU state is changed, and source vertices are never replayed.
    if(family!=3u || !vertices || topology!=D3DPT_TRIANGLELIST ||
       !primitive_count || vs_status!=2u || ps_status!=2u ||
       !m2vr::drawprobe::ExactDaytonaUPDeclaration(
           elements.data(),count,stride))return id;
    std::array<m2vr::drawprobe::DaytonaUPVertex,3> samples{};
    for(unsigned i=0;i<3u;++i) {
        std::array<std::uint8_t,32> bytes{};
        const std::size_t source_offset=std::size_t{i}*stride;
        if(!SafeReadBytes(vertices,source_offset,bytes.data(),16u) ||
           !SafeReadBytes(vertices,source_offset+24u,
                          bytes.data()+24u,8u) ||
           !m2vr::drawprobe::DecodeDaytonaUPVertex(bytes,samples[i]))
            return id;
    }
    m2vr::LogEvent("model2_d3d9_up_position_source",id,
        CallerRva(caller),primitive_count,3u);
    // Bind the known shader-UP witness to the original Present frame.
    m2vr::LogEvent("model2_d3d9_up_layout_frame",id,
        g_present_count.load(std::memory_order_relaxed)+1u,
        CallerRva(caller),0u);
    for(unsigned i=0;i<3u;++i) {
        const auto& v=samples[i];
        m2vr::LogEvent("model2_d3d9_up_position",id,i,
            m2vr::drawprobe::PackPair(v.position[0],v.position[1]),
            m2vr::drawprobe::PackPair(v.position[2],v.position[3]));
        m2vr::LogEvent("model2_d3d9_up_uv",id,i,
            m2vr::drawprobe::PackPair(v.texcoord[0],v.texcoord[1]),0u);
    }
    // Hypothesis only: evaluate the original 32-byte shader-UP POSITION0
    // as clip-space, compare its viewport pixels to native CPU projector
    // witnesses from the same thread/Present epoch. Unknown shader WVP
    // must never be promoted to a calibrated 6DoF game camera.
    D3DVIEWPORT9 vp{};
    if(SUCCEEDED(dev->GetViewport(&vp)) &&
       vp.MinZ==0.f && vp.MaxZ==1.f){
        std::array<m2vr::drawprobe::ProjectedDrawVertex,3> screen{};
        bool candidate=true;
        for(unsigned i=0;i<3u;++i){
            if(!m2vr::drawprobe::CandidateClipToViewport(
                   samples[i].position.data(),vp.X,vp.Y,
                   vp.Width,vp.Height,screen[i])){
                candidate=false;break;
            }
        }
        if(candidate){
            const auto join=m2vr::drawprobe::ThreadProjectDrawCorrelator().Match(
                m2vr::drawprobe::CurrentProjectDrawEpoch(),
                screen.data(),screen.size());
            m2vr::LogEvent("model2_d3d9_cpu_shader_up_join",id,
                join.matched,join.ambiguous,join.source_caller_rva);
            // The earlier witness stopped after the first triangle even
            // for hundreds of original shader-UP world candidates. Sample
            // up to seven more, never read shader bytecode or change state.
            constexpr unsigned max_vertices=
                3u*m2vr::drawprobe::ProjectDrawCorrelator::kMaxDiagnosticJoinTriangles;
            std::array<m2vr::drawprobe::ProjectedDrawVertex,max_vertices>
                batch_vertices{};
            for(unsigned i=0;i<3u;++i)batch_vertices[i]=screen[i];
            unsigned valid_triangles=1u;
            const unsigned limit=primitive_count<
                m2vr::drawprobe::ProjectDrawCorrelator::kMaxDiagnosticJoinTriangles?
                primitive_count:static_cast<unsigned>(
                m2vr::drawprobe::ProjectDrawCorrelator::kMaxDiagnosticJoinTriangles);
            for(unsigned tri=1u;tri<limit;++tri){
                bool valid=true;
                for(unsigned j=0u;j<3u;++j){
                    const std::size_t vertex_index=std::size_t(tri)*3u+j;
                    std::array<std::uint8_t,32> bytes{};
                    m2vr::drawprobe::DaytonaUPVertex vertex{};
                    if(!SafeReadBytes(vertices,vertex_index*stride,
                                      bytes.data(),16u) ||
                       !SafeReadBytes(vertices,vertex_index*stride+24u,
                                      bytes.data()+24u,8u) ||
                       !m2vr::drawprobe::DecodeDaytonaUPVertex(bytes,vertex) ||
                       !m2vr::drawprobe::CandidateClipToViewport(
                           vertex.position.data(),vp.X,vp.Y,
                           vp.Width,vp.Height,
                           batch_vertices[vertex_index])){
                        valid=false;break;
                    }
                }
                if(!valid)break; // never count a partial triangle
                ++valid_triangles;
            }
            const auto batch=
                m2vr::drawprobe::ThreadProjectDrawCorrelator()
                    .MatchTriangleList(
                        m2vr::drawprobe::CurrentProjectDrawEpoch(),
                        batch_vertices.data(),
                        std::size_t(valid_triangles)*3u);
            m2vr::LogEvent("model2_d3d9_cpu_shader_up_join_batch",id,
                (std::uint64_t{batch.inspected}<<32)|batch.complete,
                (std::uint64_t{batch.partial}<<32)|batch.ambiguous,
                batch.common_source_caller_rva);
            m2vr::LogEvent("model2_d3d9_cpu_shader_matrix_identity",id,
                batch.common_matrix_identity,
                batch.mixed_matrix_identity?1u:0u,
                batch.common_source_caller_rva);
            m2vr::LogEvent("model2_d3d9_cpu_shader_matrix_content",id,
                batch.common_matrix_fingerprint,
                batch.mixed_matrix_fingerprint?1u:0u,
                batch.common_matrix_identity);
            m2vr::LogEvent("model2_d3d9_cpu_shader_geometry_command",id,
                batch.common_geometry_command,
                batch.mixed_geometry_command?1u:0u,
                batch.common_matrix_fingerprint);
            ObserveProjectionFit(dev,id,batch);
        }
    }
    return id;
}

// Source-only D3D9 DrawPrimitive/DrawIndexedPrimitive provenance.
// No buffer Lock(), texture copy, original draw replay or world-pass flag.
// The actual real game source ranges and fixed-function camera state are
// associated by witness ID for offline, authorized render-path analysis.
std::uint64_t ObserveBufferSource(IDirect3DDevice9* dev,unsigned family,
                         D3DPRIMITIVETYPE type,void* caller,
                         UINT start_vertex,INT base_vertex,
                         UINT min_vertex,UINT num_vertices,
                         UINT start_index,UINT primitive_count) {
    using namespace m2vr::drawprobe;
    if(!dev || (family!=1u && family!=2u) ||
       !m2vr::drawprobe::PrimitiveSourceElementCount(
           type,primitive_count))return 0u;
    DWORD fvf=0;
    if(FAILED(dev->GetFVF(&fvf)))return 0u;
    IDirect3DVertexBuffer9* vb=nullptr;
    UINT offset=0,stride=0;
    if(FAILED(dev->GetStreamSource(0,&vb,&offset,&stride)) || !vb)
        return 0u;
    D3DVERTEXBUFFER_DESC vdesc{};
    const bool desc_ok=SUCCEEDED(vb->GetDesc(&vdesc));
    vb->Release();
    if(!desc_ok)return 0u;
    const auto layout=DecodeWorldUpFVF(fvf,stride);
    const bool projected=ExactProjectedUPWorldFVF(fvf,stride);
    if(!layout.eligible && !projected)return 0u; // no guessed declarations
    D3DMATRIX world{},view{};
    const bool transforms_ok=
        SUCCEEDED(dev->GetTransform(D3DTS_WORLD,&world)) &&
        SUCCEEDED(dev->GetTransform(D3DTS_VIEW,&view)) &&
        AffineMatrix(world)&&AffineMatrix(view);
    const auto id=g_witness_serial.fetch_add(
        1u,std::memory_order_relaxed)+1u;
    // Exact upcoming original main-device Present, never simulation tick.
    // This distinguishes independent scene passes from repeated calls
    // within one frame when validating original world/HUD ownership.
    LogOriginalSourcePassContext(dev,id);
    LogOriginalFixedStageEvidence(dev,id,fvf,stride);
    m2vr::LogEvent("model2_d3d9_vb_present_frame",id,
        g_present_count.load(std::memory_order_relaxed)+1u,
        family,0u);
    m2vr::LogEvent("model2_d3d9_vb_source",id,CallerRva(caller),
        (std::uint64_t{family}<<32)|fvf,
        (std::uint64_t{vdesc.Size}<<32)|stride);
    m2vr::LogEvent("model2_d3d9_vb_stream",id,
        (std::uint64_t{offset}<<32)|vdesc.Usage,
        vdesc.Pool,projected?16u:layout.uv_offset);
    if(projected){
        // The source XYZRHW screen position can be matched against this
        // thread's original CPU projected points, unlike raw XYZ stream.
        // Still only candidate evidence: never authorizes stereo rendering.
        ProjectedTriangleBatchJoin joined{};
        if(InspectProjectedD3D9VBTopologyJoin(dev,family==2u,type,
                start_vertex,base_vertex,min_vertex,num_vertices,
                start_index,primitive_count,joined)){
            m2vr::LogEvent("model2_d3d9_cpu_vb_join_batch",id,
                (std::uint64_t{joined.inspected}<<32)|joined.complete,
                (std::uint64_t{joined.partial}<<32)|joined.ambiguous,
                joined.common_source_caller_rva);
            m2vr::LogEvent("model2_d3d9_cpu_vb_matrix_identity",id,
                joined.common_matrix_identity,
                joined.mixed_matrix_identity?1u:0u,
                joined.common_source_caller_rva);
            m2vr::LogEvent("model2_d3d9_cpu_vb_matrix_content",id,
                joined.common_matrix_fingerprint,
                joined.mixed_matrix_fingerprint?1u:0u,
                joined.common_matrix_identity);
            m2vr::LogEvent("model2_d3d9_cpu_vb_geometry_command",id,
                joined.common_geometry_command,
                joined.mixed_geometry_command?1u:0u,
                joined.common_matrix_fingerprint);
            ObserveProjectionFit(dev,id,joined);
        }
    }
    if(family==1u){
        m2vr::LogEvent("model2_d3d9_vb_draw_args",id,family,
            start_vertex,primitive_count);
    }else{
        m2vr::LogEvent("model2_d3d9_vb_draw_args",id,
            static_cast<std::uint32_t>(base_vertex),
            (std::uint64_t{min_vertex}<<32)|num_vertices,
            (std::uint64_t{start_index}<<32)|primitive_count);
        IDirect3DIndexBuffer9* ib=nullptr;
        if(SUCCEEDED(dev->GetIndices(&ib))&&ib){
            D3DINDEXBUFFER_DESC idesc{};
            if(SUCCEEDED(ib->GetDesc(&idesc)))
                m2vr::LogEvent("model2_d3d9_ib_source",id,
                    idesc.Format,idesc.Size,idesc.Usage);
            ib->Release();
        }
    }
    if(transforms_ok && !projected){
        m2vr::LogEvent("model2_d3d9_vb_camera",id,
            PackPair(world._41,world._42),
            PackPair(world._43,view._41),
            PackPair(view._42,view._43));
    }else{
        m2vr::LogEvent("model2_d3d9_vb_camera_unknown",id);
    }
    // Evidence-only snapshot of the sampler/alpha modes that the native
    // stereo material adapter can (or cannot) currently represent.
    // Unknown states remain unknown, never reported as supported.
    DWORD cull=0,zfunc=0,zwrite=0,stencil=0,depth_bias=0,slope_bias=0;
    DWORD srgb_write=0,color_mask=0;
    if(SUCCEEDED(dev->GetRenderState(D3DRS_CULLMODE,&cull)) &&
       SUCCEEDED(dev->GetRenderState(D3DRS_ZFUNC,&zfunc)) &&
       SUCCEEDED(dev->GetRenderState(D3DRS_ZWRITEENABLE,&zwrite)) &&
       SUCCEEDED(dev->GetRenderState(D3DRS_STENCILENABLE,&stencil)) &&
       SUCCEEDED(dev->GetRenderState(D3DRS_DEPTHBIAS,&depth_bias)) &&
       SUCCEEDED(dev->GetRenderState(D3DRS_SLOPESCALEDEPTHBIAS,&slope_bias)) &&
       SUCCEEDED(dev->GetRenderState(D3DRS_SRGBWRITEENABLE,&srgb_write)) &&
       SUCCEEDED(dev->GetRenderState(D3DRS_COLORWRITEENABLE,&color_mask))) {
        m2vr::LogEvent("model2_d3d9_vb_raster",id,
            (std::uint64_t{cull}<<32)|zfunc,
            (std::uint64_t{zwrite}<<32)|stencil,
            (std::uint64_t{depth_bias}<<32)|slope_bias);
        m2vr::LogEvent("model2_d3d9_vb_write_color",id,
            (std::uint64_t{srgb_write}<<32)|color_mask,0u,0u);
    }
    DWORD au=0,av=0,mn=0,mg=0,mip=0,srgb=0;
    if(SUCCEEDED(dev->GetSamplerState(0,D3DSAMP_ADDRESSU,&au)) &&
       SUCCEEDED(dev->GetSamplerState(0,D3DSAMP_ADDRESSV,&av)) &&
       SUCCEEDED(dev->GetSamplerState(0,D3DSAMP_MINFILTER,&mn)) &&
       SUCCEEDED(dev->GetSamplerState(0,D3DSAMP_MAGFILTER,&mg)) &&
       SUCCEEDED(dev->GetSamplerState(0,D3DSAMP_MIPFILTER,&mip)) &&
       SUCCEEDED(dev->GetSamplerState(0,D3DSAMP_SRGBTEXTURE,&srgb))) {
        m2vr::LogEvent("model2_d3d9_vb_sampler",id,
            (std::uint64_t{au}<<32)|av,
            (std::uint64_t{mn}<<32)|mg,
            (std::uint64_t{mip}<<32)|srgb);
    }
    DWORD ae=0,af=0,ar=0;
    if(SUCCEEDED(dev->GetRenderState(D3DRS_ALPHATESTENABLE,&ae)) &&
       SUCCEEDED(dev->GetRenderState(D3DRS_ALPHAFUNC,&af)) &&
       SUCCEEDED(dev->GetRenderState(D3DRS_ALPHAREF,&ar))) {
        m2vr::LogEvent("model2_d3d9_vb_alpha",id,
            (std::uint64_t{ae}<<32)|af,ar,0u);
    }
    IDirect3DBaseTexture9* base=nullptr;
    if(SUCCEEDED(dev->GetTexture(0,&base))&&base){
        if(base->GetType()==D3DRTYPE_TEXTURE){
            auto* tex=static_cast<IDirect3DTexture9*>(base);
            D3DSURFACE_DESC tdesc{};
            if(SUCCEEDED(tex->GetLevelDesc(0,&tdesc)))
                m2vr::LogEvent("model2_d3d9_vb_texture",id,
                    tdesc.Format,tdesc.Width,tdesc.Height);
        }
        base->Release();
    }
    return id;
}

void RecordOriginalSourceResult(std::uint64_t witness,HRESULT hr,
                                void* caller,unsigned family,
                                std::uint64_t original_frame,
                                std::uint64_t frame_ordinal) {
    if(!witness)return;
    // Capture phase BEFORE the original COM Draw; a concurrent Present
    // must not relabel the returned HRESULT with the next frame.
    m2vr::LogEvent("model2_d3d9_source_draw_phase",witness,
        original_frame,frame_ordinal,
        (std::uint64_t{CallerRva(caller)}<<32)|family);
    m2vr::LogEvent(family<=2u?"model2_d3d9_vb_original_result":
                               "model2_d3d9_up_original_result",witness,
        static_cast<std::uint32_t>(hr),CallerRva(caller),
        (original_frame<<32)|family);
    g_original_source_result_samples.fetch_add(1u,std::memory_order_relaxed);
}
HRESULT STDMETHODCALLTYPE DrawHook(IDirect3DDevice9* d,D3DPRIMITIVETYPE p,UINT start,UINT count){
    // Consume the original CPU projector witnesses once this physical
    // D3D9 Draw completes; no same-Present sample reuse by later Draws.
    const m2vr::drawprobe::ScopedOriginalDrawBoundary draw_boundary;
    void* caller=_ReturnAddress();
    if(!g_primary_device.Owns(d))
        return TrackOriginalD3D9DrawResult(
            g_draw(d,p,start,count),1u,caller);
    g_draws.fetch_add(1,std::memory_order_relaxed);
    const auto source_frame=g_present_count.load(std::memory_order_relaxed)+1u;
    const auto source_ordinal=g_original_frame_draw_ordinal.fetch_add(
        1u,std::memory_order_relaxed)+1u;
    const std::uint64_t witness=Sample(d,1,p,count,caller)?
        ObserveBufferSource(d,1,p,caller,start,0,0,0,0,count):0u;
    const HRESULT hr=g_draw(d,p,start,count);
    RecordOriginalSourceResult(witness,hr,caller,1u,
                               source_frame,source_ordinal);
    TrackOriginalD3D9DrawResult(hr,1u,caller);
    if(SUCCEEDED(hr) && g_native_stereo)
        m2vr::daytona::MirrorPrimitive(d,p,start,count,g_draw);
    return hr;
}
HRESULT STDMETHODCALLTYPE IndexedHook(IDirect3DDevice9* d,D3DPRIMITIVETYPE p,INT base,
                                      UINT min,UINT num,UINT start,UINT count){
    // Consume the original CPU projector witnesses once this physical
    // D3D9 Draw completes; no same-Present sample reuse by later Draws.
    const m2vr::drawprobe::ScopedOriginalDrawBoundary draw_boundary;
    void* caller=_ReturnAddress();
    if(!g_primary_device.Owns(d))
        return TrackOriginalD3D9DrawResult(
            g_draw_indexed(d,p,base,min,num,start,count),2u,caller);
    g_indexed.fetch_add(1,std::memory_order_relaxed);
    const auto source_frame=g_present_count.load(std::memory_order_relaxed)+1u;
    const auto source_ordinal=g_original_frame_draw_ordinal.fetch_add(
        1u,std::memory_order_relaxed)+1u;
    const std::uint64_t witness=Sample(d,2,p,count,caller)?
        ObserveBufferSource(d,2,p,caller,0,base,min,num,start,count):0u;
    const HRESULT hr=g_draw_indexed(d,p,base,min,num,start,count);
    RecordOriginalSourceResult(witness,hr,caller,2u,
                               source_frame,source_ordinal);
    TrackOriginalD3D9DrawResult(hr,2u,caller);
    if(SUCCEEDED(hr) && g_native_stereo)
        m2vr::daytona::MirrorIndexed(d,p,base,min,num,start,count,g_draw_indexed);
    return hr;
}
HRESULT STDMETHODCALLTYPE DrawUPHook(IDirect3DDevice9* d,D3DPRIMITIVETYPE p,UINT count,
                                    const void* verts,UINT stride){
    // Consume the original CPU projector witnesses once this physical
    // D3D9 Draw completes; no same-Present sample reuse by later Draws.
    const m2vr::drawprobe::ScopedOriginalDrawBoundary draw_boundary;
    void* caller=_ReturnAddress();
    if(!g_primary_device.Owns(d))
        return TrackOriginalD3D9DrawResult(
            g_draw_up(d,p,count,verts,stride),3u,caller);
    g_up.fetch_add(1,std::memory_order_relaxed);
    const auto source_frame=g_present_count.load(std::memory_order_relaxed)+1u;
    const auto source_ordinal=g_original_frame_draw_ordinal.fetch_add(
        1u,std::memory_order_relaxed)+1u;
    const std::uint64_t witness=Sample(d,3,p,count,caller)?
        ObserveUP(d,3,p,count,caller,verts,stride):0u;
    const HRESULT hr=g_draw_up(d,p,count,verts,stride);
    RecordOriginalSourceResult(witness,hr,caller,3u,
                               source_frame,source_ordinal);
    TrackOriginalD3D9DrawResult(hr,3u,caller);
    if(SUCCEEDED(hr) && g_native_stereo)
        m2vr::daytona::MirrorDraw(d,p,count,verts,stride,CallerRva(caller),g_draw_up);
    return hr;
}
HRESULT STDMETHODCALLTYPE IndexedUPHook(IDirect3DDevice9* d,D3DPRIMITIVETYPE p,UINT min,
     UINT num,UINT count,const void* idx,D3DFORMAT fmt,const void* verts,UINT stride){
    // Consume the original CPU projector witnesses once this physical
    // D3D9 Draw completes; no same-Present sample reuse by later Draws.
    const m2vr::drawprobe::ScopedOriginalDrawBoundary draw_boundary;
    void* caller=_ReturnAddress();
    if(!g_primary_device.Owns(d))
        return TrackOriginalD3D9DrawResult(
            g_draw_indexed_up(d,p,min,num,count,idx,fmt,verts,stride),4u,caller);
    g_indexed_up.fetch_add(1,std::memory_order_relaxed);
    const auto source_frame=g_present_count.load(std::memory_order_relaxed)+1u;
    const auto source_ordinal=g_original_frame_draw_ordinal.fetch_add(
        1u,std::memory_order_relaxed)+1u;
    const std::uint64_t witness=Sample(d,4,p,count,caller)?
        ObserveUP(d,4,p,count,caller,verts,stride,idx,fmt,min,num):0u;
    const HRESULT hr=g_draw_indexed_up(d,p,min,num,count,idx,fmt,verts,stride);
    RecordOriginalSourceResult(witness,hr,caller,4u,
                               source_frame,source_ordinal);
    TrackOriginalD3D9DrawResult(hr,4u,caller);
    if(SUCCEEDED(hr) && g_native_stereo)
        m2vr::daytona::MirrorIndexedUP(d,p,min,num,count,idx,fmt,verts,stride,g_draw_indexed_up);
    return hr;
}
HRESULT STDMETHODCALLTYPE ClearHook(IDirect3DDevice9* d,DWORD count,
                                   const D3DRECT* rects,DWORD flags,
                                   D3DCOLOR color,float z,DWORD stencil){
    if(!g_primary_device.Owns(d))return g_clear(d,count,rects,flags,color,z,stencil);
    const HRESULT hr=g_clear(d,count,rects,flags,color,z,stencil);
    if(SUCCEEDED(hr) && g_native_stereo)
        m2vr::daytona::MirrorClear(d,count,rects,flags,color,z,stencil,g_clear);
    return hr;
}
HRESULT STDMETHODCALLTYPE UpdateSurfaceHook(IDirect3DDevice9* d,
     IDirect3DSurface9* src,const RECT* src_rect,IDirect3DSurface9* dst,
     const POINT* dst_point){
    if(!g_primary_device.Owns(d))return g_update_surface(d,src,src_rect,dst,dst_point);
    const HRESULT hr=g_update_surface(d,src,src_rect,dst,dst_point);
    if(SUCCEEDED(hr) && g_native_stereo)
        m2vr::daytona::MirrorUpdateSurface(
            d,src,src_rect,dst,dst_point,g_update_surface);
    return hr;
}
HRESULT STDMETHODCALLTYPE StretchRectHook(IDirect3DDevice9* d,
     IDirect3DSurface9* src,const RECT* src_rect,IDirect3DSurface9* dst,
     const RECT* dst_rect,D3DTEXTUREFILTERTYPE filter){
    if(!g_primary_device.Owns(d))return g_stretch_rect(d,src,src_rect,dst,dst_rect,filter);
    const HRESULT hr=g_stretch_rect(d,src,src_rect,dst,dst_rect,filter);
    if(SUCCEEDED(hr) && g_native_stereo)
        m2vr::daytona::MirrorStretchRect(
            d,src,src_rect,dst,dst_rect,filter,g_stretch_rect);
    return hr;
}
HRESULT STDMETHODCALLTYPE ColorFillHook(IDirect3DDevice9* d,
     IDirect3DSurface9* dst,const RECT* rect,D3DCOLOR color){
    if(!g_primary_device.Owns(d))return g_color_fill(d,dst,rect,color);
    const HRESULT hr=g_color_fill(d,dst,rect,color);
    if(SUCCEEDED(hr) && g_native_stereo)
        m2vr::daytona::MirrorColorFill(d,dst,rect,color,g_color_fill);
    return hr;
}
// Every COM Present path must execute the original game's method exactly
// once; a secondary device is excluded from all primary frame accounting.
HRESULT CallOriginalPresentOnce(IDirect3DDevice9* d,CONST RECT* a,
    CONST RECT* b,HWND w,CONST RGNDATA* dirty){
    return g_present(d,a,b,w,dirty);
}
HRESULT STDMETHODCALLTYPE PresentHook(IDirect3DDevice9* d,CONST RECT* a,CONST RECT* b,
                                     HWND w,CONST RGNDATA* dirty){
    if(!g_primary_device.Owns(d)){
        const unsigned seen=g_secondary_presents.fetch_add(
            1u,std::memory_order_relaxed)+1u;
        if(seen==1u || seen%120u==0u)
            m2vr::LogEvent("model2_d3d9_secondary_device_present",
                           seen,0u,0u,0u);
        return CallOriginalPresentOnce(d,a,b,w,dirty);
    }
    const unsigned frame=g_present_count.fetch_add(1,std::memory_order_relaxed)+1;
    g_original_frame_draw_ordinal.exchange(0u,std::memory_order_relaxed);
    // Native replay must also drain the *original* source-family counters.
    // Previously these only reset in passive 2D mode, so a long VR session
    // accumulated lifetime totals instead of per-Present evidence.
    const unsigned dp=g_draws.exchange(0u,std::memory_order_relaxed);
    const unsigned dip=g_indexed.exchange(0u,std::memory_order_relaxed);
    const unsigned dpup=g_up.exchange(0u,std::memory_order_relaxed);
    const unsigned dipup=g_indexed_up.exchange(0u,std::memory_order_relaxed);
    // Query the authenticated source before the physical Present; this
    // permits staging original mono as backup instead of forcing two
    // speculative GPU eye readbacks that cannot win the frame arbitration.
    const bool verified_cpu_pending_pre=
        Model2VR_HasVerifiedWorldPendingForOriginalPresent(frame)!=FALSE;
    if(g_native_stereo){
        m2vr::daytona::PrepareFrameBeforePresent(d,frame,verified_cpu_pending_pre);
        // Aggregate original draw counts remain available even if the
        // HMD is disconnected, and never log game vertices or shaders.
        if(frame==1u || frame%120u==0u)
            m2vr::LogEvent("model2_vr_original_draw_families",frame,
                (std::uint64_t{dp}<<32)|dip,
                (std::uint64_t{dpup}<<32)|dipup);
    }else{
        m2vr::LogEvent("model2_d3d9_frame",frame,dp+dip,dpup+dipup);
    }
    // Snapshot sampling state belongs to a frame, not a process lifetime.
    for(auto& counter:g_family_samples)
        counter.store(0u,std::memory_order_relaxed);
    // Original Present runs exactly once. Preserve its HRESULT and publish
    // the staged GameStereo pair only if the real source Present succeeded.
    const HRESULT original_result=g_present(d,a,b,w,dirty);
    const auto source_samples=g_original_source_result_samples.exchange(
        0u,std::memory_order_relaxed);
    if(source_samples)
        m2vr::LogEvent("model2_d3d9_original_present_result",frame,
            static_cast<std::uint32_t>(original_result),source_samples,0u);
    m2vr::drawprobe::NextProjectDrawEpoch();
    // Both source paths staged their images BEFORE the single physical
    // game Present. An explicitly authenticated same-frame CPU-world pair
    // wins only after its original HRESULT succeeds and pose stays live.
    const bool verified_cpu_pending=verified_cpu_pending_pre &&
        Model2VR_HasVerifiedWorldPendingForOriginalPresent(frame)!=FALSE;
    const auto stereo_mode=m2vr::vrbridge::SelectOriginalStereoSourceMode(
        g_native_stereo,verified_cpu_pending);
    const bool verified_cpu_published=
        Model2VR_CompleteVerifiedWorldOriginalPresent(
            frame,SUCCEEDED(original_result)?1u:0u,
            static_cast<unsigned int>(stereo_mode))!=FALSE;
    if(g_native_stereo){
        if(verified_cpu_published){
            // Do not publish a second GameStereo packet for this frame or
            // overwrite producer health from the verified CPU world.
            m2vr::daytona::DiscardPreparedFrameAfterCpuWorld(frame);
        }else{
            // Absent/expired/wrong-owner world: preserve experimental eye
            // replay and original 2D backbuffer fallback, no extra game Draw.
            m2vr::daytona::FinishFrameAfterPresent(frame,original_result);
        }
    }
    return original_result;
}
HRESULT STDMETHODCALLTYPE ResetHook(IDirect3DDevice9* d,
                                     D3DPRESENT_PARAMETERS* pp){
    if(!g_primary_device.Owns(d))return g_reset(d,pp);
    g_original_frame_draw_ordinal.store(0u,std::memory_order_relaxed);
    m2vr::drawprobe::NextProjectDrawEpoch();
    Model2VR_DiscardPendingWorldOnReset();
    if(g_native_stereo)m2vr::daytona::ResetResources();
    return g_reset(d,pp);
}
bool HookDevice(IDirect3DDevice9* dev){
    if(!dev)return false;
    std::lock_guard lock(g_setup_mutex);
    if(g_device_installed)return true;
    // COM IDirect3DDevice9 method slots per d3d9.h (IUnknown included).
    auto** vtable=*reinterpret_cast<void***>(dev);
    const bool p=Hook(vtable[17],reinterpret_cast<void*>(&PresentHook),g_present);
    const bool dp=Hook(vtable[81],reinterpret_cast<void*>(&DrawHook),g_draw);
    const bool di=Hook(vtable[82],reinterpret_cast<void*>(&IndexedHook),g_draw_indexed);
    const bool du=Hook(vtable[83],reinterpret_cast<void*>(&DrawUPHook),g_draw_up);
    const bool iu=Hook(vtable[84],reinterpret_cast<void*>(&IndexedUPHook),g_draw_indexed_up);
    // Even passive mode can host an explicitly verified CPU-world frame.
    // Always discard its pending eye pair on original device Reset.
    const bool reset=Hook(
        vtable[16],reinterpret_cast<void*>(&ResetHook),g_reset);
    const bool clear=!g_native_stereo ||
        Hook(vtable[43],reinterpret_cast<void*>(&ClearHook),g_clear);
    const bool update=!g_native_stereo ||
        Hook(vtable[30],reinterpret_cast<void*>(&UpdateSurfaceHook),g_update_surface);
    const bool stretch=!g_native_stereo ||
        Hook(vtable[34],reinterpret_cast<void*>(&StretchRectHook),g_stretch_rect);
    const bool fill=!g_native_stereo ||
        Hook(vtable[35],reinterpret_cast<void*>(&ColorFillHook),g_color_fill);
    if(!(p&&dp&&di&&du&&iu&&reset&&clear&&update&&stretch&&fill)){
        // Transactional rollback: partial GPU hooks must never be left armed
        // after hook setup fails. Keep the emulator's normal render path.
        if(p){MH_DisableHook(vtable[17]);MH_RemoveHook(vtable[17]);}
        if(dp){MH_DisableHook(vtable[81]);MH_RemoveHook(vtable[81]);}
        if(di){MH_DisableHook(vtable[82]);MH_RemoveHook(vtable[82]);}
        if(du){MH_DisableHook(vtable[83]);MH_RemoveHook(vtable[83]);}
        if(iu){MH_DisableHook(vtable[84]);MH_RemoveHook(vtable[84]);}
        if(reset){MH_DisableHook(vtable[16]);MH_RemoveHook(vtable[16]);}
        if(g_native_stereo&&clear){MH_DisableHook(vtable[43]);MH_RemoveHook(vtable[43]);}
        if(g_native_stereo&&update){MH_DisableHook(vtable[30]);MH_RemoveHook(vtable[30]);}
        if(g_native_stereo&&stretch){MH_DisableHook(vtable[34]);MH_RemoveHook(vtable[34]);}
        if(g_native_stereo&&fill){MH_DisableHook(vtable[35]);MH_RemoveHook(vtable[35]);}
        g_present=nullptr;g_draw=nullptr;g_draw_indexed=nullptr;
        g_reset=nullptr;g_clear=nullptr;
        g_update_surface=nullptr;
        g_stretch_rect=nullptr;g_color_fill=nullptr;
        g_draw_up=nullptr;g_draw_indexed_up=nullptr;
        m2vr::LogEvent("model2_d3d9_partial_hook_rolled_back",p,dp,di,du);
        m2vr::LogEvent("model2_d3d9_surface_hook_rollback",
                       update,stretch,fill,clear);
        return false;
    }
    // Vtable methods are shared by D3D9 devices. Bind the real first
    // successfully hooked game device before declaring the hook ready.
    if(!g_primary_device.BindOnce(dev))return false;
    g_device_installed.store(true,std::memory_order_release);
    m2vr::LogEvent("model2_d3d9_device_hooks_ready");
    return true;
}
HRESULT STDMETHODCALLTYPE CreateDeviceHook(IDirect3D9* api,UINT adapter,D3DDEVTYPE type,
    HWND hwnd,DWORD flags,D3DPRESENT_PARAMETERS* pp,IDirect3DDevice9** out){
    const HRESULT hr=g_create_device(api,adapter,type,hwnd,flags,pp,out);
    if(SUCCEEDED(hr)&&out&&*out){
        m2vr::LogEvent("model2_d3d9_device_created",
            pp?pp->BackBufferWidth:0,pp?pp->BackBufferHeight:0,flags);
        if(pp){
            m2vr::LogEvent("model2_d3d9_device_surface",
                static_cast<std::uint32_t>(pp->BackBufferFormat),
                static_cast<std::uint32_t>(pp->MultiSampleType),
                static_cast<std::uint32_t>(pp->AutoDepthStencilFormat),
                pp->EnableAutoDepthStencil?1u:0u);
        }
        const bool hooks_ready=HookDevice(*out);
        m2vr::LogEvent("model2_d3d9_device_hook_result",hooks_ready?1u:0u);
        // This is a one-time startup boundary. Flush it immediately so a
        // later device/render failure cannot hide the exact hook outcome.
        m2vr::FlushLog();
    }
    return hr;
}
IDirect3D9* WINAPI Create9Hook(UINT sdk){
    IDirect3D9* api=g_create9(sdk);
    if(api && !g_create_device){
        // IDirect3D9::CreateDevice is method 16.
        auto** vtable=*reinterpret_cast<void***>(api);
        std::lock_guard lock(g_setup_mutex);
        if(!g_create_device){
            if(!Hook(vtable[16],reinterpret_cast<void*>(&CreateDeviceHook),g_create_device))
                m2vr::LogEvent("model2_d3d9_create_device_hook_failed");
        }
    }
    return api;
}
} // namespace

// The authenticated source adapter must use this specific original D3D9
// frame identifier instead of assuming its emulator simulation frame index
// equals a real device Present. Only an independently authenticated world
// transaction can replace experimental GPU replay on that physical frame.
extern "C" __declspec(dllexport) BOOL __stdcall
Model2VR_IsOriginalPrimaryD3D9Device(const void* device){
    return g_device_installed.load(std::memory_order_acquire) &&
           g_primary_device.Owns(device) ? TRUE : FALSE;
}
extern "C" __declspec(dllexport) unsigned long long __stdcall
Model2VR_NextOriginalD3D9PresentFrameId(){
    return m2vr::vrbridge::NextOriginalPresentFrameId(
        g_present_count.load(std::memory_order_relaxed),
        g_device_installed.load(std::memory_order_acquire),
        g_native_stereo);
}

namespace m2vr {
bool InstallD3D9ReadOnlyProbe(HMODULE executable,std::uint32_t exe_size,
                             std::uint32_t draw_sample_every,
                             bool native_stereo,std::uint32_t eye_shift_milliunits) {
    if(!executable || exe_size==0)return false;
    g_exe_base=reinterpret_cast<std::uintptr_t>(executable);
    g_exe_size=exe_size;
    // A diagnostic setting is an integer [64,4096] after INI parsing.
    // Preserve conservative sampling if this function is called directly.
    g_native_stereo=native_stereo;
    m2vr::daytona::Configure(native_stereo,eye_shift_milliunits);
    g_draw_sample_every=(draw_sample_every>=64u &&
        draw_sample_every<=4096u)?draw_sample_every:1024u;
    LogEvent("model2_d3d9_draw_interval",g_draw_sample_every);
    HMODULE d3d9=GetModuleHandleW(L"d3d9.dll");
    if(!d3d9) {
        // The loader injects before the suspended emulator main thread
        // resumes, so d3d9.dll commonly has not been loaded yet.
        // Load the process's normal D3D9 runtime *without creating a GPU
        // device* so the original Direct3DCreate9 call can be hooked.
        // The reference stays loaded for process-lifetime probe hooks.
        d3d9=LoadLibraryW(L"d3d9.dll");
        if(d3d9)LogEvent("model2_d3d9_runtime_preloaded");
    }
    if(!d3d9){
        LogEvent("model2_d3d9_runtime_load_failed",GetLastError());
        return false;
    }
    auto* create=reinterpret_cast<void*>(GetProcAddress(d3d9,"Direct3DCreate9"));
    if(!create)return false;
    const MH_STATUS status=MH_CreateHook(create,
        reinterpret_cast<void*>(&Create9Hook),reinterpret_cast<LPVOID*>(&g_create9));
    if(status!=MH_OK){LogEvent("model2_d3d9_create9_hook_failed",status);return false;}
    // The main probe installer enables all hooks atomically after signatures
    // pass. Runtime-created device hooks are enabled when discovered.
    LogEvent("model2_d3d9_create9_hook_armed");
    return true;
}
} // namespace m2vr