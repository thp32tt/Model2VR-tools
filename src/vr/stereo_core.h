#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace m2vr::vr {

struct Vec3 { float x=0, y=0, z=0; };
struct Quaternion { float x=0, y=0, z=0, w=1; };
struct HeadPose {
    Vec3 position_m{};
    Quaternion rotation{};
    bool valid=false;
};
enum class Eye : std::uint8_t { Left=0, Right=1 };
enum class PassClass : std::uint8_t {
    Unknown=0, World3D=1, Hud2D=2, Sky=3, Mirror=4
};
struct ViewConfig {
    float ipd_m=0.063f;
    float units_per_meter=1.0f;
    bool allow_head_position=false;
    bool allow_head_rotation=false;
    bool sky_stereo=false;
    float min_depth=0.01f;
};
struct DrawClassification {
    std::uint32_t caller_rva=0;
    PassClass kind=PassClass::Unknown;
};
struct FrameSnapshot {
    std::uint64_t frame_id=0;
    HeadPose pose{};
    ViewConfig config{};
};
struct StereoPoints { Vec3 left{}; Vec3 right{}; bool stereo=false; };
struct Projection {
    float x_over_depth=0.0f;
    float y_over_depth=0.0f;
    bool valid=false;
};
class GameProfile {
public:
    explicit GameProfile(ViewConfig config={}): config_(config) {}
    bool set_view_config(ViewConfig config);
    bool set_caller(std::uint32_t caller_rva, PassClass kind);
    PassClass classify(std::uint32_t caller_rva) const;
    const ViewConfig& view_config() const { return config_; }
private:
    ViewConfig config_{};
    std::vector<DrawClassification> callers_{};
};
class StereoCoordinator {
public:
    // Called exactly once on the render thread per emulated frame, never from a per-eye replay.
    bool begin_frame(std::uint64_t frame_id, const HeadPose& pose, const GameProfile& profile);
    std::optional<FrameSnapshot> snapshot() const { return current_; }
    StereoPoints transform(Vec3 game_view_point, PassClass kind) const;
    // Used only after classification and after the game's single simulation/update.
    std::array<Projection,2> project_preview(Vec3 game_view_point, PassClass kind) const;
private:
    std::optional<FrameSnapshot> current_;
};
Vec3 rotate(const Quaternion& q, Vec3 p);
bool is_finite(const HeadPose& pose);
std::string_view pass_name(PassClass kind);

}  // namespace m2vr::vr
