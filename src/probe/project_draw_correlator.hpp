#pragma once
// Same-thread, same-Present-epoch join of source CPU projection and actual
// D3D9 XYZRHW UP draws. This is evidence only, NOT world/HUD proof.
// Raw game pointers and emulator-specific addresses never leave the process.
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include "model2_camera_projection_fit.hpp"

namespace m2vr::drawprobe {
struct ProjectedDrawVertex {
    float x=0.f, y=0.f;
};
// Only a diagnostic screen-coordinate hypothesis for a known shader-UP
// POSITION0 clip vector. Actual shader transforms may differ; a match
// does not calibrate the game camera or authenticate a world draw.
inline bool CandidateClipToViewport(const float clip[4],
                                    std::uint32_t x,std::uint32_t y,
                                    std::uint32_t width,std::uint32_t height,
                                    ProjectedDrawVertex& out) noexcept {
    if(!clip || !width || !height ||
       width>16384u || height>16384u ||
       x>16384u || y>16384u)return false;
    for(std::size_t i=0;i<4;++i)
        if(!std::isfinite(clip[i]))return false;
    const float w=clip[3];
    if(w<0.05f || w>65536.f)return false;
    const float nx=clip[0]/w,ny=clip[1]/w;
    if(!std::isfinite(nx) || !std::isfinite(ny) ||
       std::abs(nx)>1.25f || std::abs(ny)>1.25f)return false;
    const float px=static_cast<float>(x)+
        (nx+1.f)*(static_cast<float>(width)*0.5f);
    const float py=static_cast<float>(y)+
        (1.f-ny)*(static_cast<float>(height)*0.5f);
    if(!std::isfinite(px)||!std::isfinite(py))return false;
    out={px,py};
    return true;
}
struct ProjectedTriangleBatchJoin {
    // First N original triangles from ONE Draw. Each complete member
    // independently matches three distinct same-thread CPU projections.
    // No field here establishes world/HUD classification.
    std::uint32_t inspected=0;
    std::uint32_t complete=0;
    std::uint32_t partial=0;
    std::uint32_t ambiguous=0;
    std::uint32_t common_source_caller_rva=0;
    std::uint32_t common_matrix_identity=0;
    bool mixed_matrix_identity=false;
    // Filled only from individually complete original triangles; never
    // admits a mixed CPU caller as calibrated camera source.
    std::array<SourceProjectionSample,24> camera_samples{};
    std::size_t camera_sample_count=0u;
    bool CompleteSameSource() const noexcept {
        return inspected>=2u && complete==inspected &&
            ambiguous==0u && partial==0u && !mixed_matrix_identity &&
            common_source_caller_rva!=0u &&
            camera_sample_count==std::size_t(complete)*3u;
    }
};
struct ProjectedSourceJoin {
    std::uint32_t matched=0;
    std::uint32_t ambiguous=0;
    std::uint32_t source_caller_rva=0;
    std::uint32_t matrix_identity=0;
    bool mixed_matrix_identity=false;
    std::array<std::array<float,3>,3> source_xyz{};
    // Three distinct, unambiguous matches are a correlation candidate;
    // they do NOT independently authenticate a world pass or camera units.
    bool Complete() const noexcept {
        return matched==3u && ambiguous==0u &&
               !mixed_matrix_identity && source_caller_rva!=0u;
    }
};
class ProjectDrawCorrelator final {
public:
    static constexpr std::size_t kCapacity=256u;
    static constexpr float kMaxDeltaPixels=0.25f;
    void Capture(std::uint64_t epoch,std::uint32_t caller_rva,
                 const float source_xyz[3],
                 const float projected_xyz[3],
                 std::uint32_t matrix_identity=0u) noexcept {
        if(!epoch || !caller_rva || !source_xyz || !projected_xyz)return;
        for(std::size_t i=0;i<3;++i) {
            if(!std::isfinite(source_xyz[i]) ||
               !std::isfinite(projected_xyz[i]) ||
               std::abs(source_xyz[i])>10000000.f ||
               std::abs(projected_xyz[i])>10000000.f)return;
        }
        // A new Present/Reset epoch prevents cached screen vertices from
        // being reinterpreted as current CPU-world points.
        if(epoch_!=epoch){
            epoch_=epoch;
            size_=0u;
            cursor_=0u;
        }
        Entry& entry=entries_[cursor_];
        for(std::size_t i=0;i<3;++i)entry.source[i]=source_xyz[i];
        entry.x=projected_xyz[0];
        entry.y=projected_xyz[1];
        entry.caller_rva=caller_rva;
        entry.matrix_identity=matrix_identity;
        cursor_=(cursor_+1u)%kCapacity;
        if(size_<kCapacity)++size_;
    }
    ProjectedSourceJoin Match(std::uint64_t epoch,
                              const ProjectedDrawVertex* vertices,
                              std::size_t count) const noexcept {
        ProjectedSourceJoin result{};
        if(!epoch || epoch!=epoch_ || !vertices || count!=3u)return result;
        std::array<std::size_t,3> chosen{
            kCapacity,kCapacity,kCapacity};
        std::uint32_t common_caller=0u;
        std::uint32_t common_matrix=0u;
        for(std::size_t v=0;v<count;++v){
            const auto& pos=vertices[v];
            if(!std::isfinite(pos.x)||!std::isfinite(pos.y))return {};
            std::size_t match_index=kCapacity;
            unsigned hits=0u;
            // The ring is bounded, contains no pointers, and is only ever
            // accessed from the current original game render thread.
            for(std::size_t i=0;i<size_;++i){
                const auto& sample=entries_[i];
                if(std::abs(sample.x-pos.x)<=kMaxDeltaPixels &&
                   std::abs(sample.y-pos.y)<=kMaxDeltaPixels){
                    ++hits;
                    match_index=i;
                    if(hits>1u)break;
                }
            }
            if(hits>1u){++result.ambiguous;continue;}
            if(hits==0u)continue;
            bool reused=false;
            for(std::size_t j=0;j<v;++j)
                if(chosen[j]==match_index){reused=true;break;}
            if(reused){++result.ambiguous;continue;}
            chosen[v]=match_index;
            const auto& sample=entries_[match_index];
            result.source_xyz[v]=sample.source;
            if(!result.matched){
                common_caller=sample.caller_rva;
                common_matrix=sample.matrix_identity;
            }else{
                if(common_caller!=sample.caller_rva)common_caller=0u;
                if(common_matrix!=sample.matrix_identity)
                    result.mixed_matrix_identity=true;
            }
            ++result.matched;
        }
        result.source_caller_rva=common_caller;
        result.matrix_identity=result.mixed_matrix_identity?0u:common_matrix;
        return result;
    }
    // Analyze up to eight original triangle-list primitives, not merely
    // draws consisting of exactly one triangle. A capped batch avoids
    // unbounded CPU work in the hooked D3D9 render path.
    static constexpr std::size_t kMaxJoinTriangles=8u;
    ProjectedTriangleBatchJoin MatchTriangleList(
        std::uint64_t epoch,const ProjectedDrawVertex* vertices,
        std::size_t vertex_count) const noexcept {
        ProjectedTriangleBatchJoin batch{};
        if(!vertices || !epoch || vertex_count<3u ||
           vertex_count%3u || vertex_count>3u*kMaxJoinTriangles)
            return batch;
        for(std::size_t i=0;i<vertex_count;i+=3u){
            const auto join=Match(epoch,vertices+i,3u);
            ++batch.inspected;
            if(join.Complete()){
                if(!batch.complete){
                    batch.common_source_caller_rva=join.source_caller_rva;
                    batch.common_matrix_identity=join.matrix_identity;
                }else{
                    if(batch.common_source_caller_rva!=join.source_caller_rva)
                        batch.common_source_caller_rva=0u;
                    if(batch.common_matrix_identity!=join.matrix_identity)
                        batch.mixed_matrix_identity=true;
                }
                for(std::size_t v=0;v<3u;++v) {
                    auto& dest=batch.camera_samples[batch.camera_sample_count++];
                    dest.source_xyz=join.source_xyz[v];
                    dest.screen_xy={vertices[i+v].x,vertices[i+v].y};
                }
                ++batch.complete;
            }else if(join.matched){
                ++batch.partial;
            }
            if(join.ambiguous)++batch.ambiguous;
            if(join.mixed_matrix_identity)batch.mixed_matrix_identity=true;
        }
        return batch;
    }
private:
    struct Entry {
        std::array<float,3> source{};
        float x=0.f,y=0.f;
        std::uint32_t caller_rva=0u;
        std::uint32_t matrix_identity=0u;
    };
    std::array<Entry,kCapacity> entries_{};
    std::uint64_t epoch_=0u;
    std::size_t size_=0u,cursor_=0u;
};
// ProjectVertexToScreen and IDirect3DDevice9::Draw callbacks on the
// same thread share one bounded witness; unrelated threads cannot join.
inline ProjectDrawCorrelator& ThreadProjectDrawCorrelator() noexcept {
    thread_local ProjectDrawCorrelator state{};
    return state;
}
inline std::atomic<std::uint64_t> g_project_draw_epoch{1u};
inline std::uint64_t CurrentProjectDrawEpoch() noexcept {
    return g_project_draw_epoch.load(std::memory_order_relaxed);
}
inline void NextProjectDrawEpoch() noexcept {
    g_project_draw_epoch.fetch_add(1u,std::memory_order_relaxed);
}
} // namespace m2vr::drawprobe
