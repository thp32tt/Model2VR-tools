// Headless Windows D3D11 contract for the OpenXR/VDXR upload path.
// VDXR/OpenXR D3D11 swapchain resources may be TYPELESS while the app
// creates a typed RTV. Prove that the exact upload strategy used by the
// host works without an HMD: TYPELESS resource -> UNORM RTV ->
// UpdateSubresource -> staging readback.
#include <Windows.h>
#include <d3d11.h>
#include <wrl/client.h>
#include "../src/common/daytona_host_pixel_format.hpp"

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


    // D3D11 nonzero-origin UpdateSubresource box: pSrcData begins at the
    // changed rectangle, while row pitch remains the full original image.
    const D3D11_BOX dirty_box{1u,1u,0u,3u,3u,1u};
    auto changed_rgba=pixels;
    for(unsigned y=1u;y<3u;++y)
        for(unsigned x=1u;x<3u;++x){
            const auto at=(y*4u+x)*4u;
            changed_rgba[at]=233u;changed_rgba[at+1u]=77u;
            changed_rgba[at+2u]=14u;changed_rgba[at+3u]=255u;
        }
    const auto dirty_offset=(1u*4u+1u)*4u;
    context->UpdateSubresource(texture.Get(),0,&dirty_box,
        changed_rgba.data()+dirty_offset,16u,0u);
    context->CopyResource(staging.Get(),texture.Get());
    REQUIRE_HR(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&mapped));
    same=true;
    for(unsigned y=0;y<4u && same;++y){
        const auto* row=static_cast<const std::uint8_t*>(mapped.pData)+
                        static_cast<std::size_t>(y)*mapped.RowPitch;
        for(unsigned b=0;b<16u;++b)
            if(row[b]!=changed_rgba[static_cast<std::size_t>(y)*16u+b]){
                same=false;break;
            }
    }
    context->Unmap(staging.Get(),0);
    REQUIRE(same);

    // Two disjoint boxes represent an old and a far-away new reticle.
    // Every untouched pixel must survive the TWO nonzero/split writes.
    const D3D11_BOX far_boxes[]={
        {0u,0u,0u,1u,1u,1u}, {3u,3u,0u,4u,4u,1u}};
    auto twice_rgba=changed_rgba;
    for(const unsigned at:{0u,60u}){
        twice_rgba[at]=211u;twice_rgba[at+1u]=44u;
        twice_rgba[at+2u]=199u;twice_rgba[at+3u]=255u;
    }
    for(unsigned n=0;n<2u;++n){
        const auto& box=far_boxes[n];
        const auto offset=(box.top*4u+box.left)*4u;
        context->UpdateSubresource(texture.Get(),0,&box,
            twice_rgba.data()+offset,16u,0u);
    }
    context->CopyResource(staging.Get(),texture.Get());
    REQUIRE_HR(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&mapped));
    same=true;
    for(unsigned y=0;y<4u && same;++y){
        const auto* row=static_cast<const std::uint8_t*>(mapped.pData)+
                        std::size_t(y)*mapped.RowPitch;
        for(unsigned b=0;b<16u;++b)
            if(row[b]!=twice_rgba[std::size_t(y)*16u+b]){
                same=false;break;
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

    // Some OpenXR runtimes enumerate BGRA8 but not RGBA8. Verify the real
    // D3D11 WARP upload into a typeless runtime texture + typed BGRA view:
    // x86 producer RGBA bytes must be swizzled BEFORE UpdateSubresource.
    D3D11_TEXTURE2D_DESC bgra_desc=desc;
    bgra_desc.Format=DXGI_FORMAT_B8G8R8A8_TYPELESS;
    ComPtr<ID3D11Texture2D> bgra_texture;
    REQUIRE_HR(device->CreateTexture2D(&bgra_desc,nullptr,&bgra_texture));
    auto bgra_rtv_desc=rtv_desc;
    bgra_rtv_desc.Format=DXGI_FORMAT_B8G8R8A8_UNORM;
    ComPtr<ID3D11RenderTargetView> bgra_rtv;
    REQUIRE_HR(device->CreateRenderTargetView(
        bgra_texture.Get(),&bgra_rtv_desc,&bgra_rtv));
    std::array<std::uint8_t,4u*4u*4u> bgra_upload{};
    REQUIRE(m2vr::hostcolor::ConvertRgbaToBgra(
        pixels.data(),pixels.size(),bgra_upload.data(),bgra_upload.size()));
    context->UpdateSubresource(
        bgra_texture.Get(),0,nullptr,bgra_upload.data(),4u*4u,0);
    auto bgra_staging_desc=bgra_desc;
    bgra_staging_desc.Usage=D3D11_USAGE_STAGING;
    bgra_staging_desc.BindFlags=0;
    bgra_staging_desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    ComPtr<ID3D11Texture2D> bgra_staging;
    REQUIRE_HR(device->CreateTexture2D(
        &bgra_staging_desc,nullptr,&bgra_staging));
    context->CopyResource(bgra_staging.Get(),bgra_texture.Get());
    REQUIRE_HR(context->Map(
        bgra_staging.Get(),0,D3D11_MAP_READ,0,&mapped));
    same=true;
    for(unsigned y=0;y<4 && same;++y){
        const auto* row=static_cast<const std::uint8_t*>(mapped.pData)+
                        static_cast<std::size_t>(y)*mapped.RowPitch;
        for(unsigned x=0;x<16u;++x){
            if(row[x]!=bgra_upload[static_cast<std::size_t>(y)*16u+x]){
                same=false;break;
            }
        }
    }
    context->Unmap(bgra_staging.Get(),0);
    REQUIRE(same);
    bgra_texture->GetDesc(&observed);
    REQUIRE(observed.Format==DXGI_FORMAT_B8G8R8A8_TYPELESS);
    bgra_rtv->GetDesc(&observed_rtv);
    REQUIRE(observed_rtv.Format==DXGI_FORMAT_B8G8R8A8_UNORM);

    // BGRA8 native runtime: only the box is converted. WARP validates
    // untouched pixels and the same nonzero source pointer/row stride.
    auto changed_bgra=bgra_upload;
    REQUIRE(m2vr::hostcolor::ConvertRgbaToBgraRect(
        changed_rgba.data(),changed_rgba.size(),
        changed_bgra.data(),changed_bgra.size(),
        4u,4u,1u,1u,3u,3u));
    context->UpdateSubresource(bgra_texture.Get(),0,&dirty_box,
        changed_bgra.data()+dirty_offset,16u,0u);
    context->CopyResource(bgra_staging.Get(),bgra_texture.Get());
    REQUIRE_HR(context->Map(bgra_staging.Get(),0,D3D11_MAP_READ,0,&mapped));
    same=true;
    for(unsigned y=0;y<4u && same;++y){
        const auto* row=static_cast<const std::uint8_t*>(mapped.pData)+
                        static_cast<std::size_t>(y)*mapped.RowPitch;
        for(unsigned b=0;b<16u;++b)
            if(row[b]!=changed_bgra[static_cast<std::size_t>(y)*16u+b]){
                same=false;break;
            }
    }
    context->Unmap(bgra_staging.Get(),0);
    REQUIRE(same);

    // BGRA TYPELESS + UNORM RTV must also preserve unrelated GPU pixels
    // across TWO nonzero-origin, full-pitch, independently swizzled boxes.
    auto twice_bgra=changed_bgra;
    for(const auto& box:far_boxes)
        REQUIRE(m2vr::hostcolor::ConvertRgbaToBgraRect(
            twice_rgba.data(),twice_rgba.size(),
            twice_bgra.data(),twice_bgra.size(),
            4u,4u,box.left,box.top,box.right,box.bottom));
    for(const auto& box:far_boxes){
        const auto offset=(box.top*4u+box.left)*4u;
        context->UpdateSubresource(bgra_texture.Get(),0,&box,
            twice_bgra.data()+offset,16u,0u);
    }
    context->CopyResource(bgra_staging.Get(),bgra_texture.Get());
    REQUIRE_HR(context->Map(
        bgra_staging.Get(),0,D3D11_MAP_READ,0,&mapped));
    same=true;
    for(unsigned y=0;y<4u && same;++y){
        const auto* row=static_cast<const std::uint8_t*>(mapped.pData)+
                        std::size_t(y)*mapped.RowPitch;
        for(unsigned b=0;b<16u;++b)
            if(row[b]!=twice_bgra[std::size_t(y)*16u+b]){
                same=false;break;
            }
    }
    context->Unmap(bgra_staging.Get(),0);
    REQUIRE(same);

    // A typed BGRA RTV must write red in the physical *third* byte,
    // not silently interpret our RGBA pixels as blue on the Quest.
    const float pure_red[4]={1.f,0.f,0.f,1.f};
    context->ClearRenderTargetView(bgra_rtv.Get(),pure_red);
    context->CopyResource(bgra_staging.Get(),bgra_texture.Get());
    REQUIRE_HR(context->Map(
        bgra_staging.Get(),0,D3D11_MAP_READ,0,&mapped));
    const auto* first=static_cast<const std::uint8_t*>(mapped.pData);
    const bool red_is_bgra=first[0]==0u && first[1]==0u &&
                           first[2]==255u && first[3]==255u;
    context->Unmap(bgra_staging.Get(),0);
    REQUIRE(red_is_bgra);

    std::puts("D3D11 typeless RGBA8/BGRA8 OpenXR upload contract PASS");
    return 0;
}
