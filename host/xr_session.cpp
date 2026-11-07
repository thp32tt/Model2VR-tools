#include "xr_app.h"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <iterator>
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
    menu_controller_.Shutdown();
    for(auto& c:chains_)if(c.handle!=XR_NULL_HANDLE)xrDestroySwapchain(c.handle);
    if(local_space_!=XR_NULL_HANDLE)xrDestroySpace(local_space_);
    if(session_!=XR_NULL_HANDLE)xrDestroySession(session_);
    if(instance_!=XR_NULL_HANDLE)xrDestroyInstance(instance_);
}
void XrApp::initialize(){
    LARGE_INTEGER host_start{};
    if(!QueryPerformanceCounter(&host_start) || host_start.QuadPart<=0)
        throw std::runtime_error("OpenXR source-generation QPC unavailable");
    host_source_epoch_qpc_=static_cast<std::uint64_t>(host_start.QuadPart);
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
    XrInstanceProperties instance_props{XR_TYPE_INSTANCE_PROPERTIES};
    check(xrGetInstanceProperties(instance_,&instance_props),"get OpenXR runtime properties");
    std::cout<<"OpenXR runtime="<<instance_props.runtimeName
             <<" version="<<XR_VERSION_MAJOR(instance_props.runtimeVersion)
             <<"."<<XR_VERSION_MINOR(instance_props.runtimeVersion)
             <<"."<<XR_VERSION_PATCH(instance_props.runtimeVersion)<<std::endl;
    XrSystemGetInfo system_info{XR_TYPE_SYSTEM_GET_INFO};
    system_info.formFactor=XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
    check(xrGetSystem(instance_,&system_info,&system_),"get HMD");
    XrSystemProperties system_props{XR_TYPE_SYSTEM_PROPERTIES};
    check(xrGetSystemProperties(instance_,system_,&system_props),"get HMD system properties");
    std::cout<<"OpenXR system="<<system_props.systemName
             <<" max_swapchain="<<system_props.graphicsProperties.maxSwapchainImageWidth
             <<"x"<<system_props.graphicsProperties.maxSwapchainImageHeight
             <<" position_tracking="<<(system_props.trackingProperties.positionTracking?1:0)
             <<" orientation_tracking="<<(system_props.trackingProperties.orientationTracking?1:0)
             <<std::endl;
    if(source_pid_ &&
       (system_props.graphicsProperties.maxSwapchainImageWidth<transport::kWidth ||
        system_props.graphicsProperties.maxSwapchainImageHeight<transport::kHeight))
        throw std::runtime_error("OpenXR runtime maximum swapchain size is below 1024x1024 transport");
    PFN_xrGetD3D11GraphicsRequirementsKHR graphics_requirements=nullptr;
    check(xrGetInstanceProcAddr(instance_,"xrGetD3D11GraphicsRequirementsKHR",
        reinterpret_cast<PFN_xrVoidFunction*>(&graphics_requirements)),"D3D11 entrypoint");
    if(!graphics_requirements)throw std::runtime_error("D3D11 requirements unavailable");
    XrGraphicsRequirementsD3D11KHR req{XR_TYPE_GRAPHICS_REQUIREMENTS_D3D11_KHR};
    check(graphics_requirements(instance_,system_,&req),"D3D11 requirements");
    std::cout<<"OpenXR D3D11 minimum_feature_level=0x"<<std::hex
             <<static_cast<unsigned>(req.minFeatureLevel)<<std::dec<<std::endl;
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
    DXGI_ADAPTER_DESC1 selected_desc{};
    checkhr(adapter->GetDesc1(&selected_desc),"get OpenXR GPU adapter description");
    std::wcout<<L"OpenXR GPU adapter="<<selected_desc.Description
              <<L" vendor=0x"<<std::hex<<selected_desc.VendorId
              <<L" device=0x"<<selected_desc.DeviceId<<std::dec<<std::endl;

    // D3D11 can expose FL12_x on modern Windows/GPU combinations. Ask from
    // highest to lowest while still accepting the runtime's minimum.
    constexpr D3D_FEATURE_LEVEL levels[]={
        D3D_FEATURE_LEVEL_12_1,D3D_FEATURE_LEVEL_12_0,
        D3D_FEATURE_LEVEL_11_1,D3D_FEATURE_LEVEL_11_0
    };
    D3D_FEATURE_LEVEL obtained{};
    HRESULT create_device=D3D11CreateDevice(adapter.Get(),D3D_DRIVER_TYPE_UNKNOWN,nullptr,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT,levels,
        static_cast<UINT>(std::size(levels)),D3D11_SDK_VERSION,
        &device_,&obtained,&context_);
    // Older D3D11 runtimes can reject newer enum values with E_INVALIDARG.
    if(create_device==E_INVALIDARG){
        constexpr D3D_FEATURE_LEVEL fallback_levels[]={
            D3D_FEATURE_LEVEL_11_1,D3D_FEATURE_LEVEL_11_0
        };
        create_device=D3D11CreateDevice(adapter.Get(),D3D_DRIVER_TYPE_UNKNOWN,nullptr,
            D3D11_CREATE_DEVICE_BGRA_SUPPORT,fallback_levels,
            static_cast<UINT>(std::size(fallback_levels)),D3D11_SDK_VERSION,
            &device_,&obtained,&context_);
    }
    checkhr(create_device,"create GPU device");
    if(obtained<req.minFeatureLevel)throw std::runtime_error("D3D11 feature level too low");
    UINT rgba_support=0;
    checkhr(device_->CheckFormatSupport(DXGI_FORMAT_R8G8B8A8_UNORM,&rgba_support),
            "check RGBA8 D3D11 format support");
    if((rgba_support&D3D11_FORMAT_SUPPORT_TEXTURE2D)==0 ||
       (rgba_support&D3D11_FORMAT_SUPPORT_RENDER_TARGET)==0)
        throw std::runtime_error("OpenXR GPU cannot use RGBA8 as a D3D11 render target");
    std::cout<<"D3D11 feature_level=0x"<<std::hex<<static_cast<unsigned>(obtained)
             <<" rgba8_support=0x"<<rgba_support<<std::dec<<std::endl;
    XrGraphicsBindingD3D11KHR binding{XR_TYPE_GRAPHICS_BINDING_D3D11_KHR};
    binding.device=device_.Get();
    XrSessionCreateInfo session_info{XR_TYPE_SESSION_CREATE_INFO};
    session_info.next=&binding;session_info.systemId=system_;
    check(xrCreateSession(instance_,&session_info,&session_),"create OpenXR session");
    XrReferenceSpaceCreateInfo space_info{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
    space_info.referenceSpaceType=XR_REFERENCE_SPACE_TYPE_LOCAL;
    space_info.poseInReferenceSpace.orientation.w=1.0f;
    check(xrCreateReferenceSpace(session_,&space_info,&local_space_),"create local space");
    if(!menu_controller_.Initialize(instance_,session_))
        std::cout<<"Model2VR XR emulator menu controller unavailable; original desktop mouse remains active\n";
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
