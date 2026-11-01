#include "../src/probe/draw_up_witness.hpp"
#include <array>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <limits>
using namespace m2vr::drawprobe;
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"UP witness failed line %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(){
    CHECK(ValidStride(16u) && ValidStride(40u));
    CHECK(!ValidStride(15u) && !ValidStride(257u) && !ValidStride(18u));
    CHECK(PrimitiveElements(4,2)==6 && PrimitiveElements(5,2)==4);
    CHECK(PrimitiveElements(2,5)==10 && PrimitiveElements(1,1)==1);
    CHECK(PrimitiveElements(3,1)==2 && PrimitiveElements(6,1)==3);
    CHECK(PrimitiveElements(0,3)==0 && PrimitiveElements(4,8193)==0);
    const std::array<std::uint32_t,4> raw{{
        std::bit_cast<std::uint32_t>(320.f),
        std::bit_cast<std::uint32_t>(240.f),
        std::bit_cast<std::uint32_t>(0.5f),
        std::bit_cast<std::uint32_t>(1.f)}};
    VertexWitness v{};
    CHECK(DecodeXYZRHW(raw,v));
    CHECK(v.x==320.f && v.y==240.f && v.rhw==1.f);
    CHECK(PackPair(v.x,v.y)==((std::uint64_t{raw[0]}<<32)|raw[1]));
    auto invalid=raw;
    invalid[0]=std::bit_cast<std::uint32_t>((std::numeric_limits<float>::infinity)());
    CHECK(!DecodeXYZRHW(invalid,v));
    invalid=raw;invalid[3]=std::bit_cast<std::uint32_t>(0.f);
    CHECK(!DecodeXYZRHW(invalid,v));
    return 0;
}
