#include "../src/common/stereo_pose_ipc.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <atomic>
#include <thread>
#include <string>
#include <vector>
#include <cstring>
#define REQUIRE(expr) do {if(!(expr)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#expr);std::exit(1);}}while(0)
namespace {
std::wstring PoseLeaseEventName(DWORD emulator_pid,const wchar_t* suffix){
    return L"Local\\Model2VR_XrPublisherLease_"+
        std::to_wstring(emulator_pid)+suffix;
}
}
int main(int argc,char** argv){
    using namespace m2vr::pose;
    if(argc==3 && std::strcmp(argv[1],"--pose-publisher-child")==0){
        const auto emulator_pid=static_cast<DWORD>(std::strtoul(argv[2],nullptr,10));
        REQUIRE(emulator_pid!=0u);
        Link publisher;
        REQUIRE(publisher.Create(emulator_pid));
        Frame actual{};
        actual.sequence=51u;
        actual.predicted_display_time=9000001;
        actual.reserved=7u;
        actual.view_flags=kFullSixDofTracked;
        actual.eyes[0].position_m[0]=-0.032f;
        actual.eyes[1].position_m[0]=0.032f;
        REQUIRE(publisher.Publish(actual));
        const HANDLE ready=OpenEventW(EVENT_MODIFY_STATE,FALSE,
            PoseLeaseEventName(emulator_pid,L"_ready").c_str());
        const HANDLE release=OpenEventW(SYNCHRONIZE,FALSE,
            PoseLeaseEventName(emulator_pid,L"_release").c_str());
        REQUIRE(ready && release);
        REQUIRE(SetEvent(ready)!=FALSE);
        REQUIRE(WaitForSingleObject(release,20000u)==WAIT_OBJECT_0);
        CloseHandle(release);CloseHandle(ready);
        return 0;
    }
    REQUIRE(argc==1);
    Link host,emulator;
    const auto key=GetCurrentProcessId();
    REQUIRE(host.Create(key));
    REQUIRE(!emulator.Create(key));
    REQUIRE(emulator.Open(key));
    Frame got{};
    REQUIRE(!emulator.Read(got));
    Frame frame{};
    frame.sequence=1;
    frame.predicted_display_time=8123456;
    frame.view_flags=kOrientationValid|kPositionValid;
    frame.eyes[0].position_m[0]=-0.032f;
    frame.eyes[1].position_m[0]=0.032f;
    frame.eyes[0].fov_left_right_up_down[0]=-0.79f;
    frame.eyes[1].fov_left_right_up_down[1]=0.8f;
    REQUIRE(host.Publish(frame));
    REQUIRE(!host.Publish(frame));
    REQUIRE(emulator.Read(got));
    REQUIRE(got.sequence==1 && got.predicted_display_time==8123456);
    REQUIRE(!emulator.ReadTracked(got)); // VALID is not tracked
    REQUIRE(got.eyes[0].position_m[0]<0 && got.eyes[1].position_m[0]>0);
    REQUIRE(std::fabs(got.eyes[1].fov_left_right_up_down[1]-0.8f)<0.00001f);
    frame.sequence=2;frame.eyes[0].position_m[1]=0.5f;
    REQUIRE(host.Publish(frame));
    REQUIRE(emulator.Read(got));
    REQUIRE(got.sequence==2 && std::fabs(got.eyes[0].position_m[1]-0.5f)<0.00001f);
    REQUIRE(host.Invalidate());
    REQUIRE(!emulator.Read(got)); // transient tracking loss clears stale pose
    frame.sequence=3;
    REQUIRE(host.Publish(frame));
    REQUIRE(emulator.Read(got));
    REQUIRE(got.sequence==3); // tracking recovery
    // Full original HMD pose pairs may arrive close to tracking invalidation.
    // A release/acquire seqlock must never publish a half-old eye pair.
    for(std::uint64_t id=4u;id<24u;++id){
        Frame first=frame,second=frame;
        first.sequence=second.sequence=id;
        first.eyes[0].position_m[0]=-0.03f;
        first.eyes[1].position_m[0]=0.03f;
        second.eyes[0].position_m[0]=-0.07f;
        second.eyes[1].position_m[0]=0.07f;
        std::atomic<unsigned> ready{0u};
        std::atomic<bool> go{false};
        bool wrote_first=false,wrote_second=false;
        std::thread a([&]{
            ready.fetch_add(1u);
            while(!go.load())std::this_thread::yield();
            wrote_first=host.Publish(first);
        });
        std::thread b([&]{
            ready.fetch_add(1u);
            while(!go.load())std::this_thread::yield();
            wrote_second=host.Publish(second);
        });
        while(ready.load()!=2u)std::this_thread::yield();
        go.store(true);
        a.join();b.join();
        REQUIRE(wrote_first!=wrote_second);
        REQUIRE(emulator.Read(got));
        REQUIRE(got.sequence==id);
        const float x=wrote_first?0.03f:0.07f;
        REQUIRE(std::fabs(got.eyes[0].position_m[0]+x)<0.00001f);
        REQUIRE(std::fabs(got.eyes[1].position_m[0]-x)<0.00001f);
    }
    REQUIRE(host.Invalidate());
    REQUIRE(!emulator.Read(got));
    frame.sequence=24u;
    REQUIRE(host.Publish(frame));
    REQUIRE(emulator.Read(got) && got.sequence==24u);
    Frame tracked=frame;
    tracked.sequence=25u;
    tracked.reserved=7u;
    tracked.predicted_display_time=9000000;
    tracked.view_flags=kFullSixDofTracked;
    REQUIRE(host.Publish(tracked));
    REQUIRE(emulator.ReadTracked(got));
    REQUIRE(got.sequence==25u && got.reserved==7u && got.published_qpc!=0u);
    tracked.sequence=26u;tracked.reserved=0u;
    REQUIRE(host.Publish(tracked));
    REQUIRE(!emulator.ReadTracked(got));
    tracked.sequence=27u;tracked.reserved=7u;
    tracked.predicted_display_time=0;
    REQUIRE(host.Publish(tracked));
    REQUIRE(!emulator.ReadTracked(got));
    tracked.sequence=28u;tracked.predicted_display_time=9000001;
    tracked.view_flags=kFullSixDofTracked&~kPositionTracked;
    REQUIRE(host.Publish(tracked));
    REQUIRE(!emulator.ReadTracked(got));
    tracked.sequence=29u;tracked.view_flags=kFullSixDofTracked;
    REQUIRE(host.Publish(tracked));
    REQUIRE(emulator.ReadTracked(got));
    REQUIRE(got.sequence==29u && got.predicted_display_time==9000001);
    // Actual Windows named-mapping test: emulate a disconnected host whose
    // pose QPC is still within legacy 2 seconds but too old to drive an
    // original D3D9 World3D Draw. Keep the same source sequence/eyes.
    LARGE_INTEGER clock{},frequency{};
    REQUIRE(QueryPerformanceCounter(&clock) && clock.QuadPart>0);
    REQUIRE(QueryPerformanceFrequency(&frequency) && frequency.QuadPart>0);
    if(static_cast<std::uint64_t>(clock.QuadPart)>
       static_cast<std::uint64_t>(frequency.QuadPart)){
        HANDLE mapped=OpenFileMappingW(FILE_MAP_WRITE,FALSE,
                                       NameForPid(key).c_str());
        REQUIRE(mapped!=nullptr);
        auto* raw=static_cast<Shared*>(MapViewOfFile(
            mapped,FILE_MAP_WRITE,0,0,sizeof(Shared)));
        REQUIRE(raw!=nullptr);
        const LONG before=raw->generation;
        REQUIRE(!(before&1));
        REQUIRE(InterlockedCompareExchange(
            &raw->generation,before+1,before)==before);
        const auto live_qpc=raw->frame.published_qpc;
        raw->frame.published_qpc=static_cast<std::uint64_t>(
            clock.QuadPart-frequency.QuadPart/2); // 500 ms stale
        InterlockedExchange(&raw->generation,before+2);
        REQUIRE(emulator.Read(got)); // diagnostic two-second compatibility
        REQUIRE(!emulator.ReadTracked(got)); // 125-ms genuine game gate
        const LONG second=raw->generation;
        REQUIRE(InterlockedCompareExchange(
            &raw->generation,second+1,second)==second);
        raw->frame.published_qpc=live_qpc;
        InterlockedExchange(&raw->generation,second+2);
        UnmapViewOfFile(raw);CloseHandle(mapped);
    }
    emulator.Close();host.Close();
    REQUIRE(!emulator.Read(got));
    // Real Windows process lifetime regression: a separate publisher
    // exits immediately after delivering tracked views. The original
    // game keeps its mapping open, and QPC is still fresh (<125ms).
    // A timestamp-only lease used to incorrectly admit this dead source.
    const HANDLE ready=CreateEventW(nullptr,TRUE,FALSE,
        PoseLeaseEventName(key,L"_ready").c_str());
    const HANDLE release=CreateEventW(nullptr,TRUE,FALSE,
        PoseLeaseEventName(key,L"_release").c_str());
    REQUIRE(ready && release);
    wchar_t executable[MAX_PATH]{};
    const DWORD path_length=GetModuleFileNameW(nullptr,executable,MAX_PATH);
    REQUIRE(path_length>0u && path_length<MAX_PATH);
    const std::wstring command=L"\""+std::wstring(executable)+
        L"\" --pose-publisher-child "+std::to_wstring(key);
    std::vector<wchar_t> args(command.begin(),command.end());
    args.push_back(L'\0');
    STARTUPINFOW startup{};startup.cb=sizeof(startup);
    PROCESS_INFORMATION child{};
    REQUIRE(CreateProcessW(executable,args.data(),nullptr,nullptr,FALSE,0,
        nullptr,nullptr,&startup,&child)!=FALSE);
    CloseHandle(child.hThread);
    REQUIRE(WaitForSingleObject(ready,20000u)==WAIT_OBJECT_0);
    REQUIRE(emulator.Open(key));
    REQUIRE(emulator.ReadTracked(got));
    REQUIRE(got.sequence==51u && got.reserved==7u);
    REQUIRE(SetEvent(release)!=FALSE);
    REQUIRE(WaitForSingleObject(child.hProcess,20000u)==WAIT_OBJECT_0);
    DWORD exit_code=1u;
    REQUIRE(GetExitCodeProcess(child.hProcess,&exit_code) && exit_code==0u);
    // Keep the map pinned in the x86 consumer. With no artificial delay,
    // the last stamped pose is still present, but the creator process
    // HANDLE is now signalled and must defeat BOTH read entrypoints.
    REQUIRE(!emulator.Valid());
    REQUIRE(!emulator.Read(got));
    REQUIRE(!emulator.ReadTracked(got));
    emulator.Close();
    REQUIRE(!emulator.Open(key)); // producer exited; no named writer
    CloseHandle(child.hProcess);
    CloseHandle(release);CloseHandle(ready);
    return 0;
}
