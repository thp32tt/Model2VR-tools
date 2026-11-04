#pragma once
// One-use permission from a separately authenticated Daytona gameplay
// adapter to a REAL primary-device original D3D9 Draw hook. Merely arming
// this token is NOT World3D proof; BeginVerifiedWorldFrame must first
// authenticate the race pass, source camera scale/axes and current XR pose.
// Do not infer permission from a caller address, FVF, or scene ordinal.
#include <cstdint>
namespace m2vr::vrbridge {
struct ArmedOriginalDraw {
    std::uint64_t frame=0u,ordinal=0u;
    std::uint32_t family=0u,caller=0u,scene=0u;
    std::uint32_t cpu_projector=0u,alpha_cutoff=0u;
    constexpr bool Valid()const noexcept {
        return frame!=0u && ordinal>0u && ordinal<=10000000u &&
               family>=1u && family<=4u && caller!=0u &&
               scene!=0u && scene<=100000u && alpha_cutoff<=256u;
    }
};
class OriginalDrawOneShot final {
public:
    constexpr bool Arm(const ArmedOriginalDraw& request)noexcept {
        if(armed_ || !request.Valid())return false;
        expected_=request;armed_=true;return true;
    }
    constexpr bool Take(std::uint64_t frame,std::uint64_t ordinal,
                        std::uint32_t family,
                        std::uint32_t caller,std::uint32_t scene,
                        bool genuine_scene_open,ArmedOriginalDraw& result)noexcept {
        if(!armed_ || !genuine_scene_open || frame!=expected_.frame ||
           ordinal!=expected_.ordinal ||
           family!=expected_.family || caller!=expected_.caller ||
           scene!=expected_.scene)return false;
        result=expected_;
        Reset();
        return true;
    }
    constexpr bool Pending()const noexcept{return armed_;}
    constexpr void Reset()noexcept {expected_={};armed_=false;}
private:
    ArmedOriginalDraw expected_{};
    bool armed_=false;
};
// Private in-module bridge, used ONLY from a physical original Draw hook.
// A successful Take establishes a one-use source identity, not gameplay
// pass/world ownership, which is independently required at Begin.
bool TryTakeAuthenticatedOriginalDraw(std::uint64_t frame,
    std::uint64_t ordinal,std::uint32_t family,std::uint32_t caller,std::uint32_t scene,
    bool genuine_scene_open,ArmedOriginalDraw& result) noexcept;
} // namespace m2vr::vrbridge
