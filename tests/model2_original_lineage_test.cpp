#include "../src/probe/original_d3d9_lineage.hpp"
#include "../src/probe/original_d3d9_control_present.hpp"
#include "../src/probe/original_d3d9_scene_draw_coverage.hpp"
#include "../src/probe/original_d3d9_caller_bloom.hpp"
#include "../src/probe/original_d3d9_caller_inventory.hpp"
#include "../src/probe/original_d3d9_validation_policy.hpp"
#include "../src/probe/original_d3d9_resource_owner.hpp"
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
    using namespace m2vr::vrbridge;
    static_assert(DecideOriginalResourceOwner(true,true,true)==
                  OriginalResourceOwner::Primary);
    static_assert(DecideOriginalResourceOwner(true,true,false)==
                  OriginalResourceOwner::Secondary);
    static_assert(DecideOriginalResourceOwner(false,true,true)==
                  OriginalResourceOwner::Unknown);
    static_assert(DecideOriginalResourceOwner(true,false,true)==
                  OriginalResourceOwner::Unknown);
    static_assert(DecideOriginalD3D9Validation(0u,true,1u)==
                  OriginalDeviceValidationDecision::TrustedSinglePass);
    static_assert(DecideOriginalD3D9Validation(0u,true,2u)==
                  OriginalDeviceValidationDecision::MultiplePasses);
    static_assert(DecideOriginalD3D9Validation(0x8876086cu,true,1u)==
                  OriginalDeviceValidationDecision::OriginalFailure);
    static_assert(DecideOriginalD3D9Validation(0u,false,1u)==
                  OriginalDeviceValidationDecision::ResultUnknown);
    static_assert(DecideOriginalD3D9Validation(1u,true,1u)==
                  OriginalDeviceValidationDecision::ResultUnknown);
    static_assert(DecideOriginalD3D9Validation(0u,true,0u)==
                  OriginalDeviceValidationDecision::ResultUnknown);
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
    // Independent four-family physical Draw fingerprint rejects a caller
    // reused by an UP-only control even without sampled UP vertex evidence.
    static_assert(!OriginalDrawCallerCouldBeInBloom({},0x004a1234u));
    constexpr auto caller_bits=OriginalDrawCallerBits(0x004a1234u);
    static_assert(caller_bits.lane0 && caller_bits.lane1);
    static_assert(OriginalDrawCallerCouldBeInBloom(caller_bits,0x004a1234u));
    static_assert(!OriginalDrawCallerCouldBeInBloom(caller_bits,0u));
    constexpr auto second_bits=OriginalDrawCallerBits(0x004bb400u);
    static_assert(OriginalDrawCallerCouldBeInBloom(
        {caller_bits.lane0|second_bits.lane0,
         caller_bits.lane1|second_bits.lane1},0x004bb400u));
    // Fully exact physical original Draw caller inventory, not a 128-bit
    // bloom oracle; long sessions retain independent caller identities.
    // Real post-COM BeginScene/Draw/EndScene gate across all four original
    // families. No source counts are inferred from sampled VB/UP vertices.
    OriginalPhysicalSceneDrawCoverage scene{};
    scene.Begin(true,true);
    scene.Draw(true,true,true); // primary DrawPrimitive COM succeeded
    scene.Draw(true,true,true); // indexed UP original COM succeeded
    scene.End(true,true,true);
    auto good_scene=scene.FinishPresent(false,false,true);
    assert(good_scene.begins==1u && good_scene.ends==1u);
    assert(good_scene.successful_in_scene==2u);
    assert(good_scene.TotalDraws()==2u && good_scene.CompleteSourceScene());
    // Each physical Present starts a fresh scene proof epoch.
    auto empty_scene=scene.FinishPresent(false,false,true);
    assert(empty_scene.TotalDraws()==0u && empty_scene.flags==0u);
    scene.Begin(true,true);
    scene.Draw(true,false,true); // source draw outside original scene
    scene.End(true,true,true);
    auto outside_scene=scene.FinishPresent(false,false,true);
    assert(!outside_scene.CompleteSourceScene());
    assert(outside_scene.successful_untrusted==1u &&
           outside_scene.successful_in_scene==0u);
    scene.Draw(false,false,true); // non-EXE primary Draw is unknown
    auto unknown_scene=scene.FinishPresent(false,false,true);
    assert((unknown_scene.flags&OriginalPhysicalSceneDrawCoverage::kUnknownDraw)!=0u);
    scene.Begin(true,true);
    scene.Draw(true,true,false); // actual failed original COM draw
    scene.End(true,true,true);
    auto failed_scene=scene.FinishPresent(false,false,true);
    assert(failed_scene.failed_draws==1u &&
           !failed_scene.CompleteSourceScene());
    scene.Begin(true,true);
    assert((scene.FinishPresent(true,false,true).flags&
        OriginalPhysicalSceneDrawCoverage::kUnbalanced)!=0u);
    scene.Begin(true,true);
    scene.End(true,true,true);
    assert((scene.FinishPresent(false,true,true).flags&
        OriginalPhysicalSceneDrawCoverage::kFailedScene)!=0u);
    scene.Draw(true,true,true);
    assert((scene.FinishPresent(false,false,false).flags&
        OriginalPhysicalSceneDrawCoverage::kHooksMissing)!=0u);
    scene.Reset();
    assert(scene.FinishPresent(false,false,true).TotalDraws()==0u);
    OriginalDrawCallerInventory exact{};
    assert(exact.Count()==0u && !exact.Incomplete());
    assert(exact.Observe(0x004a1234u)==OriginalCallerInsert::NewCaller);
    assert(exact.Observe(0x004a1234u)==OriginalCallerInsert::AlreadyObserved);
    assert(exact.Observe(0x004a1235u)==OriginalCallerInsert::NewCaller);
    assert(exact.Count()==2u && !exact.Incomplete());
    assert(exact.Observe(0u)==OriginalCallerInsert::Incomplete);
    assert(exact.Incomplete());
    assert(exact.Observe(0x004a1234u)==OriginalCallerInsert::Incomplete);
    static_assert(kOriginalLineageKinds==61);
    static_assert(OriginalControlPresentSample(1u,0u,0u,0u,0u));
    static_assert(OriginalControlPresentSample(8u,100u,5u,21u,8u));
    static_assert(!OriginalControlPresentSample(0u,0u,0u,0u,0u));
    static_assert(!OriginalControlPresentSample(7u,1u,0u,0u,0u));
    static_assert(!OriginalControlPresentSample(8u,10000001u,0u,0u,0u));
    static_assert(!OriginalControlPresentSample(8u,0u,0u,0u,10000001u));
    static_assert(OriginalControlPresentPair(7u,19u)==
                  ((std::uint64_t{7u}<<32u)|19u));
    static_assert(static_cast<unsigned>(OriginalLineageKind::DeviceEvictManagedResources)==56u);
    static_assert(static_cast<unsigned>(OriginalLineageKind::ResourceSetPriority)==57u);
    static_assert(static_cast<unsigned>(OriginalLineageKind::ResourcePreLoad)==58u);
    static_assert(static_cast<unsigned>(OriginalLineageKind::ResourceSetPrivateData)==59u);
    static_assert(static_cast<unsigned>(OriginalLineageKind::ResourceFreePrivateData)==60u);
    // A genuine primary original Resource9 metadata update cannot retain
    // a previous same-physical-Present CPU/GPU world source nonce.
    OriginalLineageStamp private_before{};
    private_before.physical_present=97u;
    private_before.successful_writes[static_cast<std::size_t>(
        OriginalLineageKind::View)]=1u;
    assert(OriginalLineageUsable(private_before));
    auto private_after_set=private_before;
    private_after_set.successful_writes[static_cast<std::size_t>(
        OriginalLineageKind::ResourceSetPrivateData)]=1u;
    assert(!SameOriginalLineage(private_before,private_after_set));
    assert(OriginalLineageNonce(private_before)!=
           OriginalLineageNonce(private_after_set));
    auto private_after_free=private_after_set;
    private_after_free.successful_writes[static_cast<std::size_t>(
        OriginalLineageKind::ResourceFreePrivateData)]=1u;
    assert(!SameOriginalLineage(private_after_set,private_after_free));
    assert(OriginalLineageNonce(private_after_set)!=
           OriginalLineageNonce(private_after_free));
    static_assert(static_cast<unsigned>(OriginalLineageKind::DeviceDialogBoxMode)==49u);
    static_assert(static_cast<unsigned>(OriginalLineageKind::DeviceGammaRamp)==50u);
    static_assert(static_cast<unsigned>(OriginalLineageKind::DeviceCursorProperties)==51u);
    static_assert(static_cast<unsigned>(OriginalLineageKind::DeviceCursorPosition)==52u);
    static_assert(static_cast<unsigned>(OriginalLineageKind::DeviceCursorVisibility)==53u);
    static_assert(static_cast<unsigned>(OriginalLineageKind::QueryIssue)==54u);
    static_assert(static_cast<unsigned>(OriginalLineageKind::QueryReady)==55u);
    static_assert(static_cast<unsigned>(OriginalLineageKind::GpuSurfaceContents)==48u);
    static_assert(static_cast<unsigned>(OriginalLineageKind::ProcessVertices)==35u);
    static_assert(static_cast<unsigned>(OriginalLineageKind::TextureTransform)==36u);
    static_assert(static_cast<unsigned>(OriginalLineageKind::OtherTransform)==37u);
    static_assert(static_cast<unsigned>(OriginalLineageKind::VertexBufferContent)==38u);
    static_assert(static_cast<unsigned>(OriginalLineageKind::IndexBufferContent)==39u);
    static_assert(static_cast<unsigned>(OriginalLineageKind::TextureLockContent)==40u);
    static_assert(static_cast<unsigned>(OriginalLineageKind::CubeTextureContent)==41u);
    static_assert(static_cast<unsigned>(OriginalLineageKind::VolumeTextureContent)==42u);
    static_assert(static_cast<unsigned>(OriginalLineageKind::SurfaceTexelContent)==43u);
    static_assert(static_cast<unsigned>(OriginalLineageKind::TextureMipGeneration)==44u);
    static_assert(static_cast<unsigned>(OriginalLineageKind::TextureDirtyRegion)==45u);
    static_assert(static_cast<unsigned>(OriginalLineageKind::TextureAutoGenFilter)==46u);
    static_assert(static_cast<unsigned>(OriginalLineageKind::TextureManagedLOD)==47u);
    OriginalBoundWriteLocks active{};
    assert(!active.HasOutstandingWrites());
    assert(active.Begin(reinterpret_cast<const void*>(0x1234)));
    assert(active.HasOutstandingWrites());
    assert(active.Contains(reinterpret_cast<const void*>(0x1234)));
    assert(!active.Contains(reinterpret_cast<const void*>(0x5678)));
    assert(!active.Begin(reinterpret_cast<const void*>(0x1234)));
    active.SuccessfulUnlock(reinterpret_cast<const void*>(0x5678));
    assert(active.HasOutstandingWrites());
    active.SuccessfulUnlock(reinterpret_cast<const void*>(0x1234));
    assert(!active.HasOutstandingWrites());
    assert(active.Begin(reinterpret_cast<const void*>(0x1234)));
    active.AfterSuccessfulDeviceReset();
    assert(!active.HasOutstandingWrites());
    // At most 64 outstanding original writable resource mappings. The 65th
    // must fail closed without overwriting any earlier pointer state.
    for(std::uintptr_t i=0;i<64u;++i)
        assert(active.Begin(reinterpret_cast<const void*>(0x1000u+i*16u)));
    assert(active.HasOutstandingWrites());
    assert(!active.Begin(reinterpret_cast<const void*>(0x9000u)));
    active.SuccessfulUnlock(reinterpret_cast<const void*>(0x1000u));
    assert(active.Begin(reinterpret_cast<const void*>(0x9000u)));
    active.AfterSuccessfulDeviceReset();
    assert(!active.HasOutstandingWrites());
    assert(!active.Contains(reinterpret_cast<const void*>(0x9000u)));
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
    // A successful original GPU Surface write is a distinct source mutation
    // even without CPU LockRect/texture binds; never recycle earlier eyes.
    static_assert(static_cast<unsigned>(OriginalLineageKind::GpuSurfaceContents)==48u);
    auto gpu_surface_changed=initial;
    ++gpu_surface_changed.successful_writes[48];
    assert(!SameOriginalLineage(initial,gpu_surface_changed));
    assert(OriginalLineageNonce(initial)!=OriginalLineageNonce(gpu_surface_changed));
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
