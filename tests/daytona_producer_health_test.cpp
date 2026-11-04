#include "../src/common/daytona_producer_health.hpp"
#include <cstdio>
#include <cstdlib>
#define REQUIRE(x) do {if(!(x)){std::fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
int main(){
 using namespace m2vr::producerhealth;
 auto pid=GetCurrentProcessId();
 Link writer,reader,duplicate;
 Snapshot got{};
 REQUIRE(!reader.Open(pid));
 REQUIRE(writer.Create(pid));
 REQUIRE(!duplicate.Create(pid));
 REQUIRE(reader.Open(pid));
 REQUIRE(!reader.Read(got));
 REQUIRE(!writer.Publish(0u,NoVerifiedScene,0,0,0));
 REQUIRE(!writer.Publish(1u,0x100u,0,0,0));
 REQUIRE(writer.Publish(1u,NoVerifiedScene|PoseUnavailable,0,12,1));
 REQUIRE(!writer.Publish(1u,0u,1,12,1));
 REQUIRE(reader.Read(got));
 REQUIRE(got.frame_id==1);
 REQUIRE(got.reason_mask==(NoVerifiedScene|PoseUnavailable));
 REQUIRE(got.mirrored_draws==12 && got.mirrored_clears==1);
 REQUIRE(got.verified_scene_draws==0 && got.timestamp_qpc>0);
 REQUIRE(Fresh(got,got.timestamp_qpc,1000000u));
 REQUIRE(!Fresh(got,got.timestamp_qpc+3000001u,1000000u));
 REQUIRE(!Fresh(got,got.timestamp_qpc-1u,1000000u));
 REQUIRE(!Fresh(got,got.timestamp_qpc,0u));
 REQUIRE(ReasonName(0u)[0]=='P');
 REQUIRE(ReasonName(NoVerifiedScene)[0]=='N');
 REQUIRE(writer.Publish(2u,0u,7,29,3));
 REQUIRE(reader.Read(got));
 REQUIRE(got.frame_id==2 && got.reason_mask==0u);
 REQUIRE(got.verified_scene_draws==7 && got.mirrored_draws==29);
 reader.Close();writer.Close();
 REQUIRE(!reader.Read(got));
 return 0;
}
