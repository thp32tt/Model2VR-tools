#include "../src/common/stereo_frame_ipc.h"
#include <cassert>
#include <cstdint>
#include <vector>
int main(){
    using namespace m2vr::transport;
    const auto key=GetCurrentProcessId();
    SharedFrames writer,reader;
    assert(writer.Create(key));
    assert(!reader.Create(key)); // no second producer
    assert(reader.Open(key));
    std::vector<std::uint8_t> a(kBytesPerEye,0x21),b(kBytesPerEye,0xCD);
    Snapshot snap;
    assert(!reader.Read(snap)); // no complete frame
    assert(writer.Publish(a.data(),b.data(),1,FrameKind::Diagnostic));
    assert(!writer.Publish(a.data(),b.data(),1,FrameKind::GameStereo)); // monotonic id
    assert(reader.Read(snap));
    assert(snap.frame_id==1 && snap.kind==FrameKind::Diagnostic);
    assert(snap.eyes[0].size()==kBytesPerEye);
    assert(snap.eyes[0][0]==0x21 && snap.eyes[1][0]==0xCD);
    a[0]=0x18;b[0]=0xEF;
    assert(writer.Publish(a.data(),b.data(),2,FrameKind::GameStereo));
    assert(reader.Read(snap));
    assert(snap.frame_id==2 && snap.kind==FrameKind::GameStereo);
    assert(snap.eyes[0][0]==0x18 && snap.eyes[1][0]==0xEF);
    reader.Close();writer.Close();
    assert(!reader.Read(snap));
    return 0;
}
