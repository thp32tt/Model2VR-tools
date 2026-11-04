#include "../src/probe/d3d9_rgb16_decode.hpp"
#include <array>
#include <cstdio>
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"RGB16 source %d: %s\n",__LINE__,#x);return 1;}}while(0)
using namespace m2vr::drawprobe;
int main(){
    CHECK(Supported16BitColor(D3DFMT_R5G6B5));
    CHECK(Supported16BitColor(D3DFMT_X1R5G5B5));
    CHECK(Supported16BitColor(D3DFMT_A1R5G5B5));
    CHECK(Supported16BitColor(D3DFMT_A4R4G4B4));
    CHECK(Supported16BitColor(D3DFMT_X4R4G4B4));
    CHECK(Supported16BitColor(D3DFMT_A8R3G3B2));
    CHECK(!Supported16BitColor(D3DFMT_A8R8G8B8));
    std::array<std::uint8_t,8> source{
        0x00,0xF8, // R5G6B5 red 255
        0xE0,0x07, // R5G6B5 green 255
        0x1F,0x00, // R5G6B5 blue 255
        0xFF,0xFF
    };
    std::array<std::uint8_t,16> out{};
    CHECK(Decode16BitColorRowRGBA(source.data(),out.data(),4,D3DFMT_R5G6B5));
    CHECK(out[0]==255 && out[1]==0 && out[2]==0 && out[3]==255);
    CHECK(out[4]==0 && out[5]==255 && out[6]==0 && out[7]==255);
    CHECK(out[8]==0 && out[9]==0 && out[10]==255 && out[11]==255);
    CHECK(out[12]==255 && out[13]==255 && out[14]==255);
    // A1R5G5B5 exact alpha bit zero and one.
    source={0x00,0x7C, 0x00,0xFC,0,0,0,0};
    CHECK(Decode16BitColorRowRGBA(source.data(),out.data(),2,D3DFMT_A1R5G5B5));
    CHECK(out[0]==255 && out[3]==0 && out[4]==255 && out[7]==255);
    CHECK(Decode16BitColorRowRGBA(source.data(),out.data(),2,D3DFMT_X1R5G5B5));
    CHECK(out[3]==255 && out[7]==255);
    // ARGB4444: A=8, R=F, G=0, B=4; second transparent.
    source={0x04,0x8F, 0x00,0x00,0,0,0,0};
    CHECK(Decode16BitColorRowRGBA(source.data(),out.data(),2,D3DFMT_A4R4G4B4));
    CHECK(out[0]==255 && out[1]==0 && out[2]==68 && out[3]==136);
    CHECK(out[7]==0);
    CHECK(Decode16BitColorRowRGBA(source.data(),out.data(),2,D3DFMT_X4R4G4B4));
    CHECK(out[0]==255 && out[1]==0 && out[2]==68 && out[3]==255);
    CHECK(out[7]==255);
    // Original A8R3G3B2: high 8-bit alpha, low byte packed RGB332.
    source={0xE0,0x80, 0x1C,0x22, 0x03,0x00, 0xFF,0xFF};
    CHECK(Decode16BitColorRowRGBA(
        source.data(),out.data(),4,D3DFMT_A8R3G3B2));
    CHECK(out[0]==255 && out[1]==0 && out[2]==0 && out[3]==128);
    CHECK(out[4]==0 && out[5]==255 && out[6]==0 && out[7]==34);
    CHECK(out[8]==0 && out[9]==0 && out[10]==255 && out[11]==0);
    CHECK(out[12]==255 && out[13]==255 &&
          out[14]==255 && out[15]==255);
    CHECK(!Decode16BitColorRowRGBA(source.data(),out.data(),2,D3DFMT_A8R8G8B8));
    CHECK(!Decode16BitColorRowRGBA(source.data(),out.data(),0,D3DFMT_R5G6B5));
    CHECK(!Decode16BitColorRowRGBA(source.data(),out.data(),1025,D3DFMT_R5G6B5));
    CHECK(!Decode16BitColorRowRGBA(nullptr,out.data(),1,D3DFMT_R5G6B5));
    CHECK(!Decode16BitColorRowRGBA(source.data(),nullptr,1,D3DFMT_R5G6B5));
    return 0;
}
