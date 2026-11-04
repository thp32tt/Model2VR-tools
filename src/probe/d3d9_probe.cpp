#include "logger.h"
#include "draw_up_witness.hpp"
#include "daytona_native_stereo.hpp"
#include "daytona_up_decl_witness.hpp"
#include "draw_origin_gate.hpp"
#include "project_draw_correlator.hpp"
#include "model2_camera_projection_fit.hpp"
#include "d3d9_primary_device_owner.hpp"
#include "verified_world_present_gate.hpp"
#include "d3d9_world_source.hpp"
#include "d3d9_projected_vb_observer.hpp"
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
std::atomic<std::uint64_t> g_witness_serial{0};
std::uint32_t g_draw_sample_every=1024u;
std::atomic<bool> g_device_installed{false};
m2vr::drawprobe::OriginalD3D9DeviceOwner g_primary_device{};
std::atomic<unsigned> g_secondary_presents{0u};
bool g_native_stereo=false;
// Internal callback into the independently proven CPU-world renderer.
// No verified-world frame is generated or self-authorized by this probe.
extern "C" BOOL __stdcall Model2VR_CompleteVerifiedWorldOriginalPresent(
    unsigned long long,unsigned int,unsigned int);
extern "C" void __stdcall Model2VR_DiscardPendingWorldOnReset();

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
void ObserveWorldUP(IDirect3DDevice9* dev,unsigned family,
                    D3DPRIMITIVETYPE type,UINT primitive_count,void* caller,
                    const void* vertices,UINT stride,const void* indices,
                    D3DFORMAT index_format,UINT first,UINT vertex_count) {
    using namespace m2vr::drawprobe;
    if(!dev||!vertices||type!=D3DPT_TRIANGLELIST)return;
    DWORD fvf=0;
    if(FAILED(dev->GetFVF(&fvf)))return;
    const auto layout=DecodeWorldUpFVF(fvf,stride);
    if(!layout.eligible)return;
    if(family==4u && (!indices||first!=0u||!vertex_count||
       vertex_count>65536u ||
       (index_format!=D3DFMT_INDEX16 &&
        index_format!=D3DFMT_INDEX32)))return;
    D3DMATRIX world{},view{};
    if(FAILED(dev->GetTransform(D3DTS_WORLD,&world)) ||
       FAILED(dev->GetTransform(D3DTS_VIEW,&view)) ||
       !AffineMatrix(world)||!AffineMatrix(view))return;
    const auto elements=PrimitiveElements(
        static_cast<std::uint32_t>(type),primitive_count);
    if(!elements)return;
    constexpr unsigned max_sample=3u;
    std::array<WorldVertex,max_sample> samples{};
    std::array<std::uint32_t,max_sample> ids{};
    const unsigned count=elements<max_sample?elements:max_sample;
    for(unsigned i=0;i<count;++i) {
        std::uint32_t index=i;
        if(family==4u) {
            const unsigned bytes=index_format==D3DFMT_INDEX16?2u:4u;
            std::array<std::uint8_t,4> raw{};
            if(!SafeReadBytes(indices,std::size_t(i)*bytes,
                              raw.data(),bytes))return;
            if(bytes==2u){std::uint16_t x=0;std::memcpy(&x,raw.data(),2);index=x;}
            else std::memcpy(&index,raw.data(),4);
            if(index>=vertex_count)return;
        }
        // Read only fixed layout fields. Never dereference beyond 65536
        // source bytes, and never ask the caller to provide hidden memory.
        std::array<std::uint8_t,256> source{};
        if(!SafeReadBytes(vertices,std::size_t(index)*stride,
                          source.data(),12u) ||
           !SafeReadBytes(vertices,std::size_t(index)*stride+
                              layout.uv_offset,
                          source.data()+layout.uv_offset,8u) ||
           !DecodeWorldVertex(source.data(),stride,layout,
                              world,view,samples[i]))return;
        ids[i]=index;
    }
    const auto id=g_witness_serial.fetch_add(
        1u,std::memory_order_relaxed)+1u;
    m2vr::LogEvent("model2_world_up_candidate",id,
         CallerRva(caller),fvf,
         (std::uint64_t{family}<<32)|stride);
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
}
// Candidate camera fit is implemented after ObserveUP below.
void ObserveProjectionFit(IDirect3DDevice9* dev,std::uint64_t witness,
        const m2vr::drawprobe::ProjectedTriangleBatchJoin& batch);
// Defined below: bounded original FVF=0 declaration observation.
void ObserveUnknownUpLayout(IDirect3DDevice9* dev,unsigned family,
                            void* caller,UINT stride,
                            D3DPRIMITIVETYPE topology,
                            UINT primitive_count,const void* vertices);
void ObserveUP(IDirect3DDevice9* dev,unsigned family,
               D3DPRIMITIVETYPE type,UINT primitive_count,void* caller,
               const void* vertices,UINT stride,const void* indices=nullptr,
               D3DFORMAT index_format=D3DFMT_UNKNOWN,UINT first=0,
               UINT vertex_count=0) {
    using namespace m2vr::drawprobe;
    const auto elements=PrimitiveElements(
        static_cast<std::uint32_t>(type),primitive_count);
    if(!dev||!vertices||!ValidStride(stride)||!elements)return;
    DWORD fvf=0;
    if(FAILED(dev->GetFVF(&fvf)))return;
    if(fvf==0u) {
        ObserveUnknownUpLayout(dev,family,caller,stride,type,
                               primitive_count,vertices);
        return;
    }
    if((fvf&D3DFVF_POSITION_MASK)==D3DFVF_XYZ){
        ObserveWorldUP(dev,family,type,primitive_count,caller,
                       vertices,stride,indices,index_format,first,vertex_count);
        return;
    }
    if((fvf&D3DFVF_POSITION_MASK)!=D3DFVF_XYZRHW)return;

    // For indexed UP the vertex base is only unambiguous at index zero.
    // Skip all other layouts rather than risk a false draw association.
    if(family==4u && (!indices||first!=0u||!vertex_count||
       vertex_count>65536u ||
       (index_format!=D3DFMT_INDEX16 &&
        index_format!=D3DFMT_INDEX32)))return;

    struct Observation {std::uint32_t index=0; VertexWitness vertex{};};
    // Capture at most eight triangle-list primitives in one original Draw.
    // Leave unrelated topology sampling at the historical three vertices.
    constexpr unsigned kJoinSampleVertices=
        3u*m2vr::drawprobe::ProjectDrawCorrelator::kMaxJoinTriangles;
    std::array<Observation,kJoinSampleVertices> observations{};
    const unsigned limit=type==D3DPT_TRIANGLELIST?
        kJoinSampleVertices:MaxVertices;
    const unsigned count=elements<limit?elements:limit;
    for(unsigned i=0;i<count;++i){
        std::uint32_t index=i;
        if(family==4u){
            std::array<std::uint8_t,4> bytes{};
            const unsigned width=index_format==D3DFMT_INDEX16?2u:4u;
            if(!SafeReadBytes(indices,std::size_t{i}*width,
                              bytes.data(),width))return;
            if(width==2u){
                std::uint16_t short_index=0;
                std::memcpy(&short_index,bytes.data(),2u);
                index=short_index;
            }else{
                std::memcpy(&index,bytes.data(),4u);
            }
            if(index>=vertex_count)return;
        }
        std::array<std::uint32_t,4> bits{};
        if(!SafeReadBytes(vertices,std::size_t{index}*stride,
                          bits.data(),sizeof(bits)) ||
           !DecodeXYZRHW(bits,observations[i].vertex))return;
        observations[i].index=index;
    }

    const auto sample_id=g_witness_serial.fetch_add(
        1u,std::memory_order_relaxed)+1u;
    m2vr::LogEvent("model2_d3d9_up_meta",sample_id,
        g_present_count.load(std::memory_order_relaxed)+1u,
        CallerRva(caller),(std::uint64_t{family}<<32)|stride);
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
    if(type==D3DPT_TRIANGLELIST && count>=3u){
        std::array<ProjectedDrawVertex,kJoinSampleVertices> screen{};
        for(unsigned i=0;i<count;++i) {
            screen[i]={observations[i].vertex.x,observations[i].vertex.y};
        }
        const auto epoch=CurrentProjectDrawEpoch();
        const auto& correlator=ThreadProjectDrawCorrelator();
        // Backward-compatible first-triangle event.
        const auto join=correlator.Match(epoch,screen.data(),3u);
        m2vr::LogEvent("model2_d3d9_cpu_up_join",sample_id,
            join.matched,join.ambiguous,join.source_caller_rva);
        const auto batch=correlator.MatchTriangleList(
            epoch,screen.data(),count);
        m2vr::LogEvent("model2_d3d9_cpu_up_join_batch",sample_id,
            (std::uint64_t{batch.inspected}<<32)|batch.complete,
            (std::uint64_t{batch.partial}<<32)|batch.ambiguous,
            batch.common_source_caller_rva);
        ObserveProjectionFit(dev,sample_id,batch);
    }
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
void ObserveUnknownUpLayout(IDirect3DDevice9* dev,unsigned family,
                            void* caller,UINT stride,
                            D3DPRIMITIVETYPE topology,
                            UINT primitive_count,const void* vertices) {
    if(!dev || family<3u || family>4u)return;
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
           elements.data(),count,stride))return;
    std::array<m2vr::drawprobe::DaytonaUPVertex,3> samples{};
    for(unsigned i=0;i<3u;++i) {
        std::array<std::uint8_t,32> bytes{};
        const std::size_t source_offset=std::size_t{i}*stride;
        if(!SafeReadBytes(vertices,source_offset,bytes.data(),16u) ||
           !SafeReadBytes(vertices,source_offset+24u,
                          bytes.data()+24u,8u) ||
           !m2vr::drawprobe::DecodeDaytonaUPVertex(bytes,samples[i]))
            return;
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
                3u*m2vr::drawprobe::ProjectDrawCorrelator::kMaxJoinTriangles;
            std::array<m2vr::drawprobe::ProjectedDrawVertex,max_vertices>
                batch_vertices{};
            for(unsigned i=0;i<3u;++i)batch_vertices[i]=screen[i];
            unsigned valid_triangles=1u;
            const unsigned limit=primitive_count<
                m2vr::drawprobe::ProjectDrawCorrelator::kMaxJoinTriangles?
                primitive_count:static_cast<unsigned>(
                m2vr::drawprobe::ProjectDrawCorrelator::kMaxJoinTriangles);
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
            ObserveProjectionFit(dev,id,batch);
        }
    }
}

// Source-only D3D9 DrawPrimitive/DrawIndexedPrimitive provenance.
// No buffer Lock(), texture copy, original draw replay or world-pass flag.
// The actual real game source ranges and fixed-function camera state are
// associated by witness ID for offline, authorized render-path analysis.
void ObserveBufferSource(IDirect3DDevice9* dev,unsigned family,
                         D3DPRIMITIVETYPE type,void* caller,
                         UINT start_vertex,INT base_vertex,
                         UINT min_vertex,UINT num_vertices,
                         UINT start_index,UINT primitive_count) {
    using namespace m2vr::drawprobe;
    if(!dev || (family!=1u && family!=2u) ||
       type!=D3DPT_TRIANGLELIST || !primitive_count)return;
    DWORD fvf=0;
    if(FAILED(dev->GetFVF(&fvf)))return;
    IDirect3DVertexBuffer9* vb=nullptr;
    UINT offset=0,stride=0;
    if(FAILED(dev->GetStreamSource(0,&vb,&offset,&stride)) || !vb)
        return;
    D3DVERTEXBUFFER_DESC vdesc{};
    const bool desc_ok=SUCCEEDED(vb->GetDesc(&vdesc));
    vb->Release();
    if(!desc_ok)return;
    const auto layout=DecodeWorldUpFVF(fvf,stride);
    const bool projected=ExactProjectedUPWorldFVF(fvf,stride);
    if(!layout.eligible && !projected)return; // no guessed declarations
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
        if(InspectProjectedD3D9VBJoin(dev,family==2u,
                start_vertex,base_vertex,min_vertex,num_vertices,
                start_index,primitive_count,joined)){
            m2vr::LogEvent("model2_d3d9_cpu_vb_join_batch",id,
                (std::uint64_t{joined.inspected}<<32)|joined.complete,
                (std::uint64_t{joined.partial}<<32)|joined.ambiguous,
                joined.common_source_caller_rva);
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
}

HRESULT STDMETHODCALLTYPE DrawHook(IDirect3DDevice9* d,D3DPRIMITIVETYPE p,UINT start,UINT count){
    if(!g_primary_device.Owns(d))return g_draw(d,p,start,count);
    g_draws.fetch_add(1,std::memory_order_relaxed);
    void* caller=_ReturnAddress();
    if(Sample(d,1,p,count,caller))
        ObserveBufferSource(d,1,p,caller,start,0,0,0,0,count);
    const HRESULT hr=g_draw(d,p,start,count);
    if(SUCCEEDED(hr) && g_native_stereo)
        m2vr::daytona::MirrorPrimitive(d,p,start,count,g_draw);
    return hr;
}
HRESULT STDMETHODCALLTYPE IndexedHook(IDirect3DDevice9* d,D3DPRIMITIVETYPE p,INT base,
                                      UINT min,UINT num,UINT start,UINT count){
    if(!g_primary_device.Owns(d))return g_draw_indexed(d,p,base,min,num,start,count);
    g_indexed.fetch_add(1,std::memory_order_relaxed);
    void* caller=_ReturnAddress();
    if(Sample(d,2,p,count,caller))
        ObserveBufferSource(d,2,p,caller,0,base,min,num,start,count);
    const HRESULT hr=g_draw_indexed(d,p,base,min,num,start,count);
    if(SUCCEEDED(hr) && g_native_stereo)
        m2vr::daytona::MirrorIndexed(d,p,base,min,num,start,count,g_draw_indexed);
    return hr;
}
HRESULT STDMETHODCALLTYPE DrawUPHook(IDirect3DDevice9* d,D3DPRIMITIVETYPE p,UINT count,
                                    const void* verts,UINT stride){
    if(!g_primary_device.Owns(d))return g_draw_up(d,p,count,verts,stride);
    g_up.fetch_add(1,std::memory_order_relaxed);
    void* caller=_ReturnAddress();
    if(Sample(d,3,p,count,caller))
        ObserveUP(d,3,p,count,caller,verts,stride);
    const HRESULT hr=g_draw_up(d,p,count,verts,stride);
    if(SUCCEEDED(hr) && g_native_stereo)
        m2vr::daytona::MirrorDraw(d,p,count,verts,stride,CallerRva(caller),g_draw_up);
    return hr;
}
HRESULT STDMETHODCALLTYPE IndexedUPHook(IDirect3DDevice9* d,D3DPRIMITIVETYPE p,UINT min,
     UINT num,UINT count,const void* idx,D3DFORMAT fmt,const void* verts,UINT stride){
    if(!g_primary_device.Owns(d))return g_draw_indexed_up(d,p,min,num,count,idx,fmt,verts,stride);
    g_indexed_up.fetch_add(1,std::memory_order_relaxed);
    void* caller=_ReturnAddress();
    if(Sample(d,4,p,count,caller))
        ObserveUP(d,4,p,count,caller,verts,stride,idx,fmt,min,num);
    const HRESULT hr=g_draw_indexed_up(d,p,min,num,count,idx,fmt,verts,stride);
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
    // Native replay must also drain the *original* source-family counters.
    // Previously these only reset in passive 2D mode, so a long VR session
    // accumulated lifetime totals instead of per-Present evidence.
    const unsigned dp=g_draws.exchange(0u,std::memory_order_relaxed);
    const unsigned dip=g_indexed.exchange(0u,std::memory_order_relaxed);
    const unsigned dpup=g_up.exchange(0u,std::memory_order_relaxed);
    const unsigned dipup=g_indexed_up.exchange(0u,std::memory_order_relaxed);
    if(g_native_stereo){
        m2vr::daytona::PrepareFrameBeforePresent(d,frame);
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
    m2vr::drawprobe::NextProjectDrawEpoch();
    if(g_native_stereo)
        m2vr::daytona::FinishFrameAfterPresent(frame,original_result);
    // A separately proven CPU-world source can publish only after this
    // original Present completes. GPU-replay mode retains sole ownership
    // of its per-frame writer; a conflicting CPU frame is dropped.
    Model2VR_CompleteVerifiedWorldOriginalPresent(
        frame,SUCCEEDED(original_result)?1u:0u,g_native_stereo?1u:0u);
    return original_result;
}
HRESULT STDMETHODCALLTYPE ResetHook(IDirect3DDevice9* d,
                                     D3DPRESENT_PARAMETERS* pp){
    if(!g_primary_device.Owns(d))return g_reset(d,pp);
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
// equals a real device Present. Never activate CPU-world submission when
// the separate GPU shader-UP replay already owns the transport.
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