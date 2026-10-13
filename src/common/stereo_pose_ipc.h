#pragma once
// ABI shared between 64-bit OpenXR renderer and 32-bit Model2 game renderer.
// The host publishes predicted display-time stereo camera views. A consumer
// must latch one coherent packet per emulated render frame; never read it per
// vertex and never rerun game simulation for another eye.
#include <Windows.h>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
namespace m2vr::pose {
constexpr std::uint32_t kMagic=0x3250564Du; // "MVP2"
constexpr std::uint32_t kVersion=1;
constexpr std::uint32_t kOrientationValid=0x1u;
constexpr std::uint32_t kPositionValid=0x2u;
struct EyeView {
    float position_m[3]{};
    float orientation_xyzw[4]{0,0,0,1};
    float fov_left_right_up_down[4]{};
};
static_assert(sizeof(EyeView)==44);
struct Frame {
    std::uint64_t sequence=0;
    std::int64_t predicted_display_time=0;
    EyeView eyes[2]{};
    std::uint32_t view_flags=0;
    std::uint32_t reserved=0;
};
static_assert(sizeof(Frame)==112);
struct alignas(8) Shared {
    std::uint32_t magic=0,version=0,size=0,reserved=0;
    volatile LONG generation=0;
    std::uint32_t writer_pid=0;
    Frame frame{};
};
static_assert(offsetof(Shared,frame)==24);
static_assert(sizeof(Shared)==136);
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
        if(!writable_||!Valid()||value.sequence<=last_written_||
           !(value.view_flags&kOrientationValid))return false;
        const LONG previous=InterlockedCompareExchange(&data_->generation,0,0);
        if(previous & 1)return false;
        InterlockedExchange(&data_->generation,previous+1);
        std::memcpy(&data_->frame,&value,sizeof(Frame));
        InterlockedExchange(&data_->generation,previous+2);
        last_written_=value.sequence;
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
        out=copy;
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
