#pragma once
// Model2VR MIT original code. Shared Win32 stereo frame transport.
// ABI v1: single writer, multiple readers; RGBA8, two independent eyes.
// The frame kind is mandatory so diagnostics cannot be mislabeled as game VR.
#include <Windows.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace m2vr::transport {
constexpr std::uint32_t kMagic = 0x3253464Du;  // "MFS2"
constexpr std::uint32_t kVersion = 1;
constexpr std::uint32_t kWidth = 1024;
constexpr std::uint32_t kHeight = 1024;
constexpr std::uint32_t kPitch = 4 * kWidth;
constexpr std::size_t kBytesPerEye = static_cast<std::size_t>(kPitch) * kHeight;
constexpr std::uint32_t kPixelRGBA8 = 1;
// GameMonoFallback carries pixels from the ORIGINAL x86 D3D9 game backbuffer.
// It deliberately has no source-eye pose and is NOT calibrated native stereo.
// CompactMono=4 is an ADDITIVE subtype (no wire/header/layout change).
// Legacy GameMonoFallback=3 keeps the original two-4MiB-eye semantics.
// CompactMono carries one *real* original D3D9 backbuffer in eye 0 only;
// its second eye has no valid payload. An old reader will reject kind=4.
enum class FrameKind : std::uint32_t {
    Invalid=0, GameStereo=1, Diagnostic=2,
    GameMonoFallback=3, CompactMono=4,
    // Only source-authenticated CPU world, not hardware HMD validation.
    VerifiedCpuWorldStereo=5
};
constexpr bool IsStereoGame(FrameKind k)noexcept{
    return k==FrameKind::GameStereo || k==FrameKind::VerifiedCpuWorldStereo;
}
// Host pose-history tags increase during one process lifetime. A newer
// original D3D9 frame may legitimately reuse a tag, but never roll back.
// Tag zero stays ABI-readable; the live HMD gate rejects untagged stereo.
constexpr bool SourcePoseNotRegressed(FrameKind kind,
                                     std::uint32_t tag,
                                     std::uint32_t highest) noexcept {
    return !IsStereoGame(kind) || tag==0u || highest==0u ||
           tag>=highest;
}
constexpr bool IsMonoGame(FrameKind kind) noexcept {
    return kind==FrameKind::GameMonoFallback || kind==FrameKind::CompactMono;
}
constexpr bool ValidSourceKind(FrameKind kind) noexcept {
    return IsStereoGame(kind) || kind==FrameKind::Diagnostic ||
           IsMonoGame(kind);
}
constexpr bool ValidEyePayload(FrameKind kind,std::size_t left,
                               std::size_t right) noexcept {
    return left==kBytesPerEye && (kind==FrameKind::CompactMono
        ? right==0u
        : right==kBytesPerEye);
}

// Non-pointer wire format: identical for 32-bit producer and 64-bit host.
struct alignas(64) Header {
    std::uint32_t magic;
    std::uint32_t version;
    std::uint32_t width;
    std::uint32_t height;
    std::uint32_t pitch;
    std::uint32_t pixel_format;
    std::uint32_t bytes_per_eye;
    std::uint32_t header_size;
    volatile LONG sequence; // odd while writer owns buffers, even when committed
    volatile LONG kind;
    std::uint64_t frame_id;
    std::uint64_t timestamp_qpc;
    std::uint32_t writer_pid;
    std::uint32_t pose_sequence; // 0 unknown; otherwise source OpenXR pose
};
static_assert(sizeof(Header) == 64);
static_assert(offsetof(Header, frame_id)==40);
constexpr std::size_t kSharedBytes = sizeof(Header) + kBytesPerEye * 2;
inline std::wstring NameForPid(std::uint32_t emulator_pid) {
    return L"Local\\Model2VR_StereoFrames_v1_" + std::to_wstring(emulator_pid);
}
struct FrameStamp {
    std::uint64_t frame_id=0;
    std::uint64_t timestamp_qpc=0;
    FrameKind kind=FrameKind::Invalid;
    std::uint32_t writer_pid=0;
    std::uint32_t pose_sequence=0;
};
struct Snapshot {
    std::uint64_t frame_id=0;
    std::uint64_t timestamp_qpc=0;
    FrameKind kind=FrameKind::Invalid;
    std::uint32_t pose_sequence=0;
    std::array<std::vector<std::uint8_t>,2> eyes;
};
class SharedFrames {
public:
    SharedFrames()=default;
    SharedFrames(const SharedFrames&)=delete;
    SharedFrames& operator=(const SharedFrames&)=delete;
    ~SharedFrames(){ Close(); }
    bool Create(std::uint32_t key_pid){
        Close();
        // Only the genuine source process can create its own mapping.
        if(!key_pid || key_pid!=GetCurrentProcessId())return false;
        const auto name=NameForPid(key_pid);
        mapping_=CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,
               0,static_cast<DWORD>(kSharedBytes),name.c_str());
        if(!mapping_)return false;
        if(GetLastError()==ERROR_ALREADY_EXISTS){Close();return false;} // one writer
        base_=static_cast<std::uint8_t*>(MapViewOfFile(mapping_,FILE_MAP_ALL_ACCESS,
                                                        0,0,kSharedBytes));
        if(!base_){Close();return false;}
        auto* h=reinterpret_cast<Header*>(base_);
        std::memset(h,0,sizeof(Header));
        h->magic=kMagic;h->version=kVersion;
        h->width=kWidth;h->height=kHeight;h->pitch=kPitch;
        h->pixel_format=kPixelRGBA8;
        h->bytes_per_eye=static_cast<std::uint32_t>(kBytesPerEye);
        h->header_size=sizeof(Header);
        h->writer_pid=key_pid;
        key_pid_=key_pid;
        writable_=true;
        return true;
    }
    bool Open(std::uint32_t key_pid){
        Close();
        if(!key_pid)return false;
        const auto name=NameForPid(key_pid);
        mapping_=OpenFileMappingW(FILE_MAP_READ,FALSE,name.c_str());
        if(!mapping_)return false;
        base_=static_cast<std::uint8_t*>(MapViewOfFile(mapping_,FILE_MAP_READ,0,0,kSharedBytes));
        if(!base_){Close();return false;}
        key_pid_=key_pid;
        if(!Valid()){Close();return false;}
        return true;
    }
    bool Valid()const{
        if(!base_)return false;
        const auto* h=reinterpret_cast<const Header*>(base_);
        return h->magic==kMagic && h->version==kVersion &&
               h->width==kWidth && h->height==kHeight && h->pitch==kPitch &&
               h->pixel_format==kPixelRGBA8 && h->bytes_per_eye==kBytesPerEye &&
               h->header_size==sizeof(Header) &&
               key_pid_!=0u && h->writer_pid==key_pid_;
    }
    bool Publish(const std::uint8_t* left,const std::uint8_t* right,
                 std::uint64_t frame_id,FrameKind kind,
                 std::uint32_t pose_sequence=0) {
        if(!writable_||!Valid()||!left||frame_id==0||
           (!right && kind!=FrameKind::CompactMono) ||
           !ValidSourceKind(kind) ||
           (kind==FrameKind::VerifiedCpuWorldStereo && pose_sequence==0u) ||
           (!IsStereoGame(kind) && pose_sequence!=0u))
            return false;
        auto* h=reinterpret_cast<Header*>(base_);
        // A writer must not overlap itself. No consumer ever writes.
        const LONG before=InterlockedCompareExchange(&h->sequence,0,0);
        // The original D3D9 Present and the CPU verified-world fallback
        // can attempt publication on separate x86 threads. Reading an
        // even seqlock and then blindly exchanging it is NOT mutual exclusion:
        // two 8-MiB writers could claim the same generation and mix eyes.
        // Acquire ownership with a real compare-and-swap, fail closed on a
        // competing writer or a generation about to overflow signed LONG.
        if((before&1) || before>=(std::numeric_limits<LONG>::max)()-2)
            return false;
        if(InterlockedCompareExchange(&h->sequence,before+1,before)!=before)
            return false;
        // last_written_ is protected by the odd-generation writer lease,
        // not sampled before ownership (which would be a C++ data race).
        if(frame_id<=last_written_ ||
           !SourcePoseNotRegressed(kind,pose_sequence,last_stereo_pose_)){
            // Keep the accepted HMD camera order through original mono
            // fallback. Reject before overwriting shared eye pixels.
            InterlockedExchange(&h->sequence,before+2);
            return false;
        }
        // Fail before touching either eye on a broken native QPC clock.
        // A zero timestamp cannot be linked to the post-COM Present
        // receipt, and must never overwrite a previously valid eye packet.
        LARGE_INTEGER time{};
        if(!QueryPerformanceCounter(&time) || time.QuadPart<=0 ||
           (last_written_!=0u &&
            (h->timestamp_qpc==0u ||
             static_cast<std::uint64_t>(time.QuadPart)<=h->timestamp_qpc))){
            // Windows QPC must advance between successful original source
            // publications. Never overwrite 4/8 MiB of valid eye pixels
            // under a newer ID but an equal/reversed source timestamp.
            InterlockedExchange(&h->sequence,before+2);
            return false;
        }
        std::memcpy(base_+sizeof(Header),left,kBytesPerEye);
        // Do NOT copy 4 MiB of redundant pixels for a mono cinema quad.
        // The kind=4 marker is published only after the seqlock commits.
        // GameStereo, Diagnostic and legacy mono retain two source images.
        if(kind!=FrameKind::CompactMono)
            std::memcpy(base_+sizeof(Header)+kBytesPerEye,right,kBytesPerEye);
        h->frame_id=frame_id;
        h->pose_sequence=pose_sequence;
        h->timestamp_qpc=static_cast<std::uint64_t>(time.QuadPart);
        InterlockedExchange(&h->kind,static_cast<LONG>(kind));
        // Publish the monotonic source high-water mark BEFORE releasing
        // the writer lease, not after: a second x86 caller may acquire the
        // next even generation immediately and must see this frame ID.
        last_written_=frame_id;
        if(IsStereoGame(kind) && pose_sequence!=0u)
            last_stereo_pose_=pose_sequence;
        InterlockedExchange(&h->sequence,before+2);
        return true;
    }
    // Small, coherent metadata check before copying two 4-MiB eyes.
    // No writer lock, no extra mapped memory, and no ABI wire change.
    bool Peek(FrameStamp& out)const{
        if(!Valid())return false;
        const auto* h=reinterpret_cast<const Header*>(base_);
        const LONG start=h->sequence;
        MemoryBarrier();
        if(start&1)return false;
        FrameStamp stamp{};
        stamp.frame_id=h->frame_id;
        stamp.timestamp_qpc=h->timestamp_qpc;
        stamp.kind=static_cast<FrameKind>(h->kind);
        stamp.writer_pid=h->writer_pid;
        stamp.pose_sequence=h->pose_sequence;
        MemoryBarrier();
        const LONG end=h->sequence;
        if(start!=end || (end&1) || stamp.frame_id==0 ||
           stamp.timestamp_qpc==0u || stamp.writer_pid!=key_pid_ ||
           !ValidSourceKind(stamp.kind) ||
           (stamp.kind==FrameKind::VerifiedCpuWorldStereo &&
            stamp.pose_sequence==0u) ||
           (!IsStereoGame(stamp.kind) && stamp.pose_sequence!=0u))
            return false;
        out=stamp;
        return true;
    }
    // A real 60Hz Win32 producer can send two 4-MiB eyes on every game
    // frame. The x64 host owns a separate staging Snapshot and can reuse
    // both vectors without fresh allocation after the initial two slots.
    // On any torn seqlock read this staging buffer is UNSAFE to display;
    // the caller must retain its independent previously accepted Snapshot.
    bool ReadReusable(Snapshot& staging)const{
        if(!Valid())return false;
        const auto* h=reinterpret_cast<const Header*>(base_);
        const LONG start=h->sequence;
        MemoryBarrier();
        if(start&1)return false;
        const FrameKind kind=static_cast<FrameKind>(h->kind);
        if(!ValidSourceKind(kind))return false;
        const auto frame=h->frame_id, timestamp=h->timestamp_qpc;
        const auto pose_sequence=h->pose_sequence;
        if(frame==0u || timestamp==0u ||
           (kind==FrameKind::VerifiedCpuWorldStereo &&
            pose_sequence==0u) ||
           (!IsStereoGame(kind) && pose_sequence!=0u))return false;
        if(staging.eyes[0].size()!=kBytesPerEye)
            staging.eyes[0].resize(kBytesPerEye);
        std::memcpy(staging.eyes[0].data(),base_+sizeof(Header),kBytesPerEye);
        if(kind==FrameKind::CompactMono){
            // Invalidate any previous right-eye stereo frame so the host
            // cannot reuse it accidentally when switching source kinds.
            staging.eyes[1].clear();
        }else{
            if(staging.eyes[1].size()!=kBytesPerEye)
                staging.eyes[1].resize(kBytesPerEye);
            std::memcpy(staging.eyes[1].data(),
                        base_+sizeof(Header)+kBytesPerEye,kBytesPerEye);
        }
        MemoryBarrier();
        const LONG end=h->sequence;
        if(start!=end || (end&1) || frame==0u ||
           h->writer_pid!=key_pid_)return false;
        // Publish the metadata only once both eyes passed a single coherent
        // source generation. A rejected staging copy remains recyclable.
        staging.frame_id=frame;
        staging.timestamp_qpc=timestamp;
        staging.kind=kind;
        staging.pose_sequence=pose_sequence;
        return true;
    }
    // Legacy caller contract: preserve the output entirely on failure.
    bool Read(Snapshot& out)const{
        Snapshot tmp{};
        if(!ReadReusable(tmp))return false;
        out=std::move(tmp);
        return true;
    }
    void Close(){
        if(base_)UnmapViewOfFile(base_);
        if(mapping_)CloseHandle(mapping_);
        base_=nullptr;mapping_=nullptr;writable_=false;
        last_written_=0;last_stereo_pose_=0;key_pid_=0;
    }
private:
    HANDLE mapping_=nullptr;
    std::uint8_t* base_=nullptr;
    bool writable_=false;
    std::uint32_t key_pid_=0;
    std::uint64_t last_written_=0;
    std::uint32_t last_stereo_pose_=0;
};
} // namespace m2vr::transport
