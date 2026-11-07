#pragma once
// ABI shared between 64-bit OpenXR renderer and 32-bit Model2 game renderer.
// The host publishes predicted display-time stereo camera views. A consumer
// must latch one coherent packet per emulated render frame; never read it per
// vertex and never rerun game simulation for another eye.
#include <Windows.h>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
namespace m2vr::pose {
constexpr std::uint32_t kMagic=0x3250564Du; // "MVP2"
constexpr std::uint32_t kVersion=1;
constexpr std::uint32_t kOrientationValid=0x1u;
constexpr std::uint32_t kPositionValid=0x2u;
// Mirrors OpenXR XrViewStateFlags bit values; no frame/IPC layout change.
constexpr std::uint32_t kOrientationTracked=0x4u;
constexpr std::uint32_t kPositionTracked=0x8u;
constexpr std::uint32_t kFullSixDofTracked=
    kOrientationValid|kPositionValid|
    kOrientationTracked|kPositionTracked;
struct EyeView {
    float position_m[3]{};
    float orientation_xyzw[4]{0,0,0,1};
    float fov_left_right_up_down[4]{};
};
static_assert(sizeof(EyeView)==44);
struct Frame {
    std::uint64_t sequence=0;
    std::int64_t predicted_display_time=0;
    std::uint64_t published_qpc=0;
    EyeView eyes[2]{};
    std::uint32_t view_flags=0;
    // Existing ABI slot: 0 means legacy/no LOCAL recenter provenance;
    // nonzero increments on XR session restart or LOCAL changeTime.
    std::uint32_t reserved=0;
};
static_assert(sizeof(Frame)==120);
struct alignas(8) Shared {
    std::uint32_t magic=0,version=0,size=0,reserved=0;
    volatile LONG generation=0;
    std::uint32_t writer_pid=0;
    Frame frame{};
};
static_assert(offsetof(Shared,frame)==24);
static_assert(sizeof(Shared)==144);
inline std::wstring NameForPid(std::uint32_t pid){
    return L"Local\\Model2VR_HeadViews_v1_"+std::to_wstring(pid);
}
class Link {
public:
    Link()=default;
    Link(const Link&)=delete;
    Link& operator=(const Link&)=delete;
    ~Link(){Close();}
    bool Create(std::uint32_t emulator_pid){
        Close();
        if(!emulator_pid)return false;
        const auto name=NameForPid(emulator_pid);
        handle_=CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,
                                  0,sizeof(Shared),name.c_str());
        if(!handle_)return false;
        if(GetLastError()==ERROR_ALREADY_EXISTS){Close();return false;}
        data_=static_cast<Shared*>(MapViewOfFile(handle_,FILE_MAP_ALL_ACCESS,0,0,sizeof(Shared)));
        if(!data_){Close();return false;}
        std::memset(data_,0,sizeof(Shared));
        data_->magic=kMagic;data_->version=kVersion;data_->size=sizeof(Shared);
        data_->writer_pid=GetCurrentProcessId();
        writable_=true;
        return true;
    }
    bool Open(std::uint32_t emulator_pid){
        Close();
        if(!emulator_pid)return false;
        const auto name=NameForPid(emulator_pid);
        handle_=OpenFileMappingW(FILE_MAP_READ,FALSE,name.c_str());
        if(!handle_)return false;
        data_=static_cast<Shared*>(MapViewOfFile(handle_,FILE_MAP_READ,0,0,sizeof(Shared)));
        if(!Valid()){Close();return false;}
        return true;
    }
    bool Valid()const{
        return data_ && data_->magic==kMagic && data_->version==kVersion &&
               data_->size==sizeof(Shared);
    }
    bool Publish(const Frame& value){
        if(!writable_||!Valid()||value.sequence==0u||
           !(value.view_flags&kOrientationValid))return false;
        const LONG previous=InterlockedCompareExchange(&data_->generation,0,0);
        // Host pose Publish and tracking-loss Invalidate must NEVER both
        // own the same 120-byte x86/x64 IPC packet. A read-then-exchange
        // seqlock acquisition is not a writer lock.
        if((previous&1) || previous>=(std::numeric_limits<LONG>::max)()-2)
            return false;
        if(InterlockedCompareExchange(&data_->generation,
                                      previous+1,previous)!=previous)
            return false;
        // This high-water mark is protected by the odd writer generation.
        if(value.sequence<=last_written_){
            InterlockedExchange(&data_->generation,previous+2);
            return false;
        }
        Frame stamped=value;
        LARGE_INTEGER now{};
        // The Win64 xrLocateViews producer must not claim publication when
        // QPC cannot authenticate the source time. x86 will later reject
        // zero QPC, but a false success here would commit a stale pose tag.
        if(!QueryPerformanceCounter(&now) || now.QuadPart<=0){
            InterlockedExchange(&data_->generation,previous+2);
            return false;
        }
        stamped.published_qpc=static_cast<std::uint64_t>(now.QuadPart);
        std::memcpy(&data_->frame,&stamped,sizeof(Frame));
        // The next host write may claim the lease as soon as this goes
        // even. Keep sequence monotonic while the current writer owns it.
        last_written_=value.sequence;
        InterlockedExchange(&data_->generation,previous+2);
        return true;
    }
    // Explicitly revoke the last valid pose on transient tracking loss.
    // Leaving an old pose in shared memory can otherwise produce a stale
    // two-second headset orientation even when the host stops rendering.
    bool Invalidate() {
        if(!writable_ || !Valid())return false;
        const LONG previous=InterlockedCompareExchange(&data_->generation,0,0);
        if((previous&1) || previous>=(std::numeric_limits<LONG>::max)()-2)
            return false;
        if(InterlockedCompareExchange(&data_->generation,
                                      previous+1,previous)!=previous)
            return false;
        const Frame invalid{};
        std::memcpy(&data_->frame,&invalid,sizeof(Frame));
        InterlockedExchange(&data_->generation,previous+2);
        return true;
    }
    bool Read(Frame& out)const{
        if(!Valid())return false;
        const LONG before=data_->generation;
        if(before&1)return false;
        MemoryBarrier();
        Frame copy{};
        std::memcpy(&copy,&data_->frame,sizeof(Frame));
        MemoryBarrier();
        const LONG after=data_->generation;
        if(before!=after||(after&1)||copy.sequence==0||
           !(copy.view_flags&kOrientationValid))return false;
        LARGE_INTEGER now{},freq{};
        if(!QueryPerformanceCounter(&now) || now.QuadPart<=0 ||
           !QueryPerformanceFrequency(&freq) || freq.QuadPart<=0)
            return false;
        const auto stamp=copy.published_qpc;
        const auto current=static_cast<std::uint64_t>(now.QuadPart);
        const auto hz=static_cast<std::uint64_t>(freq.QuadPart);
        // Monotonic, nonfuture source QPC with bounded 2-second lease.
        if(!stamp || current<stamp ||
           hz>(std::numeric_limits<std::uint64_t>::max)()/2u ||
           current-stamp>hz*2u)
            return false; // fail closed on disconnected/abandoned XR host
        out=copy;
        return true;
    }
    // The original Win32 game Draw adapter uses this stricter entry,
    // while Read() remains backward-compatible for 2D/diagnostic users.
    // Non-6DoF, missing XrTime or unknown LOCAL provenance never become
    // a genuine D3D9 independently projected eye camera.
    bool ReadTracked(Frame& out)const{
        Frame candidate{};
        if(!Read(candidate) ||
           (candidate.view_flags & kFullSixDofTracked)!=kFullSixDofTracked ||
           candidate.predicted_display_time<=0 ||
           candidate.reserved==0u ||
           candidate.sequence>
                (std::numeric_limits<std::uint32_t>::max)())
            return false;
        // The backwards-compatible 2-second Read() lease is appropriate
        // for remote diagnostics but FAR too old for authentic x86 D3D9
        // head-tracked Draw. Cap original-source exposure to the same
        // 125-ms native eye freshness window as the Win64 compositor.
        LARGE_INTEGER now{},frequency{};
        if(!QueryPerformanceCounter(&now) || now.QuadPart<=0 ||
           !QueryPerformanceFrequency(&frequency) ||
           frequency.QuadPart<=0)
            return false;
        const auto current=static_cast<std::uint64_t>(now.QuadPart);
        const auto hz=static_cast<std::uint64_t>(frequency.QuadPart);
        if(current<candidate.published_qpc ||
           current-candidate.published_qpc>hz/8u)
            return false;
        out=candidate;
        return true;
    }
    void Close(){
        if(data_)UnmapViewOfFile(data_);
        if(handle_)CloseHandle(handle_);
        data_=nullptr;handle_=nullptr;writable_=false;last_written_=0;
    }
private:
    HANDLE handle_=nullptr;
    Shared* data_=nullptr;
    bool writable_=false;
    std::uint64_t last_written_=0;
};
} // namespace m2vr::pose
