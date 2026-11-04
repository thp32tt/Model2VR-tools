#include "../src/probe/daytona_original_mono_fallback.hpp"
#include <cassert>
using namespace m2vr::daytona;
int main(){
    assert(ChooseCpuWorldBackup(false,false)==CpuWorldBackupPath::NoPendingWorld);
    assert(ChooseCpuWorldBackup(false,true)==CpuWorldBackupPath::NoPendingWorld);
    assert(ChooseCpuWorldBackup(true,true)==CpuWorldBackupPath::OriginalMonoReady);
    assert(ChooseCpuWorldBackup(true,false)==CpuWorldBackupPath::GpuEyesRequired);
    assert(CanStageOriginalMono(true,true,true,true,1u));
    assert(!CanStageOriginalMono(false,true,true,true,1u)); // no opt-in
    assert(!CanStageOriginalMono(true,false,true,true,1u)); // foreign device
    assert(!CanStageOriginalMono(true,true,false,true,1u)); // native pair wins
    assert(!CanStageOriginalMono(true,true,true,false,1u)); // no live XR
    assert(!CanStageOriginalMono(true,true,true,true,0u)); // no original frame
    assert(CanPublishOriginalMono(true,true,false,true));
    assert(!CanPublishOriginalMono(true,false,false,true)); // failed Present
    assert(!CanPublishOriginalMono(true,true,true,true)); // native pair first
    assert(!CanPublishOriginalMono(true,true,false,false)); // tracking loss
    assert(!CanPublishOriginalMono(false,true,false,true)); // no pixels
    return 0;
}
