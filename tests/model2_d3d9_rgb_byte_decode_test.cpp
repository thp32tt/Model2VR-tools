#include "../src/probe/d3d9_rgb_byte_decode.hpp"
#include <array>
#include <cstdio>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"RGB byte decode failed at %d\n",__LINE__);return 1;}}while(0)
using namespace m2vr::drawprobe;
int main(){
    CHECK(RgbBytePixelBytes(D3DFMT_R8G8B8)==3u);
    CHECK(RgbBytePixelBytes(D3DFMT_R3G3B2)==1u);
    CHECK(RgbBytePixelBytes(D3DFMT_A8)==0u);
    const std::array<std::uint8_t,9> bgr{{0,0,255, 0,255,0, 255,0,0}};
    std::array<std::uint8_t,12> rgba{};
    const std::array<std::uint8_t,12> expected{{
        255,0,0,255, 0,255,0,255, 0,0,255,255}};
    CHECK(DecodeRgbByteRowRGBA(bgr.data(),rgba.data(),3u,D3DFMT_R8G8B8));
    CHECK(rgba==expected);
    const std::array<std::uint8_t,3> rgb332{{0xe0,0x1c,0x03}};
    CHECK(DecodeRgbByteRowRGBA(rgb332.data(),rgba.data(),3u,D3DFMT_R3G3B2));
    CHECK(rgba==expected);
    CHECK(!DecodeRgbByteRowRGBA(rgb332.data(),rgba.data(),3u,D3DFMT_A8));
    CHECK(rgba==expected);
    CHECK(!DecodeRgbByteRowRGBA(nullptr,rgba.data(),1u,D3DFMT_R3G3B2));
    CHECK(!DecodeRgbByteRowRGBA(rgb332.data(),nullptr,1u,D3DFMT_R3G3B2));
    CHECK(!DecodeRgbByteRowRGBA(rgb332.data(),rgba.data(),0u,D3DFMT_R3G3B2));
    CHECK(!DecodeRgbByteRowRGBA(rgb332.data(),rgba.data(),1025u,D3DFMT_R3G3B2));
    return 0;
}
