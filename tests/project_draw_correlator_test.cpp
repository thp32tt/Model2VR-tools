#include "../src/probe/project_draw_correlator.hpp"
#include <array>
#include <cmath>
#include <cstdio>
#include <limits>
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"source/draw correlation line %d: %s\n",__LINE__,#x);return 1;}}while(0)
using namespace m2vr::drawprobe;
static void Capture(ProjectDrawCorrelator& c,std::uint64_t epoch,
                    std::uint32_t caller,float sx,float sy,
                    float vx,float vy,float vz){
    const float view[3]{vx,vy,vz},screen[3]{sx,sy,0.5f};
    c.Capture(epoch,caller,view,screen);
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
    CHECK(full_batch.partial==0u && full_batch.ambiguous==0u);
    CHECK(full_batch.common_source_caller_rva==0x55u);
    CHECK(multi.MatchTriangleList(44u,two_faces,6u).complete==0u);
    CHECK(multi.MatchTriangleList(43u,two_faces,5u).inspected==0u);
    CHECK(multi.MatchTriangleList(43u,two_faces,27u).inspected==0u);
    ProjectDrawCorrelator cross_source{};
    for(unsigned i=0;i<6;++i)
        Capture(cross_source,44u,i<3u?0x55u:0x66u,
                two_faces[i].x,two_faces[i].y,
                float(i+1),float(i+2),5.f);
    const auto mixed_batch=cross_source.MatchTriangleList(
        44u,two_faces,6u);
    CHECK(mixed_batch.complete==2u &&
          mixed_batch.common_source_caller_rva==0u);
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
    return 0;
}
