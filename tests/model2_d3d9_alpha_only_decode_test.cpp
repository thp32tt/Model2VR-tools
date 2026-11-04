#include "../src/probe/d3d9_alpha_only_decode.hpp"
#include <array>
#include <cstdio>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"alpha only source %d: %s\n",__LINE__,#x);return 1;}}while(0)
using namespace m2vr::drawprobe;
int main(){
    CHECK(AlphaOnlyPixelBytes(D3DFMT_A8)==1u);
    CHECK(AlphaOnlyPixelBytes(D3DFMT_L8)==0u);
    const std::array<std::uint8_t,4> a8{{0u,63u,128u,255u}};
    std::array<std::uint8_t,16> rgba{};
    CHECK(DecodeAlphaOnlyRowRGBA(a8.data(),rgba.data(),4u,D3DFMT_A8));
    const std::array<std::uint8_t,16> expected{{
        0,0,0,0, 0,0,0,63, 0,0,0,128, 0,0,0,255}};
    CHECK(rgba==expected);
    CHECK(!DecodeAlphaOnlyRowRGBA(a8.data(),rgba.data(),4u,D3DFMT_L8));
    CHECK(rgba==expected);
    CHECK(!DecodeAlphaOnlyRowRGBA(nullptr,rgba.data(),1u,D3DFMT_A8));
    CHECK(!DecodeAlphaOnlyRowRGBA(a8.data(),nullptr,1u,D3DFMT_A8));
    CHECK(!DecodeAlphaOnlyRowRGBA(a8.data(),rgba.data(),0u,D3DFMT_A8));
    CHECK(!DecodeAlphaOnlyRowRGBA(a8.data(),rgba.data(),1025u,D3DFMT_A8));
    return 0;
}
