#pragma once
// A Win64 OpenXR reader must bind the lifetime of the ORIGINAL x86
// D3D9 process, not merely a reusable numeric Windows PID. Keeping the
// process HANDLE alive makes PID reuse incapable of reauthorizing a
// different game process against stale mapped stereo/pose/receipt data.
#include <Windows.h>
#include <cstdint>

namespace m2vr::host {
class OriginalSourceProcessLease final {
public:
    OriginalSourceProcessLease()=default;
    OriginalSourceProcessLease(const OriginalSourceProcessLease&)=delete;
    OriginalSourceProcessLease& operator=(const OriginalSourceProcessLease&)=delete;
    ~OriginalSourceProcessLease(){if(handle_)CloseHandle(handle_);}
    bool Attach(std::uint32_t pid) noexcept {
        if(!pid || terminated_)return false;
        if(handle_)return pid_==pid && Alive();
        HANDLE h=OpenProcess(
            SYNCHRONIZE|PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);
        if(!h)return false;
        if(WaitForSingleObject(h,0u)!=WAIT_TIMEOUT){
            CloseHandle(h);terminated_=true;return false;
        }
        FILETIME created{},exited{},kernel{},user{};
        if(!GetProcessTimes(h,&created,&exited,&kernel,&user)){
            CloseHandle(h);return false;
        }
        handle_=h;pid_=pid;created_=created;
        return true;
    }
    bool Alive() noexcept {
        if(!handle_ || terminated_)return false;
        if(WaitForSingleObject(handle_,0u)!=WAIT_TIMEOUT){
            // Permanent quarantine for this host session. Even if the
            // OS later reuses pid_, never bind a new D3D9 producer.
            terminated_=true;
            return false;
        }
        return true;
    }
    std::uint32_t Pid()const noexcept{return pid_;}
    bool Terminal()const noexcept{return terminated_;}
    std::uint64_t CreationTime100ns()const noexcept{
        ULARGE_INTEGER v{};
        v.LowPart=created_.dwLowDateTime;
        v.HighPart=created_.dwHighDateTime;
        return v.QuadPart;
    }
private:
    HANDLE handle_=nullptr;
    std::uint32_t pid_=0u;
    FILETIME created_{};
    bool terminated_=false;
};
} // namespace m2vr::host
