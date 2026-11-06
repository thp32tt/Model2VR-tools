#pragma once
// Model2VR native producer health: 64-byte, pointer-free, x86/x64 ABI.
// No original game assets, addresses, textures, geometry or shader bytes.
#include <Windows.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
namespace m2vr::producerhealth {
constexpr std::uint32_t kMagic=0x3248504Du; // MPH2
constexpr std::uint32_t kVersion=2u;
enum Reason : std::uint32_t {
    NoMirroredPass=1u, NoVerifiedScene=2u, WrongDevice=4u,
    PoseUnavailable=8u, PoseExpired=16u, ReadbackFailed=32u,
    IdenticalEyes=64u, TransportFailed=128u,
    OriginalPresentFailed=256u, OriginalMonoFallback=512u,
    // Verified CPU-world pair was prepared for the physical original
    // Present, so speculative GPU two-eye readback was not performed.
    CpuWorldDeferredGpuReadback=1024u
};
constexpr std::uint32_t kKnownReasons=2047u;
constexpr bool OriginalMonoPublished(std::uint32_t mask) noexcept {
    return mask==OriginalMonoFallback;
}
// Capture the GPU eye targets before the original swap-chain Present, then
// publish only when the real game's Present HRESULT AND stereo tracking pass.
constexpr std::uint32_t FinalPresentReason(std::uint32_t prepared,
                                            bool original_succeeded,
                                            bool pose_still_live)noexcept{
    if(!original_succeeded)return prepared|OriginalPresentFailed;
    if(!prepared && !pose_still_live)return PoseExpired;
    return prepared;
}
constexpr bool MayPublishGameStereo(std::uint32_t reason)noexcept{
    return reason==0u;
}
struct Snapshot {
    std::uint64_t frame_id=0, timestamp_qpc=0;
    std::uint32_t reason_mask=0, verified_scene_draws=0;
    std::uint32_t mirrored_draws=0, mirrored_clears=0;
    std::array<std::uint32_t,4> family_draws{}; // DP, DIP, DPUP, DIPUP
};
static_assert(sizeof(Snapshot)==48);
struct alignas(64) Wire {
    std::uint32_t magic=0, version=0;
    volatile LONG generation=0;
    std::uint32_t writer_pid=0;
    Snapshot status{};
};
static_assert(sizeof(Wire)==64);
static_assert(offsetof(Wire,status)==16);
inline std::wstring NameForPid(std::uint32_t pid){
    return L"Local\\Model2VR_ProducerHealth_v2_"+std::to_wstring(pid);
}
constexpr std::uint64_t SumFamilies(const std::array<std::uint32_t,4>& c)noexcept {
    return std::uint64_t{c[0]}+c[1]+c[2]+c[3];
}
constexpr bool Fresh(const Snapshot& s,std::uint64_t now,
                     std::uint64_t hz) noexcept {
    return s.frame_id && s.timestamp_qpc && hz && hz<=UINT64_MAX/3u &&
        now>=s.timestamp_qpc && now-s.timestamp_qpc<=hz*3u;
}
// A newer failed original Present must not leave the HMD displaying
// an earlier successful eye frame as if it were the latest live game.
constexpr bool InvalidatesCachedPair(const Snapshot& health,bool fresh,
                                      std::uint64_t cached_frame_id,
                                      bool cached_was_native_stereo=false)noexcept {
    if(!fresh || health.frame_id<=cached_frame_id)return false;
    // An x86 original Present switching from GameStereo to *published*
    // original mono must immediately revoke stale projected native eyes,
    // even before the x64 host copies the newer 4-MiB compact payload.
    // A mono-to-mono update does not invalidate its predecessor; bounded
    // frame age already handles a missing/stalled producer.
    if(OriginalMonoPublished(health.reason_mask))
        return cached_was_native_stereo;
    return health.reason_mask!=0u;
}
constexpr const char* ReasonName(std::uint32_t mask)noexcept {
    if(!mask)return "PAIR_PUBLISHED";
    if(OriginalMonoPublished(mask))return "ORIGINAL_GAME_MONO_FALLBACK_PUBLISHED";
    if(mask&OriginalPresentFailed)return "ORIGINAL_D3D9_PRESENT_FAILED";
    if(mask&PoseUnavailable)return "POSE_NOT_ADMITTED";
    if(mask&PoseExpired)return "POSE_STALE";
    if(mask&NoVerifiedScene)return "NO_VERIFIED_WORLD_SCENE";
    if(mask&NoMirroredPass)return "NO_BACKBUFFER_REPLAY";
    if(mask&WrongDevice)return "WRONG_DEVICE";
    if(mask&ReadbackFailed)return "D3D9_READBACK_FAILED";
    if(mask&IdenticalEyes)return "IDENTICAL_EYE_PIXELS";
    if(mask&TransportFailed)return "IPC_PUBLISH_FAILED";
    if(mask&CpuWorldDeferredGpuReadback)
        return "VERIFIED_WORLD_FAILED_OR_ORIGINAL_MONO_UNAVAILABLE";
    return "UNKNOWN";
}
class Link {
public:
    Link()=default;
    Link(const Link&)=delete;
    Link& operator=(const Link&)=delete;
    ~Link(){Close();}
    bool Create(std::uint32_t pid){
        Close();
        if(!pid||pid!=GetCurrentProcessId())return false;
        auto name=NameForPid(pid);
        mapping_=CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,
                                    PAGE_READWRITE,0,sizeof(Wire),name.c_str());
        if(!mapping_)return false;
        if(GetLastError()==ERROR_ALREADY_EXISTS){Close();return false;}
        wire_=static_cast<Wire*>(MapViewOfFile(mapping_,FILE_MAP_ALL_ACCESS,0,0,sizeof(Wire)));
        if(!wire_){Close();return false;}
        std::memset(wire_,0,sizeof(Wire));
        wire_->magic=kMagic;wire_->version=kVersion;wire_->writer_pid=pid;
        pid_=pid;writer_=true;
        return true;
    }
    bool Open(std::uint32_t pid){
        Close();
        if(!pid)return false;
        auto name=NameForPid(pid);
        mapping_=OpenFileMappingW(FILE_MAP_READ,FALSE,name.c_str());
        if(!mapping_)return false;
        wire_=static_cast<Wire*>(MapViewOfFile(mapping_,FILE_MAP_READ,0,0,sizeof(Wire)));
        pid_=pid;
        if(!Valid()){Close();return false;}
        return true;
    }
    bool Valid()const noexcept {
        return wire_ && wire_->magic==kMagic &&
            wire_->version==kVersion && wire_->writer_pid==pid_;
    }
    bool Publish(std::uint64_t frame,std::uint32_t mask,
                 std::uint32_t scene,std::uint32_t draws,
                 std::uint32_t clears,
                 const std::array<std::uint32_t,4>& family)noexcept {
        if(!writer_||!Valid()||!frame||
           (mask&~kKnownReasons)||SumFamilies(family)!=draws)return false;
        LARGE_INTEGER now{};
        if(!QueryPerformanceCounter(&now)||now.QuadPart<=0)return false;
        const LONG old=InterlockedCompareExchange(&wire_->generation,0,0);
        // Health is the one-way receipt of the original physical Present.
        // Claim exclusive writer ownership before touching the 48-byte
        // record or its frame high-water mark; otherwise a concurrent CPU
        // world/GPU fallback status can erase a successful newer receipt.
        if((old&1) || old>=(std::numeric_limits<LONG>::max)()-2)
            return false;
        if(InterlockedCompareExchange(&wire_->generation,old+1,old)!=old)
            return false;
        if(frame<=last_frame_){
            InterlockedExchange(&wire_->generation,old+2);
            return false;
        }
        wire_->status={frame,static_cast<std::uint64_t>(now.QuadPart),
                       mask,scene,draws,clears,family};
        InterlockedExchange(&wire_->generation,old+2);
        last_frame_=frame;
        return true;
    }
    bool Read(Snapshot& out)const noexcept {
        if(!Valid())return false;
        const LONG start=wire_->generation;
        if(start&1)return false;
        MemoryBarrier();
        Snapshot copy{};
        std::memcpy(&copy,&wire_->status,sizeof(copy));
        MemoryBarrier();
        const LONG end=wire_->generation;
        if(start!=end||(end&1)||!copy.frame_id||
           (copy.reason_mask&~kKnownReasons)||
           SumFamilies(copy.family_draws)!=copy.mirrored_draws)return false;
        out=copy;
        return true;
    }
    void Close()noexcept {
        if(wire_)UnmapViewOfFile(wire_);
        if(mapping_)CloseHandle(mapping_);
        wire_=nullptr;mapping_=nullptr;
        writer_=false;pid_=0;last_frame_=0;
    }
private:
    Wire* wire_=nullptr;
    HANDLE mapping_=nullptr;
    std::uint32_t pid_=0;
    std::uint64_t last_frame_=0;
    bool writer_=false;
};
} // namespace m2vr::producerhealth
