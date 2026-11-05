#include "../src/probe/original_d3d9_lineage.hpp"
#include <cassert>
int main(){
    using namespace m2vr::drawprobe;
    static_assert(kOriginalLineageKinds==8);
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
    auto new_frame=initial;
    ++new_frame.physical_present;
    assert(!SameOriginalLineage(initial,new_frame));
    assert(OriginalLineageNonce(initial)!=OriginalLineageNonce(new_frame));
    initial.successful_writes[3]=kMaxLineageWritesPerPresent+1;
    assert(!OriginalLineageUsable(initial));
    assert(OriginalLineageNonce(initial)==0);
}
