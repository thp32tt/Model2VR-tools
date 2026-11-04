#include "../src/common/stereo_frame_ipc.h"
#include <cstdio>
#include <cstdlib>
#define REQUIRE(expr) do { if (!(expr)) { std::fprintf(stderr,"CHECK FAILED at line %d: %s\n",__LINE__,#expr); std::exit(1); } } while(0)
#include <cstdint>
#include <vector>
int main(){
    using namespace m2vr::transport;
    const auto key=GetCurrentProcessId();
    SharedFrames writer,reader;
    REQUIRE(writer.Create(key));
    REQUIRE(!reader.Create(key)); // no second producer
    REQUIRE(reader.Open(key));
    std::vector<std::uint8_t> a(kBytesPerEye,0x21),b(kBytesPerEye,0xCD);
    Snapshot snap;
    FrameStamp metadata{};
    REQUIRE(!reader.Peek(metadata)); // no complete frame
    REQUIRE(!reader.Read(snap)); // no complete frame
    Snapshot staging;
    REQUIRE(!reader.ReadReusable(staging));
    REQUIRE(staging.frame_id==0u && staging.eyes[0].empty());
    REQUIRE(writer.Publish(a.data(),b.data(),1,FrameKind::Diagnostic));
    REQUIRE(!writer.Publish(a.data(),b.data(),1,FrameKind::GameStereo)); // monotonic id
    REQUIRE(reader.Peek(metadata));
    REQUIRE(metadata.frame_id==1 && metadata.kind==FrameKind::Diagnostic);
    REQUIRE(metadata.timestamp_qpc>0 && metadata.writer_pid==key);
    REQUIRE(reader.Read(snap));
    REQUIRE(snap.frame_id==1 && snap.kind==FrameKind::Diagnostic);
    REQUIRE(snap.eyes[0].size()==kBytesPerEye);
    REQUIRE(snap.eyes[0][0]==0x21 && snap.eyes[1][0]==0xCD);
    a[0]=0x18;b[0]=0xEF;
    REQUIRE(writer.Publish(a.data(),b.data(),2,FrameKind::GameStereo));
    REQUIRE(reader.Peek(metadata));
    REQUIRE(metadata.frame_id==2 && metadata.kind==FrameKind::GameStereo);
    REQUIRE(reader.Read(snap));
    REQUIRE(snap.frame_id==2 && snap.kind==FrameKind::GameStereo);
    REQUIRE(snap.eyes[0][0]==0x18 && snap.eyes[1][0]==0xEF);
    REQUIRE(reader.ReadReusable(staging));
    REQUIRE(staging.frame_id==2 && staging.kind==FrameKind::GameStereo);
    REQUIRE(staging.eyes[0][0]==0x18 && staging.eyes[1][0]==0xEF);
    const auto* ptr_left=staging.eyes[0].data();
    const auto* ptr_right=staging.eyes[1].data();
    a[0]=0x7Bu;b[0]=0x5Eu;
    REQUIRE(writer.Publish(a.data(),b.data(),3,FrameKind::GameStereo));
    REQUIRE(reader.ReadReusable(staging));
    REQUIRE(staging.frame_id==3 && staging.kind==FrameKind::GameStereo);
    REQUIRE(staging.eyes[0].data()==ptr_left &&
            staging.eyes[1].data()==ptr_right);
    REQUIRE(staging.eyes[0][0]==0x7Bu && staging.eyes[1][0]==0x5Eu);
    REQUIRE(writer.Publish(a.data(),b.data(),4,FrameKind::GameStereo,117u));
    REQUIRE(reader.Peek(metadata));
    REQUIRE(metadata.pose_sequence==117u && metadata.frame_id==4);
    REQUIRE(reader.ReadReusable(staging));
    REQUIRE(staging.pose_sequence==117u && staging.frame_id==4);
    REQUIRE(!writer.Publish(a.data(),b.data(),5,FrameKind::Diagnostic,117u));
    REQUIRE(writer.Publish(a.data(),b.data(),5,FrameKind::Diagnostic));
    REQUIRE(reader.ReadReusable(staging));
    REQUIRE(staging.pose_sequence==0u && staging.kind==FrameKind::Diagnostic);
    // A real original mono backbuffer is game imagery, NOT independently
    // projected stereo. It cannot carry an XR source pose sequence.
    REQUIRE(writer.Publish(a.data(),a.data(),6,FrameKind::GameMonoFallback));
    REQUIRE(reader.Peek(metadata));
    REQUIRE(metadata.kind==FrameKind::GameMonoFallback);
    REQUIRE(metadata.pose_sequence==0u);
    REQUIRE(reader.ReadReusable(staging));
    REQUIRE(staging.kind==FrameKind::GameMonoFallback);
    REQUIRE(staging.eyes[0][0]==staging.eyes[1][0]);
    REQUIRE(!writer.Publish(a.data(),b.data(),7,FrameKind::GameMonoFallback,117u));
    // New single-copy transport: second eye is invalid and must never
    // silently contain pixels from an earlier original GameStereo frame.
    REQUIRE(writer.Publish(a.data(),nullptr,7,FrameKind::CompactMono));
    REQUIRE(reader.Peek(metadata));
    REQUIRE(metadata.kind==FrameKind::CompactMono && metadata.pose_sequence==0u);
    REQUIRE(reader.ReadReusable(staging));
    REQUIRE(staging.kind==FrameKind::CompactMono &&
            staging.eyes[0][0]==a[0] && staging.eyes[1].empty());
    REQUIRE(!writer.Publish(a.data(),nullptr,8,FrameKind::GameStereo,117u));
    REQUIRE(!writer.Publish(a.data(),b.data(),8,FrameKind::CompactMono,117u));
    // A real stereo pair MUST repopulate the second eye on the following
    // original Present, never recycle its old pre-mono bytes.
    b[0]=0x91u;
    REQUIRE(writer.Publish(a.data(),b.data(),8,FrameKind::GameStereo,118u));
    REQUIRE(reader.ReadReusable(staging));
    REQUIRE(staging.kind==FrameKind::GameStereo &&
            staging.eyes[1].size()==kBytesPerEye && staging.eyes[1][0]==0x91u);
    reader.Close();writer.Close();
    REQUIRE(!reader.Read(snap));
    return 0;
}
