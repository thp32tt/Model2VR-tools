#include "../src/common/daytona_host_pixel_format.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
using namespace m2vr::hostcolor;
#define REQUIRE(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\\n",__LINE__,#x);std::exit(1);}}while(0)
int main(){
    constexpr std::int64_t rgba=28, bgra=87, unrelated=91;
    const std::array<std::int64_t,3> formats{bgra,unrelated,rgba};
    REQUIRE(ChooseSwapchainFormat(formats.data(),formats.size(),rgba,bgra)==rgba);
    const std::array<std::int64_t,2> bgra_only{unrelated,bgra};
    REQUIRE(ChooseSwapchainFormat(bgra_only.data(),bgra_only.size(),rgba,bgra)==bgra);
    REQUIRE(ChooseSwapchainFormat(&unrelated,1u,rgba,bgra)==0);
    REQUIRE(ChooseSwapchainFormat(nullptr,1u,rgba,bgra)==0);
    REQUIRE(ChooseSwapchainFormat(formats.data(),0u,rgba,bgra)==0);
    REQUIRE(ChooseSwapchainFormat(formats.data(),3u,rgba,rgba)==0);
    const std::array<std::uint8_t,12> input{
        252,13,5,255, 20,80,230,17, 0,31,255,0
    };
    std::array<std::uint8_t,14> result{};
    result.fill(0xA5);
    REQUIRE(ConvertRgbaToBgra(input.data(),input.size(),
                              result.data()+1u,result.size()-2u));
    const std::array<std::uint8_t,12> expected{
        5,13,252,255, 230,80,20,17, 255,31,0,0
    };
    for(std::size_t i=0;i<input.size();++i)
        REQUIRE(result[i+1u]==expected[i]);
    REQUIRE(result.front()==0xA5 && result.back()==0xA5);
    REQUIRE(!ConvertRgbaToBgra(input.data(),input.size(),result.data(),11u));
    REQUIRE(!ConvertRgbaToBgra(input.data(),11u,result.data(),14u));
    REQUIRE(!ConvertRgbaToBgra(input.data(),input.size(),nullptr,14u));
    REQUIRE(!ConvertRgbaToBgra(input.data(),input.size(),
                              const_cast<std::uint8_t*>(input.data()),12u));
    return 0;
}
