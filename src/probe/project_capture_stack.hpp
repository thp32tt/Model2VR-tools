#pragma once
// In-flight original ProjectVertexToScreen calls are paired by invocation,
// not by the value of EAX after the unmodified original trampoline returns.
// Bounded per-render-thread state; overflow fails closed and remains balanced
// through nested calls. Only the authenticated x86 source hook fills a record.
#include <array>
#include <cstdint>

namespace m2vr::drawprobe {
struct ProjectCaptureRecord {
    const void* original_point=nullptr;
    std::array<float,3> source_xyz{};
    std::uint32_t source_caller_rva=0;
    // Original active matrix identity sampled before the CPU projector.
    std::uint32_t matrix_identity=0;
    // Snapshot of all 16 source matrix floats before the original projector.
    // Zero means unavailable; source adapter never treats it as proof.
    std::uint32_t matrix_fingerprint=0;
    // Read-only active geometry stream command word at original CPU call.
    // It is a *candidate* command context, not a world-pass bit.
    std::uint32_t geometry_stream_pointer=0;
    std::uint32_t geometry_command_word=0;
    // A legitimate zero command is distinct from an unreadable stream.
    bool geometry_command_valid=false;
    bool emit_log=false;
    bool capture=false;
};
class ProjectCaptureStack final {
public:
    static constexpr unsigned kMaxDepth=16u;
    bool Begin(const void* original_point,bool sampled) noexcept {
        if(overflow_ || depth_==kMaxDepth){
            ++overflow_;
            return false;
        }
        auto& record=entries_[depth_++];
        record={};
        record.original_point=original_point;
        record.emit_log=sampled;
        return true;
    }
    ProjectCaptureRecord* Top() noexcept {
        return !overflow_ && depth_?&entries_[depth_-1u]:nullptr;
    }
    bool Finish(ProjectCaptureRecord& output) noexcept {
        if(overflow_){
            --overflow_;
            return false;
        }
        if(!depth_)return false;
        --depth_;
        output=entries_[depth_];
        entries_[depth_]={};
        return true;
    }
    unsigned Depth() const noexcept {return depth_;}
private:
    std::array<ProjectCaptureRecord,kMaxDepth> entries_{};
    unsigned depth_=0,overflow_=0;
};
} // namespace m2vr::drawprobe
