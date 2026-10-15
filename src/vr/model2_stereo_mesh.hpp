#pragma once
#include "model2_cpu_stereo.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>
#include <utility>
namespace m2vr::vr {
struct StereoTriangle { std::array<std::array<EyeProjection,3>,2> eyes{}; };
inline bool BuildStereoTriangles(
 const CpuPoint* vertices, std::size_t vertex_count,
 const std::uint32_t* indices, std::size_t index_count,
 const pose::Frame& frame, const Model2CameraCalibration& calibration,
 const DrawEvidence& evidence, std::vector<StereoTriangle>& output) {
 if (!vertices || !indices || !vertex_count || !index_count ||
     index_count%3 || index_count/3>8192 ||
     ChooseEyeAction(evidence)!=EyeAction::StereoGeometry ||
     !ValidCalibration(calibration)) return false;
 std::vector<StereoTriangle> next;
 next.reserve(index_count/3);
 for (std::size_t i=0;i<index_count;i+=3) {
  StereoTriangle triangle{};
  for (std::size_t corner=0;corner<3;++corner) {
   const auto index=indices[i+corner];
   if (index>=vertex_count) return false;
   const auto pair=ProjectModel2Stereo(vertices[index],frame,calibration,evidence);
   if (!pair.stereo) return false;
   for (std::size_t eye=0;eye<2;++eye) {
    const auto& p=pair.eyes[eye];
    if (!p.valid || p.ndc_x< -1.f || p.ndc_x>1.f ||
        p.ndc_y< -1.f || p.ndc_y>1.f) return false;
    triangle.eyes[eye][corner]=p;
   }
  }
  next.push_back(triangle);
 }
 output=std::move(next);
 return true;
}
}
