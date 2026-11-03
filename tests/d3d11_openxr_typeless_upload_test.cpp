// Headless Windows D3D11 contract for the OpenXR/VDXR upload path.
// VDXR/OpenXR D3D11 swapchain resources may be TYPELESS while the app
// creates a typed RTV. Prove that the exact upload strategy used by the
// host works without an HMD: TYPELESS resource -> UNORM RTV ->
// UpdateSubresource -> staging readback.
#include <Windows.h>
#include <d3d11.h>
#include <wrl/client.h>

#include <array>
#include <cstdint>
#include <cstdio>

using Microsoft::WRL::ComPtr;

#define REQUIRE_HR(expr) do { \
    const HRESULT _hr=(expr); \
    if(FAILED(_hr)){std::fprintf(stderr,"%s failed hr=0x%08lx at %d\n",#expr,static_cast<unsigned long>(_hr),__LINE__);return 1;} \
} while(0)
#define REQUIRE(expr) do { \
    if(!(expr)){std::fprintf(stderr,"%s failed at %d\n",#expr,__LINE__);return 1;} \
} while(0)

int main(){
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    D3D_FEATURE_LEVEL obtained{};
    const D3D_FEATURE_LEVEL levels[]={
        D3D_FEATURE_LEVEL_11_1,D3D_FEATURE_LEVEL_11_0
    };
    HRESULT hr=D3D11CreateDevice(
        nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,
        levels,2,D3D11_SDK_VERSION,&device,&obtained,&context);
    if(hr==E_INVALIDARG){
        REQUIRE_HR(D3D11CreateDevice(
            nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,
            &levels[1],1,D3D11_SDK_VERSION,&device,&obtained,&context));
    }else{
        REQUIRE_HR(hr);
    }

    D3D11_TEXTURE2D_DESC desc{};
    desc.Width=4;
    desc.Height=4;
    desc.MipLevels=1;
    desc.ArraySize=1;
    desc.Format=DXGI_FORMAT_R8G8B8A8_TYPELESS;
    desc.SampleDesc.Count=1;
    desc.Usage=D3D11_USAGE_DEFAULT;
    desc.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;

    ComPtr<ID3D11Texture2D> texture;
    REQUIRE_HR(device->CreateTexture2D(&desc,nullptr,&texture));

    D3D11_RENDER_TARGET_VIEW_DESC rtv_desc{};
    rtv_desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
    rtv_desc.ViewDimension=D3D11_RTV_DIMENSION_TEXTURE2D;
    rtv_desc.Texture2D.MipSlice=0;
    ComPtr<ID3D11RenderTargetView> rtv;
    REQUIRE_HR(device->CreateRenderTargetView(texture.Get(),&rtv_desc,&rtv));
    REQUIRE(rtv!=nullptr);

    std::array<std::uint8_t,4*4*4> pixels{};
    for(std::size_t i=0;i<pixels.size()/4;++i){
        pixels[i*4+0]=static_cast<std::uint8_t>(0x10u+i);
        pixels[i*4+1]=static_cast<std::uint8_t>(0x40u+i);
        pixels[i*4+2]=static_cast<std::uint8_t>(0x80u+i);
        pixels[i*4+3]=0xffu;
    }
    context->UpdateSubresource(texture.Get(),0,nullptr,pixels.data(),4u*4u,0);

    D3D11_TEXTURE2D_DESC staging_desc=desc;
    staging_desc.Usage=D3D11_USAGE_STAGING;
    staging_desc.BindFlags=0;
    staging_desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    ComPtr<ID3D11Texture2D> staging;
    REQUIRE_HR(device->CreateTexture2D(&staging_desc,nullptr,&staging));
    context->CopyResource(staging.Get(),texture.Get());

    D3D11_MAPPED_SUBRESOURCE mapped{};
    REQUIRE_HR(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&mapped));
    bool same=true;
    for(unsigned y=0;y<4 && same;++y){
        const auto* row=static_cast<const std::uint8_t*>(mapped.pData)+
                        static_cast<std::size_t>(y)*mapped.RowPitch;
        for(unsigned x=0;x<4*4;++x){
            if(row[x]!=pixels[static_cast<std::size_t>(y)*16u+x]){
                same=false;
                break;
            }
        }
    }
    context->Unmap(staging.Get(),0);
    REQUIRE(same);

    // Confirm the resource really remained typeless while the view was typed.
    D3D11_TEXTURE2D_DESC observed{};
    texture->GetDesc(&observed);
    REQUIRE(observed.Format==DXGI_FORMAT_R8G8B8A8_TYPELESS);
    D3D11_RENDER_TARGET_VIEW_DESC observed_rtv{};
    rtv->GetDesc(&observed_rtv);
    REQUIRE(observed_rtv.Format==DXGI_FORMAT_R8G8B8A8_UNORM);

    std::puts("D3D11 typeless OpenXR upload contract PASS");
    return 0;
}
