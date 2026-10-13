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
#include <vector>

namespace m2vr::transport {
constexpr std::uint32_t kMagic = 0x3253464Du;  // "MFS2"
constexpr std::uint32_t kVersion = 1;
constexpr std::uint32_t kWidth = 1024;
constexpr std::uint32_t kHeight = 1024;
constexpr std::uint32_t kPitch = 4 * kWidth;
constexpr std::size_t kBytesPerEye = static_cast<std::size_t>(kPitch) * kHeight;
constexpr std::uint32_t kPixelRGBA8 = 1;
enum class FrameKind : std::uint32_t { Invalid=0, GameStereo=1, Diagnostic=2 };

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
    std::uint32_t reserved;
};
static_assert(sizeof(Header) == 64);
static_assert(offsetof(Header, frame_id)==40);
constexpr std::size_t kSharedBytes = sizeof(Header) + kBytesPerEye * 2;
inline std::wstring NameForPid(std::uint32_t emulator_pid) {
    return L"Local\\Model2VR_StereoFrames_v1_" + std::to_wstring(emulator_pid);
}
struct Snapshot {
    std::uint64_t frame_id=0;
    std::uint64_t timestamp_qpc=0;
    FrameKind kind=FrameKind::Invalid;
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
        if(!key_pid)return false;
        const auto name=NameForPid(key_pid);
        mapping_=CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,
               static_cast<DWORD>(kSharedBytes>>32),static_cast<DWORD>(kSharedBytes),name.c_str());
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
        h->writer_pid=GetCurrentProcessId();
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
        if(!Valid()){Close();return false;}
        return true;
    }
    bool Valid()const{
        if(!base_)return false;
        const auto* h=reinterpret_cast<const Header*>(base_);
        return h->magic==kMagic && h->version==kVersion &&
               h->width==kWidth && h->height==kHeight && h->pitch==kPitch &&
               h->pixel_format==kPixelRGBA8 && h->bytes_per_eye==kBytesPerEye &&
               h->header_size==sizeof(Header);
    }
    bool Publish(const std::uint8_t* left,const std::uint8_t* right,
                 std::uint64_t frame_id,FrameKind kind) {
        if(!writable_||!Valid()||!left||!right||frame_id==0||
           (kind!=FrameKind::GameStereo && kind!=FrameKind::Diagnostic))
            return false;
        auto* h=reinterpret_cast<Header*>(base_);
        // A writer must not overlap itself. No consumer ever writes.
        const LONG before=InterlockedCompareExchange(&h->sequence,0,0);
        if(before & 1)return false;
        if(frame_id<=last_written_)return false;
        InterlockedExchange(&h->sequence,before+1);
        std::memcpy(base_+sizeof(Header),left,kBytesPerEye);
        std::memcpy(base_+sizeof(Header)+kBytesPerEye,right,kBytesPerEye);
        h->frame_id=frame_id;
        LARGE_INTEGER time{};
        QueryPerformanceCounter(&time);
        h->timestamp_qpc=static_cast<std::uint64_t>(time.QuadPart);
        InterlockedExchange(&h->kind,static_cast<LONG>(kind));
        InterlockedExchange(&h->sequence,before+2);
        last_written_=frame_id;
        return true;
    }
    bool Read(Snapshot& out)const{
        if(!Valid())return false;
        const auto* h=reinterpret_cast<const Header*>(base_);
        const LONG start=InterlockedCompareExchange(
            const_cast<volatile LONG*>(&h->sequence),0,0);
        if(start & 1)return false;
        const FrameKind kind=static_cast<FrameKind>(h->kind);
        if(kind!=FrameKind::GameStereo && kind!=FrameKind::Diagnostic)return false;
        Snapshot tmp{};
        tmp.frame_id=h->frame_id;tmp.timestamp_qpc=h->timestamp_qpc;tmp.kind=kind;
        for(auto& eye:tmp.eyes)eye.resize(kBytesPerEye);
        std::memcpy(tmp.eyes[0].data(),base_+sizeof(Header),kBytesPerEye);
        std::memcpy(tmp.eyes[1].data(),base_+sizeof(Header)+kBytesPerEye,kBytesPerEye);
        const LONG end=InterlockedCompareExchange(
            const_cast<volatile LONG*>(&h->sequence),0,0);
        if(start!=end || (end & 1) || tmp.frame_id==0)return false;
        out=std::move(tmp);
        return true;
    }
    void Close(){
        if(base_)UnmapViewOfFile(base_);
        if(mapping_)CloseHandle(mapping_);
        base_=nullptr;mapping_=nullptr;writable_=false;last_written_=0;
    }
private:
    HANDLE mapping_=nullptr;
    std::uint8_t* base_=nullptr;
    bool writable_=false;
    std::uint64_t last_written_=0;
};
} // namespace m2vr::transport
