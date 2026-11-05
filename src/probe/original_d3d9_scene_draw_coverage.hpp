#pragma once
// Unsampled original game Scene/Draw coverage for each REAL primary physical
// D3D9 Present. One aggregate observation covers all four primitive families;
// no original game vertices, texture bytes, render state or camera matrices.
// Never independently authorizes World3D or HMD eye replay.
#include <atomic>
#include <cstdint>
#include <mutex>

namespace m2vr::drawprobe {
// The original Draw can recursively run emulator hooks or allow another
// thread to end/change Scene while it executes. A scene merely open at Draw
// ENTRY is not proof that it still belongs to that Draw at COM return.
constexpr bool OriginalDrawRetainedSourceScene(
    bool entered_owned,bool exited_owned,bool not_poisoned,
    std::uint32_t entry_ordinal,std::uint32_t exit_ordinal,
    bool same_original_thread) noexcept {
    return entered_owned && exited_owned && not_poisoned &&
        entry_ordinal!=0u && entry_ordinal==exit_ordinal &&
        same_original_thread;
}
struct OriginalPhysicalSceneDrawSummary {
    std::uint32_t begins=0u, ends=0u;
    std::uint32_t successful_in_scene=0u;
    std::uint32_t successful_untrusted=0u;
    std::uint32_t failed_draws=0u;
    std::uint32_t flags=0u;
    std::uint64_t TotalDraws()const noexcept {
        return std::uint64_t{successful_in_scene}+
            successful_untrusted+failed_draws;
    }
    bool CompleteSourceScene()const noexcept {
        return flags==0u && begins==ends &&
            successful_in_scene>0u && !successful_untrusted && !failed_draws;
    }
    // A real hardware source outside trusted Scene, or any failed original
    // Draw/Scene, poisons BOTH stereo routes for this same physical Present.
    // A draw-free menu frame is not automatically a stereo-world candidate.
    bool RejectUnprovenStereoSource()const noexcept {
        return flags!=0u || begins!=ends || successful_untrusted!=0u ||
            failed_draws!=0u ||
            (TotalDraws()!=0u && successful_in_scene==0u);
    }
};
class OriginalPhysicalSceneDrawCoverage final {
public:
    // The physical Present cuts a source epoch BEFORE its original COM call.
    // A Draw or Scene transition that overlaps that cut poisons both adjacent
    // frames, never allowing late COM return to certify the next frame.
    using OperationTicket=std::uint64_t;
    static constexpr std::uint32_t kLimit=10000000u;
    static constexpr std::uint32_t kFailedScene=1u;
    static constexpr std::uint32_t kExternalScene=2u;
    static constexpr std::uint32_t kUnknownDraw=4u;
    static constexpr std::uint32_t kOrphanEnd=8u;
    static constexpr std::uint32_t kUnbalanced=16u;
    static constexpr std::uint32_t kOverflow=32u;
    static constexpr std::uint32_t kHooksMissing=64u;
    static constexpr std::uint32_t kInFlightAtPresent=128u;
    static constexpr std::uint32_t kCrossPresent=256u;
    OperationTicket EnterOperation() noexcept {
        std::lock_guard<std::mutex> guard(lock_);
        if(active_>=kLimit){Flag(kOverflow);return 0u;}
        ++active_;
        // Covers the narrow interval between Scene/Draw cut and the physical
        // Present counter increment. This is an invalid original source.
        if(present_cut_)Flag(kCrossPresent);
        return epoch_;
    }
    // Legacy direct observations are retained for independent native tests.
    void Begin(bool original_exe_caller,bool successful_source_begin) noexcept {
        std::lock_guard<std::mutex> guard(lock_);
        BeginCore(original_exe_caller,successful_source_begin);
    }
    void End(bool original_exe_caller,bool succeeded,
             bool source_scene_was_open) noexcept {
        std::lock_guard<std::mutex> guard(lock_);
        EndCore(original_exe_caller,succeeded,source_scene_was_open);
    }
    void Draw(bool original_exe_caller,bool source_scene_owned_on_thread,
              bool original_com_succeeded) noexcept {
        std::lock_guard<std::mutex> guard(lock_);
        DrawCore(original_exe_caller,source_scene_owned_on_thread,
                 original_com_succeeded);
    }
    // Ticketed post-COM observations must complete in their original epoch.
    // Every path consumes exactly one matching entry, including failed COM.
    void Begin(OperationTicket ticket,bool caller,bool succeeded) noexcept {
        std::lock_guard<std::mutex> guard(lock_);
        if(Complete(ticket))BeginCore(caller,succeeded);
    }
    void End(OperationTicket ticket,bool caller,bool succeeded,
             bool source_was_open) noexcept {
        std::lock_guard<std::mutex> guard(lock_);
        if(Complete(ticket))EndCore(caller,succeeded,source_was_open);
    }
    void Draw(OperationTicket ticket,bool caller,bool owned,
              bool succeeded) noexcept {
        std::lock_guard<std::mutex> guard(lock_);
        if(Complete(ticket))DrawCore(caller,owned,succeeded);
    }
    OriginalPhysicalSceneDrawSummary FinishPresent(
        bool original_scene_open,bool original_scene_poisoned,
        bool source_scene_hooks_ready) noexcept {
        std::lock_guard<std::mutex> guard(lock_);
        OriginalPhysicalSceneDrawSummary r{
            begins_.exchange(0u,std::memory_order_relaxed),
            ends_.exchange(0u,std::memory_order_relaxed),
            inside_.exchange(0u,std::memory_order_relaxed),
            untrusted_.exchange(0u,std::memory_order_relaxed),
            failed_.exchange(0u,std::memory_order_relaxed),
            flags_.exchange(0u,std::memory_order_relaxed)};
        if(active_!=0u)r.flags|=kInFlightAtPresent;
        // All source entry/completion accounting is serialized against this
        // cut, but not the real D3D9 COM calls or OpenXR/GPU operations.
        ++epoch_;
        present_cut_=true;
        if(original_scene_poisoned)r.flags|=kFailedScene;
        if(original_scene_open || r.begins!=r.ends)r.flags|=kUnbalanced;
        if(!source_scene_hooks_ready && r.TotalDraws()!=0u)
            r.flags|=kHooksMissing;
        if(r.TotalDraws()>kLimit)r.flags|=kOverflow;
        return r;
    }
    void ReopenAfterPhysicalPresentAdvance() noexcept {
        std::lock_guard<std::mutex> guard(lock_);
        present_cut_=false;
    }
    void Reset()noexcept{
        std::lock_guard<std::mutex> guard(lock_);
        ++epoch_;
        present_cut_=false;
        begins_.store(0u,std::memory_order_relaxed);
        ends_.store(0u,std::memory_order_relaxed);
        inside_.store(0u,std::memory_order_relaxed);
        untrusted_.store(0u,std::memory_order_relaxed);
        failed_.store(0u,std::memory_order_relaxed);
        flags_.store(0u,std::memory_order_relaxed);
        // Keep active_ so a late completion cannot silently disappear.
        // A live original Draw/Scene at the next Present still rejects.
    }
private:
    bool Complete(OperationTicket ticket)noexcept {
        if(active_==0u){Flag(kCrossPresent);return false;}
        --active_;
        if(ticket==0u || ticket!=epoch_ || present_cut_){
            Flag(kCrossPresent);
            return false;
        }
        return true;
    }
    void BeginCore(bool caller,bool succeeded)noexcept{
        if(!caller){Flag(kExternalScene);return;}
        if(!succeeded){Flag(kFailedScene);return;}
        Increment(begins_);
    }
    void EndCore(bool caller,bool succeeded,bool was_open)noexcept{
        if(!caller){Flag(kExternalScene);return;}
        if(!succeeded){Flag(kFailedScene);return;}
        if(!was_open){Flag(kOrphanEnd);return;}
        Increment(ends_);
    }
    void DrawCore(bool caller,bool owned,bool succeeded)noexcept{
        if(!caller)Flag(kUnknownDraw);
        if(!succeeded)Increment(failed_);
        else if(caller && owned)Increment(inside_);
        else Increment(untrusted_);
    }
    void Flag(std::uint32_t value)noexcept{
        flags_.fetch_or(value,std::memory_order_relaxed);
    }
    void Increment(std::atomic<std::uint32_t>& counter)noexcept{
        if(counter.load(std::memory_order_relaxed)>=kLimit){
            Flag(kOverflow);return;
        }
        counter.fetch_add(1u,std::memory_order_relaxed);
    }
    std::mutex lock_;
    OperationTicket epoch_=1u;
    std::uint32_t active_=0u;
    bool present_cut_=false;
    std::atomic<std::uint32_t> begins_{0u},ends_{0u},inside_{0u},
        untrusted_{0u},failed_{0u},flags_{0u};
};
} // namespace m2vr::drawprobe
