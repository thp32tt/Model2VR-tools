#include "../src/probe/daytona_triangle_eye_shift.hpp"
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#define REQUIRE(expr) do { if(!(expr)) {std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#expr);std::exit(1);} } while(0)
int main(){
    using namespace m2vr::daytona;
    std::array<std::uint8_t,192> base{},left{},right{};
    base.fill(0x2d);
    for(unsigned v=0;v<6u;++v){
        const float x=0.35f+0.1f*v;
        const float w=v==4u?-1.f:1.f;
        std::memcpy(base.data()+v*32u,&x,sizeof x);
        std::memcpy(base.data()+v*32u+12u,&w,sizeof w);
    }
    left=base;right=base;
    const auto stats=ShiftSafeUPTriangles(left.data(),right.data(),
                                         left.size(),2u,0.07f);
    REQUIRE(stats.shifted==1u && stats.held_flat==1u);
    for(unsigned v=0;v<6u;++v){
        float orig=0,l=0,r=0;
        std::memcpy(&orig,base.data()+v*32u,sizeof orig);
        std::memcpy(&l,left.data()+v*32u,sizeof l);
        std::memcpy(&r,right.data()+v*32u,sizeof r);
        if(v<3u){
            REQUIRE(std::fabs(l-(orig+0.07f))<1e-6f);
            REQUIRE(std::fabs(r-(orig-0.07f))<1e-6f);
        }else REQUIRE(l==orig && r==orig);
        // Signed W, UV and material colors must never change.
        REQUIRE(std::memcmp(left.data()+v*32u+4u,
                            base.data()+v*32u+4u,28u)==0);
        REQUIRE(std::memcmp(right.data()+v*32u+4u,
                            base.data()+v*32u+4u,28u)==0);
    }
    // Out-of-band byte count cannot write past an incomplete vertex.
    left=base;right=base;
    auto bad=ShiftSafeUPTriangles(left.data(),right.data(),191u,2u,0.07f);
    REQUIRE(bad.shifted==0u && bad.held_flat==0u);
    REQUIRE(left==base && right==base);
    REQUIRE(ShiftSafeUPTriangles(left.data(),left.data(),192u,2u,0.07f).shifted==0u);
    REQUIRE(ShiftSafeUPTriangles(left.data(),right.data(),192u,2u,-0.1f).shifted==0u);
    REQUIRE(left==base && right==base);
    // One bad vertex must hold only its own triangle, not another triangle.
    float nan=std::numeric_limits<float>::quiet_NaN();
    std::memcpy(base.data()+32u+12u,&nan,sizeof nan);
    left=base;right=base;
    const auto held=ShiftSafeUPTriangles(left.data(),right.data(),192u,2u,0.07f);
    REQUIRE(held.shifted==0u && held.held_flat==2u);
    REQUIRE(left==base && right==base);
    return 0;
}
