#include <Windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cwctype>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#define XR_USE_PLATFORM_WIN32
#define XR_USE_GRAPHICS_API_D3D11
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
using Microsoft::WRL::ComPtr;
namespace {
void XrOk(XrResult r,const char* step) {
    if(XR_FAILED(r))throw std::runtime_error(std::string(step)+" OpenXR="+std::to_string(r));
}
void HrOk(HRESULT hr,const char* step) {
    if(FAILED(hr))throw std::runtime_error(std::string(step)+" HRESULT="+std::to_string(hr));
}
struct XrOwned {
    XrInstance instance=XR_NULL_HANDLE;
    XrSession session=XR_NULL_HANDLE;
    XrSpace space=XR_NULL_HANDLE;
    XrSwapchain swapchain=XR_NULL_HANDLE;
    ~XrOwned(){
        if(swapchain!=XR_NULL_HANDLE)xrDestroySwapchain(swapchain);
        if(space!=XR_NULL_HANDLE)xrDestroySpace(space);
        if(session!=XR_NULL_HANDLE)xrDestroySession(session);
        if(instance!=XR_NULL_HANDLE)xrDestroyInstance(instance);
    }
};
struct Capture {
    HDC screen=nullptr,dc=nullptr;
    HBITMAP image=nullptr;
    HGDIOBJ old=nullptr;
    void* pixels=nullptr;
    int w=1280,h=800;
    Capture(){
        screen=GetDC(nullptr);
        if(!screen)throw std::runtime_error("desktop DC missing");
        dc=CreateCompatibleDC(screen);
        if(!dc)throw std::runtime_error("compatible DC missing");
        BITMAPINFO info{};
        info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth=w;
        info.bmiHeader.biHeight=-h;
        info.bmiHeader.biPlanes=1;
        info.bmiHeader.biBitCount=32;
        info.bmiHeader.biCompression=BI_RGB;
        image=CreateDIBSection(screen,&info,DIB_RGB_COLORS,&pixels,nullptr,0);
        if(!image||!pixels)throw std::runtime_error("BGRA DIB allocation failed");
        old=SelectObject(dc,image);
    }
    ~Capture(){
        if(dc&&old)SelectObject(dc,old);
        if(image)DeleteObject(image);
        if(dc)DeleteDC(dc);
        if(screen)ReleaseDC(nullptr,screen);
    }
    bool Grab(HWND hwnd)const{
        if(!IsWindow(hwnd)||IsIconic(hwnd)||!IsWindowVisible(hwnd))return false;
        RECT rc{};
        if(!GetClientRect(hwnd,&rc))return false;
        const int sw=rc.right-rc.left,sh=rc.bottom-rc.top;
        if(sw<1||sh<1)return false;
        POINT pt{0,0};
        if(!ClientToScreen(hwnd,&pt))return false;
        RECT full{0,0,w,h};
        FillRect(dc,&full,static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
        const double scale=std::min(double(w)/sw,double(h)/sh);
        const int dw=std::max(1,int(sw*scale)),dh=std::max(1,int(sh*scale));
        SetStretchBltMode(dc,COLORONCOLOR);
        return StretchBlt(dc,(w-dw)/2,(h-dh)/2,dw,dh,
                          screen,pt.x,pt.y,sw,sh,SRCCOPY)!=FALSE;
    }
};
struct Search { std::wstring fragment; HWND result=nullptr; };
BOOL CALLBACK FindWindowCallback(HWND hwnd,LPARAM userdata){
    auto& search=*reinterpret_cast<Search*>(userdata);
    if(!IsWindowVisible(hwnd)||GetWindow(hwnd,GW_OWNER))return TRUE;
    wchar_t name[512]{};
    GetWindowTextW(hwnd,name,512);
    std::wstring haystack(name);
    std::transform(haystack.begin(),haystack.end(),haystack.begin(),::towlower);
    if(haystack.find(search.fragment)!=std::wstring::npos){
        search.result=hwnd;
        return FALSE;
    }
    return TRUE;
}
HWND FindEmulator(std::wstring keyword){
    std::transform(keyword.begin(),keyword.end(),keyword.begin(),::towlower);
    Search s{keyword,nullptr};
    EnumWindows(FindWindowCallback,reinterpret_cast<LPARAM>(&s));
    return s.result;
}
ComPtr<IDXGIAdapter1> FindRuntimeAdapter(LUID luid){
    ComPtr<IDXGIFactory1> factory;
    HrOk(CreateDXGIFactory1(IID_PPV_ARGS(&factory)),"CreateDXGIFactory1");
    for(UINT index=0;;++index){
        ComPtr<IDXGIAdapter1> adapter;
        if(factory->EnumAdapters1(index,&adapter)==DXGI_ERROR_NOT_FOUND)break;
        DXGI_ADAPTER_DESC1 desc{};
        adapter->GetDesc1(&desc);
        if(std::memcmp(&desc.AdapterLuid,&luid,sizeof(LUID))==0)return adapter;
    }
    throw std::runtime_error("No D3D11 GPU matches OpenXR runtime adapter LUID");
}
int Run(std::wstring window_title){
    HWND hwnd=FindEmulator(std::move(window_title));
    if(!hwnd)throw std::runtime_error("Model 2 window not found (start emulator first)");
    XrOwned xr;
    const char* ext[]={XR_KHR_D3D11_ENABLE_EXTENSION_NAME};
    XrInstanceCreateInfo create{XR_TYPE_INSTANCE_CREATE_INFO};
    strcpy_s(create.applicationInfo.applicationName,"Model2VR");
    strcpy_s(create.applicationInfo.engineName,"Model2VRHost");
    create.applicationInfo.applicationVersion=1;
    create.applicationInfo.engineVersion=1;
    create.applicationInfo.apiVersion=XR_CURRENT_API_VERSION;
    create.enabledExtensionNames=ext;
    create.enabledExtensionCount=1;
    XrOk(xrCreateInstance(&create,&xr.instance),"xrCreateInstance");
    XrSystemGetInfo sysInfo{XR_TYPE_SYSTEM_GET_INFO};
    sysInfo.formFactor=XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
    XrSystemId system=XR_NULL_SYSTEM_ID;
    XrOk(xrGetSystem(xr.instance,&sysInfo,&system),"xrGetSystem");
    PFN_xrGetD3D11GraphicsRequirementsKHR graphics=nullptr;
    XrOk(xrGetInstanceProcAddr(xr.instance,"xrGetD3D11GraphicsRequirementsKHR",
         reinterpret_cast<PFN_xrVoidFunction*>(&graphics)),"xrGetInstanceProcAddr");
    if(!graphics)throw std::runtime_error("OpenXR D3D11 graphics requirements unavailable");
    XrGraphicsRequirementsD3D11KHR req{XR_TYPE_GRAPHICS_REQUIREMENTS_D3D11_KHR};
    XrOk(graphics(xr.instance,system,&req),"xrGetD3D11GraphicsRequirementsKHR");
    auto adapter=FindRuntimeAdapter(req.adapterLuid);
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    D3D_FEATURE_LEVEL feature{};
    D3D_FEATURE_LEVEL levels[]={D3D_FEATURE_LEVEL_11_1,D3D_FEATURE_LEVEL_11_0};
    HrOk(D3D11CreateDevice(adapter.Get(),D3D_DRIVER_TYPE_UNKNOWN,nullptr,
         D3D11_CREATE_DEVICE_BGRA_SUPPORT,levels,2,D3D11_SDK_VERSION,
         &device,&feature,&context),"D3D11CreateDevice");
    if(feature<req.minFeatureLevel)throw std::runtime_error("Insufficient D3D11 feature level");
    XrGraphicsBindingD3D11KHR binding{XR_TYPE_GRAPHICS_BINDING_D3D11_KHR};
    binding.device=device.Get();
    XrSessionCreateInfo sessionInfo{XR_TYPE_SESSION_CREATE_INFO};
    sessionInfo.next=&binding;
    sessionInfo.systemId=system;
    XrOk(xrCreateSession(xr.instance,&sessionInfo,&xr.session),"xrCreateSession");
    XrReferenceSpaceCreateInfo spaceInfo{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
    spaceInfo.referenceSpaceType=XR_REFERENCE_SPACE_TYPE_LOCAL;
    spaceInfo.poseInReferenceSpace.orientation.w=1.f;
    XrOk(xrCreateReferenceSpace(xr.session,&spaceInfo,&xr.space),"xrCreateReferenceSpace");
    uint32_t formatCount=0;
    XrOk(xrEnumerateSwapchainFormats(xr.session,0,&formatCount,nullptr),"swapchain format count");
    std::vector<int64_t> formats(formatCount);
    XrOk(xrEnumerateSwapchainFormats(xr.session,formatCount,&formatCount,formats.data()),"swapchain formats");
    const auto chosen=static_cast<int64_t>(DXGI_FORMAT_B8G8R8A8_UNORM);
    if(std::find(formats.begin(),formats.end(),chosen)==formats.end())
        throw std::runtime_error("Runtime does not support BGRA8 quad-layer swapchain");
    Capture capture;
    XrSwapchainCreateInfo sc{XR_TYPE_SWAPCHAIN_CREATE_INFO};
    sc.usageFlags=XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
    sc.format=chosen;sc.sampleCount=1;sc.width=capture.w;sc.height=capture.h;
    sc.faceCount=1;sc.arraySize=1;sc.mipCount=1;
    XrOk(xrCreateSwapchain(xr.session,&sc,&xr.swapchain),"xrCreateSwapchain");
    uint32_t imageCount=0;
    XrOk(xrEnumerateSwapchainImages(xr.swapchain,0,&imageCount,nullptr),"image count");
    std::vector<XrSwapchainImageD3D11KHR> images(imageCount);
    for(auto& image:images)image.type=XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR;
    XrOk(xrEnumerateSwapchainImages(xr.swapchain,imageCount,&imageCount,
       reinterpret_cast<XrSwapchainImageBaseHeader*>(images.data())),"images");
    uint32_t blendCount=0;
    XrOk(xrEnumerateEnvironmentBlendModes(xr.instance,system,
       XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,0,&blendCount,nullptr),"blend count");
    std::vector<XrEnvironmentBlendMode> blends(blendCount);
    XrOk(xrEnumerateEnvironmentBlendModes(xr.instance,system,
       XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,blendCount,&blendCount,blends.data()),"blends");
    if(std::find(blends.begin(),blends.end(),XR_ENVIRONMENT_BLEND_MODE_OPAQUE)==blends.end())
        throw std::runtime_error("Opaque OpenXR environment blend mode unavailable");
    std::cout<<"Model2VR OpenXR quad-screen mode active (not native stereo).\n";
    bool alive=true,running=false;
    while(alive){
        XrEventDataBuffer event{XR_TYPE_EVENT_DATA_BUFFER};
        while(xrPollEvent(xr.instance,&event)==XR_SUCCESS){
            if(event.type==XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED){
                auto s=*reinterpret_cast<const XrEventDataSessionStateChanged*>(&event);
                if(s.state==XR_SESSION_STATE_READY && !running){
                    XrSessionBeginInfo begin{XR_TYPE_SESSION_BEGIN_INFO};
                    begin.primaryViewConfigurationType=XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
                    XrOk(xrBeginSession(xr.session,&begin),"xrBeginSession");
                    running=true;
                }else if(s.state==XR_SESSION_STATE_STOPPING && running){
                    XrOk(xrEndSession(xr.session),"xrEndSession");running=false;
                }else if(s.state==XR_SESSION_STATE_EXITING||
                          s.state==XR_SESSION_STATE_LOSS_PENDING)alive=false;
            }else if(event.type==XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING)alive=false;
            event={XR_TYPE_EVENT_DATA_BUFFER};
        }
        if(!alive||!IsWindow(hwnd))break;
        if(!running){Sleep(25);continue;}
        XrFrameWaitInfo wait{XR_TYPE_FRAME_WAIT_INFO};
        XrFrameState state{XR_TYPE_FRAME_STATE};
        XrOk(xrWaitFrame(xr.session,&wait,&state),"xrWaitFrame");
        XrFrameBeginInfo begin{XR_TYPE_FRAME_BEGIN_INFO};
        XrOk(xrBeginFrame(xr.session,&begin),"xrBeginFrame");
        XrCompositionLayerQuad quad{XR_TYPE_COMPOSITION_LAYER_QUAD};
        const XrCompositionLayerBaseHeader* layer=nullptr;
        uint32_t layerCount=0;
        if(state.shouldRender && capture.Grab(hwnd)){
            XrSwapchainImageAcquireInfo acq{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
            uint32_t index=0;
            XrOk(xrAcquireSwapchainImage(xr.swapchain,&acq,&index),"xrAcquireSwapchainImage");
            XrSwapchainImageWaitInfo ready{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
            ready.timeout=XR_INFINITE_DURATION;
            XrOk(xrWaitSwapchainImage(xr.swapchain,&ready),"xrWaitSwapchainImage");
            if(index>=images.size())throw std::runtime_error("Swapchain image index invalid");
            context->UpdateSubresource(images[index].texture,nullptr,nullptr,
                                       capture.pixels,capture.w*4,0);
            XrSwapchainImageReleaseInfo rel{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
            XrOk(xrReleaseSwapchainImage(xr.swapchain,&rel),"xrReleaseSwapchainImage");
            quad.space=xr.space;
            quad.eyeVisibility=XR_EYE_VISIBILITY_BOTH;
            quad.pose.orientation.w=1.f;
            quad.pose.position={0.f,0.f,-1.65f};
            quad.size={2.2f,2.2f*capture.h/capture.w};
            quad.subImage.swapchain=xr.swapchain;
            quad.subImage.imageRect.offset={0,0};
            quad.subImage.imageRect.extent={capture.w,capture.h};
            layer=reinterpret_cast<const XrCompositionLayerBaseHeader*>(&quad);
            layerCount=1;
        }
        XrFrameEndInfo end{XR_TYPE_FRAME_END_INFO};
        end.displayTime=state.predictedDisplayTime;
        end.environmentBlendMode=XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
        end.layerCount=layerCount;
        end.layers=layerCount?&layer:nullptr;
        XrOk(xrEndFrame(xr.session,&end),"xrEndFrame");
    }
    if(running)xrEndSession(xr.session);
    return 0;
}
}
int wmain(int argc,wchar_t** argv){
    try{return Run(argc>1?argv[1]:L"Model 2");}
    catch(const std::exception& e){
        std::cerr<<"Model2VR OpenXR host: "<<e.what()<<"\n";
        return 1;
    }
}
