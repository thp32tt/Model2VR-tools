#pragma once
// One coherent OpenXR view pair per emulated frame; no simulation callbacks.
#include "model2_cpu_stereo.hpp"
#include <cstdint>
namespace m2vr::vr {
class Model2FramePoseLatch {
public:
 bool Begin(std::uint64_t game_frame,const pose::Frame& predicted) noexcept {
  valid_=false; // failed refresh must never expose the previous pose
  if(!game_frame||game_frame<=last_game_frame_||
     !predicted.sequence||predicted.sequence<=last_pose_sequence_||
     (predicted.view_flags&(pose::kOrientationValid|pose::kPositionValid))!=
      (pose::kOrientationValid|pose::kPositionValid))return false;
  for(const auto& eye:predicted.eyes) {
   if(!QuaternionValid(eye.orientation_xyzw))return false;
   for(float v:eye.position_m)if(!Finite(v))return false;
   for(float v:eye.fov_left_right_up_down)
    if(!Finite(v)||v<=-1.55f||v>=1.55f)return false;
   const auto& f=eye.fov_left_right_up_down;
   if(!(f[0]<0.f&&f[1]>0.f&&f[2]>0.f&&f[3]<0.f))return false;
  }
  frame_=predicted;
  last_game_frame_=game_frame;
  last_pose_sequence_=predicted.sequence;
  valid_=true;
  return true;
 }
 bool Read(std::uint64_t game_frame,pose::Frame& out)const noexcept {
  if(!valid_||game_frame!=last_game_frame_)return false;
  out=frame_;return true;
 }
 void Reset()noexcept {
  valid_=false;last_game_frame_=last_pose_sequence_=0;frame_={};
 }
private:
 pose::Frame frame_{};
 std::uint64_t last_game_frame_=0,last_pose_sequence_=0;
 bool valid_=false;
};
}
