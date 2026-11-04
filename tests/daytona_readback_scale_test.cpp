#include "../src/probe/daytona_readback_scale.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <vector>
#define REQUIRE(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
int main(){
    using namespace m2vr::daytona;
    ReadbackScalePlan plan{};
    REQUIRE(!plan.Configure(0,480));
    REQUIRE(!plan.Configure(640,0));
    REQUIRE(!plan.Configure(2049,480));
    REQUIRE(plan.Configure(4,2));
    REQUIRE(plan.dst_w==1024u && plan.dst_h==512u &&
            plan.left==0u && plan.top==256u);
    std::array<std::uint8_t,32> input{};
    // Top-left BGRA, bottom-right BGRA.
    input[0]=11u;input[1]=22u;input[2]=33u;
    input[7]=200u;
    input[28]=77u;input[29]=88u;input[30]=99u;
    std::vector<std::uint8_t> out(ReadbackScalePlan::kOutputBytes+4u,0x5au);
    REQUIRE(!plan.Convert(input.data(),15,out.data(),
                          ReadbackScalePlan::kOutputBytes));
    REQUIRE(!plan.Convert(input.data(),-16,out.data(),
                          ReadbackScalePlan::kOutputBytes));
    REQUIRE(!plan.Convert(input.data(),16,out.data(),
                          ReadbackScalePlan::kOutputBytes-1u));
    REQUIRE(!plan.Convert(nullptr,16,out.data(),
                          ReadbackScalePlan::kOutputBytes));
    REQUIRE(plan.Convert(input.data(),16,out.data(),
                         ReadbackScalePlan::kOutputBytes));
    const auto pix=[&](unsigned x,unsigned y){
        return (std::size_t{y}*1024u+x)*4u;
    };
    REQUIRE(out[pix(0,0)]==0u && out[pix(0,255)]==0u);
    REQUIRE(out[pix(0,256)]==33u && out[pix(0,256)+1u]==22u &&
            out[pix(0,256)+2u]==11u && out[pix(0,256)+3u]==255u);
    REQUIRE(out[pix(1023,767)]==99u && out[pix(1023,767)+1u]==88u &&
            out[pix(1023,767)+2u]==77u);
    REQUIRE(out[pix(0,768)]==0u);
    for(unsigned j=0;j<4u;++j)
        REQUIRE(out[ReadbackScalePlan::kOutputBytes+j]==0x5au);
    REQUIRE(plan.Configure(2,4));
    REQUIRE(plan.dst_w==512u && plan.dst_h==1024u &&
            plan.left==256u && plan.top==0u);
    REQUIRE(plan.Configure(660,384));
    REQUIRE(plan.dst_w==1024u && plan.dst_h==595u);
    REQUIRE(plan.sy[plan.dst_h-1u]<384u);
    REQUIRE(plan.sx[plan.dst_w-1u]<660u);
    return 0;
}
