#pragma once
// Actual primary original x86 D3D9 camera/source-state COM entry-to-return
// lifetime. A post-COM lineage counter alone cannot identify a SetTransform
// or SetVSConstant* overlapping another thread's real hardware Draw/Present.
// This is strictly negative source evidence, never Daytona World3D proof.
// No shader constants, original matrices, pointers or game assets are stored.
#include <cstdint>
#include <mutex>

namespace m2vr::drawprobe {
struct OriginalCameraComSummary {
    std::uint32_t flags=0u, draws=0u, writes=0u;
    bool RejectStereoSource() const noexcept {return flags!=0u;}
};
class OriginalCameraComLifetime final {
public:
    static constexpr std::uint32_t kOverlap=1u;
    static constexpr std::uint32_t kCrossPresent=2u;
    static constexpr std::uint32_t kUntrustedWriter=4u;
    static constexpr std::uint32_t kFailedWriter=8u;
    static constexpr std::uint32_t kInFlight=16u;
    static constexpr std::uint32_t kOverflow=32u;
    static constexpr std::uint32_t kRevisionDrift=64u;
    static constexpr std::uint32_t kLimit=10000000u;
    struct Ticket {
        std::uint64_t epoch=0u,revision=0u;
        bool valid=false,eligible=false;
    };
    Ticket EnterWrite() noexcept {
        std::lock_guard<std::mutex> lock(mu_);
        if(active_writes_>=kLimit){flags_|=kOverflow;return {};}
        const bool eligible=!sealed_ && !active_draws_ && !active_writes_;
        if(!eligible) flags_|=sealed_?kCrossPresent:kOverlap;
        ++active_writes_;
        return {epoch_,revision_,true,eligible};
    }
    void CompleteWrite(Ticket ticket,bool exe_caller,bool succeeded) noexcept {
        std::lock_guard<std::mutex> lock(mu_);
        if(!ticket.valid){flags_|=kOverflow;return;}
        if(!active_writes_){flags_|=kCrossPresent;return;}
        --active_writes_;
        if(ticket.epoch!=epoch_ || sealed_){flags_|=kCrossPresent;return;}
        if(!ticket.eligible || active_draws_ || active_writes_)
            flags_|=kOverlap;
        if(ticket.revision!=revision_)flags_|=kRevisionDrift;
        if(!exe_caller)flags_|=kUntrustedWriter;
        if(!succeeded)flags_|=kFailedWriter;
        // A real shader/camera write can have ambiguous effects even when
        // its HRESULT failed. Advance revision to deny stale Draw tickets.
        if(revision_==UINT64_MAX){flags_|=kOverflow;return;}
        ++revision_;
        if(writes_==kLimit)flags_|=kOverflow;
        else ++writes_;
    }
    Ticket EnterDraw() noexcept {
        std::lock_guard<std::mutex> lock(mu_);
        if(active_draws_>=kLimit){flags_|=kOverflow;return {};}
        const bool eligible=!sealed_ && !active_writes_ && !flags_;
        if(!eligible)flags_|=sealed_?kCrossPresent:kOverlap;
        ++active_draws_;
        return {epoch_,revision_,true,eligible};
    }
    bool CompleteDraw(Ticket ticket) noexcept {
        std::lock_guard<std::mutex> lock(mu_);
        if(!ticket.valid){flags_|=kOverflow;return false;}
        if(!active_draws_){flags_|=kCrossPresent;return false;}
        --active_draws_;
        if(draws_==kLimit)flags_|=kOverflow;
        else ++draws_;
        if(ticket.epoch!=epoch_ || sealed_){
            flags_|=kCrossPresent;return false;
        }
        if(ticket.revision!=revision_)flags_|=kRevisionDrift;
        if(active_writes_)flags_|=kOverlap;
        return ticket.eligible && flags_==0u;
    }
    bool MayForwardCpuWorld() noexcept {
        std::lock_guard<std::mutex> lock(mu_);
        return !sealed_ && !active_writes_ && !flags_;
    }
    OriginalCameraComSummary FinishPresent() noexcept {
        std::lock_guard<std::mutex> lock(mu_);
        const auto safety=flags_ |
            (active_writes_ || active_draws_?kInFlight:0u);
        const OriginalCameraComSummary result{safety,draws_,writes_};
        flags_=draws_=writes_=0u;
        ++epoch_;
        sealed_=true;
        return result;
    }
    void ReopenAfterPhysicalPresentReturn() noexcept {
        std::lock_guard<std::mutex> lock(mu_);
        sealed_=false;
    }
    void Reset() noexcept {
        std::lock_guard<std::mutex> lock(mu_);
        // Keep outstanding counts: a late genuine COM return must be
        // consumed and fail in the new epoch, never certify new eyes.
        ++epoch_;
        ++revision_;
        flags_=draws_=writes_=0u;
        sealed_=false;
    }
private:
    std::mutex mu_;
    std::uint64_t epoch_=1u,revision_=1u;
    std::uint32_t active_writes_=0u,active_draws_=0u;
    std::uint32_t flags_=0u,draws_=0u,writes_=0u;
    bool sealed_=false;
};
} // namespace m2vr::drawprobe
