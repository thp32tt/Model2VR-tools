#pragma once
// Unsampled original game Scene/Draw coverage for each REAL primary physical
// D3D9 Present. One aggregate observation covers all four primitive families;
// no original game vertices, texture bytes, render state or camera matrices.
// Never independently authorizes World3D or HMD eye replay.
#include <atomic>
#include <cstdint>

namespace m2vr::drawprobe {
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
};
class OriginalPhysicalSceneDrawCoverage final {
public:
    static constexpr std::uint32_t kLimit=10000000u;
    static constexpr std::uint32_t kFailedScene=1u;
    static constexpr std::uint32_t kExternalScene=2u;
    static constexpr std::uint32_t kUnknownDraw=4u;
    static constexpr std::uint32_t kOrphanEnd=8u;
    static constexpr std::uint32_t kUnbalanced=16u;
    static constexpr std::uint32_t kOverflow=32u;
    static constexpr std::uint32_t kHooksMissing=64u;
    void Begin(bool original_exe_caller,bool successful_source_begin) noexcept {
        if(!original_exe_caller) {Flag(kExternalScene);return;}
        if(!successful_source_begin) {Flag(kFailedScene);return;}
        Increment(begins_);
    }
    void End(bool original_exe_caller,bool succeeded,
             bool source_scene_was_open) noexcept {
        if(!original_exe_caller){Flag(kExternalScene);return;}
        if(!succeeded){Flag(kFailedScene);return;}
        if(!source_scene_was_open){Flag(kOrphanEnd);return;}
        Increment(ends_);
    }
    void Draw(bool original_exe_caller,bool source_scene_owned_on_thread,
              bool original_com_succeeded) noexcept {
        if(!original_exe_caller)Flag(kUnknownDraw);
        if(!original_com_succeeded)Increment(failed_);
        else if(original_exe_caller && source_scene_owned_on_thread)
            Increment(inside_);
        else Increment(untrusted_);
    }
    OriginalPhysicalSceneDrawSummary FinishPresent(
        bool original_scene_open,bool original_scene_poisoned,
        bool source_scene_hooks_ready) noexcept {
        OriginalPhysicalSceneDrawSummary r{
            begins_.exchange(0u,std::memory_order_acq_rel),
            ends_.exchange(0u,std::memory_order_acq_rel),
            inside_.exchange(0u,std::memory_order_acq_rel),
            untrusted_.exchange(0u,std::memory_order_acq_rel),
            failed_.exchange(0u,std::memory_order_acq_rel),
            flags_.exchange(0u,std::memory_order_acq_rel)};
        if(original_scene_poisoned)r.flags|=kFailedScene;
        if(original_scene_open || r.begins!=r.ends)r.flags|=kUnbalanced;
        if(!source_scene_hooks_ready && r.TotalDraws()!=0u)
            r.flags|=kHooksMissing;
        if(r.TotalDraws()>kLimit)r.flags|=kOverflow;
        return r;
    }
    void Reset()noexcept{
        begins_.store(0u,std::memory_order_release);
        ends_.store(0u,std::memory_order_release);
        inside_.store(0u,std::memory_order_release);
        untrusted_.store(0u,std::memory_order_release);
        failed_.store(0u,std::memory_order_release);
        flags_.store(0u,std::memory_order_release);
    }
private:
    void Flag(std::uint32_t value)noexcept{
        flags_.fetch_or(value,std::memory_order_acq_rel);
    }
    void Increment(std::atomic<std::uint32_t>& counter)noexcept {
        auto old=counter.load(std::memory_order_relaxed);
        while(old<kLimit) {
            if(counter.compare_exchange_weak(old,old+1u,
                std::memory_order_acq_rel,std::memory_order_relaxed))return;
        }
        Flag(kOverflow);
    }
    std::atomic<std::uint32_t> begins_{0u},ends_{0u},inside_{0u},
        untrusted_{0u},failed_{0u},flags_{0u};
};
} // namespace m2vr::drawprobe
