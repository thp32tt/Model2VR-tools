#include "../src/probe/d3d9_palette_decode.hpp"
#include <array>
#include <cstdio>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"source palette decode line %d\n",__LINE__);return 1;}}while(0)
using namespace m2vr::drawprobe;
int main(){
    CHECK(PalettedPixelBytes(D3DFMT_P8)==1u);
    CHECK(PalettedPixelBytes(D3DFMT_A8P8)==2u);
    CHECK(PalettedPixelBytes(D3DFMT_A8)==0u);
    CpuPaletteRGBA palette{};
    palette.colors[3]={11u,22u,33u,44u};
    palette.colors[5]={55u,66u,77u,88u};
    const std::array<std::uint8_t,2> index8{{3u,5u}};
    std::array<std::uint8_t,8> rgba{};
    CHECK(DecodePalettedRowRGBA(index8.data(),rgba.data(),2u,
                                D3DFMT_P8,palette));
    const std::array<std::uint8_t,8> opaque{{
        11u,22u,33u,255u,55u,66u,77u,255u}};
    CHECK(rgba==opaque);
    palette.palette_alpha_supported=true;
    CHECK(DecodePalettedRowRGBA(index8.data(),rgba.data(),2u,
                                D3DFMT_P8,palette));
    const std::array<std::uint8_t,8> pal_alpha{{
        11u,22u,33u,44u,55u,66u,77u,88u}};
    CHECK(rgba==pal_alpha);
    const std::array<std::uint8_t,4> a8p8{{3u,0u,5u,128u}};
    CHECK(DecodePalettedRowRGBA(a8p8.data(),rgba.data(),2u,
                                D3DFMT_A8P8,palette));
    const std::array<std::uint8_t,8> pixel_alpha{{
        11u,22u,33u,0u,55u,66u,77u,128u}};
    CHECK(rgba==pixel_alpha);
    palette.palette_alpha_supported=false;
    CHECK(DecodePalettedRowRGBA(a8p8.data(),rgba.data(),2u,
                                D3DFMT_A8P8,palette));
    CHECK(rgba==pixel_alpha);
    CHECK(!DecodePalettedRowRGBA(a8p8.data(),rgba.data(),2u,
                                 D3DFMT_A8,palette));
    CHECK(!DecodePalettedRowRGBA(nullptr,rgba.data(),1u,
                                 D3DFMT_P8,palette));
    CHECK(!DecodePalettedRowRGBA(a8p8.data(),nullptr,1u,
                                 D3DFMT_A8P8,palette));
    CHECK(!DecodePalettedRowRGBA(a8p8.data(),rgba.data(),0u,
                                 D3DFMT_P8,palette));
    CHECK(!DecodePalettedRowRGBA(a8p8.data(),rgba.data(),1025u,
                                 D3DFMT_P8,palette));
    return 0;
}
