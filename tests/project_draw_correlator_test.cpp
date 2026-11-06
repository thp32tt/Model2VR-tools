#include "../src/probe/project_draw_correlator.hpp"
#include "../src/probe/d3d9_primitive_topology.hpp"
#include <vector>
#include <array>
#include <cmath>
#include <cstdio>
#include <limits>
#include "../src/probe/source_matrix_fingerprint.hpp"
#include "../src/probe/original_cpu_source_draw_gate.hpp"
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"source/draw correlation line %d: %s\n",__LINE__,#x);return 1;}}while(0)
using namespace m2vr::drawprobe;
static void Capture(ProjectDrawCorrelator& c,std::uint64_t epoch,
                    std::uint32_t caller,float sx,float sy,
                    float vx,float vy,float vz,
                    std::uint32_t matrix_identity=0u,
                    std::uint32_t matrix_fingerprint=0u,
                    std::uint32_t geometry_command=0u){
    const float view[3]{vx,vy,vz},screen[3]{sx,sy,0.5f};
    c.Capture(epoch,caller,view,screen,matrix_identity,matrix_fingerprint,
              geometry_command);
}
int main(){
    ProjectDrawCorrelator c{};
    const ProjectedDrawVertex face[3]{{100.f,150.f},{200.f,150.f},{100.f,250.f}};
    CHECK(!c.Match(1,face,3).Complete());
    Capture(c,5,0x1234,100.f,150.f,1.f,2.f,3.f);
    Capture(c,5,0x1234,200.f,150.f,4.f,5.f,6.f);
    Capture(c,5,0x1234,100.f,250.f,7.f,8.f,9.f);
    const auto joined=c.Match(5,face,3);
    CHECK(joined.Complete() && joined.matched==3u);
    CHECK(joined.source_xyz[0][0]==1.f);
    CHECK(joined.source_xyz[2][2]==9.f);
    CHECK(joined.source_caller_rva==0x1234u);
    CHECK(!c.Match(6,face,3).Complete());
    CHECK(!c.Match(5,face,2).Complete());
    // Multi-triangle original DPUP/DIPUP draws must not silently lose
    // provenance merely because their vertex count exceeds three.
    ProjectDrawCorrelator multi{};
    const ProjectedDrawVertex two_faces[6]{
        {10.f,11.f},{20.f,11.f},{10.f,21.f},
        {30.f,11.f},{40.f,11.f},{30.f,21.f}};
    for(unsigned i=0;i<6;++i)
        Capture(multi,43u,0x55u,two_faces[i].x,two_faces[i].y,
                float(i+1),float(i+2),5.f);
    const auto full_batch=multi.MatchTriangleList(43u,two_faces,6u);
    CHECK(full_batch.inspected==2u && full_batch.complete==2u);
    CHECK(full_batch.distinct_captures==6u &&
          full_batch.alias_violations==0u);
    // Six GPU vertices that reuse only three genuine CPU projections
    // must NOT be mistaken for a six-point camera fit.
    ProjectDrawCorrelator recycled{};
    for(unsigned i=0;i<3u;++i)
        Capture(recycled,91u,0x55u,two_faces[i].x,two_faces[i].y,
                float(i+1),float(i+2),5.f,0x1000u,0xAABBCCDDu,0x12u);
    ProjectedDrawVertex recycled_faces[6]{};
    for(unsigned i=0;i<6u;++i)recycled_faces[i]=two_faces[i%3u];
    const auto borrowed=recycled.MatchTriangleList(
        91u,recycled_faces,6u);
    CHECK(borrowed.inspected==2u && borrowed.complete==1u &&
          borrowed.alias_violations==1u && borrowed.ambiguous==1u);
    CHECK(!borrowed.CompleteSameSource() &&
          !borrowed.StableOriginalProjectionProvenance());
    const std::uint32_t forged_indices[6]{0u,1u,2u,3u,4u,5u};
    CHECK(recycled.MatchTriangleList(91u,recycled_faces,6u,
        forged_indices).alias_violations==1u);
    // Reverse source alias: an original source index cannot represent
    // two different projected CPU outputs, even if XY values differ.
    const std::uint32_t false_same_id[6]{0u,1u,2u,0u,4u,5u};
    const auto opposite_alias=multi.MatchTriangleList(
        43u,two_faces,6u,false_same_id);
    CHECK(opposite_alias.alias_violations==1u &&
          !opposite_alias.CompleteSameSource());
    // A real index window (no invented alias) retains six unique calls.
    CHECK(multi.MatchTriangleList(
        43u,two_faces,6u,forged_indices).CompleteSameSource());
    CHECK(full_batch.partial==0u && full_batch.ambiguous==0u);
    CHECK(full_batch.common_source_caller_rva==0x55u);
    CHECK(multi.MatchTriangleList(44u,two_faces,6u).complete==0u);
    CHECK(multi.MatchTriangleList(43u,two_faces,5u).inspected==0u);
    CHECK(multi.MatchTriangleList(43u,two_faces,
        kMaxOriginalProjectionSamples+3u).inspected==0u);
    ProjectDrawCorrelator cross_source{};
    for(unsigned i=0;i<6;++i)
        Capture(cross_source,44u,i<3u?0x55u:0x66u,
                two_faces[i].x,two_faces[i].y,
                float(i+1),float(i+2),5.f);
    const auto mixed_batch=cross_source.MatchTriangleList(
        44u,two_faces,6u);
    CHECK(mixed_batch.complete==2u &&
          mixed_batch.common_source_caller_rva==0u);
    // CPU points from different original geometry matrices cannot be
    // aggregated into one guessed world camera despite equal callers.
    ProjectDrawCorrelator matrices{};
    for(unsigned i=0;i<6;++i)
        Capture(matrices,84u,0x55u,two_faces[i].x,two_faces[i].y,
                float(i+1),float(i+2),5.f,0x1000u);
    const auto single_matrix=matrices.MatchTriangleList(84u,two_faces,6u);
    CHECK(single_matrix.CompleteSameSource());
    CHECK(!single_matrix.StableOriginalProjectionProvenance());
    CHECK(single_matrix.common_matrix_identity==0x1000u);
    ProjectDrawCorrelator mixed_matrices{};
    for(unsigned i=0;i<6;++i)
        Capture(mixed_matrices,85u,0x55u,two_faces[i].x,two_faces[i].y,
                float(i+1),float(i+2),5.f,
                i<3u?0x1000u:0x2000u);
    const auto switched=mixed_matrices.MatchTriangleList(
        85u,two_faces,6u);
    CHECK(switched.complete==2u && switched.mixed_matrix_identity);
    CHECK(!switched.CompleteSameSource());
    // Same matrix pointer with different contents is a separate camera
    // state and cannot be silently classified as one source world Draw.
    ProjectDrawCorrelator rewritten_matrix{};
    for(unsigned i=0;i<6;++i)
        Capture(rewritten_matrix,85u,0x55u,two_faces[i].x,two_faces[i].y,
                float(i+1),float(i+2),5.f,0x1000u,
                i<3u?0xAABBCCDDu:0xDEADBEEFu);
    const auto drift=rewritten_matrix.MatchTriangleList(85u,two_faces,6u);
    CHECK(drift.complete==2u && drift.mixed_matrix_fingerprint);
    CHECK(drift.common_matrix_identity==0x1000u);
    CHECK(!drift.CompleteSameSource());
    ProjectDrawCorrelator within_triangle_drift{};
    for(unsigned i=0;i<3;++i)
        Capture(within_triangle_drift,86u,0x55u,face[i].x,face[i].y,
                float(i+1),float(i+2),5.f,0x1000u,
                i==2u?0xDEADBEEFu:0xAABBCCDDu);
    CHECK(!within_triangle_drift.Match(86u,face,3u).Complete());
    ProjectDrawCorrelator consistent_content{};
    for(unsigned i=0;i<6;++i)
        Capture(consistent_content,87u,0x55u,two_faces[i].x,two_faces[i].y,
                float(i+1),float(i+2),5.f,0x1000u,0xAABBCCDDu);
    const auto stable_content=consistent_content.MatchTriangleList(
        87u,two_faces,6u);
    CHECK(stable_content.CompleteSameSource());
    CHECK(stable_content.StableOriginalProjectionProvenance());
    CHECK(SameOriginalCpuSourceAtDraw(
        0x1000u,0xAABBCCDDu,0x12u,
        0x1000u,0xAABBCCDDu,true,0x12u));
    CHECK(!SameOriginalCpuSourceAtDraw(
        0x1000u,0xAABBCCDDu,0x12u,
        0x1000u,0xDEADBEEFu,true,0x12u));
    CHECK(!SameOriginalCpuSourceAtDraw(
        0x1000u,0xAABBCCDDu,0x12u,
        0x2000u,0xAABBCCDDu,true,0x12u));
    CHECK(!SameOriginalCpuSourceAtDraw(
        0x1000u,0xAABBCCDDu,0x12u,
        0x1000u,0xAABBCCDDu,true,0x13u));
    CHECK(!SameOriginalCpuSourceAtDraw(
        0x1000u,0xAABBCCDDu,0x12u,
        0x1000u,0xAABBCCDDu,false,0x12u));
    CHECK(!SameOriginalCpuSourceAtDraw(
        0u,0xAABBCCDDu,0x12u,
        0u,0xAABBCCDDu,true,0x12u));
    CHECK(!SameOriginalCpuSourceAtDraw(
        0x1000u,0u,0x12u,0x1000u,0u,true,0x12u));
    CHECK(stable_content.common_matrix_fingerprint==0xAABBCCDDu);
    std::array<float,16> original_matrix{};
    original_matrix[0]=1.f;original_matrix[5]=1.f;
    original_matrix[10]=1.f;original_matrix[15]=1.f;
    const auto original_fingerprint=SourceMatrixFingerprint(original_matrix);
    CHECK(original_fingerprint!=0u);
    auto changed_matrix=original_matrix;
    changed_matrix[12]=0.5f;
    CHECK(SourceMatrixFingerprint(changed_matrix)!=original_fingerprint);
    changed_matrix=original_matrix;
    changed_matrix[12]=-0.0f;
    CHECK(SourceMatrixFingerprint(changed_matrix)==original_fingerprint);
    changed_matrix[5]=std::numeric_limits<float>::infinity();
    CHECK(SourceMatrixFingerprint(changed_matrix)==0u);
    // Geometry-command provenance is retained independently of matrix
    // stability. Different command words can belong to a genuine batched
    // Draw, and MUST NOT silently become world-pass authentication.
    ProjectDrawCorrelator command_witness{};
    for(unsigned i=0;i<6;++i)
        Capture(command_witness,88u,0x55u,two_faces[i].x,two_faces[i].y,
                float(i+1),float(i+2),5.f,0x1000u,0xAABBCCDDu,
                i<3u?0x12u:0x13u);
    const auto cmds=command_witness.MatchTriangleList(88u,two_faces,6u);
    CHECK(cmds.CompleteSameSource() && cmds.mixed_geometry_command);
    CHECK(!cmds.StableOriginalProjectionProvenance());
    CHECK(cmds.common_matrix_fingerprint==0xAABBCCDDu);
    CHECK(cmds.common_geometry_command==0x12u);
    ProjectDrawCorrelator stable_command{};
    for(unsigned i=0;i<6;++i)
        Capture(stable_command,88u,0x55u,two_faces[i].x,two_faces[i].y,
                float(i+1),float(i+2),5.f,0x1000u,0xAABBCCDDu,0x12u);
    const auto stable=stable_command.MatchTriangleList(88u,two_faces,6u);
    CHECK(stable.StableOriginalProjectionProvenance());
    CHECK(stable.CompleteSameSource() && !stable.mixed_geometry_command &&
          stable.common_geometry_command==0x12u);
    ProjectDrawCorrelator intra_triangle{};
    for(unsigned i=0;i<6;++i)
        Capture(intra_triangle,86u,0x55u,two_faces[i].x,two_faces[i].y,
                float(i+1),float(i+2),5.f,
                i==1u?0x2000u:0x1000u);
    const auto mixed_inside=intra_triangle.MatchTriangleList(
        86u,two_faces,6u);
    CHECK(mixed_inside.mixed_matrix_identity);
    CHECK(!mixed_inside.CompleteSameSource());
    CHECK(!intra_triangle.Match(86u,two_faces,3u).Complete());
    CHECK(!matrices.MatchTriangleList(87u,two_faces,6u).CompleteSameSource());
    Capture(multi,43u,0x55u,10.f,11.f,9.f,9.f,9.f);
    const auto conflicting_batch=multi.MatchTriangleList(
        43u,two_faces,6u);
    CHECK(conflicting_batch.complete==1u &&
          conflicting_batch.ambiguous==1u);

    const ProjectedDrawVertex shifted[3]{{101.f,150.f},{200.f,150.f},{100.f,250.f}};
    CHECK(c.Match(5,shifted,3).matched==2u);
    // Overlapping CPU points must never silently become a unique source.
    Capture(c,5,0x1234,100.f,150.f,10.f,11.f,12.f);
    const auto ambiguous=c.Match(5,face,3);
    CHECK(!ambiguous.Complete() && ambiguous.ambiguous==1u);
    // A genuine original UP strip/fan uses shared source vertices and
    // a non-trivial index order. Reconstruct from the actual original
    // source topology before comparing to same-thread CPU projections.
    ProjectDrawCorrelator strip_source{};
    constexpr ProjectedDrawVertex strip_vertices[5]{
        {12.f,20.f},{30.f,22.f},{15.f,48.f},{40.f,55.f},{80.f,60.f}};
    for(unsigned i=0;i<5;++i)
        Capture(strip_source,61u,0x456u,
            strip_vertices[i].x,strip_vertices[i].y,
            float(i+1),float(i+2),float(i+3));
    std::vector<std::uint32_t> triangle_order;
    CHECK(ExpandOriginalTriangles(D3DPT_TRIANGLESTRIP,3u,
        {0u,1u,2u,3u,4u},triangle_order));
    std::array<ProjectedDrawVertex,9> ordered{};
    for(std::size_t i=0;i<triangle_order.size();++i)
        ordered[i]=strip_vertices[triangle_order[i]];
    const auto strip_join=strip_source.MatchTriangleList(
        61u,ordered.data(),triangle_order.size(),triangle_order.data());
    // Three strip triangles share two original vertices; five distinct
    // projector calls cannot support the required six-point camera fit.
    CHECK(!strip_join.CompleteSameSource());
    CHECK(strip_join.complete==3u && strip_join.distinct_captures==5u &&
          strip_join.legitimate_shared_vertices==4u &&
          strip_join.alias_violations==0u);
    CHECK(strip_source.MatchTriangleList(
        61u,ordered.data(),triangle_order.size()).alias_violations>0u);
    // A fourth genuine strip triangle introduces a sixth independent
    // projection and can now retain source topology without recycling.
    const ProjectedDrawVertex extra{92.f,78.f};
    Capture(strip_source,61u,0x456u,extra.x,extra.y,6.f,7.f,8.f);
    const ProjectedDrawVertex strip6[6]{
        strip_vertices[0],strip_vertices[1],strip_vertices[2],
        strip_vertices[3],strip_vertices[4],extra};
    CHECK(ExpandOriginalTriangles(D3DPT_TRIANGLESTRIP,4u,
        {0u,1u,2u,3u,4u,5u},triangle_order));
    std::array<ProjectedDrawVertex,12> order4{};
    for(std::size_t i=0;i<triangle_order.size();++i)
        order4[i]=strip6[triangle_order[i]];
    const auto authentic_strip=strip_source.MatchTriangleList(
        61u,order4.data(),triangle_order.size(),triangle_order.data());
    CHECK(authentic_strip.CompleteSameSource() &&
          authentic_strip.distinct_captures==6u &&
          authentic_strip.legitimate_shared_vertices==6u);
    CHECK(authentic_strip.fit_sample_count==6u &&
          authentic_strip.camera_sample_count==12u);
    // The six authentic source vertices of one four-triangle indexed fan
    // are twelve GPU triangle vertices. The eye raster requires all
    // twelve, while camera least-squares must see each CPU call ONCE.
    ProjectDrawCorrelator optical_fan{};
    constexpr float ox[6]{-2.f,1.f,2.f,-1.f,1.5f,0.f};
    constexpr float oy[6]{-1.f,2.f,-2.f,1.5f,0.7f,2.f};
    constexpr float oz[6]{5.f,5.f,8.f,8.f,10.f,5.f};
    std::array<ProjectedDrawVertex,6> fan_source{};
    for(unsigned i=0;i<6u;++i){
        fan_source[i]={
            320.f+400.f*ox[i]/oz[i]+(i==0u?0.8f:0.f),
            240.f-380.f*oy[i]/oz[i]};
        Capture(optical_fan,73u,0x345u,
            fan_source[i].x,fan_source[i].y,
            ox[i],oy[i],oz[i],0x1000u,0xAABBCCDDu,0x12u);
    }
    std::vector<std::uint32_t> fan_order{};
    CHECK(ExpandOriginalTriangles(D3DPT_TRIANGLEFAN,4u,
        {0u,1u,2u,3u,4u,5u},fan_order));
    std::array<ProjectedDrawVertex,12> fan_triangles{};
    for(std::size_t i=0;i<fan_order.size();++i)
        fan_triangles[i]=fan_source[fan_order[i]];
    const auto fan_join=optical_fan.MatchTriangleList(
        73u,fan_triangles.data(),fan_triangles.size(),fan_order.data());
    CHECK(fan_join.StableOriginalProjectionProvenance());
    CHECK(fan_join.fit_sample_count==6u &&
          fan_join.camera_sample_count==12u &&
          fan_join.legitimate_shared_vertices==6u);
    // The original source-eye geometry must retain its exact duplicate
    // point order, while the optical fit must not bias the fan center 4x.
    for(std::size_t i=0;i<fan_triangles.size();++i)
        CHECK(fan_join.camera_samples[i].screen_xy[0]==fan_triangles[i].x &&
              fan_join.camera_samples[i].screen_xy[1]==fan_triangles[i].y);
    const auto independent_fit=FitSourcePerspective(
        fan_join.fit_samples.data(),fan_join.fit_sample_count,
        0u,0u,640u,480u);
    const auto overweighted_fit=FitSourcePerspective(
        fan_join.camera_samples.data(),fan_join.camera_sample_count,
        0u,0u,640u,480u);
    CHECK(independent_fit.plausible && overweighted_fit.plausible);
    CHECK(independent_fit.accepted_samples==6u &&
          overweighted_fit.accepted_samples==12u);
    CHECK(std::abs(independent_fit.scale_x-overweighted_fit.scale_x)>0.1f);
    // Physical DrawIndexedPrimitiveUP carries real ORIGINAL 16/32-bit
    // vertex IDs, not sequential positions in the index stream. Genuine
    // repeated corner IDs must survive 4-triangle fan expansion, whereas
    // treating source stream offsets as IDs falsely invents 12 vertices.
    std::array<std::uint32_t,12> authentic_index16{};
    std::array<std::uint32_t,12> authentic_index32{};
    std::array<std::uint32_t,12> fabricated_stream_ordinals{};
    for(std::size_t i=0u;i<fan_order.size();++i){
        authentic_index16[i]=500u+fan_order[i];
        authentic_index32[i]=65536u+fan_order[i];
        fabricated_stream_ordinals[i]=static_cast<std::uint32_t>(i);
    }
    const auto real_index16=optical_fan.MatchTriangleList(
        73u,fan_triangles.data(),fan_triangles.size(),
        authentic_index16.data());
    const auto real_index32=optical_fan.MatchTriangleList(
        73u,fan_triangles.data(),fan_triangles.size(),
        authentic_index32.data());
    CHECK(real_index16.StableOriginalProjectionProvenance() &&
          real_index32.StableOriginalProjectionProvenance());
    CHECK(real_index16.distinct_captures==6u &&
          real_index32.distinct_captures==6u);
    CHECK(real_index16.camera_sample_count==12u &&
          real_index16.fit_sample_count==6u);
    const auto fabricated_ordinal_join=optical_fan.MatchTriangleList(
        73u,fan_triangles.data(),fan_triangles.size(),
        fabricated_stream_ordinals.data());
    CHECK(!fabricated_ordinal_join.CompleteSameSource() &&
          fabricated_ordinal_join.alias_violations>0u);
    // One source vertex ID cannot borrow a different CPU projector point.
    authentic_index16[2]=authentic_index16[1];
    CHECK(!optical_fan.MatchTriangleList(
        73u,fan_triangles.data(),fan_triangles.size(),
        authentic_index16.data()).CompleteSameSource());
    // Maximum supported original 64-triangle fan expands 66 distinct
    // projector calls into 192 eye vertices, never 192 fit observations.
    ProjectDrawCorrelator full_fan{};
    std::vector<std::uint32_t> full_fan_ids(66u);
    std::array<ProjectedDrawVertex,66> full_fan_points{};
    for(unsigned i=0u;i<66u;++i){
        full_fan_ids[i]=i;
        full_fan_points[i]={20.f+float(i)*3.f,40.f+float(i)*2.f};
        Capture(full_fan,74u,0x345u,full_fan_points[i].x,
                full_fan_points[i].y,float(i),float(i+1u),5.f,
                0x1000u,0xAABBCCDDu,0x12u);
    }
    std::vector<std::uint32_t> full_fan_order{};
    CHECK(ExpandOriginalTriangles(D3DPT_TRIANGLEFAN,64u,
        full_fan_ids,full_fan_order));
    std::array<ProjectedDrawVertex,kMaxOriginalProjectionSamples>
        full_fan_triangles{};
    for(std::size_t i=0u;i<full_fan_order.size();++i)
        full_fan_triangles[i]=full_fan_points[full_fan_order[i]];
    const auto full_fan_join=full_fan.MatchTriangleList(
        74u,full_fan_triangles.data(),full_fan_order.size(),
        full_fan_order.data());
    CHECK(full_fan_join.StableOriginalProjectionProvenance());
    CHECK(full_fan_join.fit_sample_count==66u &&
          full_fan_join.camera_sample_count==192u &&
          full_fan_join.legitimate_shared_vertices==126u);
    CHECK(!full_fan.MatchTriangleList(74u,full_fan_triangles.data(),
        full_fan_order.size()).CompleteSameSource());
    CHECK(ExpandOriginalTriangles(D3DPT_TRIANGLEFAN,3u,
        {0u,1u,2u,3u,4u},triangle_order));
    for(std::size_t i=0;i<triangle_order.size();++i)
        ordered[i]=strip_vertices[triangle_order[i]];
    CHECK(!strip_source.MatchTriangleList(
        61u,ordered.data(),triangle_order.size(),
        triangle_order.data()).CompleteSameSource());
    CHECK(strip_source.MatchTriangleList(
        61u,order4.data(),triangle_order.size()).alias_violations>0u);
    CHECK(!strip_source.MatchTriangleList(
        62u,ordered.data(),triangle_order.size()).CompleteSameSource());
    // A different CPU caller may not be disguised as one proven pass.
    ProjectDrawCorrelator mixed{};
    Capture(mixed,7,0x1,100.f,150.f,1.f,2.f,3.f);
    Capture(mixed,7,0x2,200.f,150.f,4.f,5.f,6.f);
    Capture(mixed,7,0x1,100.f,250.f,7.f,8.f,9.f);
    CHECK(mixed.Match(7,face,3).matched==3u);
    CHECK(!mixed.Match(7,face,3).Complete());
    // New epoch invalidates every old projected point, even if coordinates
    // happen to repeat after camera/scene changes.
    Capture(c,8,0x1234,100.f,150.f,1.f,1.f,1.f);
    CHECK(c.Match(8,face,3).matched==1u);
    CHECK(c.Match(5,face,3).matched==0u);
    // Invalid/fabricated source coordinates and unknown caller are rejected.
    const float nan=(std::numeric_limits<float>::quiet_NaN)();
    const float bad[3]{nan,2.f,3.f},screen[3]{200.f,150.f,0.f};
    c.Capture(8,0x1234,bad,screen);
    c.Capture(8,0u,screen,screen);
    CHECK(c.Match(8,face,3).matched==1u);
    CHECK(CurrentProjectDrawEpoch()>0u);
    const auto previous=CurrentProjectDrawEpoch();
    NextProjectDrawEpoch();
    CHECK(CurrentProjectDrawEpoch()==previous+1u);
    // One source point cannot match multiple draw vertices.
    const ProjectedDrawVertex repeated[3]{{100.f,150.f},{100.f,150.f},{100.f,150.f}};
    const auto reused=c.Match(8,repeated,3);
    CHECK(reused.matched==1u && reused.ambiguous==2u);
    // Standard D3D9 viewport hypothesis for a shader POSITION0 clip
    // vector: pure candidate evidence, never an automatic world flag.
    const float center_clip[4]{0.f,0.f,0.5f,1.f};
    ProjectedDrawVertex center{};
    CHECK(CandidateClipToViewport(center_clip,0u,0u,640u,480u,center));
    CHECK(center.x==320.f && center.y==240.f);
    ProjectDrawCorrelator shader{};
    Capture(shader,99u,0x44u,320.f,240.f,1.f,2.f,3.f);
    const float upper_left[4]{-1.f,1.f,0.5f,1.f};
    const float lower_left[4]{-1.f,-1.f,0.5f,1.f};
    ProjectedDrawVertex shader_points[3]{};
    CHECK(CandidateClipToViewport(center_clip,0,0,640,480,shader_points[0]));
    CHECK(CandidateClipToViewport(upper_left,0,0,640,480,shader_points[1]));
    CHECK(CandidateClipToViewport(lower_left,0,0,640,480,shader_points[2]));
    Capture(shader,99u,0x44u,0.f,0.f,4.f,5.f,6.f);
    Capture(shader,99u,0x44u,0.f,480.f,7.f,8.f,9.f);
    CHECK(shader.Match(99u,shader_points,3).Complete());
    float invalid_clip[4]{0.f,0.f,0.5f,0.f};
    CHECK(!CandidateClipToViewport(invalid_clip,0,0,640,480,center));
    invalid_clip[3]=-1.f;
    CHECK(!CandidateClipToViewport(invalid_clip,0,0,640,480,center));
    invalid_clip[3]=std::numeric_limits<float>::infinity();
    CHECK(!CandidateClipToViewport(invalid_clip,0,0,640,480,center));
    CHECK(!CandidateClipToViewport(center_clip,0,0,0,480,center));
    CHECK(!CandidateClipToViewport(center_clip,0,0,640,0,center));
    // Previously 9+ original triangles were rejected even for complete
    // same-thread source projector data. A single larger actual D3D9 Draw
    // must remain one atomic two-eye/material/camera source transaction.
    CHECK(ProjectDrawCorrelator::kMaxJoinTriangles==64u);
    ProjectDrawCorrelator larger{};
    std::array<ProjectedDrawVertex,kMaxOriginalProjectionSamples> screen64{};
    for(std::size_t i=0;i<screen64.size();++i){
        const float x=20.f+float(i)*3.f;
        const float y=30.f+float(i%48u)*4.f;
        screen64[i]={x,y};
        const float z=5.f+float(i%7u);
        Capture(larger,123u,0xA551u,x,y,
            (x-320.f)*z/400.f,(240.f-y)*z/380.f,z,
            0x1111u,0xA0A0u,0x77u);
    }
    const auto joined64=larger.MatchTriangleList(
        123u,screen64.data(),screen64.size());
    CHECK(joined64.inspected==64u && joined64.complete==64u);
    CHECK(joined64.StableOriginalProjectionProvenance());
    CHECK(joined64.camera_sample_count==kMaxOriginalProjectionSamples);
    const auto fitted64=FitSourcePerspective(
        joined64.camera_samples.data(),joined64.camera_sample_count,
        0u,0u,640u,480u);
    CHECK(fitted64.plausible && fitted64.accepted_samples==192u);
    CHECK(fitted64.rms_pixels<0.01f);
    CHECK(larger.MatchTriangleList(123u,screen64.data(),
          kMaxOriginalProjectionSamples+3u).inspected==0u);
    larger.RetireOriginalDraw(123u);
    CHECK(!larger.MatchTriangleList(
        123u,screen64.data(),screen64.size()).CompleteSameSource());
    // Fresh CPU projector calls can join the NEXT original Draw in the
    // same Present, but the already retired points may not be recycled.
    for(unsigned i=0;i<6u;++i){
        const auto& v=screen64[i];
        const float xyz[3]{(v.x-320.f)*5.f/400.f,
                           (240.f-v.y)*5.f/380.f,5.f};
        const float xy[3]{v.x,v.y,0.f};
        larger.Capture(123u,0xA551u,xyz,xy,0x1111u,0xA0A0u,0x77u);
    }
    CHECK(larger.MatchTriangleList(123u,screen64.data(),6u)
        .StableOriginalProjectionProvenance());
    larger.RetireOriginalDraw(122u); // stale epoch must not rewind
    CHECK(larger.MatchTriangleList(123u,screen64.data(),6u)
        .StableOriginalProjectionProvenance());
    larger.RetireOriginalDraw(123u);
    CHECK(!larger.MatchTriangleList(123u,screen64.data(),6u)
        .StableOriginalProjectionProvenance());
    // Four hooked physical Draw methods use this RAII boundary.
    for(unsigned i=0;i<3u;++i)
        Capture(ThreadProjectDrawCorrelator(),CurrentProjectDrawEpoch(),
            0x55u,face[i].x,face[i].y,1.f+float(i),2.f,4.f);
    CHECK(ThreadProjectDrawCorrelator().Match(
        CurrentProjectDrawEpoch(),face,3u).Complete());
    {
        const ScopedOriginalDrawBoundary draw_boundary;
    }
    CHECK(!ThreadProjectDrawCorrelator().Match(
        CurrentProjectDrawEpoch(),face,3u).Complete());
    return 0;
}
