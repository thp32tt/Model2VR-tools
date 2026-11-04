// Deterministic D3D9 normalized point/bilinear + wrap/clamp sampling tests.
// No emulator, GPU, ROM, headset or force-feedback device needed.
#include "../src/vr/model2_texture_sampler.hpp"
#include "../src/probe/d3d9_sampler_states.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <limits>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"Texture sampler regression %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(){
    using namespace m2vr::vr;
    // Top row RED (alpha 255), GREEN (alpha 0).
    // Bottom row BLUE (alpha 0), WHITE (alpha 255).
    const std::array<std::uint8_t,16> rgba{
        255,0,0,255, 0,255,0,0,
        0,0,255,0, 255,255,255,255
    };
    const auto point=SamplerPolicy{};
    SamplerPolicy bilinear{TextureAddress::Clamp,TextureAddress::Clamp,
                           TextureFilter::Linear};
    SamplerPolicy repeat{TextureAddress::Wrap,TextureAddress::Wrap,
                         TextureFilter::Linear};
    SampledRGBA px{};
    CHECK(SampleTextureRGBA(rgba.data(),2,2,8,0.25,0.25,point,px));
    CHECK(px.r==255 && px.g==0 && px.b==0 && px.a==255);
    CHECK(SampleTextureRGBA(rgba.data(),2,2,8,0.75,0.25,point,px));
    CHECK(px.r==0 && px.g==255 && px.b==0 && px.a==0);
    CHECK(SampleTextureRGBA(rgba.data(),2,2,8,0.5,0.5,bilinear,px));
    CHECK(px.r==128 && px.g==128 && px.b==128 && px.a==128);
    CHECK(SampleTextureRGBA(rgba.data(),2,2,8,-0.25,-0.25,point,px));
    CHECK(px.r==255 && px.g==0 && px.b==0); // clamp negative
    CHECK(SampleTextureRGBA(rgba.data(),2,2,8,1.5,1.5,point,px));
    CHECK(px.r==255 && px.g==255 && px.b==255); // clamp high
    CHECK(SampleTextureRGBA(rgba.data(),2,2,8,0.0,0.0,repeat,px));
    CHECK(px.r==128 && px.g==128 && px.b==128 && px.a==128);
    CHECK(SampleTextureRGBA(rgba.data(),2,2,8,1.0,1.0,repeat,px));
    CHECK(px.r==128 && px.g==128 && px.b==128 && px.a==128);
    CHECK(SampleTextureRGBA(rgba.data(),2,2,8,-1.0,-1.0,repeat,px));
    CHECK(px.r==128 && px.g==128 && px.b==128 && px.a==128);
    CHECK(SampleTextureRGBA(rgba.data(),2,2,8,0.25,0.25,repeat,px));
    CHECK(px.r==255 && px.a==255); // exact center untouched
    // MIRROR repeats on a 2*size period. Both bilinear and POINT must
    // reflect at texture edges rather than wrap to the opposite texel.
    SamplerPolicy mirror{TextureAddress::Mirror,TextureAddress::Mirror,
                         TextureFilter::Point};
    CHECK(SampleTextureRGBA(rgba.data(),2,2,8,1.25,0.25,mirror,px));
    CHECK(px.r==0 && px.g==255 && px.b==0 && px.a==0);
    CHECK(SampleTextureRGBA(rgba.data(),2,2,8,1.75,0.25,mirror,px));
    CHECK(px.r==255 && px.g==0 && px.b==0 && px.a==255);
    CHECK(SampleTextureRGBA(rgba.data(),2,2,8,-0.25,-0.25,mirror,px));
    CHECK(px.r==255 && px.g==0 && px.b==0 && px.a==255);
    CHECK(SampleTextureRGBA(rgba.data(),2,2,8,-0.75,0.25,mirror,px));
    CHECK(px.r==0 && px.g==255 && px.b==0 && px.a==0);
    mirror.filter=TextureFilter::Linear;
    CHECK(SampleTextureRGBA(rgba.data(),2,2,8,1.,0.25,mirror,px));
    CHECK(px.r==0 && px.g==255 && px.b==0 && px.a==0);
    CHECK(SampleTextureRGBA(rgba.data(),2,2,8,2.,0.25,mirror,px));
    CHECK(px.r==255 && px.g==0 && px.b==0 && px.a==255);
    CHECK(SampleTextureRGBA(rgba.data(),2,2,8,-1.,0.25,mirror,px));
    CHECK(px.r==0 && px.g==255 && px.b==0 && px.a==0);
    // MIRRORONCE reflects negative addresses once and clamps beyond
    // the first reflection instead of repeating the MIRROR period.
    SamplerPolicy once{TextureAddress::MirrorOnce,
        TextureAddress::MirrorOnce,TextureFilter::Point};
    CHECK(SampleTextureRGBA(rgba.data(),2,2,8,-0.25,0.25,once,px));
    CHECK(px.r==255 && px.g==0 && px.b==0);
    CHECK(SampleTextureRGBA(rgba.data(),2,2,8,-0.75,0.25,once,px));
    CHECK(px.r==0 && px.g==255 && px.b==0);
    CHECK(SampleTextureRGBA(rgba.data(),2,2,8,-1.75,0.25,once,px));
    CHECK(px.r==0 && px.g==255 && px.b==0);
    CHECK(SampleTextureRGBA(rgba.data(),2,2,8,1.75,0.25,once,px));
    CHECK(px.r==0 && px.g==255 && px.b==0);
    once.filter=TextureFilter::Linear;
    CHECK(SampleTextureRGBA(rgba.data(),2,2,8,0.,0.25,once,px));
    CHECK(px.r==255 && px.g==0 && px.b==0);
    CHECK(SampleTextureRGBA(rgba.data(),2,2,8,-1.,0.25,once,px));
    CHECK(px.r==0 && px.g==255 && px.b==0);
    // Default fixed-function point/clamp behavior remains unchanged.
    CHECK(SampleTextureRGBA(rgba.data(),2,2,8,1.,1.,point,px));
    CHECK(px.r==255 && px.a==255);
    CHECK(!SampleTextureRGBA(rgba.data(),2,2,9,0.,0.,point,px));
    CHECK(!SampleTextureRGBA(rgba.data(),2,2,8,
        (std::numeric_limits<double>::quiet_NaN)(),0.,point,px));
    CHECK(!SampleTextureRGBA(rgba.data(),2,2,8,0.,0.,
        SamplerPolicy{TextureAddress::Clamp,TextureAddress::Clamp,
            static_cast<TextureFilter>(99)},px));
    using m2vr::drawprobe::DecodeSamplerPolicy;
    SamplerPolicy converted{};
    CHECK(DecodeSamplerPolicy(D3DTADDRESS_WRAP,D3DTADDRESS_CLAMP,
        D3DTEXF_LINEAR,D3DTEXF_LINEAR,D3DTEXF_NONE,0,converted));
    CHECK(converted.u==TextureAddress::Wrap &&
          converted.v==TextureAddress::Clamp &&
          converted.filter==TextureFilter::Linear);
    CHECK(DecodeSamplerPolicy(D3DTADDRESS_CLAMP,D3DTADDRESS_CLAMP,
        D3DTEXF_POINT,D3DTEXF_POINT,D3DTEXF_NONE,0,converted));
    CHECK(converted.filter==TextureFilter::Point);
    CHECK(DecodeSamplerPolicy(D3DTADDRESS_MIRROR,D3DTADDRESS_WRAP,
        D3DTEXF_LINEAR,D3DTEXF_LINEAR,D3DTEXF_NONE,0,converted));
    CHECK(converted.u==TextureAddress::Mirror &&
          converted.v==TextureAddress::Wrap);
    CHECK(DecodeSamplerPolicy(D3DTADDRESS_MIRRORONCE,D3DTADDRESS_MIRRORONCE,
        D3DTEXF_LINEAR,D3DTEXF_LINEAR,D3DTEXF_NONE,0,converted));
    CHECK(converted.u==TextureAddress::MirrorOnce &&
          converted.v==TextureAddress::MirrorOnce);
    CHECK(!DecodeSamplerPolicy(D3DTADDRESS_BORDER,D3DTADDRESS_WRAP,
        D3DTEXF_LINEAR,D3DTEXF_LINEAR,D3DTEXF_NONE,0,converted));
    CHECK(!DecodeSamplerPolicy(D3DTADDRESS_WRAP,D3DTADDRESS_WRAP,
        D3DTEXF_LINEAR,D3DTEXF_POINT,D3DTEXF_NONE,0,converted));
    CHECK(!DecodeSamplerPolicy(D3DTADDRESS_WRAP,D3DTADDRESS_WRAP,
        D3DTEXF_LINEAR,D3DTEXF_LINEAR,D3DTEXF_LINEAR,0,converted));
    CHECK(!DecodeSamplerPolicy(D3DTADDRESS_WRAP,D3DTADDRESS_WRAP,
        D3DTEXF_LINEAR,D3DTEXF_LINEAR,D3DTEXF_NONE,1,converted));
    using m2vr::drawprobe::AlphaCutoffMatches;
    CHECK(AlphaCutoffMatches(0,D3DCMP_ALWAYS,0,0));
    CHECK(!AlphaCutoffMatches(0,D3DCMP_ALWAYS,0,128));
    CHECK(AlphaCutoffMatches(1,D3DCMP_GREATEREQUAL,128,128));
    CHECK(!AlphaCutoffMatches(1,D3DCMP_GREATER,128,128));
    CHECK(!AlphaCutoffMatches(1,D3DCMP_GREATEREQUAL,128,127));
    CHECK(!AlphaCutoffMatches(1,D3DCMP_GREATEREQUAL,256,0));
    return 0;
}
