#include "../src/common/stereo_frame_ipc.h"
#include <cstdio>
#include <cstdlib>
#define REQUIRE(expr) do { if (!(expr)) { std::fprintf(stderr,"CHECK FAILED at line %d: %s\n",__LINE__,#expr); std::exit(1); } } while(0)
#include <cstdint>
#include <atomic>
#include <thread>
#include <vector>
int main(){
    using namespace m2vr::transport;
    const auto key=GetCurrentProcessId();
    SharedFrames writer,reader,wrong_pid_writer;
    REQUIRE(!wrong_pid_writer.Create(key+1u));
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
    // Original mono must not clear the earlier HMD camera sequence.
    REQUIRE(!writer.Publish(a.data(),b.data(),8,
        FrameKind::GameStereo,116u));
    REQUIRE(reader.Peek(metadata) && metadata.frame_id==7u);
    // A real stereo pair MUST repopulate the second eye on the following
    // original Present, never recycle its old pre-mono bytes.
    b[0]=0x91u;
    REQUIRE(writer.Publish(a.data(),b.data(),8,FrameKind::GameStereo,118u));
    REQUIRE(reader.ReadReusable(staging));
    REQUIRE(staging.kind==FrameKind::GameStereo &&
            staging.eyes[1].size()==kBytesPerEye && staging.eyes[1][0]==0x91u);
    REQUIRE(!writer.Publish(a.data(),b.data(),9,
        FrameKind::VerifiedCpuWorldStereo,0u));
    REQUIRE(!writer.Publish(a.data(),nullptr,9,
        FrameKind::VerifiedCpuWorldStereo,119u));
    REQUIRE(writer.Publish(a.data(),b.data(),9,
        FrameKind::VerifiedCpuWorldStereo,119u));
    REQUIRE(reader.ReadReusable(staging));
    REQUIRE(staging.kind==FrameKind::VerifiedCpuWorldStereo);
    REQUIRE(staging.pose_sequence==119u);
    REQUIRE(staging.eyes[0].size()==kBytesPerEye);
    REQUIRE(staging.eyes[1].size()==kBytesPerEye);
    // A stale or polluted mapping must not impersonate the original
    // emulator writer, even when its name/format remain unchanged.
    HANDLE map=OpenFileMappingW(FILE_MAP_WRITE,FALSE,NameForPid(key).c_str());
    REQUIRE(map!=nullptr);
    auto* raw=static_cast<Header*>(MapViewOfFile(
        map,FILE_MAP_WRITE,0,0,sizeof(Header)));
    REQUIRE(raw!=nullptr);
    raw->writer_pid=key+1u;
    MemoryBarrier();
    REQUIRE(!reader.Valid());
    REQUIRE(!reader.Peek(metadata));
    REQUIRE(!reader.ReadReusable(staging));
    reader.Close();
    REQUIRE(!reader.Open(key));
    raw->writer_pid=key;
    MemoryBarrier();
    REQUIRE(reader.Open(key));
    REQUIRE(reader.Peek(metadata));
    REQUIRE(reader.ReadReusable(staging));
    UnmapViewOfFile(raw);CloseHandle(map);
    // Two calls from ONE source process must not concurrently own the
    // same shared seqlock. Either whole coherent pair wins, never mixed
    // left/right bytes from two competing original Present paths.
    std::vector<std::uint8_t> a2(kBytesPerEye,0x4Au);
    std::vector<std::uint8_t> b2(kBytesPerEye,0xB4u);
    for(std::uint64_t iteration=10u;iteration<26u;++iteration){
        std::atomic<unsigned> ready{0u};
        std::atomic<bool> go{false};
        bool accepted_a=false,accepted_b=false;
        std::thread first([&]{
            ready.fetch_add(1u);
            while(!go.load())std::this_thread::yield();
            accepted_a=writer.Publish(a.data(),a.data(),iteration,FrameKind::GameStereo,119u);
        });
        std::thread second([&]{
            ready.fetch_add(1u);
            while(!go.load())std::this_thread::yield();
            accepted_b=writer.Publish(b2.data(),b2.data(),iteration,FrameKind::GameStereo,119u);
        });
        while(ready.load()!=2u)std::this_thread::yield();
        go.store(true);
        first.join();second.join();
        REQUIRE(accepted_a != accepted_b);
        REQUIRE(reader.ReadReusable(staging));
        REQUIRE(staging.frame_id==iteration);
        const auto expected=accepted_a?a[0]:b2[0];
        REQUIRE(staging.eyes[0][0]==expected && staging.eyes[1][0]==expected);
        REQUIRE(staging.eyes[0][kBytesPerEye-1]==(accepted_a?a.back():b2.back()));
        REQUIRE(staging.eyes[1][kBytesPerEye-1]==(accepted_a?a.back():b2.back()));
    }
    // Earlier HMD camera tag cannot advance a new physical original
    // Present even when frame ID/QPC would otherwise be valid.
    REQUIRE(!writer.Publish(a.data(),b.data(),26u,
        FrameKind::GameStereo,118u));
    REQUIRE(reader.Peek(metadata) && metadata.frame_id==25u);
    // Deterministic QPC rollback injection at the actual Win32 shared
    // mapping: despite a valid higher ID and complete stereo source, the
    // real producer must refuse to overwrite the last accepted pixels.
    HANDLE clock_map=OpenFileMappingW(
        FILE_MAP_WRITE,FALSE,NameForPid(key).c_str());
    REQUIRE(clock_map!=nullptr);
    auto* clock_header=static_cast<Header*>(MapViewOfFile(
        clock_map,FILE_MAP_WRITE,0,0,sizeof(Header)));
    REQUIRE(clock_header!=nullptr);
    const auto saved_qpc=clock_header->timestamp_qpc;
    clock_header->timestamp_qpc=0u;
    REQUIRE(!writer.Publish(a.data(),b.data(),26u,
        FrameKind::GameStereo,119u));
    clock_header->timestamp_qpc=UINT64_MAX;
    REQUIRE(!writer.Publish(a.data(),b.data(),26u,
        FrameKind::GameStereo,119u));
    clock_header->timestamp_qpc=saved_qpc;
    REQUIRE(reader.Peek(metadata) && metadata.frame_id==25u);
    REQUIRE(writer.Publish(a.data(),b.data(),26u,
        FrameKind::GameStereo,119u));
    REQUIRE(reader.Peek(metadata) && metadata.frame_id==26u);
    UnmapViewOfFile(clock_header);CloseHandle(clock_map);
    reader.Close();writer.Close();
    REQUIRE(!reader.Read(snap));
    return 0;
}

// Runtime-oriented source/receipt gate tests live in
// daytona_source_handoff_gate_test.cpp, compiled for both native bitnesses.
