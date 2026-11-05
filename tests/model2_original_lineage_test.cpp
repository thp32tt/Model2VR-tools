#include "../src/probe/original_d3d9_lineage.hpp"
#include "../src/probe/original_d3d9_buffer_mutation.hpp"
#include "../src/probe/original_d3d9_transform_lineage.hpp"
#include "../src/probe/original_d3d9_stateblock_audit.hpp"
#include "../src/probe/original_d3d9_stream_frequency.hpp"
#include "../src/probe/original_d3d9_raster_mode.hpp"
#include "../src/probe/original_d3d9_unmirrored_geometry.hpp"
#include "../src/probe/original_d3d9_authenticated_draw.hpp"
#include <cassert>
#include <limits>
int main(){
    using namespace m2vr::drawprobe;
    OriginalStreamFrequencyState freq{};
    assert(freq.UnsupportedMask()==0u);
    freq.SuccessfulOriginalWrite(0u,0x40000002u);
    assert(freq.UnsupportedMask()==1u);
    freq.SuccessfulOriginalWrite(1u,0x80000001u);
    assert(freq.UnsupportedMask()==3u);
    freq.SuccessfulOriginalWrite(0u,1u);
    assert(freq.UnsupportedMask()==2u); // other instanced stream persists
    freq.SuccessfulOriginalWrite(1u,1u);
    assert(freq.UnsupportedMask()==0u);
    freq.SuccessfulOriginalWrite(17u,2u);
    assert(freq.UnsupportedMask()==OriginalStreamFrequencyState::kUnknownStream);
    freq.AfterSuccessfulDeviceReset();
    assert(freq.UnsupportedMask()==0u);
    OriginalStateBlockAuditOncePerPresent audit{};
    assert(!audit.Claim(0u));
    assert(audit.Claim(77u));
    assert(!audit.Claim(77u));
    assert(!audit.Claim(76u));
    assert(audit.Claim(78u));
    assert(!audit.Claim(77u));
    static_assert(kOriginalLineageKinds==40);
    static_assert(static_cast<unsigned>(OriginalLineageKind::ProcessVertices)==35u);
    static_assert(static_cast<unsigned>(OriginalLineageKind::TextureTransform)==36u);
    static_assert(static_cast<unsigned>(OriginalLineageKind::OtherTransform)==37u);
    static_assert(static_cast<unsigned>(OriginalLineageKind::VertexBufferContent)==38u);
    static_assert(static_cast<unsigned>(OriginalLineageKind::IndexBufferContent)==39u);
    OriginalBoundWriteLocks active{};
    assert(!active.HasOutstandingWrites());
    assert(active.Begin(reinterpret_cast<const void*>(0x1234)));
    assert(active.HasOutstandingWrites());
    assert(!active.Begin(reinterpret_cast<const void*>(0x1234)));
    active.SuccessfulUnlock(reinterpret_cast<const void*>(0x5678));
    assert(active.HasOutstandingWrites());
    active.SuccessfulUnlock(reinterpret_cast<const void*>(0x1234));
    assert(!active.HasOutstandingWrites());
    assert(active.Begin(reinterpret_cast<const void*>(0x1234)));
    active.AfterSuccessfulDeviceReset();
    assert(!active.HasOutstandingWrites());
    static_assert(IsWritableBufferLock(0u));
    static_assert(IsWritableBufferLock(0x2000u)); // DISCARD
    static_assert(IsWritableBufferLock(0x1000u)); // NOOVERWRITE
    static_assert(!IsWritableBufferLock(0x10u)); // READONLY
    static_assert(OriginalBufferFailureMethodValid(37u));
    static_assert(OriginalBufferFailureMethodValid(40u));
    static_assert(!OriginalBufferFailureMethodValid(36u));
    static_assert(!OriginalBufferFailureMethodValid(41u));
    static_assert(ClassifyOriginalTransform(2u)==OriginalTransformCategory::View);
    static_assert(ClassifyOriginalTransform(3u)==OriginalTransformCategory::Projection);
    static_assert(ClassifyOriginalTransform(256u)==OriginalTransformCategory::World);
    static_assert(ClassifyOriginalTransform(511u)==OriginalTransformCategory::World);
    static_assert(ClassifyOriginalTransform(16u)==OriginalTransformCategory::Texture);
    static_assert(ClassifyOriginalTransform(23u)==OriginalTransformCategory::Texture);
    static_assert(ClassifyOriginalTransform(24u)==OriginalTransformCategory::Other);
    static_assert(OriginalTransformLineageKind(OriginalTransformCategory::Texture)==OriginalLineageKind::TextureTransform);
    static_assert(OriginalTransformLineageKind(OriginalTransformCategory::Other)==OriginalLineageKind::OtherTransform);
    OriginalUnmirroredGeometryState unmirrored{};
    assert(!unmirrored.HasProcessedVertices());
    unmirrored.SuccessfulProcessVertices();
    assert(unmirrored.HasProcessedVertices());
    unmirrored.AfterSuccessfulDeviceReset();
    assert(!unmirrored.HasProcessedVertices());
    static_assert(static_cast<unsigned>(OriginalLineageKind::ClipStatus)==32u);
    static_assert(static_cast<unsigned>(OriginalLineageKind::SoftwareVertexProcessing)==33u);
    static_assert(static_cast<unsigned>(OriginalLineageKind::NPatchMode)==34u);
    OriginalRasterModeState mode{};
    assert(mode.UnsupportedMask()==0u);
    mode.SuccessfulSoftwareVertexWrite(true);
    assert(mode.UnsupportedMask()==OriginalRasterModeState::kSoftwareVertex);
    mode.SuccessfulNPatchWrite(3.0f);
    assert(mode.UnsupportedMask()==3u);
    mode.SuccessfulSoftwareVertexWrite(false);
    assert(mode.UnsupportedMask()==OriginalRasterModeState::kNPatch);
    mode.SuccessfulNPatchWrite(1.0f);
    assert(!mode.UnsupportedMask());
    mode.SuccessfulNPatchWrite(0.0f);
    assert(!mode.UnsupportedMask());
    mode.SuccessfulNPatchWrite(-1.0f);
    assert(mode.UnsupportedMask()==OriginalRasterModeState::kNPatch);
    mode.SuccessfulNPatchWrite(2.0f);
    mode.SuccessfulNPatchWrite(std::numeric_limits<float>::quiet_NaN());
    assert(mode.UnsupportedMask()==OriginalRasterModeState::kNPatch);
    mode.AfterSuccessfulDeviceReset();
    assert(!mode.UnsupportedMask());
    static_assert(static_cast<unsigned>(OriginalLineageKind::Material)==25u);
    static_assert(static_cast<unsigned>(OriginalLineageKind::Light)==26u);
    static_assert(static_cast<unsigned>(OriginalLineageKind::LightEnable)==27u);
    static_assert(static_cast<unsigned>(OriginalLineageKind::ClipPlane)==28u);
    static_assert(static_cast<unsigned>(OriginalLineageKind::TextureUpload)==29u);
    static_assert(static_cast<unsigned>(OriginalLineageKind::PaletteEntries)==30u);
    static_assert(static_cast<unsigned>(OriginalLineageKind::CurrentPalette)==31u);
    OriginalLineageStamp initial{77,{}};
    assert(!OriginalLineageUsable(initial));
    assert(OriginalLineageNonce(initial)==0);
    initial.successful_writes[0]=1;
    initial.successful_writes[1]=2;
    initial.successful_writes[4]=3;
    initial.successful_writes[6]=4;
    assert(OriginalLineageUsable(initial));
    assert(OriginalLineageNonce(initial)!=0);
    assert(SameOriginalLineage(initial,initial));
    for(std::size_t i=0;i<kOriginalLineageKinds;++i){
        auto next=initial;
        ++next.successful_writes[i];
        assert(!SameOriginalLineage(initial,next));
        assert(OriginalLineageNonce(initial)!=OriginalLineageNonce(next));
    }
    // Geometry, shader, texture and raster mutations change the Draw nonce.
    for(std::size_t kind=8;kind<kOriginalLineageKinds;++kind){
        auto next=initial;
        ++next.successful_writes[kind];
        assert(OriginalLineageNonce(initial)!=OriginalLineageNonce(next));
    }
    auto texture_changed=initial;
    ++texture_changed.successful_writes[static_cast<std::size_t>(OriginalLineageKind::TextureTransform)];
    assert(OriginalLineageNonce(initial)!=OriginalLineageNonce(texture_changed));
    auto unclassified_changed=initial;
    ++unclassified_changed.successful_writes[static_cast<std::size_t>(OriginalLineageKind::OtherTransform)];
    assert(!SameOriginalLineage(initial,unclassified_changed));
    auto vertex_contents=initial;
    ++vertex_contents.successful_writes[38];
    assert(!SameOriginalLineage(initial,vertex_contents));
    assert(OriginalLineageNonce(initial)!=OriginalLineageNonce(vertex_contents));
    auto index_contents=initial;
    ++index_contents.successful_writes[39];
    assert(!SameOriginalLineage(initial,index_contents));
    assert(OriginalLineageNonce(initial)!=OriginalLineageNonce(index_contents));
    auto new_frame=initial;
    ++new_frame.physical_present;
    assert(!SameOriginalLineage(initial,new_frame));
    assert(OriginalLineageNonce(initial)!=OriginalLineageNonce(new_frame));
    using namespace m2vr::vrbridge;
    ArmedOriginalDraw rule{};
    rule.frame=77u;rule.ordinal=5u;rule.family=2u;
    rule.caller=0x345u;rule.scene=3u;
    rule.expected_lineage_nonce=OriginalLineageNonce(initial);
    OriginalDrawOneShot one_shot{};
    ArmedOriginalDraw accepted{};
    bool mismatch=false;
    assert(one_shot.Arm(rule));
    // Mismatched draw identity does not consume the one-shot token.
    assert(!one_shot.Take(77u,4u,2u,0x345u,3u,true,accepted,
                          rule.expected_lineage_nonce,&mismatch));
    assert(one_shot.Pending() && !mismatch);
    // Actual matching Draw with mutated source state revokes the one-shot.
    auto changed=initial;
    ++changed.successful_writes[6];
    assert(!one_shot.Take(77u,5u,2u,0x345u,3u,true,accepted,
                         OriginalLineageNonce(changed),&mismatch));
    assert(mismatch && !one_shot.Pending());
    assert(!one_shot.Take(77u,5u,2u,0x345u,3u,true,accepted,
                          rule.expected_lineage_nonce,&mismatch));
    assert(one_shot.Arm(rule));
    assert(one_shot.Take(77u,5u,2u,0x345u,3u,true,accepted,
                         rule.expected_lineage_nonce,&mismatch));
    assert(!mismatch && !one_shot.Pending());
    assert(accepted.expected_lineage_nonce==rule.expected_lineage_nonce);
    rule.expected_lineage_nonce=OriginalLineageNonce(initial);
    assert(one_shot.Arm(rule));
    auto rebound=initial;
    ++rebound.successful_writes[static_cast<std::size_t>(OriginalLineageKind::StreamFrequency)];
    assert(!one_shot.Take(77u,5u,2u,0x345u,3u,true,accepted,
                         OriginalLineageNonce(rebound),&mismatch));
    assert(mismatch && !one_shot.Pending());
    // Fixed-function mutations invalidate the exact original Draw one-shot.
    for(auto kind:{OriginalLineageKind::Material,OriginalLineageKind::Light,
                   OriginalLineageKind::LightEnable,OriginalLineageKind::ClipPlane}){
        auto token=rule;
        token.expected_lineage_nonce=OriginalLineageNonce(initial);
        assert(one_shot.Arm(token));
        auto changed_fixed=initial;
        ++changed_fixed.successful_writes[static_cast<std::size_t>(kind)];
        assert(!one_shot.Take(77u,5u,2u,0x345u,3u,true,accepted,
            OriginalLineageNonce(changed_fixed),&mismatch));
        assert(mismatch && !one_shot.Pending());
    }
    // Legacy ABI is unchanged; the external world-proof gate remains.
    rule.expected_lineage_nonce=0u;
    assert(one_shot.Arm(rule));
    assert(one_shot.Take(77u,5u,2u,0x345u,3u,true,accepted,0u,&mismatch));
    assert(!mismatch);
    initial.successful_writes[3]=kMaxLineageWritesPerPresent+1;
    assert(!OriginalLineageUsable(initial));
    assert(OriginalLineageNonce(initial)==0);
}
