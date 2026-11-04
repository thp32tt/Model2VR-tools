// Original D3D9 source stage policy is fail-closed for unsupported ops.
// Synthetic Win32 gate, no emulator/Quest/FFB/device required.
#include "../src/probe/d3d9_fixed_stage_policy.hpp"
#include <cstdio>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"fixed stage failure %d\n",__LINE__);return 1;}}while(0)
using namespace m2vr::drawprobe;
int main(){
    bool modulate=true;
    CHECK(DecodeFixedTextureDiffuseOp(
        D3DTOP_SELECTARG1,D3DTA_TEXTURE,0,modulate) && !modulate);
    CHECK(DecodeFixedTextureDiffuseOp(
        D3DTOP_SELECTARG2,D3DTA_TFACTOR,D3DTA_TEXTURE,modulate) &&
        !modulate);
    CHECK(DecodeFixedTextureDiffuseOp(
        D3DTOP_MODULATE,D3DTA_TEXTURE,D3DTA_DIFFUSE,modulate) &&
        modulate);
    CHECK(DecodeFixedTextureDiffuseOp(
        D3DTOP_MODULATE,D3DTA_DIFFUSE,D3DTA_TEXTURE,modulate) &&
        modulate);
    CHECK(!DecodeFixedTextureDiffuseOp(
        D3DTOP_MODULATE,D3DTA_TEXTURE,D3DTA_TFACTOR,modulate));
    CHECK(!DecodeFixedTextureDiffuseOp(
        D3DTOP_MODULATE,D3DTA_TEXTURE|D3DTA_COMPLEMENT,
        D3DTA_DIFFUSE,modulate));
    CHECK(!DecodeFixedTextureDiffuseOp(
        D3DTOP_SELECTARG1,D3DTA_DIFFUSE,D3DTA_TEXTURE,modulate));
    CHECK(!DecodeFixedTextureDiffuseOp(
        D3DTOP_SELECTARG2,D3DTA_TEXTURE,D3DTA_DIFFUSE,modulate));
    CHECK(!DecodeFixedTextureDiffuseOp(
        D3DTOP_SELECTARG2,D3DTA_DIFFUSE,
        D3DTA_TEXTURE|D3DTA_ALPHAREPLICATE,modulate));
    CHECK(!DecodeFixedTextureDiffuseOp(
        D3DTOP_ADD,D3DTA_DIFFUSE,D3DTA_TEXTURE,modulate));
    CHECK(!DecodeFixedTextureDiffuseOp(
        D3DTOP_DISABLE,D3DTA_TEXTURE,D3DTA_DIFFUSE,modulate));
    // Independently selected stage-0 RGB and alpha may use texture,
    // original per-vertex diffuse, or original texel*diffuse.
    FixedStageSource source=FixedStageSource::Modulate;
    CHECK(DecodeFixedStageSource(
        D3DTOP_SELECTARG1,D3DTA_DIFFUSE,D3DTA_TEXTURE,source));
    CHECK(source==FixedStageSource::Diffuse);
    CHECK(DecodeFixedStageSource(
        D3DTOP_SELECTARG2,D3DTA_TEXTURE,D3DTA_DIFFUSE,source));
    CHECK(source==FixedStageSource::Diffuse);
    CHECK(DecodeFixedStageSource(
        D3DTOP_SELECTARG2,D3DTA_TFACTOR,D3DTA_TEXTURE,source));
    CHECK(source==FixedStageSource::Texture);
    CHECK(DecodeFixedStageSource(
        D3DTOP_MODULATE,D3DTA_DIFFUSE,D3DTA_TEXTURE,source));
    CHECK(source==FixedStageSource::Modulate);
    const auto before=source;
    CHECK(!DecodeFixedStageSource(
        D3DTOP_SELECTARG1,D3DTA_DIFFUSE|D3DTA_COMPLEMENT,
        D3DTA_TEXTURE,source));
    CHECK(!DecodeFixedStageSource(
        D3DTOP_MODULATE,D3DTA_TEXTURE,D3DTA_TFACTOR,source));
    CHECK(!DecodeFixedStageSource(
        D3DTOP_SELECTARG1,D3DTA_CURRENT,D3DTA_TEXTURE,source));
    CHECK(!DecodeFixedStageSource(
        D3DTOP_ADD,D3DTA_TEXTURE,D3DTA_DIFFUSE,source));
    CHECK(source==before); // failure never partially mutates policy
    CHECK(ValidFixedStageSource(FixedStageSource::Diffuse));
    CHECK(!ValidFixedStageSource(static_cast<FixedStageSource>(5u)));
    return 0;
}
