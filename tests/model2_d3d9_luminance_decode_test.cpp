// D3D9 real source L8/A8L8 bytes to native two-eye RGBA transport.
// No original emulator, display, HMD or haptics required.
#include "../src/probe/d3d9_luminance_decode.hpp"
#include <array>
#include <cstdio>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"luminance decode line %d\n",__LINE__);return 1;}}while(0)
using namespace m2vr::drawprobe;
int main() {
    CHECK(LuminancePixelBytes(D3DFMT_L8)==1u);
    CHECK(LuminancePixelBytes(D3DFMT_A8L8)==2u);
    CHECK(LuminancePixelBytes(D3DFMT_A4L4)==1u);
    CHECK(LuminancePixelBytes(D3DFMT_A8)==0u);
    CHECK(LuminancePixelBytes(D3DFMT_A8R8G8B8)==0u);
    const std::array<std::uint8_t,4> l8{{0,64,128,255}};
    std::array<std::uint8_t,16> rgba{};
    CHECK(DecodeLuminanceRowRGBA(l8.data(),rgba.data(),4u,D3DFMT_L8));
    const std::array<std::uint8_t,16> expected_l8{{
        0,0,0,255, 64,64,64,255, 128,128,128,255, 255,255,255,255}};
    CHECK(rgba==expected_l8);
    const std::array<std::uint8_t,8> a8l8{{
        255,0, 128,64, 64,128, 0,255
    }};
    CHECK(DecodeLuminanceRowRGBA(a8l8.data(),rgba.data(),4u,D3DFMT_A8L8));
    const std::array<std::uint8_t,16> expected_a8l8{{
        255,255,255,0, 128,128,128,64, 64,64,64,128, 0,0,0,255}};
    CHECK(rgba==expected_a8l8);
    const std::array<std::uint8_t,4> a4l4{{
        0x00u,0xF0u,0x0Fu,0x8Fu
    }};
    CHECK(DecodeLuminanceRowRGBA(a4l4.data(),rgba.data(),4u,D3DFMT_A4L4));
    const std::array<std::uint8_t,16> expected_a4l4{{
        0,0,0,0, 0,0,0,255, 255,255,255,0, 255,255,255,136}};
    CHECK(rgba==expected_a4l4);
    const auto prior=rgba;
    CHECK(!DecodeLuminanceRowRGBA(a8l8.data(),rgba.data(),4u,D3DFMT_A8));
    CHECK(rgba==prior);
    CHECK(!DecodeLuminanceRowRGBA(nullptr,rgba.data(),1u,D3DFMT_L8));
    CHECK(!DecodeLuminanceRowRGBA(l8.data(),nullptr,1u,D3DFMT_L8));
    CHECK(!DecodeLuminanceRowRGBA(l8.data(),rgba.data(),0u,D3DFMT_L8));
    CHECK(!DecodeLuminanceRowRGBA(l8.data(),rgba.data(),1025u,D3DFMT_L8));
    return 0;
}
