#include "../src/common/daytona_original_present_receipt.hpp"
#include "../src/common/daytona_producer_health.hpp"
#include <cstdio>
#include <cstdlib>
#include <atomic>
#include <thread>
#define REQUIRE(x) do {if(!(x)){std::fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
int main(){
 using namespace m2vr::producerhealth;
 auto pid=GetCurrentProcessId();
 Link writer,reader,duplicate;
 Snapshot got{};
 const std::array<std::uint32_t,4> zero{};
 const std::array<std::uint32_t,4> first{2u,4u,5u,1u};
 const std::array<std::uint32_t,4> second{9u,8u,10u,2u};
 REQUIRE(!reader.Open(pid));
 REQUIRE(writer.Create(pid));
 REQUIRE(!duplicate.Create(pid));
 REQUIRE(reader.Open(pid));
 REQUIRE(!reader.Read(got));
 REQUIRE(!writer.Publish(0u,NoVerifiedScene,0,0,0,zero));
 REQUIRE(!writer.Publish(1u,0x1000u,0,0,0,zero));
 REQUIRE(ReasonName(CpuWorldDeferredGpuReadback)[0]=='V');
 REQUIRE(kKnownReasons==4095u);
 REQUIRE(ReasonName(OriginalCameraComUnsafe)[0]=='O');
 REQUIRE(!MayPublishGameStereo(OriginalCameraComUnsafe));
 REQUIRE(FinalPresentReason(OriginalCameraComUnsafe,true,true)==OriginalCameraComUnsafe);
 REQUIRE(FinalPresentReason(OriginalCameraComUnsafe,false,true)==
         (OriginalCameraComUnsafe|OriginalPresentFailed));
 Snapshot camera_failed{};camera_failed.frame_id=4u;
 camera_failed.reason_mask=OriginalCameraComUnsafe;
 REQUIRE(InvalidatesCachedPair(camera_failed,true,3u,true));
 REQUIRE(FinalPresentReason(CpuWorldDeferredGpuReadback,true,true)==
         CpuWorldDeferredGpuReadback);
 REQUIRE(!MayPublishGameStereo(CpuWorldDeferredGpuReadback));
 REQUIRE(FinalPresentReason(0u,true,true)==0u);
 REQUIRE(FinalPresentReason(0u,false,true)==OriginalPresentFailed);
 REQUIRE(FinalPresentReason(ReadbackFailed,false,true)==
         (ReadbackFailed|OriginalPresentFailed));
 REQUIRE(FinalPresentReason(0u,true,false)==PoseExpired);
 REQUIRE(FinalPresentReason(ReadbackFailed,true,false)==ReadbackFailed);
 REQUIRE(!MayPublishGameStereo(FinalPresentReason(0u,false,true)));
 REQUIRE(MayPublishGameStereo(FinalPresentReason(0u,true,true)));
 REQUIRE(ReasonName(OriginalPresentFailed)[0]=='O');
 // Both IPC mappings must certify the SAME successful physical original
 // Present; a later successful report is not a receipt for an older pair.
 using m2vr::transport::FrameKind;
 using m2vr::receipt::Verdict;
 Snapshot receipt{10u,1u,0u,3u,4u,0u,{4u,0u,0u,0u}};
 REQUIRE(m2vr::receipt::Authorize(10u,FrameKind::GameStereo,1u,receipt,true)==Verdict::Admit);
 REQUIRE(m2vr::receipt::Authorize(10u,FrameKind::VerifiedCpuWorldStereo,1u,receipt,true)==Verdict::Reject);
 Snapshot cpu_receipt{10u,1u,0u,3u,0u,0u,{0u,0u,0u,0u}};
 REQUIRE(m2vr::receipt::Authorize(10u,FrameKind::VerifiedCpuWorldStereo,1u,cpu_receipt,true)==Verdict::Admit);
 REQUIRE(m2vr::receipt::Authorize(10u,FrameKind::GameStereo,1u,cpu_receipt,true)==Verdict::Reject);
 cpu_receipt.mirrored_clears=1u;
 REQUIRE(m2vr::receipt::Authorize(10u,FrameKind::VerifiedCpuWorldStereo,1u,cpu_receipt,true)==Verdict::Reject);
 cpu_receipt.mirrored_clears=0u;
 cpu_receipt.verified_scene_draws=0u;
 REQUIRE(m2vr::receipt::Authorize(10u,FrameKind::VerifiedCpuWorldStereo,1u,cpu_receipt,true)==Verdict::Reject);
 Snapshot no_mirror=receipt;
 no_mirror.mirrored_draws=0u;
 no_mirror.family_draws={0u,0u,0u,0u};
 REQUIRE(m2vr::receipt::Authorize(10u,FrameKind::GameStereo,1u,no_mirror,true)==Verdict::Reject);
 no_mirror=receipt;
 no_mirror.verified_scene_draws=0u;
 REQUIRE(m2vr::receipt::Authorize(10u,FrameKind::GameStereo,1u,no_mirror,true)==Verdict::Reject);
 REQUIRE(m2vr::receipt::Authorize(11u,FrameKind::GameStereo,1u,receipt,true)==Verdict::AwaitHealth);
 REQUIRE(m2vr::receipt::Authorize(9u,FrameKind::GameStereo,1u,receipt,true)==Verdict::Reject);
 REQUIRE(m2vr::receipt::Authorize(10u,FrameKind::GameStereo,1u,receipt,false)==Verdict::AwaitHealth);
 REQUIRE(m2vr::receipt::Authorize(0u,FrameKind::GameStereo,1u,receipt,true)==Verdict::Reject);
 REQUIRE(m2vr::receipt::Authorize(10u,FrameKind::Invalid,1u,receipt,true)==Verdict::Reject);
 REQUIRE(m2vr::receipt::Authorize(10u,FrameKind::Diagnostic,1u,receipt,false)==Verdict::Admit);
 REQUIRE(m2vr::receipt::Authorize(10u,FrameKind::CompactMono,1u,receipt,true)==Verdict::Reject);
 // A receipt cannot certify bytes written AFTER its own QPC timestamp.
 REQUIRE(m2vr::receipt::Authorize(10u,FrameKind::GameStereo,0u,receipt,true)==Verdict::Reject);
 REQUIRE(m2vr::receipt::Authorize(10u,FrameKind::GameStereo,2u,receipt,true)==Verdict::Reject);
 REQUIRE(m2vr::receipt::Authorize(10u,FrameKind::GameStereo,1u,receipt,true)==Verdict::Admit);
 REQUIRE(!m2vr::receipt::RevokesCached(10u,FrameKind::GameStereo,1u,receipt,true));
 REQUIRE(m2vr::receipt::RevokesCached(10u,FrameKind::GameStereo,2u,receipt,true));
 receipt.reason_mask=OriginalMonoFallback;
 REQUIRE(m2vr::receipt::Authorize(10u,FrameKind::CompactMono,1u,receipt,true)==Verdict::Admit);
 REQUIRE(m2vr::receipt::Authorize(10u,FrameKind::GameMonoFallback,1u,receipt,true)==Verdict::Admit);
 REQUIRE(m2vr::receipt::Authorize(10u,FrameKind::GameStereo,1u,receipt,true)==Verdict::Reject);
 REQUIRE(m2vr::receipt::RevokesCached(10u,FrameKind::GameStereo,1u,receipt,true));
 REQUIRE(!m2vr::receipt::RevokesCached(10u,FrameKind::CompactMono,1u,receipt,true));
 REQUIRE(!m2vr::receipt::RevokesCached(10u,FrameKind::CompactMono,1u,receipt,false));
 REQUIRE(!m2vr::receipt::RevokesCached(10u,FrameKind::Diagnostic,1u,receipt,true));
 receipt.frame_id=11u;
 REQUIRE(m2vr::receipt::RevokesCached(10u,FrameKind::GameStereo,1u,receipt,true));
 REQUIRE(!m2vr::receipt::RevokesCached(10u,FrameKind::CompactMono,1u,receipt,true));
 receipt.reason_mask=ReadbackFailed;
 REQUIRE(m2vr::receipt::Authorize(11u,FrameKind::GameStereo,1u,receipt,true)==Verdict::Reject);
 REQUIRE(m2vr::receipt::RevokesCached(10u,FrameKind::CompactMono,1u,receipt,true));
 receipt.reason_mask=0u;
 REQUIRE(!m2vr::receipt::RevokesCached(10u,FrameKind::GameStereo,1u,receipt,true));
 // A newer successful REAL-original stereo source changes kind, even
 // though older x86 mono/GPU/CPU pixels are still within their age TTL.
 Snapshot newer_gpu{12u,20u,0u,3u,4u,0u,{4u,0u,0u,0u}};
 Snapshot newer_cpu{12u,20u,0u,3u,0u,0u,{0u,0u,0u,0u}};
 using m2vr::receipt::NewerNativeKindTransition;
 REQUIRE(NewerNativeKindTransition(11u,FrameKind::CompactMono,10u,newer_gpu,true));
 REQUIRE(NewerNativeKindTransition(11u,FrameKind::GameMonoFallback,10u,newer_cpu,true));
 REQUIRE(NewerNativeKindTransition(11u,FrameKind::GameStereo,10u,newer_cpu,true));
 REQUIRE(NewerNativeKindTransition(11u,FrameKind::VerifiedCpuWorldStereo,10u,newer_gpu,true));
 REQUIRE(!NewerNativeKindTransition(11u,FrameKind::GameStereo,10u,newer_gpu,true));
 REQUIRE(!NewerNativeKindTransition(11u,FrameKind::VerifiedCpuWorldStereo,10u,newer_cpu,true));
 REQUIRE(!NewerNativeKindTransition(11u,FrameKind::Diagnostic,10u,newer_gpu,true));
 REQUIRE(!NewerNativeKindTransition(11u,FrameKind::CompactMono,10u,newer_gpu,false));
 REQUIRE(!NewerNativeKindTransition(12u,FrameKind::CompactMono,10u,newer_gpu,true));
 REQUIRE(!NewerNativeKindTransition(11u,FrameKind::CompactMono,21u,newer_gpu,true));
 REQUIRE(m2vr::receipt::RevokesCached(11u,FrameKind::CompactMono,10u,newer_gpu,true));
 REQUIRE(m2vr::receipt::RevokesCached(11u,FrameKind::GameStereo,10u,newer_cpu,true));
 REQUIRE(!m2vr::receipt::RevokesCached(11u,FrameKind::GameStereo,10u,newer_gpu,true));
 // The receipt must remain source-authentic; fabricated/ambiguous zero
 // success, absent Scene or broken draw totals cannot classify a transition.
 newer_gpu.verified_scene_draws=0u;
 REQUIRE(!NewerNativeKindTransition(11u,FrameKind::CompactMono,10u,newer_gpu,true));
 newer_gpu.verified_scene_draws=3u;
 newer_gpu.family_draws[0]=2u;
 REQUIRE(!NewerNativeKindTransition(11u,FrameKind::CompactMono,10u,newer_gpu,true));
 newer_cpu.mirrored_clears=1u;
 REQUIRE(!NewerNativeKindTransition(11u,FrameKind::CompactMono,10u,newer_cpu,true));
 receipt.frame_id=9u;
 REQUIRE(!m2vr::receipt::RevokesCached(10u,FrameKind::GameStereo,1u,receipt,true));
 REQUIRE(!writer.Publish(1u,NoVerifiedScene,0,11,1,first));
 REQUIRE(writer.Publish(1u,NoVerifiedScene|PoseUnavailable,0,12,1,first));
 REQUIRE(!writer.Publish(1u,0u,1,12,1,first));
 REQUIRE(reader.Read(got));
 REQUIRE(got.frame_id==1);
 REQUIRE(got.reason_mask==(NoVerifiedScene|PoseUnavailable));
 REQUIRE(got.mirrored_draws==12 && got.mirrored_clears==1);
 REQUIRE(got.family_draws==first && SumFamilies(got.family_draws)==12u);
 REQUIRE(got.verified_scene_draws==0 && got.timestamp_qpc>0);
 REQUIRE(Fresh(got,got.timestamp_qpc,1000000u));
 REQUIRE(!Fresh(got,got.timestamp_qpc+3000001u,1000000u));
 REQUIRE(!Fresh(got,got.timestamp_qpc-1u,1000000u));
 REQUIRE(!Fresh(got,got.timestamp_qpc,0u));
 REQUIRE(ReasonName(0u)[0]=='P');
 REQUIRE(ReasonName(NoVerifiedScene)[0]=='N');
 REQUIRE(OriginalMonoPublished(OriginalMonoFallback));
 REQUIRE(!OriginalMonoPublished(NoVerifiedScene));
 REQUIRE(ReasonName(OriginalMonoFallback)[0]=='O');
 REQUIRE(!InvalidatesCachedPair(got,true,1u));
 REQUIRE(InvalidatesCachedPair(got,true,0u));
 REQUIRE(!InvalidatesCachedPair(got,false,0u));
 REQUIRE(writer.Publish(2u,0u,7,29,3,second));
 REQUIRE(reader.Read(got));
 REQUIRE(got.frame_id==2 && got.reason_mask==0u);
 REQUIRE(got.verified_scene_draws==7 && got.mirrored_draws==29);
 REQUIRE(got.family_draws==second && SumFamilies(got.family_draws)==29u);
 REQUIRE(!InvalidatesCachedPair(got,true,1u)); // newer success not failure
 Snapshot failed=got;
 failed.frame_id=3u;
 failed.reason_mask=ReadbackFailed;
 REQUIRE(InvalidatesCachedPair(failed,true,2u));
 REQUIRE(!InvalidatesCachedPair(failed,true,3u));
 REQUIRE(!InvalidatesCachedPair(failed,false,2u));
 failed.reason_mask=OriginalPresentFailed;
 REQUIRE(InvalidatesCachedPair(failed,true,2u));
 failed.reason_mask=OriginalMonoFallback;
 REQUIRE(!InvalidatesCachedPair(failed,true,2u));
 REQUIRE(InvalidatesCachedPair(failed,true,2u,true));
 REQUIRE(!InvalidatesCachedPair(failed,true,3u,true));
 REQUIRE(!InvalidatesCachedPair(failed,false,2u,true));
 // Two independent original x86 paths must not write a mixed success
 // receipt, or regress the physical Present sequence seen by Win64.
 for(std::uint64_t id=3u;id<23u;++id){
     std::atomic<unsigned> ready{0u};
     std::atomic<bool> go{false};
     bool wrote_success=false,wrote_failure=false;
     std::thread success([&]{
         ready.fetch_add(1u);
         while(!go.load())std::this_thread::yield();
         wrote_success=writer.Publish(id,0u,7u,12u,2u,first);
     });
     std::thread failure([&]{
         ready.fetch_add(1u);
         while(!go.load())std::this_thread::yield();
         wrote_failure=writer.Publish(id,ReadbackFailed,0u,29u,3u,second);
     });
     while(ready.load()!=2u)std::this_thread::yield();
     go.store(true);
     success.join();failure.join();
     REQUIRE(wrote_success!=wrote_failure);
     REQUIRE(reader.Read(got));
     REQUIRE(got.frame_id==id);
     REQUIRE(got.reason_mask==(wrote_success?0u:ReadbackFailed));
     REQUIRE(got.mirrored_draws==(wrote_success?12u:29u));
     REQUIRE(got.verified_scene_draws==(wrote_success?7u:0u));
     REQUIRE(got.family_draws==(wrote_success?first:second));
 }
 // A newer physical Present receipt must never regress Windows QPC.
 // Exercise the real 64-byte x86/x64 health wire with a deterministic
 // future-timestamp corruption; the previous completed receipt survives.
 HANDLE clock_map=OpenFileMappingW(
     FILE_MAP_WRITE,FALSE,NameForPid(pid).c_str());
 REQUIRE(clock_map!=nullptr);
 auto* clock_wire=static_cast<Wire*>(MapViewOfFile(
     clock_map,FILE_MAP_WRITE,0,0,sizeof(Wire)));
 REQUIRE(clock_wire!=nullptr);
 const auto saved_qpc=clock_wire->status.timestamp_qpc;
 clock_wire->status.timestamp_qpc=UINT64_MAX;
 REQUIRE(!writer.Publish(26u,0u,7u,12u,2u,first));
 clock_wire->status.timestamp_qpc=saved_qpc;
 REQUIRE(reader.Read(got) && got.frame_id==22u);
 REQUIRE(writer.Publish(26u,0u,7u,12u,2u,first));
 REQUIRE(reader.Read(got) && got.frame_id==26u);
 UnmapViewOfFile(clock_wire);CloseHandle(clock_map);
 REQUIRE(!writer.Publish(10u,0u,7u,12u,2u,first));
 reader.Close();writer.Close();
 REQUIRE(!reader.Read(got));
 return 0;
}
