#include "../src/probe/original_d3d9_lineage.hpp"
#include "../src/probe/original_d3d9_stateblock_audit.hpp"
#include "../src/probe/original_d3d9_stream_frequency.hpp"
#include "../src/probe/original_d3d9_authenticated_draw.hpp"
#include <cassert>
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
    static_assert(kOriginalLineageKinds==30);
    static_assert(static_cast<unsigned>(OriginalLineageKind::Material)==25u);
    static_assert(static_cast<unsigned>(OriginalLineageKind::Light)==26u);
    static_assert(static_cast<unsigned>(OriginalLineageKind::LightEnable)==27u);
    static_assert(static_cast<unsigned>(OriginalLineageKind::ClipPlane)==28u);
    static_assert(static_cast<unsigned>(OriginalLineageKind::TextureUpload)==29u);
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
