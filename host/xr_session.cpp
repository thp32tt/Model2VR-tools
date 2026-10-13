#include "xr_app.h"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
namespace m2vr::host {
void check(XrResult r,const char* op){
    if(XR_FAILED(r))throw std::runtime_error(std::string(op)+" XrResult="+std::to_string(r));
}
void checkhr(HRESULT r,const char* op){
    if(FAILED(r))throw std::runtime_error(std::string(op)+" HRESULT="+std::to_string(r));
}
XrApp::~XrApp(){
    for(auto& c:chains_)if(c.handle!=XR_NULL_HANDLE)xrDestroySwapchain(c.handle);
    if(local_space_!=XR_NULL_HANDLE)xrDestroySpace(local_space_);
    if(session_!=XR_NULL_HANDLE)xrDestroySession(session_);
    if(instance_!=XR_NULL_HANDLE)xrDestroyInstance(instance_);
}
void XrApp::initialize(){
    std::uint32_t count=0;
    check(xrEnumerateInstanceExtensionProperties(nullptr,0,&count,nullptr),"list extensions");
    std::vector<XrExtensionProperties> extensions(count,{XR_TYPE_EXTENSION_PROPERTIES});
    check(xrEnumerateInstanceExtensionProperties(nullptr,count,&count,extensions.data()),"get extensions");
    bool supports_d3d11=false;
    for(const auto& e:extensions)
        if(std::strcmp(e.extensionName,XR_KHR_D3D11_ENABLE_EXTENSION_NAME)==0)supports_d3d11=true;
    if(!supports_d3d11)throw std::runtime_error("OpenXR runtime missing D3D11 extension");
    const char* names[]={XR_KHR_D3D11_ENABLE_EXTENSION_NAME};
    XrInstanceCreateInfo create{XR_TYPE_INSTANCE_CREATE_INFO};
    std::strncpy(create.applicationInfo.applicationName,"Model2VR",XR_MAX_APPLICATION_NAME_SIZE-1);
    std::strncpy(create.applicationInfo.engineName,"Model2VR",XR_MAX_ENGINE_NAME_SIZE-1);
    create.applicationInfo.apiVersion=XR_CURRENT_API_VERSION;
    create.enabledExtensionCount=1;
    create.enabledExtensionNames=names;
    check(xrCreateInstance(&create,&instance_),"create OpenXR instance");
    XrSystemGetInfo system_info{XR_TYPE_SYSTEM_GET_INFO};
    system_info.formFactor=XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
    check(xrGetSystem(instance_,&system_info,&system_),"get HMD");
    PFN_xrGetD3D11GraphicsRequirementsKHR graphics_requirements=nullptr;
    check(xrGetInstanceProcAddr(instance_,"xrGetD3D11GraphicsRequirementsKHR",
        reinterpret_cast<PFN_xrVoidFunction*>(&graphics_requirements)),"D3D11 entrypoint");
    if(!graphics_requirements)throw std::runtime_error("D3D11 requirements unavailable");
    XrGraphicsRequirementsD3D11KHR req{XR_TYPE_GRAPHICS_REQUIREMENTS_D3D11_KHR};
    check(graphics_requirements(instance_,system_,&req),"D3D11 requirements");
    ComPtr<IDXGIFactory1> factory;
    checkhr(CreateDXGIFactory1(IID_PPV_ARGS(&factory)),"DXGI factory");
    ComPtr<IDXGIAdapter1> adapter;
    for(UINT i=0;;++i){
        ComPtr<IDXGIAdapter1> next;
        if(factory->EnumAdapters1(i,&next)==DXGI_ERROR_NOT_FOUND)break;
        DXGI_ADAPTER_DESC1 desc{};
        next->GetDesc1(&desc);
        if(std::memcmp(&desc.AdapterLuid,&req.adapterLuid,sizeof(LUID))==0){
            adapter=next;break;
        }
    }
    if(!adapter)throw std::runtime_error("OpenXR-required GPU adapter not found");
    constexpr D3D_FEATURE_LEVEL levels[]={D3D_FEATURE_LEVEL_11_1,D3D_FEATURE_LEVEL_11_0};
    D3D_FEATURE_LEVEL obtained{};
    checkhr(D3D11CreateDevice(adapter.Get(),D3D_DRIVER_TYPE_UNKNOWN,nullptr,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT,levels,2,D3D11_SDK_VERSION,
        &device_,&obtained,&context_),"create GPU device");
    if(obtained<req.minFeatureLevel)throw std::runtime_error("D3D11 feature level too low");
    XrGraphicsBindingD3D11KHR binding{XR_TYPE_GRAPHICS_BINDING_D3D11_KHR};
    binding.device=device_.Get();
    XrSessionCreateInfo session_info{XR_TYPE_SESSION_CREATE_INFO};
    session_info.next=&binding;session_info.systemId=system_;
    check(xrCreateSession(instance_,&session_info,&session_),"create OpenXR session");
    XrReferenceSpaceCreateInfo space_info{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
    space_info.referenceSpaceType=XR_REFERENCE_SPACE_TYPE_LOCAL;
    space_info.poseInReferenceSpace.orientation.w=1.0f;
    check(xrCreateReferenceSpace(session_,&space_info,&local_space_),"create local space");
    std::uint32_t n=0;
    check(xrEnumerateViewConfigurationViews(instance_,system_,
        XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,0,&n,nullptr),"view count");
    if(n!=2)throw std::runtime_error("OpenXR requires two stereo views");
    view_configs_.resize(2,{XR_TYPE_VIEW_CONFIGURATION_VIEW});
    check(xrEnumerateViewConfigurationViews(instance_,system_,
        XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,2,&n,view_configs_.data()),"stereo view config");
    create_swapchains();
    std::cout<<"Model2VR OpenXR session ready (HMD stereo test only)\n";
}
} // namespace m2vr::host
