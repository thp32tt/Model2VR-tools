#pragma once
// CPU-only Model 2 world triangle clipping into independent OpenXR eye streams.
// Requires a verified world draw and a single latched pose per game frame.
// No D3D9 replay, game simulation, input, timer or FFB callbacks are invoked.
#include "model2_frame_latch.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

namespace m2vr::vr {
struct ClippedEyeTriangle {
    std::array<EyeProjection, 3> vertices{};
};
struct ClippedStereoBatch {
    std::array<std::vector<ClippedEyeTriangle>, 2> eyes{};
    std::uint64_t game_frame = 0;
    std::uint64_t pose_sequence = 0;
};

namespace clip_detail {
// Perspective clipping must happen BEFORE division by depth. NDC-space
// interpolation would distort triangles crossing the near/side planes.
struct Vertex {
    double x = 0, y = 0, w = 0;
};
inline bool Finite3(const Vertex& v) noexcept {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.w);
}
inline bool ToEyeClip(CpuPoint point, const pose::EyeView& eye,
                      const Model2CameraCalibration& cal, Vertex& out) noexcept {
    if (!Finite(point.x) || !Finite(point.y) || !Finite(point.z))
        return false;
    const auto& m = cal.view_to_xr;
    const double scale = cal.units_per_meter;
    const double gx = point.x, gy = point.y, gz = point.z;
    const double x = (m[0]*gx + m[1]*gy + m[2]*gz)/scale - eye.position_m[0];
    const double y = (m[3]*gx + m[4]*gy + m[5]*gz)/scale - eye.position_m[1];
    const double z = (m[6]*gx + m[7]*gy + m[8]*gz)/scale - eye.position_m[2];
    // OpenXR quaternion is XYZW; conjugate rotates world relative to eye.
    const double qx = -eye.orientation_xyzw[0];
    const double qy = -eye.orientation_xyzw[1];
    const double qz = -eye.orientation_xyzw[2];
    const double qw = eye.orientation_xyzw[3];
    const double tx = 2*(qy*z-qz*y);
    const double ty = 2*(qz*x-qx*z);
    const double tz = 2*(qx*y-qy*x);
    const double px = x + qw*tx + (qy*tz-qz*ty);
    const double py = y + qw*ty + (qz*tx-qx*tz);
    const double pz = z + qw*tz + (qx*ty-qy*tx);
    const auto& f = eye.fov_left_right_up_down;
    const double left = std::tan(static_cast<double>(f[0]));
    const double right = std::tan(static_cast<double>(f[1]));
    const double up = std::tan(static_cast<double>(f[2]));
    const double down = std::tan(static_cast<double>(f[3]));
    if (!std::isfinite(left) || !std::isfinite(right) ||
        !std::isfinite(up) || !std::isfinite(down) ||
        right-left<=0.01 || up-down<=0.01)
        return false;
    const double depth = -pz;
    out = {(2*px-depth*(right+left))/(right-left),
           (2*py-depth*(up+down))/(up-down), depth};
    return Finite3(out);
}
inline double Distance(const Vertex& v, int plane, double near_depth) noexcept {
    switch (plane) {
        case 0: return v.w-near_depth;
        case 1: return v.w+v.x;
        case 2: return v.w-v.x;
        case 3: return v.w+v.y;
        default:return v.w-v.y;
    }
}
inline bool Intersect(const Vertex& a, const Vertex& b,
                      double da, double db, Vertex& out) noexcept {
    const double denominator = da-db;
    if (!std::isfinite(denominator) || denominator==0) return false;
    const double t = da/denominator;
    if (!std::isfinite(t) || t<0 || t>1) return false;
    out = {a.x+t*(b.x-a.x), a.y+t*(b.y-a.y), a.w+t*(b.w-a.w)};
    return Finite3(out);
}
inline bool Clip(const std::array<Vertex, 3>& input, double near_depth,
                 std::array<Vertex, 12>& polygon, std::size_t& count) noexcept {
    polygon = {};
    for (std::size_t i=0;i<3;++i) polygon[i]=input[i];
    count=3;
    std::array<Vertex, 12> next{};
    for (int plane=0;plane<5;++plane) {
        if (count==0) return true; // fully outside, not a corrupted batch
        std::size_t next_count=0;
        for (std::size_t i=0;i<count;++i) {
            const Vertex& a=polygon[i];
            const Vertex& b=polygon[(i+1)%count];
            const double da=Distance(a,plane,near_depth);
            const double db=Distance(b,plane,near_depth);
            if (!std::isfinite(da) || !std::isfinite(db)) return false;
            const bool in_a=da>=0, in_b=db>=0;
            if (in_a!=in_b) {
                Vertex intersection{};
                if (!Intersect(a,b,da,db,intersection) || next_count>=next.size())
                    return false;
                next[next_count++]=intersection;
            }
            if (in_b) {
                if (next_count>=next.size()) return false;
                next[next_count++]=b;
            }
        }
        polygon=next;
        count=next_count;
    }
    return true;
}
inline bool Normalize(const Vertex& v, double near_depth,
                      EyeProjection& out) noexcept {
    if (!Finite3(v) || v.w<near_depth*(1.-1.e-9)) return false;
    const double x=v.x/v.w, y=v.y/v.w;
    if (!std::isfinite(x) || !std::isfinite(y) ||
        x < -1.000001 || x > 1.000001 ||
        y < -1.000001 || y > 1.000001 ||
        v.w > std::numeric_limits<float>::max()) return false;
    out={static_cast<float>(std::clamp(x,-1.,1.)),
         static_cast<float>(std::clamp(y,-1.,1.)),
         static_cast<float>(v.w),true};
    return true;
}
} // namespace clip_detail

// Unlike the legacy all-or-nothing paired-triangle preview, each eye may
// have a different number of clipped triangles. A failure preserves output
// atomically; a wholly offscreen world draw succeeds with empty eye streams.
inline bool BuildClippedStereoBatch(
    const CpuPoint* vertices, std::size_t vertex_count,
    const std::uint32_t* indices, std::size_t index_count,
    std::uint64_t game_frame, const Model2FramePoseLatch& latch,
    const Model2CameraCalibration& cal, const DrawEvidence& evidence,
    ClippedStereoBatch& output) {
    if (!vertices || !indices || !vertex_count || !index_count ||
        index_count%3 || index_count/3>8192 ||
        ChooseEyeAction(evidence)!=EyeAction::StereoGeometry ||
        !ValidCalibration(cal)) return false;
    pose::Frame frame{};
    if (!latch.Read(game_frame,frame)) return false;
    ClippedStereoBatch next{};
    next.game_frame=game_frame;
    next.pose_sequence=frame.sequence;
    for (int eye_index=0;eye_index<2;++eye_index) {
        const auto& eye=frame.eyes[eye_index];
        auto& triangles=next.eyes[eye_index];
        for (std::size_t i=0;i<index_count;i+=3) {
            std::array<clip_detail::Vertex,3> original{};
            for (std::size_t corner=0;corner<3;++corner) {
                const std::uint32_t idx=indices[i+corner];
                if (idx>=vertex_count ||
                    !clip_detail::ToEyeClip(vertices[idx],eye,cal,original[corner]))
                    return false;
            }
            std::array<clip_detail::Vertex,12> polygon{};
            std::size_t count=0;
            if (!clip_detail::Clip(original,cal.min_depth_m,polygon,count))
                return false;
            for (std::size_t j=1;j+1<count;++j) {
                ClippedEyeTriangle triangle{};
                if (!clip_detail::Normalize(polygon[0],cal.min_depth_m,triangle.vertices[0]) ||
                    !clip_detail::Normalize(polygon[j],cal.min_depth_m,triangle.vertices[1]) ||
                    !clip_detail::Normalize(polygon[j+1],cal.min_depth_m,triangle.vertices[2]))
                    return false;
                const auto& a=triangle.vertices[0];
                const auto& b=triangle.vertices[1];
                const auto& c=triangle.vertices[2];
                const double area=(double(b.ndc_x)-a.ndc_x)*(double(c.ndc_y)-a.ndc_y)-
                                  (double(b.ndc_y)-a.ndc_y)*(double(c.ndc_x)-a.ndc_x);
                if (std::abs(area)<1.e-12) continue; // clipped edge degeneracy
                if (triangles.size()>=49152) return false;
                triangles.push_back(triangle);
            }
        }
    }
    output=std::move(next);
    return true;
}
} // namespace m2vr::vr
