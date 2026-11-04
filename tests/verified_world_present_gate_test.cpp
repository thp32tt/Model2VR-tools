#include "../src/probe/verified_world_present_gate.hpp"
#include <cstdio>
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"world present gate line %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(){
    using namespace m2vr::vrbridge;
    CHECK(NextOriginalPresentFrameId(0u,true,false)==1u);
    CHECK(NextOriginalPresentFrameId(25u,true,false)==26u);
    CHECK(NextOriginalPresentFrameId(25u,false,false)==0u);
    CHECK(NextOriginalPresentFrameId(25u,true,true)==0u);
    CHECK(NextOriginalPresentFrameId(0xffffffffu,true,false)==0u);
    CHECK(DecidePendingWorldPresent(0u,1u,true,false,true)==
          PendingWorldPresent::None);
    CHECK(DecidePendingWorldPresent(5u,4u,true,false,true)==
          PendingWorldPresent::Wait);
    CHECK(DecidePendingWorldPresent(5u,5u,true,false,true)==
          PendingWorldPresent::Publish);
    CHECK(DecidePendingWorldPresent(5u,6u,true,false,true)==
          PendingWorldPresent::Drop);
    CHECK(DecidePendingWorldPresent(5u,5u,false,false,true)==
          PendingWorldPresent::Drop);
    CHECK(DecidePendingWorldPresent(5u,5u,true,true,true)==
          PendingWorldPresent::Drop);
    CHECK(DecidePendingWorldPresent(5u,5u,true,false,false)==
          PendingWorldPresent::Drop);
    // Never reinterpret a failed original game Present as valid GameStereo,
    // even with a genuine tracked OpenXR source-pose tag.
    CHECK(DecidePendingWorldPresent(0u,0u,false,false,false)==
          PendingWorldPresent::None);
    CHECK(PendingWorldHealthReason(PendingWorldPresent::Publish,true,true)==0u);
    CHECK(PendingWorldHealthReason(PendingWorldPresent::Publish,true,false)==
          m2vr::producerhealth::TransportFailed);
    CHECK(PendingWorldHealthReason(PendingWorldPresent::Drop,false,false)==
          m2vr::producerhealth::OriginalPresentFailed);
    CHECK(PendingWorldHealthReason(PendingWorldPresent::Drop,true,false)==
          m2vr::producerhealth::WrongDevice);

    return 0;
}
