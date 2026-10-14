#include "logger.h"
#include <Windows.h>
#include <d3d9.h>
#include <MinHook.h>
#include <intrin.h>
#include <atomic>
#include <cstdint>
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
PrimitiveFn g_draw=nullptr;
IndexedFn g_draw_indexed=nullptr;
PrimitiveUPFn g_draw_up=nullptr;
IndexedUPFn g_draw_indexed_up=nullptr;
std::mutex g_setup_mutex;
std::uintptr_t g_exe_base=0;
std::uint32_t g_exe_size=0;
std::atomic<unsigned> g_draws{0},g_indexed{0},g_up{0},g_indexed_up{0};
std::atomic<unsigned> g_samples{0};
std::atomic<unsigned> g_present_count{0};
bool g_device_installed=false;

template<class T>
bool Hook(void* target,void* detour,T& original){
    if(!target)return false;
    const MH_STATUS status=MH_CreateHook(target,detour,
        reinterpret_cast<LPVOID*>(&original));
    if(status!=MH_OK)return false;
    return MH_EnableHook(target)==MH_OK;
}
std::uint32_t CallerRva(void* ret) {
    const auto raw=reinterpret_cast<std::uintptr_t>(ret);
    if(raw<g_exe_base || raw-g_exe_base>=g_exe_size)return 0;
    return static_cast<std::uint32_t>(raw-g_exe_base);
}
void Sample(IDirect3DDevice9* dev,unsigned family,void* caller){
    // The hot path only pays the sample counter, not a COM render-state roundtrip.
    if((g_samples.fetch_add(1,std::memory_order_relaxed)&0x3ffu)!=0)return;
    DWORD fvf=0,z=0,alpha=0;
    const bool ok=SUCCEEDED(dev->GetFVF(&fvf)) &&
                  SUCCEEDED(dev->GetRenderState(D3DRS_ZENABLE,&z)) &&
                  SUCCEEDED(dev->GetRenderState(D3DRS_ALPHABLENDENABLE,&alpha));
    if(!ok){m2vr::LogEvent("model2_d3d9_sample_failed",family);return;}
    // A sample shows whether pretransformed (XYZRHW) data dominates.
    // It is NEVER sufficient by itself to classify world geometry.
    const std::uint32_t packed=(z?1u:0u)|(alpha?2u:0u)|
        ((fvf&D3DFVF_POSITION_MASK)==D3DFVF_XYZRHW?4u:0u);
    m2vr::LogEvent("model2_d3d9_draw_sample",family,fvf,packed,CallerRva(caller));
}
HRESULT STDMETHODCALLTYPE DrawHook(IDirect3DDevice9* d,D3DPRIMITIVETYPE p,UINT start,UINT count){
    g_draws.fetch_add(1,std::memory_order_relaxed);
    Sample(d,1,_ReturnAddress());
    return g_draw(d,p,start,count);
}
HRESULT STDMETHODCALLTYPE IndexedHook(IDirect3DDevice9* d,D3DPRIMITIVETYPE p,INT base,
                                      UINT min,UINT num,UINT start,UINT count){
    g_indexed.fetch_add(1,std::memory_order_relaxed);
    Sample(d,2,_ReturnAddress());
    return g_draw_indexed(d,p,base,min,num,start,count);
}
HRESULT STDMETHODCALLTYPE DrawUPHook(IDirect3DDevice9* d,D3DPRIMITIVETYPE p,UINT count,
                                    const void* verts,UINT stride){
    g_up.fetch_add(1,std::memory_order_relaxed);
    Sample(d,3,_ReturnAddress());
    return g_draw_up(d,p,count,verts,stride);
}
HRESULT STDMETHODCALLTYPE IndexedUPHook(IDirect3DDevice9* d,D3DPRIMITIVETYPE p,UINT min,
     UINT num,UINT count,const void* idx,D3DFORMAT fmt,const void* verts,UINT stride){
    g_indexed_up.fetch_add(1,std::memory_order_relaxed);
    Sample(d,4,_ReturnAddress());
    return g_draw_indexed_up(d,p,min,num,count,idx,fmt,verts,stride);
}
HRESULT STDMETHODCALLTYPE PresentHook(IDirect3DDevice9* d,CONST RECT* a,CONST RECT* b,
                                     HWND w,CONST RGNDATA* dirty){
    const unsigned frame=g_present_count.fetch_add(1,std::memory_order_relaxed)+1;
    m2vr::LogEvent("model2_d3d9_frame",frame,
        g_draws.exchange(0)+g_indexed.exchange(0),
        g_up.exchange(0)+g_indexed_up.exchange(0));
    // Return the original Present result unchanged; no game GPU replay.
    return g_present(d,a,b,w,dirty);
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
    if(!(p&&dp&&di&&du&&iu)){
        // Never accept a partial installation as "native stereo ready".
        m2vr::LogEvent("model2_d3d9_partial_hook",p,dp,di,du);
        return false;
    }
    g_device_installed=true;
    m2vr::LogEvent("model2_d3d9_device_hooks_ready");
    return true;
}
HRESULT STDMETHODCALLTYPE CreateDeviceHook(IDirect3D9* api,UINT adapter,D3DDEVTYPE type,
    HWND hwnd,DWORD flags,D3DPRESENT_PARAMETERS* pp,IDirect3DDevice9** out){
    const HRESULT hr=g_create_device(api,adapter,type,hwnd,flags,pp,out);
    if(SUCCEEDED(hr)&&out&&*out){
        m2vr::LogEvent("model2_d3d9_device_created",
            pp?pp->BackBufferWidth:0,pp?pp->BackBufferHeight:0,flags);
        HookDevice(*out);
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

namespace m2vr {
bool InstallD3D9ReadOnlyProbe(HMODULE executable,std::uint32_t exe_size) {
    if(!executable || exe_size==0)return false;
    g_exe_base=reinterpret_cast<std::uintptr_t>(executable);
    g_exe_size=exe_size;
    const HMODULE d3d9=GetModuleHandleW(L"d3d9.dll");
    if(!d3d9){LogEvent("model2_d3d9_module_missing");return false;}
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
