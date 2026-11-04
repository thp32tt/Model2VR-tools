#include "../src/probe/d3d9_dxt_decode.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"DXT source decode test line %d: %s\n",__LINE__,#x);return 1;}}while(0)
using namespace m2vr::drawprobe;
int main(){
    CHECK(DxtBytesPerBlock(D3DFMT_DXT1)==8u);
    CHECK(DxtBytesPerBlock(D3DFMT_DXT3)==16u);
    CHECK(DxtBytesPerBlock(D3DFMT_DXT5)==16u);
    CHECK(DxtBytesPerBlock(D3DFMT_DXT2)==0u);
    // DXT1: red 565, green 565; 4-color interpolation.
    const std::array<std::uint8_t,8> red_green{
       0x00,0xF8,0xE0,0x07,0xE4,0x00,0x00,0x00
    };
    std::array<std::uint8_t,4u*4u*4u> rgba{};
    CHECK(DecodeDxtBlockRGBA(red_green.data(),D3DFMT_DXT1,0,0,
                            rgba.data(),4,4,16));
    CHECK(rgba[0]==255 && rgba[1]==0 && rgba[2]==0 && rgba[3]==255);
    CHECK(rgba[4]==0 && rgba[5]==255 && rgba[7]==255);
    CHECK(rgba[8]>165 && rgba[8]<175 && rgba[9]>80 && rgba[9]<90);
    CHECK(rgba[12]>80 && rgba[12]<90 && rgba[13]>165 && rgba[13]<175);
    // DXT1 c0<=c1 selects transparent index 3.
    const std::array<std::uint8_t,8> transparent{
       0x00,0x00,0xFF,0xFF,0x03,0,0,0
    };
    CHECK(DecodeDxtBlockRGBA(transparent.data(),D3DFMT_DXT1,0,0,
                            rgba.data(),4,4,16));
    CHECK(rgba[3]==0);
    // DXT3: alpha nibble F =>255, 0=>0; dark-red color index 0.
    std::array<std::uint8_t,16> dxt3{};
    dxt3[0]=0x0F;
    dxt3[8]=0x00;dxt3[9]=0xF8;dxt3[10]=0xE0;dxt3[11]=0x07;
    CHECK(DecodeDxtBlockRGBA(dxt3.data(),D3DFMT_DXT3,0,0,
                            rgba.data(),4,4,16));
    CHECK(rgba[0]==255 && rgba[3]==255 && rgba[7]==0);
    // DXT5: alpha endpoints 255 and 0, first texel index 0 (255),
    // second index 1 (0), third index 2 (~218).
    std::array<std::uint8_t,16> dxt5{};
    dxt5[0]=255;dxt5[1]=0;
    dxt5[2]=0x88; dxt5[3]=0x00; // indexes bits 000 001 010
    dxt5[8]=0x00;dxt5[9]=0xF8;dxt5[10]=0xE0;dxt5[11]=0x07;
    CHECK(DecodeDxtBlockRGBA(dxt5.data(),D3DFMT_DXT5,0,0,
                            rgba.data(),4,4,16));
    CHECK(rgba[3]==255 && rgba[7]==0 && rgba[11]==218);
    // DXT5 short palette a0<=a1 indices 6 and 7 mean 0 and 255.
    dxt5[0]=0;dxt5[1]=255;
    dxt5[2]=0x3E; // pixels indices: 6,7,...
    CHECK(DecodeDxtBlockRGBA(dxt5.data(),D3DFMT_DXT5,0,0,
                            rgba.data(),4,4,16));
    CHECK(rgba[3]==0 && rgba[7]==255);
    // A non-multiple-of-four edge block only writes within the actual
    // original source texture dimensions.
    std::array<std::uint8_t,6u*5u*4u> edge{};
    edge.fill(99u);
    CHECK(DecodeDxtBlockRGBA(red_green.data(),D3DFMT_DXT1,1,1,
                            edge.data(),6,5,24));
    CHECK(edge[(4u*6u+4u)*4u]==255); // clipped (x=4,y=4)
    CHECK(edge[0]==99u && edge[(3u*6u+3u)*4u+3u]==99u);
    CHECK(edge[edge.size()-1]==255u); // (x=5,y=4) was clipped in-bounds
    CHECK(!DecodeDxtBlockRGBA(red_green.data(),D3DFMT_DXT1,2,0,
                             edge.data(),6,5,24));
    CHECK(!DecodeDxtBlockRGBA(red_green.data(),D3DFMT_DXT1,0,0,
                             edge.data(),6,5,16)); // invalid pitch
    CHECK(!DecodeDxtBlockRGBA(red_green.data(),D3DFMT_DXT2,0,0,
                             edge.data(),6,5,24));
    CHECK(!DecodeDxtBlockRGBA(nullptr,D3DFMT_DXT1,0,0,
                             edge.data(),6,5,24));
    return 0;
}
