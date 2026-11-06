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
// Real original x86 projector callers enter the L/R World3D source ring
// only if BOTH original active matrix contents and the geometry command
// word were actually readable throughout the same native invocation.
 // A legitimately zero command word is allowed only when its *readability*
 // bit is true; an unavailable stream must not become a fake zero command.
// Bounded pointer-free original projector source gap codes. Only the
// public numeric mask is logged for sampled PRIVATE original-game runs.
 // Bit 1: missing original caller/input; 2: matrix not readable/stable;
 // bit 4: active geometry command not readable/stable. These never
 // authenticate a gameplay world or give a caller World3D permission.
inline constexpr std::uint32_t OriginalCpuProjectorSourceGapFlags(
    const ProjectCaptureRecord& record,
    bool matrix_changed,bool geometry_changed) noexcept {
    std::uint32_t mask=0u;
    if(!record.capture || !record.original_point || !record.source_caller_rva)
        mask|=1u;
    if(!record.matrix_identity || !record.matrix_fingerprint || matrix_changed)
        mask|=2u;
    if(!record.geometry_stream_pointer || !record.geometry_command_valid ||
       geometry_changed)
        mask|=4u;
    return mask;
}
inline constexpr bool OriginalCpuProjectorRecordSafeToJoin(
    const ProjectCaptureRecord& record,
    bool matrix_changed,bool geometry_changed) noexcept {
    return OriginalCpuProjectorSourceGapFlags(
        record,matrix_changed,geometry_changed)==0u;
}
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
