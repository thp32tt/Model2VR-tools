// Synthetic two-process transport test: a REAL Win32 x86 process writes
// the SAME production shared-memory header and producer-health receipt
// that the Win64 OpenXR host reads. It does not run Daytona, claim race
// World3D, or substitute synthetic frames for actual headset evidence.
#include "../src/common/daytona_source_handoff_gate.hpp"
#include "../src/common/daytona_xr_layer_commit_gate.hpp"
#include "../host/original_source_process_lease.hpp"
#include <Windows.h>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#define REQUIRE(p) do {if(!(p)){std::fprintf(stderr,"cross-arch line %d: %s\n",__LINE__,#p);std::exit(1);}}while(0)
namespace {
std::wstring EventName(DWORD pid,const wchar_t* suffix){
    return L"Local\\Model2VR_CrossArchStereo_"+std::to_wstring(pid)+suffix;
}
#ifdef M2VR_CROSS_ARCH_PRODUCER
bool WritePhase(m2vr::transport::SharedFrames& frames,
    m2vr::producerhealth::Link& health,std::uint64_t id,
    m2vr::transport::FrameKind kind,std::uint32_t tag,
    std::uint8_t a,std::uint8_t b){
    using namespace m2vr;
    std::vector<std::uint8_t> left(transport::kBytesPerEye,a);
    std::vector<std::uint8_t> right(transport::kBytesPerEye,b);
    const bool mono=kind==transport::FrameKind::CompactMono;
    if(!frames.Publish(left.data(),mono?nullptr:right.data(),id,kind,tag))
        return false;
    std::array<std::uint32_t,4> draws{};
    std::uint32_t scene=0u,count=0u,reason=0u;
    if(kind==transport::FrameKind::GameStereo){
        scene=1u;count=1u;draws[0]=1u;
    }else if(kind==transport::FrameKind::VerifiedCpuWorldStereo){
        scene=2u;
    }else reason=producerhealth::OriginalMonoFallback;
    // Emulates the *order* of physical post-COM source receipt only.
    // Does not invent a real D3D9 Draw, GPU capture or game authorization.
    return health.Publish(id,reason,scene,count,0u,draws);
}
#endif
} // namespace

#ifdef M2VR_CROSS_ARCH_PRODUCER
int wmain(){
    static_assert(sizeof(void*)==4u,"this emitter must really be 32-bit");
    const auto pid=GetCurrentProcessId();
    HANDLE ready=CreateEventW(nullptr,TRUE,FALSE,EventName(pid,L"_ready").c_str());
    HANDLE advance=CreateEventW(nullptr,FALSE,FALSE,EventName(pid,L"_next").c_str());
    REQUIRE(ready && advance);
    m2vr::transport::SharedFrames frames;
    m2vr::producerhealth::Link health;
    REQUIRE(frames.Create(pid) && health.Create(pid));
    const std::array<m2vr::transport::FrameKind,3> kinds{{
        m2vr::transport::FrameKind::GameStereo,
        m2vr::transport::FrameKind::CompactMono,
        m2vr::transport::FrameKind::VerifiedCpuWorldStereo}};
    const std::array<std::uint32_t,3> tags{{91u,0u,92u}};
    const std::array<std::uint8_t,3> colors_left{{0x17u,0x2cu,0x3du}};
    const std::array<std::uint8_t,3> colors_right{{0x8bu,0u,0xc6u}};
    for(unsigned i=0;i<3u;++i){
        REQUIRE(WritePhase(frames,health,i+1u,kinds[i],tags[i],
                           colors_left[i],colors_right[i]));
        REQUIRE(SetEvent(ready)!=FALSE);
        REQUIRE(WaitForSingleObject(advance,30000u)==WAIT_OBJECT_0);
    }
    // A fourth physical D3D9 Present can fail without producing a new
    // source-eye packet. That failure must revoke the older pair even if
    // it was already copied into the Win64 XR staging buffer.
    REQUIRE(health.Publish(4u,m2vr::producerhealth::OriginalPresentFailed,
                           0u,0u,0u,{}));
    REQUIRE(SetEvent(ready)!=FALSE);
    REQUIRE(WaitForSingleObject(advance,30000u)==WAIT_OBJECT_0);
    CloseHandle(ready);CloseHandle(advance);
    return 0;
}
#elif defined(M2VR_CROSS_ARCH_CONSUMER)
int wmain(int argc,wchar_t** argv){
    static_assert(sizeof(void*)==8u,"this OpenXR-side reader must be 64-bit");
    REQUIRE(argc==2 && argv[1] && argv[1][0]);
    std::wstring command=L"\""+std::wstring(argv[1])+L"\"";
    std::vector<wchar_t> cmd(command.begin(),command.end());
    cmd.push_back(L'\0');
    STARTUPINFOW startup{};startup.cb=sizeof(startup);
    PROCESS_INFORMATION child{};
    REQUIRE(CreateProcessW(argv[1],cmd.data(),nullptr,nullptr,FALSE,0,
                           nullptr,nullptr,&startup,&child)!=FALSE);
    CloseHandle(child.hThread);
    m2vr::host::OriginalSourceProcessLease process_lease;
    REQUIRE(process_lease.Attach(child.dwProcessId));
    REQUIRE(process_lease.CreationTime100ns()!=0u);
    HANDLE ready=nullptr,advance=nullptr;
    for(unsigned retry=0;retry<100u && (!ready || !advance);++retry){
        if(!ready)ready=OpenEventW(EVENT_MODIFY_STATE|SYNCHRONIZE,FALSE,
                     EventName(child.dwProcessId,L"_ready").c_str());
        if(!advance)advance=OpenEventW(EVENT_MODIFY_STATE|SYNCHRONIZE,FALSE,
                     EventName(child.dwProcessId,L"_next").c_str());
        if(!ready || !advance)Sleep(50u);
    }
    REQUIRE(ready && advance);
    // The x86 child creates named events BEFORE its 8-MiB frame mapping
    // and post-Present health mapping. Wait for its first fully committed
    // source receipt, otherwise OpenFileMapping races process startup.
    REQUIRE(WaitForSingleObject(ready,30000u)==WAIT_OBJECT_0);
    using namespace m2vr;
    transport::SharedFrames frame_reader;
    producerhealth::Link health_reader;
    REQUIRE(frame_reader.Open(child.dwProcessId));
    REQUIRE(health_reader.Open(child.dwProcessId));
    transport::StereoFrameGate frame_gate;
    LARGE_INTEGER hz{};REQUIRE(QueryPerformanceFrequency(&hz) && hz.QuadPart>0);
    const std::array<transport::FrameKind,3> kinds{{
        transport::FrameKind::GameStereo,transport::FrameKind::CompactMono,
        transport::FrameKind::VerifiedCpuWorldStereo}};
    const std::array<std::uint32_t,3> tags{{91u,0u,92u}};
    const std::array<std::uint8_t,3> expected_left{{0x17u,0x2cu,0x3du}};
    const std::array<std::uint8_t,3> expected_right{{0x8bu,0u,0xc6u}};
    for(unsigned i=0;i<3u;++i){
        REQUIRE(WaitForSingleObject(ready,30000u)==WAIT_OBJECT_0);
        LARGE_INTEGER now{};REQUIRE(QueryPerformanceCounter(&now) && now.QuadPart>0);
        transport::FrameStamp inspected{};
        transport::Snapshot copied{};
        producerhealth::Snapshot receipt{};
        REQUIRE(frame_reader.Peek(inspected));
        REQUIRE(health_reader.Read(receipt));
        REQUIRE(inspected.frame_id==i+1u);
        REQUIRE(inspected.kind==kinds[i] && inspected.pose_sequence==tags[i]);
        REQUIRE(frame_reader.ReadReusable(copied));
        const bool known_pose=tags[i]!=0u;
        REQUIRE(handoff::AdmitOriginalPresentToXr(inspected,copied,receipt,
            producerhealth::Fresh(receipt,
                static_cast<std::uint64_t>(now.QuadPart),
                static_cast<std::uint64_t>(hz.QuadPart)),known_pose));
        REQUIRE(frame_gate.Decide(&inspected,
            static_cast<std::uint64_t>(now.QuadPart),
            static_cast<std::uint64_t>(hz.QuadPart))==
            transport::FrameUse::ReadNew);
        REQUIRE(frame_gate.Accept(copied,
            static_cast<std::uint64_t>(now.QuadPart),
            static_cast<std::uint64_t>(hz.QuadPart)));
        REQUIRE(copied.eyes[0].front()==expected_left[i]);
        REQUIRE(copied.eyes[0].back()==expected_left[i]);
        if(kinds[i]==transport::FrameKind::CompactMono){
            REQUIRE(copied.eyes[1].empty());
        }else{
            REQUIRE(copied.eyes[1].front()==expected_right[i]);
            REQUIRE(copied.eyes[1].back()==expected_right[i]);
        }
        // Every post-copy mutation of the exact physical D3D9 source
        // description must be denied even though the original bytes fit.
        ++copied.frame_id;
        REQUIRE(!handoff::AdmitOriginalPresentToXr(
            inspected,copied,receipt,true,known_pose));
        --copied.frame_id;
        auto falsified=receipt;falsified.frame_id++;
        REQUIRE(!handoff::AdmitOriginalPresentToXr(
            inspected,copied,falsified,true,known_pose));
        REQUIRE(ResetEvent(ready)!=FALSE);
        REQUIRE(SetEvent(advance)!=FALSE);
    }
    // Actual x86 writer now reports a newer FAILED original Present while
    // the x64 process still holds frame 3's accepted native two-eye pair.
    REQUIRE(WaitForSingleObject(ready,30000u)==WAIT_OBJECT_0);
    transport::FrameStamp older{};
    producerhealth::Snapshot newer_failed{};
    REQUIRE(frame_reader.Peek(older) && older.frame_id==3u);
    REQUIRE(health_reader.Read(newer_failed));
    REQUIRE(newer_failed.frame_id==4u);
    REQUIRE(newer_failed.reason_mask==
            producerhealth::OriginalPresentFailed);
    LARGE_INTEGER end_clock{};
    REQUIRE(QueryPerformanceCounter(&end_clock) && end_clock.QuadPart>0);
    transport::Snapshot held_x86{};
    REQUIRE(frame_reader.Read(held_x86));
    REQUIRE(xrcommit::VerifyOriginalFrameAtEnd(
        process_lease.Alive(),frame_gate,held_x86,true,
        static_cast<std::uint64_t>(end_clock.QuadPart),
        static_cast<std::uint64_t>(hz.QuadPart),
        newer_failed,true).reason==xrcommit::Reject::NewerFailedPresent);
    REQUIRE(ResetEvent(ready)!=FALSE);
    REQUIRE(SetEvent(advance)!=FALSE);
    REQUIRE(WaitForSingleObject(child.hProcess,30000u)==WAIT_OBJECT_0);
    DWORD code=1;REQUIRE(GetExitCodeProcess(child.hProcess,&code));
    REQUIRE(code==0u);
    REQUIRE(!process_lease.Alive());
    REQUIRE(process_lease.Terminal());
    REQUIRE(!process_lease.Attach(child.dwProcessId));
    CloseHandle(ready);CloseHandle(advance);CloseHandle(child.hProcess);
    return 0;
}
#else
#error "Specify x86 original-source producer or x64 XR consumer"
#endif
