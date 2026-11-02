// Strict Daytona shader-UP declaration: synthetically validate 32-byte
// position/colour/UV layout. Never execute the emulator or authorize stereo.
#include "../src/probe/daytona_up_decl_witness.hpp"
#include <array>
#include <bit>
#include <cstdio>
#include <cstring>
#include <limits>
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"Daytona source gate failed at %d: %s\n",__LINE__,#x);return 1;}} while(0)
int main() {
    using namespace m2vr::drawprobe;
    std::array<D3DVERTEXELEMENT9,5> e={{
        {0,0,D3DDECLTYPE_FLOAT4,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_POSITION,0},
        {0,16,D3DDECLTYPE_D3DCOLOR,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_COLOR,0},
        {0,20,D3DDECLTYPE_D3DCOLOR,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_COLOR,1},
        {0,24,D3DDECLTYPE_FLOAT2,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_TEXCOORD,0},
        D3DDECL_END()
    }};
    CHECK(ExactDaytonaUPDeclaration(e.data(),5,32));
    CHECK(!ExactDaytonaUPDeclaration(e.data(),4,32));
    CHECK(!ExactDaytonaUPDeclaration(e.data(),5,36));
    CHECK(!ExactDaytonaUPDeclaration(nullptr,5,32));
    auto invalid=e;
    invalid[1].Offset=15;
    CHECK(!ExactDaytonaUPDeclaration(invalid.data(),5,32));
    invalid=e;invalid[2].UsageIndex=0;
    CHECK(!ExactDaytonaUPDeclaration(invalid.data(),5,32));
    invalid=e;invalid[3].Stream=1;
    CHECK(!ExactDaytonaUPDeclaration(invalid.data(),5,32));
    invalid=e;invalid[0].Type=D3DDECLTYPE_FLOAT3;
    CHECK(!ExactDaytonaUPDeclaration(invalid.data(),5,32));
    invalid=e;invalid[4].Stream=0;
    CHECK(!ExactDaytonaUPDeclaration(invalid.data(),5,32));
    std::array<std::uint8_t,32> bytes{};
    const std::array<float,4> pos{14.5f,-20.25f,0.375f,2.0f};
    const std::array<float,2> uv{0.125f,0.75f};
    std::memcpy(bytes.data(),pos.data(),16);
    std::memcpy(bytes.data()+24,uv.data(),8);
    DaytonaUPVertex out{};
    CHECK(DecodeDaytonaUPVertex(bytes,out));
    CHECK(out.position==pos && out.texcoord==uv);
    const auto old=out;
    const float nan=std::numeric_limits<float>::quiet_NaN();
    std::memcpy(bytes.data()+12,&nan,sizeof nan);
    CHECK(!DecodeDaytonaUPVertex(bytes,out));
    CHECK(out.position==old.position && out.texcoord==old.texcoord);
    return 0;
}
