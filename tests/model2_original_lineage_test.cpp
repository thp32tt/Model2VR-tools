#include "../src/probe/original_d3d9_lineage.hpp"
#include "../src/probe/original_d3d9_control_present.hpp"
#include "../src/probe/original_d3d9_scene_draw_coverage.hpp"
#include "../src/probe/original_d3d9_draw_target_coverage.hpp"
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
    static_assert(OriginalDrawRetainedSourceScene(
        true,true,true,4u,4u,true));
    static_assert(!OriginalDrawRetainedSourceScene(
        true,false,true,4u,4u,true)); // scene closed during COM
    static_assert(!OriginalDrawRetainedSourceScene(
        true,true,true,4u,5u,true)); // BeginScene epoch changed
    static_assert(!OriginalDrawRetainedSourceScene(
        true,true,true,4u,4u,false)); // another render thread
    static_assert(!OriginalDrawRetainedSourceScene(
        true,true,false,4u,4u,true)); // original failed Scene
    static_assert(!OriginalDrawRetainedSourceScene(
        false,true,true,4u,4u,true)); // scene opened during Draw
    OriginalPhysicalSceneDrawCoverage scene{};
    scene.Begin(true,true);
    scene.Draw(true,true,true); // primary DrawPrimitive COM succeeded
    scene.Draw(true,true,true); // indexed UP original COM succeeded
    scene.End(true,true,true);
    auto good_scene=scene.FinishPresent(false,false,true);
    assert(good_scene.begins==1u && good_scene.ends==1u);
    assert(good_scene.successful_in_scene==2u);
    assert(good_scene.TotalDraws()==2u && good_scene.CompleteSourceScene());
    assert(!good_scene.RejectUnprovenStereoSource());
    // Each physical Present starts a fresh scene proof epoch.
    auto empty_scene=scene.FinishPresent(false,false,true);
    assert(empty_scene.TotalDraws()==0u && empty_scene.flags==0u);
    assert(!empty_scene.RejectUnprovenStereoSource());
    scene.Begin(true,true);
    scene.Draw(true,false,true); // source draw outside original scene
    scene.End(true,true,true);
    auto outside_scene=scene.FinishPresent(false,false,true);
    assert(!outside_scene.CompleteSourceScene());
    assert(outside_scene.successful_untrusted==1u &&
           outside_scene.successful_in_scene==0u);
    assert(outside_scene.RejectUnprovenStereoSource());
    scene.Draw(false,false,true); // non-EXE primary Draw is unknown
    auto unknown_scene=scene.FinishPresent(false,false,true);
    assert((unknown_scene.flags&OriginalPhysicalSceneDrawCoverage::kUnknownDraw)!=0u);
    assert(unknown_scene.RejectUnprovenStereoSource());
    scene.Begin(true,true);
    scene.Draw(true,true,false); // actual failed original COM draw
    scene.End(true,true,true);
    auto failed_scene=scene.FinishPresent(false,false,true);
    assert(failed_scene.failed_draws==1u &&
           !failed_scene.CompleteSourceScene());
    assert(failed_scene.RejectUnprovenStereoSource());
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
    // Genuine operations now carry a physical-Present epoch through the
    // ACTUAL original COM call. All four Draw families share this contract.
    scene.ReopenAfterPhysicalPresentReturn();
    const auto scene_begin_ticket=scene.EnterOperation();
    scene.Begin(scene_begin_ticket,true,true);
    for(unsigned family=0u;family<4u;++family){
        const auto draw_ticket=scene.EnterOperation();
        scene.Draw(draw_ticket,true,true,true);
    }
    const auto scene_end_ticket=scene.EnterOperation();
    scene.End(scene_end_ticket,true,true,true);
    const auto ticketed_good=scene.FinishPresent(false,false,true);
    assert(ticketed_good.CompleteSourceScene() &&
           ticketed_good.successful_in_scene==4u &&
           ticketed_good.flags==0u);
    scene.ReopenAfterPhysicalPresentReturn();
    // An original Draw that has entered COM but has not returned at
    // physical Present invalidates THIS frame even when earlier Draws pass.
    scene.Begin(true,true);
    scene.Draw(true,true,true);
    scene.End(true,true,true);
    const auto in_flight=scene.EnterOperation();
    const auto in_flight_frame=scene.FinishPresent(false,false,true);
    assert((in_flight_frame.flags&
            OriginalPhysicalSceneDrawCoverage::kInFlightAtPresent)!=0u);
    assert(in_flight_frame.RejectUnprovenStereoSource());
    scene.ReopenAfterPhysicalPresentReturn();
    // Its late successful COM completion must NOT become a trusted Draw of
    // the next physical Present, including if source Scene appears valid.
    scene.Draw(in_flight,true,true,true);
    const auto late_frame=scene.FinishPresent(false,false,true);
    assert((late_frame.flags&
            OriginalPhysicalSceneDrawCoverage::kCrossPresent)!=0u);
    assert(late_frame.successful_in_scene==0u);
    assert(late_frame.RejectUnprovenStereoSource());
    // The small interval before the original counter actually advances
    // cannot admit Scene/Draw operations for either source frame.
    const auto during_cut=scene.EnterOperation();
    scene.Draw(during_cut,true,true,true);
    const auto cut_frame=scene.FinishPresent(false,false,true);
    assert((cut_frame.flags&
            OriginalPhysicalSceneDrawCoverage::kCrossPresent)!=0u);
    scene.ReopenAfterPhysicalPresentReturn();
    // Reset also invalidates an old original COM return without losing it.
    const auto reset_ticket=scene.EnterOperation();
    scene.Reset();
    scene.Draw(reset_ticket,true,true,true);
    const auto reset_frame=scene.FinishPresent(false,false,true);
    assert((reset_frame.flags&
            OriginalPhysicalSceneDrawCoverage::kCrossPresent)!=0u);
    assert(reset_frame.successful_in_scene==0u);
    scene.ReopenAfterPhysicalPresentReturn();
    const auto recovered_ticket=scene.EnterOperation();
    scene.Draw(recovered_ticket,true,true,true);
    const auto recovered_frame=scene.FinishPresent(false,false,true);
    assert(recovered_frame.successful_in_scene==1u &&
           recovered_frame.flags==0u);
    scene.ReopenAfterPhysicalPresentReturn();
    const auto live_ticket=scene.EnterOperation();
    // A ticket-less (capacity-rejected) operation must not consume the
    // outstanding real original Draw's slot or misattribute its result.
    scene.Draw(OriginalPhysicalSceneDrawCoverage::OperationTicket{0u},
               true,true,true);
    scene.Draw(live_ticket,true,true,true);
    const auto saturated=scene.FinishPresent(false,false,true);
    assert(saturated.successful_in_scene==1u &&
           (saturated.flags&OriginalPhysicalSceneDrawCoverage::kOverflow)!=0u &&
           (saturated.flags&OriginalPhysicalSceneDrawCoverage::kCrossPresent)==0u &&
           (saturated.flags&OriginalPhysicalSceneDrawCoverage::kInFlightAtPresent)==0u);
    // Actual RT0/depth provenance now accompanies each of the four original
    // physical Draw COM methods and survives scene/target bind changes.
    static_assert(OriginalPhysicalDepthCompatible(
        1024u,1024u,0u,0u,1024u,1024u,0u,0u));
    static_assert(OriginalPhysicalDepthCompatible(
        2048u,1024u,4u,2u,2048u,1024u,4u,2u));
    static_assert(!OriginalPhysicalDepthCompatible(
        2048u,1024u,4u,2u,2048u,1024u,0u,0u));
    static_assert(!OriginalPhysicalDepthCompatible(
        2048u,1024u,4u,2u,2048u,1024u,4u,1u));
    static_assert(!OriginalPhysicalDepthCompatible(
        2048u,1024u,4u,2u,2048u,1023u,4u,2u));
    static_assert(!OriginalPhysicalDepthCompatible(
        0u,1024u,0u,0u,0u,1024u,0u,0u));
    const OriginalTargetSnapshot main_depth{
        OriginalPhysicalRtClass::Backbuffer,OriginalPhysicalDepthClass::Compatible};
    const OriginalTargetSnapshot offscreen_depth{
        OriginalPhysicalRtClass::Auxiliary,OriginalPhysicalDepthClass::Compatible};
    const OriginalTargetSnapshot bad_depth{
        OriginalPhysicalRtClass::Backbuffer,OriginalPhysicalDepthClass::Incompatible};
    static_assert(OriginalPhysicalSampleCompletion{
        true,true,true,true}.CanRemainCandidate());
    static_assert(OriginalPhysicalSampleCompletion{
        true,true,true,true}.Encode()==15u);
    static_assert(!OriginalPhysicalSampleCompletion{
        false,true,true,true}.CanRemainCandidate());
    static_assert(!OriginalPhysicalSampleCompletion{
        true,false,true,true}.CanRemainCandidate());
    static_assert(!OriginalPhysicalSampleCompletion{
        true,true,false,true}.CanRemainCandidate());
    static_assert(!OriginalPhysicalSampleCompletion{
        true,true,true,false}.CanRemainCandidate());
    OriginalPhysicalTargetDrawCoverage targets{};
    const auto cold=targets.Revision();
    assert(targets.NeedsResolution());
    targets.ResolveIfUnchanged(cold,main_depth);
    assert(!targets.NeedsResolution());
    assert(!targets.CanAttemptVerifiedWorldDraw());
    auto inferred=targets.EnterDraw();
    targets.CompleteDraw(inferred,true,true);
    auto inferred_summary=targets.FinishPresent();
    assert(inferred_summary.backbuffer==1u &&
           (inferred_summary.flags&
            OriginalPhysicalTargetDrawCoverage::kUnownedTarget)!=0u);
    targets.ReopenAfterPhysicalPresentReturn();
    targets.RenderTargetBind(true,true,0u,true,main_depth);
    targets.DepthBind(true,true,main_depth);
    assert(!targets.CanAttemptVerifiedWorldDraw());
    // A clean original RT0+Z is NOT sufficient without real MRT slots.
    auto missing_mrt=targets.EnterDraw();
    targets.CompleteDraw(missing_mrt,true,true);
    assert((targets.FinishPresent().flags&
            OriginalPhysicalTargetDrawCoverage::kMrtUnresolved)!=0u);
    targets.ReopenAfterPhysicalPresentReturn();
    const auto mrt_revision=targets.Revision();
    targets.ResolveSecondaryIfUnchanged(mrt_revision,{0u,true});
    assert(targets.CanAttemptVerifiedWorldDraw());
    for(unsigned family=0;family<4u;++family){
        (void)family;
        auto pass=targets.EnterDraw();
        assert(targets.CompleteDraw(pass,true,true));
    }
    auto trusted_targets=targets.FinishPresent();
    assert(trusted_targets.backbuffer==4u &&
           trusted_targets.TotalDraws()==4u &&
           !trusted_targets.RejectUnprovenStereoSource());
    targets.ReopenAfterPhysicalPresentReturn();
    auto changing=targets.EnterDraw();
    targets.RenderTargetBind(true,true,0u,true,offscreen_depth);
    targets.CompleteDraw(changing,true,true);
    auto drift=targets.FinishPresent();
    assert((drift.flags&OriginalPhysicalTargetDrawCoverage::kTargetDrift)!=0u &&
           drift.unknown==1u && drift.RejectUnprovenStereoSource());
    targets.ReopenAfterPhysicalPresentReturn();
    assert(!targets.CanAttemptVerifiedWorldDraw()); // auxiliary is not an eye
    auto aux_draw=targets.EnterDraw();
    targets.CompleteDraw(aux_draw,true,true);
    auto auxiliary=targets.FinishPresent();
    assert(auxiliary.auxiliary==1u &&
           (auxiliary.flags&OriginalPhysicalTargetDrawCoverage::kAuxiliarySource)!=0u &&
           auxiliary.RejectUnprovenStereoSource());
    targets.ReopenAfterPhysicalPresentReturn();
    targets.DepthBind(true,true,bad_depth);
    auto depth_draw=targets.EnterDraw();
    targets.CompleteDraw(depth_draw,true,true);
    auto incompatible=targets.FinishPresent();
    assert((incompatible.flags&
           OriginalPhysicalTargetDrawCoverage::kInvalidDepth)!=0u);
    assert(incompatible.incompatible_depth==1u);
    targets.ReopenAfterPhysicalPresentReturn();
    targets.DepthBind(true,true,main_depth);
    auto pending=targets.EnterDraw();
    assert((targets.FinishPresent().flags&
            OriginalPhysicalTargetDrawCoverage::kInFlight)!=0u);
    targets.ReopenAfterPhysicalPresentReturn();
    targets.CompleteDraw(pending,true,true);
    const auto late_target=targets.FinishPresent();
    assert((late_target.flags&
            OriginalPhysicalTargetDrawCoverage::kCrossPresent)!=0u);
    targets.ReopenAfterPhysicalPresentReturn();
    const auto reset_target=targets.EnterDraw();
    targets.Reset();
    targets.CompleteDraw(reset_target,true,true);
    assert((targets.FinishPresent().flags&
            OriginalPhysicalTargetDrawCoverage::kCrossPresent)!=0u);
    targets.ReopenAfterPhysicalPresentReturn();
    targets.RenderTargetBind(true,true,0u,true,main_depth);
    targets.DepthBind(true,true,main_depth);
    const auto before_malformed=targets.EnterDraw();
    targets.CompleteDraw({},true,true); // a failed ticket cannot steal a slot
    targets.CompleteDraw(before_malformed,true,true);
    auto recovered_target=targets.FinishPresent();
    assert(recovered_target.backbuffer==1u &&
           (recovered_target.flags&
            OriginalPhysicalTargetDrawCoverage::kOverflow)!=0u &&
           (recovered_target.flags&
            OriginalPhysicalTargetDrawCoverage::kInFlight)==0u);
    targets.ReopenAfterPhysicalPresentReturn();
    targets.RenderTargetBind(true,true,0u,true,main_depth);
    targets.DepthBind(true,true,main_depth);
    // A successful real MRT slot 1 bind persists until the original EXE
    // explicitly unbinds it. It is not a single RT0 OpenXR source.
    targets.RenderTargetBind(true,true,1u,true,offscreen_depth);
    assert(!targets.CanAttemptVerifiedWorldDraw());
    auto mrt=targets.EnterDraw();targets.CompleteDraw(mrt,true,true);
    auto active_mrt=targets.FinishPresent();
    assert(active_mrt.backbuffer==1u &&
           (active_mrt.flags&
            OriginalPhysicalTargetDrawCoverage::kMrtActive)!=0u);
    targets.ReopenAfterPhysicalPresentReturn();
    targets.RenderTargetBind(true,true,1u,false,offscreen_depth);
    auto normal=targets.EnterDraw();targets.CompleteDraw(normal,true,true);
    assert(targets.FinishPresent().flags==0u);
    targets.ReopenAfterPhysicalPresentReturn();
    auto stateblock=targets.EnterDraw();
    targets.InvalidateOpaqueStateBlock();
    assert(!targets.CanAttemptVerifiedWorldDraw());
    targets.CompleteDraw(stateblock,true,true);
    auto opaque=targets.FinishPresent();
    assert((opaque.flags&
            OriginalPhysicalTargetDrawCoverage::kOpaqueStateBlock)!=0u);
    assert((opaque.flags&
            OriginalPhysicalTargetDrawCoverage::kTargetDrift)!=0u);
    targets.ReopenAfterPhysicalPresentReturn();
    targets.RenderTargetBind(false,true,0u,true,offscreen_depth);
    auto foreign=targets.EnterDraw();
    targets.CompleteDraw(foreign,false,true);
    auto foreign_result=targets.FinishPresent();
    assert((foreign_result.flags&
            OriginalPhysicalTargetDrawCoverage::kForeignBind)!=0u);
    assert(foreign_result.RejectUnprovenStereoSource());
    // Genuine original x86 SetRT/SetDepth are IN-FLIGHT source mutations.
    // No eye raster may run during the real COM call, even when the old
    // RT0/depth looked compatible before it entered.
    // Offscreen then backbuffer within one physical Present must reject
    // BOTH speculative OpenXR eyes; real original mono Draws still execute.
    OriginalPhysicalTargetDrawCoverage mixed_rt{};
    mixed_rt.RenderTargetBind(true,true,0u,true,main_depth);
    mixed_rt.DepthBind(true,true,main_depth);
    mixed_rt.ResolveSecondaryIfUnchanged(mixed_rt.Revision(),{0u,true});
    assert(mixed_rt.CanAttemptVerifiedWorldDraw());
    auto main_first=mixed_rt.EnterDraw();
    mixed_rt.CompleteDraw(main_first,true,true);
    mixed_rt.RenderTargetBind(true,true,0u,true,offscreen_depth);
    assert(!mixed_rt.CanAttemptVerifiedWorldDraw());
    auto aux_middle=mixed_rt.EnterDraw();
    mixed_rt.CompleteDraw(aux_middle,true,true);
    mixed_rt.RenderTargetBind(true,true,0u,true,main_depth);
    assert(!mixed_rt.CanAttemptVerifiedWorldDraw()); // Present is tainted
    auto main_last=mixed_rt.EnterDraw();
    mixed_rt.CompleteDraw(main_last,true,true);
    const auto mixed_result=mixed_rt.FinishPresent();
    assert(mixed_result.backbuffer==2u && mixed_result.auxiliary==1u);
    assert((mixed_result.flags&OriginalPhysicalTargetDrawCoverage::kAuxiliarySource)!=0u);
    assert(mixed_result.RejectUnprovenStereoSource());
    mixed_rt.ReopenAfterPhysicalPresentReturn();
    assert(mixed_rt.CanAttemptVerifiedWorldDraw()); // fresh Present
    OriginalPhysicalTargetDrawCoverage binds{};
    auto clean_rt=binds.EnterBind();
    binds.CompleteRenderTargetBind(clean_rt,true,true,0u,true,main_depth);
    auto clean_depth=binds.EnterBind();
    binds.CompleteDepthBind(clean_depth,true,true,main_depth);
    binds.ResolveSecondaryIfUnchanged(binds.Revision(),{0u,true});
    assert(binds.CanAttemptVerifiedWorldDraw());
    auto pending_bind=binds.EnterBind();
    assert(!binds.CanAttemptVerifiedWorldDraw());
    auto overlapping_bind_draw=binds.EnterDraw();
    binds.CompleteDraw(overlapping_bind_draw,true,true);
    auto bind_cut=binds.FinishPresent();
    assert((bind_cut.flags&
            OriginalPhysicalTargetDrawCoverage::kInFlight)!=0u);
    assert((bind_cut.flags&
            OriginalPhysicalTargetDrawCoverage::kTargetDrift)!=0u);
    binds.ReopenAfterPhysicalPresentReturn();
    binds.CompleteRenderTargetBind(pending_bind,true,true,0u,true,main_depth);
    assert(!binds.CanAttemptVerifiedWorldDraw()); // old epoch cannot re-own
    auto late_bind=binds.FinishPresent();
    assert((late_bind.flags&
            OriginalPhysicalTargetDrawCoverage::kCrossPresent)!=0u);
    // A bind that starts DURING the sealed physical COM Present is also
    // invalid if it finally returns after Present has reopened.
    auto sealed_bind=binds.EnterBind();
    binds.ReopenAfterPhysicalPresentReturn();
    binds.CompleteDepthBind(sealed_bind,true,true,main_depth);
    assert(!binds.CanAttemptVerifiedWorldDraw());
    assert((binds.FinishPresent().flags&
            OriginalPhysicalTargetDrawCoverage::kCrossPresent)!=0u);
    binds.ReopenAfterPhysicalPresentReturn();
    // Two concurrent original SETs cannot authenticate an out-of-order
    // post-COM snapshot. The old ticket must not re-own the new RT0.
    auto old_bind=binds.EnterBind();
    auto new_bind=binds.EnterBind();
    binds.CompleteRenderTargetBind(new_bind,true,true,0u,true,main_depth);
    binds.CompleteRenderTargetBind(old_bind,true,true,0u,true,offscreen_depth);
    assert(!binds.CanAttemptVerifiedWorldDraw());
    assert((binds.FinishPresent().flags&
            OriginalPhysicalTargetDrawCoverage::kTargetDrift)!=0u);
    binds.ReopenAfterPhysicalPresentReturn();
    // Reset never steals an old pending ticket nor launders its success.
    auto reset_bind=binds.EnterBind();
    binds.Reset();
    binds.CompleteDepthBind(reset_bind,true,true,main_depth);
    assert((binds.FinishPresent().flags&
            OriginalPhysicalTargetDrawCoverage::kCrossPresent)!=0u);
    binds.ReopenAfterPhysicalPresentReturn();
    auto fresh_rt=binds.EnterBind();
    binds.CompleteRenderTargetBind(fresh_rt,true,true,0u,true,main_depth);
    auto fresh_z=binds.EnterBind();
    binds.CompleteDepthBind(fresh_z,true,true,main_depth);
    binds.ResolveSecondaryIfUnchanged(binds.Revision(),{0u,true});
    assert(binds.CanAttemptVerifiedWorldDraw());
    assert(binds.FinishPresent().flags==0u);
    binds.ReopenAfterPhysicalPresentReturn();
    auto failure_bind=binds.EnterBind();
    binds.CompleteRenderTargetBind(failure_bind,true,false,0u,true,main_depth);
    assert((binds.FinishPresent().flags&
            OriginalPhysicalTargetDrawCoverage::kFailedBind)!=0u);
    // Read-only initial snapshot sees original MRT1 without a SetRT
    // intercepted in our process: cannot certify this as mono-world.
    OriginalPhysicalTargetDrawCoverage untouched_mrt{};
    untouched_mrt.RenderTargetBind(true,true,0u,true,main_depth);
    untouched_mrt.DepthBind(true,true,main_depth);
    auto mr=untouched_mrt.Revision();
    untouched_mrt.ResolveSecondaryIfUnchanged(mr,{1u,true});
    assert(!untouched_mrt.CanAttemptVerifiedWorldDraw());
    auto mrt_real=untouched_mrt.EnterDraw();
    untouched_mrt.CompleteDraw(mrt_real,true,true);
    assert((untouched_mrt.FinishPresent().flags&
            OriginalPhysicalTargetDrawCoverage::kMrtActive)!=0u);
    untouched_mrt.ReopenAfterPhysicalPresentReturn();
    // A real GetRT failure is UNKNOWN (rather than falsely no MRT);
    // no repeat query inside the same revision, then recheck on SetRT.
    OriginalPhysicalTargetDrawCoverage failed_get{};
    failed_get.RenderTargetBind(true,true,0u,true,main_depth);
    failed_get.DepthBind(true,true,main_depth);
    failed_get.ResolveSecondaryIfUnchanged(failed_get.Revision(),{0u,false});
    assert(!failed_get.NeedsSecondaryResolution());
    assert(!failed_get.CanAttemptVerifiedWorldDraw());
    auto notfound=failed_get.EnterDraw();
    failed_get.CompleteDraw(notfound,true,true);
    assert((failed_get.FinishPresent().flags&
            OriginalPhysicalTargetDrawCoverage::kMrtUnresolved)!=0u);
    failed_get.ReopenAfterPhysicalPresentReturn();
    failed_get.RenderTargetBind(true,true,1u,false,main_depth);
    assert(failed_get.NeedsSecondaryResolution());
    failed_get.ResolveSecondaryIfUnchanged(failed_get.Revision(),{0u,true});
    assert(failed_get.CanAttemptVerifiedWorldDraw());
    // A read-only MRT query from a stale revision cannot undo newer COM.
    failed_get.RenderTargetBind(true,true,1u,true,offscreen_depth);
    failed_get.ResolveSecondaryIfUnchanged(
        failed_get.Revision()-1u,{0u,true});
    assert(!failed_get.CanAttemptVerifiedWorldDraw());
    auto overlapping_active=failed_get.EnterDraw();
    failed_get.CompleteDraw(overlapping_active,true,true);
    assert((failed_get.FinishPresent().flags&
            OriginalPhysicalTargetDrawCoverage::kMrtActive)!=0u);
    failed_get.ReopenAfterPhysicalPresentReturn();
    // Genuine original unsampled Draw region readback: target/depth ownership
    // alone cannot certify an OpenXR eye if the actual GPU viewport, Z write,
    // or scissor window is partial/unreadable. Original mono still draws.
    // Authentic Draws with depth-only/partial color writes, stencil
    // masking, inverted Z-test or enabled user clip planes are negative
    // controls even when viewport/RT0/depth/scissor look full-screen.
    static_assert(OriginalPhysicalDirectEyeWriteProfile(15u,0u,2u,0u));
    static_assert(OriginalPhysicalDirectEyeWriteProfile(15u,0u,4u,0u));
    static_assert(!OriginalPhysicalDirectEyeWriteProfile(0u,0u,4u,0u));
    static_assert(!OriginalPhysicalDirectEyeWriteProfile(7u,0u,4u,0u));
    static_assert(!OriginalPhysicalDirectEyeWriteProfile(31u,0u,4u,0u));
    static_assert(!OriginalPhysicalDirectEyeWriteProfile(15u,1u,4u,0u));
    static_assert(!OriginalPhysicalDirectEyeWriteProfile(15u,0u,5u,0u));
    static_assert(!OriginalPhysicalDirectEyeWriteProfile(15u,0u,4u,1u));
    static_assert(!OriginalPhysicalDirectEyeWriteProfile(15u,0u,4u,0xffffffffu));
    OriginalPhysicalTargetDrawCoverage raster{true};
    raster.RenderTargetBind(true,true,0u,true,main_depth);
    raster.DepthBind(true,true,main_depth);
    raster.ResolveSecondaryIfUnchanged(raster.Revision(),{0u,true});
    assert(!raster.CanAttemptVerifiedWorldDraw());
    auto without_raster=raster.EnterDraw();
    raster.CompleteDraw(without_raster,true,true);
    assert((raster.FinishPresent().flags&
            OriginalPhysicalTargetDrawCoverage::kUnverifiedRaster)!=0u);
    raster.ReopenAfterPhysicalPresentReturn();
    const auto first_raster_revision=raster.Revision();
    raster.ResolveRasterIfUnchanged(first_raster_revision,{true,true});
    assert(raster.CanAttemptVerifiedWorldDraw());
    auto clean_raster_draw=raster.EnterDraw();
    raster.CompleteDraw(clean_raster_draw,true,true);
    assert(raster.FinishPresent().flags==0u);
    raster.ReopenAfterPhysicalPresentReturn();
    const auto pending_region=raster.EnterBind();
    assert(!raster.CanAttemptVerifiedWorldDraw());
    raster.CompleteRasterMutation(pending_region,true,true);
    assert(raster.NeedsRasterResolution());
    raster.ResolveRasterIfUnchanged(first_raster_revision,{true,true});
    assert(!raster.CanAttemptVerifiedWorldDraw()); // old read is stale
    raster.ResolveRasterIfUnchanged(raster.Revision(),{true,false});
    assert(!raster.CanAttemptVerifiedWorldDraw()); // partial/disabled Z
    auto partial=raster.EnterDraw();
    raster.CompleteDraw(partial,true,true);
    assert((raster.FinishPresent().flags&
            OriginalPhysicalTargetDrawCoverage::kUnverifiedRaster)!=0u);
    raster.ReopenAfterPhysicalPresentReturn();
    const auto old_region=raster.EnterBind();
    assert((raster.FinishPresent().flags&
            OriginalPhysicalTargetDrawCoverage::kInFlight)!=0u);
    raster.ReopenAfterPhysicalPresentReturn();
    raster.CompleteRasterMutation(old_region,true,true);
    assert((raster.FinishPresent().flags&
            OriginalPhysicalTargetDrawCoverage::kCrossPresent)!=0u);
    raster.ReopenAfterPhysicalPresentReturn();
    const auto new_region=raster.EnterBind();
    raster.CompleteRasterMutation(new_region,true,true);
    raster.RenderTargetBind(true,true,0u,true,main_depth);
    raster.DepthBind(true,true,main_depth);
    raster.ResolveSecondaryIfUnchanged(raster.Revision(),{0u,true});
    raster.ResolveRasterIfUnchanged(raster.Revision(),{true,true});
    assert(raster.CanAttemptVerifiedWorldDraw());
    const auto stateblock_region=raster.Revision();
    raster.InvalidateOpaqueStateBlock();
    raster.ResolveRasterIfUnchanged(stateblock_region,{true,true});
    assert(!raster.CanAttemptVerifiedWorldDraw());
    raster.Reset();
    assert(raster.NeedsRasterResolution());
    // An original GetRT/GetDepth/GetMRT/GetRaster group must finish in
    // the SAME physical epoch it entered: revision alone does not change on
    // Present. No in-flight COM getter may be borrowed by concurrent Draws,
    // original binding SETs, or speculative Win64 OpenXR eye sources.
    OriginalPhysicalTargetDrawCoverage source_reads{true};
    source_reads.RenderTargetBind(true,true,0u,true,main_depth);
    source_reads.DepthBind(true,true,main_depth);
    const auto clean_read=source_reads.EnterSourceRead();
    assert(!source_reads.CanAttemptVerifiedWorldDraw());
    source_reads.CompleteSourceRead(clean_read,false,{},true,{0u,true},
                                   true,{true,true});
    assert(source_reads.CanAttemptVerifiedWorldDraw());
    auto read_draw=source_reads.EnterDraw();
    assert(source_reads.CompleteDraw(read_draw,true,true));
    assert(source_reads.FinishPresent().flags==0u);
    source_reads.ReopenAfterPhysicalPresentReturn();

    OriginalPhysicalTargetDrawCoverage getter_draw_overlap{true};
    getter_draw_overlap.RenderTargetBind(true,true,0u,true,main_depth);
    getter_draw_overlap.DepthBind(true,true,main_depth);
    const auto getter_ticket=getter_draw_overlap.EnterSourceRead();
    const auto draw_during_get=getter_draw_overlap.EnterDraw();
    getter_draw_overlap.CompleteSourceRead(
        getter_ticket,false,{},true,{0u,true},true,{true,true});
    assert(!getter_draw_overlap.CanAttemptVerifiedWorldDraw());
    assert(!getter_draw_overlap.CompleteDraw(draw_during_get,true,true));
    const auto overlapped_getter=getter_draw_overlap.FinishPresent();
    assert((overlapped_getter.flags&
        OriginalPhysicalTargetDrawCoverage::kTargetDrift)!=0u);
    assert(overlapped_getter.RejectUnprovenStereoSource());
    getter_draw_overlap.ReopenAfterPhysicalPresentReturn();
    const auto retry_read=getter_draw_overlap.EnterSourceRead();
    getter_draw_overlap.CompleteSourceRead(
        retry_read,false,{},true,{0u,true},true,{true,true});
    assert(getter_draw_overlap.CanAttemptVerifiedWorldDraw());

    OriginalPhysicalTargetDrawCoverage getter_cut{true};
    getter_cut.RenderTargetBind(true,true,0u,true,main_depth);
    getter_cut.DepthBind(true,true,main_depth);
    const auto pending_getter=getter_cut.EnterSourceRead();
    assert((getter_cut.FinishPresent().flags&
        OriginalPhysicalTargetDrawCoverage::kInFlight)!=0u);
    getter_cut.ReopenAfterPhysicalPresentReturn();
    getter_cut.CompleteSourceRead(pending_getter,false,{},true,{0u,true},
                                 true,{true,true});
    assert(!getter_cut.CanAttemptVerifiedWorldDraw());
    assert((getter_cut.FinishPresent().flags&
        OriginalPhysicalTargetDrawCoverage::kCrossPresent)!=0u);
    getter_cut.ReopenAfterPhysicalPresentReturn();
    const auto after_cut=getter_cut.EnterSourceRead();
    getter_cut.CompleteSourceRead(after_cut,false,{},true,{0u,true},
                                 true,{true,true});
    assert(getter_cut.CanAttemptVerifiedWorldDraw());

    OriginalPhysicalTargetDrawCoverage getter_bind_overlap{true};
    getter_bind_overlap.RenderTargetBind(true,true,0u,true,main_depth);
    getter_bind_overlap.DepthBind(true,true,main_depth);
    const auto before_set=getter_bind_overlap.EnterSourceRead();
    const auto simultaneous_set=getter_bind_overlap.EnterBind();
    getter_bind_overlap.CompleteDepthBind(
        simultaneous_set,true,true,main_depth);
    getter_bind_overlap.CompleteSourceRead(
        before_set,false,{},true,{0u,true},true,{true,true});
    assert(!getter_bind_overlap.CanAttemptVerifiedWorldDraw());
    assert((getter_bind_overlap.FinishPresent().flags&
        OriginalPhysicalTargetDrawCoverage::kTargetDrift)!=0u);
    getter_bind_overlap.ReopenAfterPhysicalPresentReturn();

    OriginalPhysicalTargetDrawCoverage getter_reset{true};
    const auto stale_getter=getter_reset.EnterSourceRead();
    getter_reset.Reset();
    getter_reset.CompleteSourceRead(
        stale_getter,true,main_depth,true,{0u,true},true,{true,true});
    assert(getter_reset.NeedsResolution());
    assert(getter_reset.NeedsSecondaryResolution());
    assert(getter_reset.NeedsRasterResolution());
    assert((getter_reset.FinishPresent().flags&
        OriginalPhysicalTargetDrawCoverage::kCrossPresent)!=0u);
    getter_reset.ReopenAfterPhysicalPresentReturn();
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
