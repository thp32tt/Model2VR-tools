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
// Exact D3D9 original source pixel-write profile. D3DCOLORWRITEENABLE_*
// occupies mask bits 0..3; D3DCMP_LESS/LESSEQUAL are numeric 2/4.
// A partial color target, enabled stencil, user clip plane or alternate
// Z comparator is NOT safely reproducible as a direct OpenXR color/depth eye.
// This rejects unsafe source; it never proves Daytona's World3D camera.
inline constexpr bool OriginalPhysicalDirectEyeWriteProfile(
    std::uint32_t color_mask,std::uint32_t stencil_enabled,
    std::uint32_t z_func,std::uint32_t clip_plane_mask) noexcept {
    return color_mask==0x0fu && stencil_enabled==0u &&
        (z_func==2u || z_func==4u) && clip_plane_mask==0u;
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
// Three additional MRT surfaces are never inferred from the primary RT0.
// Read-only GetRenderTarget returns D3DERR_NOTFOUND for a valid empty slot.
// Any device caps/read failure is UNKNOWN, not an observed empty target.
struct OriginalSecondaryRtSnapshot {
    std::uint32_t mask=0u;
    bool known=false;
};
// This read-only, revision-tied physical D3D9 viewport/Z/scissor check is
// NEGATIVE evidence, not authority to identify Daytona gameplay geometry.
struct OriginalRasterSnapshot {
    bool known=false;
    bool full_main_depth_view=false;
};
// Four independent post-original-COM facts for ONE sampled original Draw.
// The tuple never identifies 3D geometry by itself; it only rules out stale
// CPU-projector matches borrowing a different physical GPU Draw source.
struct OriginalPhysicalSampleCompletion {
    bool succeeded=false;
    bool scene_retained=false;
    bool target_retained=false;
    bool lineage_retained=false;
    constexpr std::uint32_t Encode() const noexcept {
        return (succeeded?1u:0u) | (scene_retained?2u:0u) |
               (target_retained?4u:0u) | (lineage_retained?8u:0u);
    }
    constexpr bool CanRemainCandidate() const noexcept {
        return Encode()==15u;
    }
};
struct OriginalPhysicalTargetSummary {
    std::uint32_t backbuffer=0u, auxiliary=0u, unknown=0u, failed=0u;
    std::uint32_t incompatible_depth=0u, flags=0u;
    std::uint64_t TotalDraws() const noexcept {
        return std::uint64_t{backbuffer}+auxiliary+unknown+failed;
    }
    bool RejectUnprovenStereoSource() const noexcept {
        return flags!=0u || unknown!=0u || failed!=0u ||
            incompatible_depth!=0u || auxiliary!=0u;
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
    static constexpr std::uint32_t kMrtUnresolved=2048u;
    // An offscreen RT0 needs independently verified compositing before eye use.
    static constexpr std::uint32_t kAuxiliarySource=4096u;
    static constexpr std::uint32_t kUnverifiedRaster=8192u;
    explicit OriginalPhysicalTargetDrawCoverage(
        bool require_original_raster=false) noexcept
        : require_raster_(require_original_raster) {}
    struct Token {
        std::uint64_t epoch=0u, revision=0u;
        OriginalTargetSnapshot state{};
        bool owned_target=false, owned_depth=false, valid=false;
    };
    bool NeedsResolution() noexcept {
        std::lock_guard<std::mutex> guard(mutex_);
        return !resolved_;
    }
    bool NeedsSecondaryResolution() noexcept {
        std::lock_guard<std::mutex> guard(mutex_);
        return !secondary_resolved_;
    }
    bool NeedsRasterResolution() noexcept {
        std::lock_guard<std::mutex> guard(mutex_);
        return require_raster_ && !raster_resolved_;
    }
    void ResolveRasterIfUnchanged(
        std::uint64_t prior,OriginalRasterSnapshot actual) noexcept {
        std::lock_guard<std::mutex> guard(mutex_);
        if(!require_raster_ || prior!=revision_ || raster_resolved_ ||
           active_binds_ || active_ || sealed_)return;
        raster_resolved_=true; // even a failed getter is cached
        raster_eligible_=actual.known && actual.full_main_depth_view;
    }
    void ResolveSecondaryIfUnchanged(
        std::uint64_t prior,OriginalSecondaryRtSnapshot actual) noexcept {
        std::lock_guard<std::mutex> guard(mutex_);
        if(prior!=revision_ || secondary_resolved_ || active_binds_ || sealed_)
            return; // a concurrent original COM SET cannot be borrowed
        secondary_mask_=actual.mask&7u;
        secondary_known_=actual.known && (actual.mask&~7u)==0u;
        secondary_resolved_=true; // cache FAILED too until next real SET
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
        if(prior!=revision_ || resolved_ || active_binds_ || sealed_)return;
        state_=actual;
        resolved_=true;
    }
    // SetRenderTarget/SetDepthStencilSurface run through actual original COM.
    // A bind is an in-flight source WRITE, not a post-COM point observation.
    struct BindToken {
        std::uint64_t epoch=0u, revision=0u;
        bool valid=false, started_sealed=false;
    };
    BindToken EnterBind() noexcept {
        std::lock_guard<std::mutex> guard(mutex_);
        if(active_binds_>=kLimit){flags_|=kOverflow;return {};}
        ++active_binds_;
        if(sealed_)flags_|=kCrossPresent;
        return {epoch_,revision_,true,sealed_};
    }
    // Direct observations remain for deterministic policy unit tests.
    void RenderTargetBind(bool exe_caller,bool succeeded,
                          std::uint32_t slot,bool nonnull,
                          OriginalTargetSnapshot actual) noexcept {
        std::lock_guard<std::mutex> guard(mutex_);
        RenderTargetBindCore(exe_caller,succeeded,slot,nonnull,actual);
    }
    void DepthBind(bool exe_caller,bool succeeded,
                   OriginalTargetSnapshot actual) noexcept {
        std::lock_guard<std::mutex> guard(mutex_);
        DepthBindCore(exe_caller,succeeded,actual);
    }
    void CompleteRenderTargetBind(const BindToken& token,bool caller,
                                  bool succeeded,std::uint32_t slot,
                                  bool nonnull,OriginalTargetSnapshot actual) noexcept {
        std::lock_guard<std::mutex> guard(mutex_);
        if(CompleteBind(token))
            RenderTargetBindCore(caller,succeeded,slot,nonnull,actual);
    }
    void CompleteDepthBind(const BindToken& token,bool caller,
                           bool succeeded,OriginalTargetSnapshot actual) noexcept {
        std::lock_guard<std::mutex> guard(mutex_);
        if(CompleteBind(token))DepthBindCore(caller,succeeded,actual);
    }
    // Native SetViewport, SetScissorRect and Z/scissor SetRenderState use
    // real pre/post-COM tickets. A transient write is never a safe raster
    // source, even when its old read-only getter had been canonical.
    void CompleteRasterMutation(const BindToken& token,
                                bool exe_caller,bool succeeded) noexcept {
        std::lock_guard<std::mutex> guard(mutex_);
        if(!CompleteBind(token))return;
        if(!exe_caller)flags_|=kForeignBind;
        if(!succeeded)flags_|=kFailedBind;
        InvalidateRaster();
        ++revision_;
    }
    void InvalidateOpaqueStateBlock() noexcept {
        std::lock_guard<std::mutex> guard(mutex_);
        flags_|=kOpaqueStateBlock;
        state_={};owned_target_=owned_depth_=false;
        resolved_=false;secondary_mask_=0u;
        secondary_resolved_=secondary_known_=false;
        InvalidateRaster();++revision_;
    }
    // Avoid building speculative CPU-world eye pixels if this original
    // physical Draw lacks an EXE-owned, compatible RT0/depth state. The
    // original hardware Draw ALWAYS runs and the mono path is unaffected.
    bool CanAttemptVerifiedWorldDraw() noexcept {
        std::lock_guard<std::mutex> guard(mutex_);
        return !sealed_ && active_binds_==0u && resolved_ && owned_target_ && owned_depth_ &&
            secondary_resolved_ && secondary_known_ &&
            secondary_mask_==0u && flags_==0u &&
            (!require_raster_ || (raster_resolved_ && raster_eligible_)) &&
            state_.target==OriginalPhysicalRtClass::Backbuffer &&
            state_.depth==OriginalPhysicalDepthClass::Compatible;
    }
    Token EnterDraw() noexcept {
        std::lock_guard<std::mutex> guard(mutex_);
        if(state_.target==OriginalPhysicalRtClass::Auxiliary)
            flags_|=kAuxiliarySource;
        if(require_raster_ && (!raster_resolved_ || !raster_eligible_))
            flags_|=kUnverifiedRaster;
        if(secondary_mask_)flags_|=kMrtActive;
        if(!secondary_resolved_ || !secondary_known_)
            flags_|=kMrtUnresolved;
        if(active_binds_)flags_|=kTargetDrift;
        Token result{epoch_,revision_,state_,owned_target_,owned_depth_,true};
        if(active_>=kLimit){
            flags_|=kOverflow;result.valid=false;return result;
        }
        ++active_;
        if(sealed_)flags_|=kCrossPresent;
        return result;
    }
    // Per-genuine-COM-return sampled CPU/GPU correspondence: a successful
    // original HRESULT alone does not retain source RT/Scene evidence. The
    // boolean is strictly negative evidence and cannot enable World3D.
    // Called on EVERY physical Draw, including unsampled original Draws.
    bool CompleteDraw(const Token& token,bool exe_caller,
                      bool original_succeeded) noexcept {
        std::lock_guard<std::mutex> guard(mutex_);
        if(!token.valid){flags_|=kOverflow;return false;}
        if(!active_){flags_|=kCrossPresent;return false;}
        --active_;
        if(token.epoch!=epoch_ || sealed_) {
            flags_|=kCrossPresent;return false;
        }
        if(token.revision!=revision_){
            flags_|=kTargetDrift;
            Increment(unknown_);
            return false;
        }
        if(!original_succeeded){Increment(failed_);return false;}
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
        return flags_==0u && active_binds_==0u &&
            token.owned_target && token.owned_depth && exe_caller &&
            token.state.target==OriginalPhysicalRtClass::Backbuffer &&
            token.state.depth==OriginalPhysicalDepthClass::Compatible &&
            secondary_resolved_ && secondary_known_ && !secondary_mask_ &&
            (!require_raster_ || (raster_resolved_ && raster_eligible_));
    }
    OriginalPhysicalTargetSummary FinishPresent() noexcept {
        std::lock_guard<std::mutex> guard(mutex_);
        OriginalPhysicalTargetSummary result{backbuffer_,auxiliary_,
            unknown_,failed_,bad_depth_,flags_};
        if(active_!=0u || active_binds_!=0u)result.flags|=kInFlight;
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
        secondary_resolved_=secondary_known_=false;
        InvalidateRaster();
        backbuffer_=auxiliary_=unknown_=failed_=bad_depth_=flags_=0u;
        sealed_=false;
        // Keep active_ AND active_binds_: late original COM operations
        // must still be consumed without granting stale next-frame ownership.
        // Keep active_: late original COM operations must still be consumed
        // as crossed-epoch failures, never credited to the next game frame.
    }
private:
    void InvalidateRaster() noexcept {
        raster_resolved_=raster_eligible_=false;
    }
    void InvalidateOwnedBinding() noexcept {
        state_={};owned_target_=owned_depth_=false;
        resolved_=false;secondary_mask_=0u;
        secondary_resolved_=secondary_known_=false;
        InvalidateRaster();++revision_;
    }
    bool CompleteBind(const BindToken& token) noexcept {
        if(!token.valid){flags_|=kOverflow;InvalidateOwnedBinding();return false;}
        if(!active_binds_){flags_|=kCrossPresent;InvalidateOwnedBinding();return false;}
        --active_binds_;
        // A SET that straddles the physical Present cut, starts in its
        // sealed COM window, or races another binding write cannot lend
        // post-COM target/depth ownership to speculative OpenXR eyes.
        if(token.epoch!=epoch_ || token.started_sealed || sealed_){
            flags_|=kCrossPresent;InvalidateOwnedBinding();return false;
        }
        if(token.revision!=revision_){
            flags_|=kTargetDrift;InvalidateOwnedBinding();return false;
        }
        return true;
    }
    void RenderTargetBindCore(bool exe_caller,bool succeeded,
                          std::uint32_t slot,bool nonnull,
                          OriginalTargetSnapshot actual) noexcept {
        if(!exe_caller) {
            flags_|=kForeignBind;
            state_={};owned_target_=false;owned_depth_=false;
            resolved_=false;secondary_mask_=0u;
            secondary_resolved_=secondary_known_=false;
            InvalidateRaster();++revision_;
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
                // If the other physical MRT slots have never been
                // witnessed, changing this single slot proves nothing.
                if(!secondary_known_)secondary_resolved_=false;
            }else flags_|=kMrtActive;
            InvalidateRaster();++revision_;
            return;
        }
        state_=actual;resolved_=true;
        owned_target_=actual.target!=OriginalPhysicalRtClass::Unknown;
        // Changing RT0 does not prove original depth-surface ownership.
        InvalidateRaster();++revision_;
    }
    void DepthBindCore(bool exe_caller,bool succeeded,
                   OriginalTargetSnapshot actual) noexcept {
        if(!exe_caller) {
            flags_|=kForeignBind;state_.depth=OriginalPhysicalDepthClass::Unknown;
            owned_depth_=false;resolved_=false;
            InvalidateRaster();++revision_;return;
        }
        if(!succeeded){flags_|=kFailedBind;return;}
        state_=actual;resolved_=true;
        owned_depth_=actual.depth!=OriginalPhysicalDepthClass::Unknown;
        InvalidateRaster();++revision_;
    }
    void Increment(std::uint32_t& value) noexcept {
        if(value==kLimit)flags_|=kOverflow;
        else ++value;
    }
    std::mutex mutex_;
    OriginalTargetSnapshot state_{};
    std::uint64_t epoch_=1u,revision_=1u;
    std::uint32_t secondary_mask_=0u,active_=0u,active_binds_=0u,backbuffer_=0u,auxiliary_=0u,
        unknown_=0u,failed_=0u,bad_depth_=0u,flags_=0u;
    bool owned_target_=false, owned_depth_=false, sealed_=false, resolved_=false;
    bool secondary_resolved_=false, secondary_known_=false;
    bool require_raster_=false, raster_resolved_=false, raster_eligible_=false;
};
} // namespace m2vr::drawprobe
