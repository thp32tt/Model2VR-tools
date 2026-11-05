#pragma once
// Genuine primary x86 D3D9 RT0/depth state at ALL four original Draw COM
// families, not merely a sampled SetRenderTarget log. This is negative
// evidence for the real physical Present, NEVER positive gameplay World3D
// authorization. No pointers, texture contents or matrices leave the hook.
#include <cstdint>
#include <mutex>

namespace m2vr::drawprobe {

// A depth Surface with the same width/height but different MSAA type or
// quality does NOT reproduce the authentic game's D3D9 raster source.
inline constexpr bool OriginalPhysicalDepthCompatible(
    std::uint32_t rt_width,std::uint32_t rt_height,
    std::uint32_t rt_msaa,std::uint32_t rt_quality,
    std::uint32_t z_width,std::uint32_t z_height,
    std::uint32_t z_msaa,std::uint32_t z_quality) noexcept {
    return rt_width!=0u && rt_height!=0u &&
        rt_width==z_width && rt_height==z_height &&
        rt_msaa==z_msaa && rt_quality==z_quality;
}
enum class OriginalPhysicalRtClass : std::uint32_t {
    Unknown=0u, Backbuffer=1u, Auxiliary=2u
};
enum class OriginalPhysicalDepthClass : std::uint32_t {
    Unknown=0u, Compatible=1u, Incompatible=2u
};
struct OriginalTargetSnapshot {
    OriginalPhysicalRtClass target=OriginalPhysicalRtClass::Unknown;
    OriginalPhysicalDepthClass depth=OriginalPhysicalDepthClass::Unknown;
};
struct OriginalPhysicalTargetSummary {
    std::uint32_t backbuffer=0u, auxiliary=0u, unknown=0u, failed=0u;
    std::uint32_t incompatible_depth=0u, flags=0u;
    std::uint64_t TotalDraws() const noexcept {
        return std::uint64_t{backbuffer}+auxiliary+unknown+failed;
    }
    bool RejectUnprovenStereoSource() const noexcept {
        return flags!=0u || unknown!=0u || failed!=0u ||
            incompatible_depth!=0u;
    }
};
class OriginalPhysicalTargetDrawCoverage final {
public:
    static constexpr std::uint32_t kLimit=10000000u;
    static constexpr std::uint32_t kForeignBind=1u;
    static constexpr std::uint32_t kFailedBind=2u;
    static constexpr std::uint32_t kUnownedTarget=4u;
    static constexpr std::uint32_t kInvalidDepth=8u;
    static constexpr std::uint32_t kTargetDrift=16u;
    static constexpr std::uint32_t kInFlight=32u;
    static constexpr std::uint32_t kCrossPresent=64u;
    static constexpr std::uint32_t kOverflow=128u;
    static constexpr std::uint32_t kCountMismatch=256u;
    static constexpr std::uint32_t kOpaqueStateBlock=512u;
    static constexpr std::uint32_t kMrtActive=1024u;
    struct Token {
        std::uint64_t epoch=0u, revision=0u;
        OriginalTargetSnapshot state{};
        bool owned_target=false, owned_depth=false, valid=false;
    };
    bool NeedsResolution() noexcept {
        std::lock_guard<std::mutex> guard(mutex_);
        return !resolved_;
    }
    // A read-only initial D3D9 COM snapshot is not EXE source ownership.
    // Do not overwrite an authentic SetRT/SetDepth completed concurrently.
    std::uint64_t Revision() noexcept {
        std::lock_guard<std::mutex> guard(mutex_);
        return revision_;
    }
    void ResolveIfUnchanged(std::uint64_t prior,
                            OriginalTargetSnapshot actual) noexcept {
        std::lock_guard<std::mutex> guard(mutex_);
        if(prior!=revision_ || resolved_)return;
        state_=actual;
        resolved_=true;
    }
    void RenderTargetBind(bool exe_caller,bool succeeded,
                          std::uint32_t slot,bool nonnull,
                          OriginalTargetSnapshot actual) noexcept {
        std::lock_guard<std::mutex> guard(mutex_);
        if(!exe_caller) {
            flags_|=kForeignBind;
            state_={};owned_target_=false;owned_depth_=false;
            resolved_=false;++revision_;
            return;
        }
        if(!succeeded){flags_|=kFailedBind;return;}
        if(slot!=0u){
            // Real SetRenderTarget(1..3) is persistent across Presents.
            // Draws with simultaneous outputs are not verified by this
            // single-RT0 CPU/GPU stereo source adapter.
            if(slot<=3u){
                const auto bit=1u<<(slot-1u);
                if(nonnull)secondary_mask_|=bit;
                else secondary_mask_&=~bit;
            }else flags_|=kMrtActive;
            ++revision_;
            return;
        }
        state_=actual;resolved_=true;
        owned_target_=actual.target!=OriginalPhysicalRtClass::Unknown;
        // Changing RT0 does not prove original depth-surface ownership.
        ++revision_;
    }
    void DepthBind(bool exe_caller,bool succeeded,
                   OriginalTargetSnapshot actual) noexcept {
        std::lock_guard<std::mutex> guard(mutex_);
        if(!exe_caller) {
            flags_|=kForeignBind;state_.depth=OriginalPhysicalDepthClass::Unknown;
            owned_depth_=false;resolved_=false;++revision_;return;
        }
        if(!succeeded){flags_|=kFailedBind;return;}
        state_=actual;resolved_=true;
        owned_depth_=actual.depth!=OriginalPhysicalDepthClass::Unknown;
        ++revision_;
    }
    void InvalidateOpaqueStateBlock() noexcept {
        std::lock_guard<std::mutex> guard(mutex_);
        flags_|=kOpaqueStateBlock;
        state_={};owned_target_=owned_depth_=false;
        resolved_=false;secondary_mask_=0u;++revision_;
    }
    Token EnterDraw() noexcept {
        std::lock_guard<std::mutex> guard(mutex_);
        if(secondary_mask_)flags_|=kMrtActive;
        Token result{epoch_,revision_,state_,owned_target_,owned_depth_,true};
        if(active_>=kLimit){
            flags_|=kOverflow;result.valid=false;return result;
        }
        ++active_;
        if(sealed_)flags_|=kCrossPresent;
        return result;
    }
    void CompleteDraw(const Token& token,bool exe_caller,
                      bool original_succeeded) noexcept {
        std::lock_guard<std::mutex> guard(mutex_);
        if(!token.valid){flags_|=kOverflow;return;}
        if(!active_){flags_|=kCrossPresent;return;}
        --active_;
        if(token.epoch!=epoch_ || sealed_) {
            flags_|=kCrossPresent;return;
        }
        if(token.revision!=revision_){
            flags_|=kTargetDrift;
            Increment(unknown_);
            return;
        }
        if(!original_succeeded){Increment(failed_);return;}
        if(!exe_caller || !token.owned_target || !token.owned_depth)
            flags_|=kUnownedTarget;
        if(token.state.depth!=OriginalPhysicalDepthClass::Compatible){
            flags_|=kInvalidDepth;Increment(bad_depth_);
        }
        switch(token.state.target){
        case OriginalPhysicalRtClass::Backbuffer: Increment(backbuffer_);break;
        case OriginalPhysicalRtClass::Auxiliary: Increment(auxiliary_);break;
        default: Increment(unknown_);break;
        }
    }
    OriginalPhysicalTargetSummary FinishPresent() noexcept {
        std::lock_guard<std::mutex> guard(mutex_);
        OriginalPhysicalTargetSummary result{backbuffer_,auxiliary_,
            unknown_,failed_,bad_depth_,flags_};
        if(active_!=0u)result.flags|=kInFlight;
        backbuffer_=auxiliary_=unknown_=failed_=bad_depth_=flags_=0u;
        ++epoch_;
        sealed_=true;
        return result;
    }
    // Never open source evidence again until actual original Present COM
    // returns; entering while sealed taints the following physical frame.
    void ReopenAfterPhysicalPresentReturn() noexcept {
        std::lock_guard<std::mutex> guard(mutex_);
        sealed_=false;
    }
    void Reset() noexcept {
        std::lock_guard<std::mutex> guard(mutex_);
        ++epoch_;
        ++revision_;
        state_={};owned_target_=owned_depth_=false;resolved_=false;
        secondary_mask_=0u;
        backbuffer_=auxiliary_=unknown_=failed_=bad_depth_=flags_=0u;
        sealed_=false;
        // Keep active_: late original COM operations must still be consumed
        // as crossed-epoch failures, never credited to the next game frame.
    }
private:
    void Increment(std::uint32_t& value) noexcept {
        if(value==kLimit)flags_|=kOverflow;
        else ++value;
    }
    std::mutex mutex_;
    OriginalTargetSnapshot state_{};
    std::uint64_t epoch_=1u,revision_=1u;
    std::uint32_t secondary_mask_=0u,active_=0u,backbuffer_=0u,auxiliary_=0u,
        unknown_=0u,failed_=0u,bad_depth_=0u,flags_=0u;
    bool owned_target_=false, owned_depth_=false, sealed_=false, resolved_=false;
};
} // namespace m2vr::drawprobe
