#include "../src/common/daytona_producer_health.hpp"
#include <cstdio>
#include <cstdlib>
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
 REQUIRE(!writer.Publish(1u,0x400u,0,0,0,zero));
 REQUIRE(FinalPresentReason(0u,true,true)==0u);
 REQUIRE(FinalPresentReason(0u,false,true)==OriginalPresentFailed);
 REQUIRE(FinalPresentReason(ReadbackFailed,false,true)==
         (ReadbackFailed|OriginalPresentFailed));
 REQUIRE(FinalPresentReason(0u,true,false)==PoseExpired);
 REQUIRE(FinalPresentReason(ReadbackFailed,true,false)==ReadbackFailed);
 REQUIRE(!MayPublishGameStereo(FinalPresentReason(0u,false,true)));
 REQUIRE(MayPublishGameStereo(FinalPresentReason(0u,true,true)));
 REQUIRE(ReasonName(OriginalPresentFailed)[0]=='O');
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
 reader.Close();writer.Close();
 REQUIRE(!reader.Read(got));
 return 0;
}
