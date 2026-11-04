// Experimental native Daytona GPU eye replay. The game executes only its
// original mono Draw once; this module renders original GPU commands into
// independent left/right D3D9 surfaces, shifting only the verified shader-UP
// scene caller. Unsupported sources remain flat/HUD-safe in both eyes.
// Source-space shift is not a calibrated physical IPD or head rotation.
#include "daytona_native_stereo.hpp"
#include "daytona_up_decl_witness.hpp"
#include "daytona_eye_pose_gate.hpp"
#include "../common/stereo_frame_ipc.h"
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
extern "C" BOOL __stdcall Model2VR_SubmitStereoRGBA(
    const unsigned char*,const unsigned char*,unsigned long long,
    unsigned int,unsigned int,unsigned int);

namespace {
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
        Reset();
    }
    void ResetExternal()noexcept{
        std::lock_guard lock(mu_);
        Reset();
    }
    void Draw(IDirect3DDevice9* d,D3DPRIMITIVETYPE primitive,UINT count,
              const void* source,UINT stride,std::uint32_t caller,
              m2vr::daytona::OriginalDrawUP original) {
        if(!enabled_||!d||!source||!original||count==0||count>512||
           stride<16u||stride>256u)return;
        std::lock_guard lock(mu_);
        // The OpenXR host supplies a coherent, fresh two-eye tracking pair.
        // Never allocate GPU resources, readback, or re-render when the Quest
        // host is absent, disconnected, stale or returning identical eyes.
        // Latch once per emulator frame, not every game Draw.
        if(!LivePoseForGameFrame() || !Ensure(d))return;
        IDirect3DSurface9* target=nullptr;
        if(FAILED(d->GetRenderTarget(0,&target))||!target)return;
        const bool backbuffer=(target==backbuffer_);
        if(!backbuffer){Drop(target);return;}
        // Original MRT passes require a separate, fully state-captured
        // renderer. Keep their Draw calls strictly on the original target.
        D3DCAPS9 caps{};
        if(FAILED(d->GetDeviceCaps(&caps)) ||
           caps.NumSimultaneousRTs<1u || caps.NumSimultaneousRTs>4u) {
            Drop(target);return;
        }
        for(DWORD slot=1u;slot<caps.NumSimultaneousRTs;++slot){
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
        if(FAILED(d->GetDepthStencilSurface(&original_depth)) ||
           !original_depth){Drop(target);return;}
        // A camera candidate requires an exact independently observed
        // source callsite and complete original shader-up declaration.
        bool scene=false;
        const std::size_t bytes=std::size_t{count}*3u*stride;
        if(caller==0x000BABADu && primitive==D3DPT_TRIANGLELIST &&
           stride==32u && bytes<=49152u) {
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
        }
        if(scene) {
            if(scratch_[0].size()<bytes)scratch_[0].resize(bytes);
            if(scratch_[1].size()<bytes)scratch_[1].resize(bytes);
            scene=GuardedCopy(scratch_[0].data(),source,bytes);
            if(scene) {
                std::memcpy(scratch_[1].data(),scratch_[0].data(),bytes);
                for(std::size_t offset=0;offset<bytes;offset+=32u) {
                    float x=0.f,w=0.f;
                    std::memcpy(&x,scratch_[0].data()+offset,sizeof x);
                    std::memcpy(&w,scratch_[0].data()+offset+12u,sizeof w);
                    // Real Daytona v4 traces include negative and mixed W
                    // triangles. Shifting their clip-like X can reverse
                    // depth/disparity, so keep the original flat source.
                    if(!std::isfinite(x) ||
                       !m2vr::daytona::SafeStereoHomogeneousW(w)){
                        scene=false;break;
                    }
                    const float eye_offset=shift_*ipd_scale_;
                    const float left=x+eye_offset,right=x-eye_offset;
                    if(!std::isfinite(left)||!std::isfinite(right)){
                        scene=false;break;
                    }
                    std::memcpy(scratch_[0].data()+offset,&left,sizeof left);
                    std::memcpy(scratch_[1].data()+offset,&right,sizeof right);
                }
            }
        }
        const D3DVIEWPORT9 viewport=ClippedViewport(original_vp);
        bool both=true;
        for(unsigned eye=0;eye<2;++eye){
            // SetDepthStencilSurface(nullptr) first avoids surface size/MSAA
            // mismatch while switching the original render-target.
            if(FAILED(d->SetDepthStencilSurface(nullptr)) ||
               FAILED(d->SetRenderTarget(0,eyes_[eye].color)) ||
               FAILED(d->SetDepthStencilSurface(eyes_[eye].depth)) ||
               FAILED(d->SetViewport(&viewport))) {
                both=false;break;
            }
            if(!frame_started_) {
                if(FAILED(d->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,
                                   D3DCOLOR_XRGB(0,0,0),1.f,0))){
                    both=false;break;
                }
            }
            const void* vertices=scene?scratch_[eye].data():source;
            if(FAILED(original(d,primitive,count,vertices,stride)))
                both=false;
        }
        // Restore original game's RT, DS and exact viewport even if one
        // stereo eye fails. Never suppress its original successful draw.
        d->SetDepthStencilSurface(nullptr);
        d->SetRenderTarget(0,target);
        d->SetDepthStencilSurface(original_depth);
        d->SetViewport(&original_vp);
        Drop(original_depth);Drop(target);
        if(!both){Reset();return;}
        frame_started_=true;
        if(scene)++scene_draws_;
    }
    void Present(IDirect3DDevice9* d,std::uint64_t frame){
        if(!enabled_||!d)return;
        std::lock_guard lock(mu_);
        if(!frame_started_ || !scene_draws_ || !frame || d!=device_ ||
           !pose_admitted_ || !LivePoseNow()){
            FinishFrame();return;
        }
        bool ready=true;
        for(unsigned eye=0;eye<2;++eye){
            IDirect3DSurface9* readback_source=eyes_[eye].color;
            if(eyes_[eye].resolve){
                // GetRenderTargetData rejects multisampled render targets.
                // Resolve the complete eye RT first, outside the game's
                // BeginScene/EndScene pair at Present time.
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
            if(FAILED(eyes_[eye].staging->LockRect(&locked,nullptr,D3DLOCK_READONLY))){
                ready=false;break;
            }
            ConvertBGRA(locked,rgba_[eye]);
            eyes_[eye].staging->UnlockRect();
        }
        // Do not advertise a pair as real stereo when the original shader
        // projection quantizes the tiny X-shift to identical eye pixels.
        if(ready && rgba_[0]==rgba_[1])ready=false;
        if(ready){
            if(!writer_ready_)writer_ready_=Model2VR_OpenStereoWriter()!=FALSE;
            if(writer_ready_)Model2VR_SubmitStereoRGBA(
                rgba_[0].data(),rgba_[1].data(),frame,
                m2vr::transport::kWidth,m2vr::transport::kHeight,
                m2vr::transport::kPitch);
        }
        FinishFrame();
    }
private:
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
        m2vr::pose::Frame frame{};
        if(!Model2VR_ReadPredictedEyeViews(&frame))return false;
        LARGE_INTEGER ticks{},frequency{};
        if(!QueryPerformanceCounter(&ticks) ||
           !QueryPerformanceFrequency(&frequency) ||
           ticks.QuadPart<=0 || frequency.QuadPart<=0)return false;
        const auto admitted=m2vr::daytona::AdmitLiveEyePose(
            frame,static_cast<std::uint64_t>(ticks.QuadPart),
            static_cast<std::uint64_t>(frequency.QuadPart));
        pose_admitted_=admitted.valid;
        if(admitted.valid)ipd_scale_=admitted.ipd_scale;
        return pose_admitted_;
    }
    void FinishFrame()noexcept{
        frame_started_=false;scene_draws_=0;
        pose_checked_=false;pose_admitted_=false;
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
        IDirect3DSurface9* depth=nullptr;
        if(FAILED(d->GetDepthStencilSurface(&depth)) || !depth) {
            Drop(back);return false;
        }
        D3DSURFACE_DESC dd{};
        // GetDepthStencilSurface already proves this is the original game's
        // active depth/stencil surface. Do not reject valid D3D9 depth formats
        // (for example D15S1, D24X4S4, D32F_LOCKABLE or D24FS8) with a partial
        // local whitelist. Clone the exact format and let CreateDepthStencilSurface
        // perform the authoritative device/format compatibility check.
        if(FAILED(depth->GetDesc(&dd)) ||
           dd.Width!=desc.Width || dd.Height!=desc.Height ||
           dd.MultiSampleType!=desc.MultiSampleType ||
           dd.MultiSampleQuality!=desc.MultiSampleQuality){
            Drop(depth);Drop(back);return false;
        }
        const D3DFORMAT depth_format=dd.Format;
        Drop(depth);
        backbuffer_=back;device_=d;
        width_=desc.Width;height_=desc.Height;
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
    void ConvertBGRA(const D3DLOCKED_RECT& source,
                     std::vector<std::uint8_t>& output)noexcept{
        const int side=static_cast<int>(m2vr::transport::kWidth);
        std::fill(output.begin(),output.end(),std::uint8_t{0});
        // Preserve Model 2's physical 660:384 image aspect; do not stretch
        // the race view to a square. Black letterbox is flat in both eyes.
        const double scale=(std::min)(double(side)/width_,double(side)/height_);
        const int dw=(std::max)(1,int(width_*scale));
        const int dh=(std::max)(1,int(height_*scale));
        const int ox=(side-dw)/2,oy=(side-dh)/2;
        for(int y=0;y<dh;++y){
            const auto sy=(std::min)(height_-1u,
                static_cast<UINT>(double(y)*height_/dh));
            const auto* src=static_cast<const std::uint8_t*>(source.pBits)+
                            std::size_t(sy)*source.Pitch;
            auto* dst=output.data()+(std::size_t(oy+y)*side+ox)*4u;
            for(int x=0;x<dw;++x){
                const auto sx=(std::min)(width_-1u,
                    static_cast<UINT>(double(x)*width_/dw));
                const auto* bgra=src+std::size_t(sx)*4u;
                dst[x*4+0]=bgra[2];dst[x*4+1]=bgra[1];
                dst[x*4+2]=bgra[0];dst[x*4+3]=255u;
            }
        }
    }
    void Reset()noexcept{
        for(auto& eye:eyes_)eye.Clear();
        Drop(backbuffer_);
        device_=nullptr;width_=height_=0u;
        FinishFrame();
    }
    std::mutex mu_;
    bool enabled_=false,frame_started_=false,writer_ready_=false;
    bool pose_checked_=false,pose_admitted_=false;
    float ipd_scale_=1.f;
    UINT width_=0,height_=0;
    float shift_=0.10f;
    unsigned scene_draws_=0;
    IDirect3DDevice9* device_=nullptr; // non-owning, Reset hook releases
    IDirect3DSurface9* backbuffer_=nullptr;
    std::array<Surface,2> eyes_{};
    std::array<std::vector<std::uint8_t>,2> scratch_{};
    std::array<std::vector<std::uint8_t>,2> rgba_{};
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
void PublishFrame(IDirect3DDevice9* d,std::uint64_t frame)noexcept{
    try {g_eye.Present(d,frame);}
    catch(...){g_eye.ResetExternal();}
}
void ResetResources()noexcept{g_eye.ResetExternal();}
}
