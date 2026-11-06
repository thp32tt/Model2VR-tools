#include "../src/probe/project_capture_stack.hpp"
#include "../src/probe/original_source_capture_mode.hpp"
#include <cstdio>
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"project capture stack line %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(){
    using namespace m2vr::drawprobe;
    ProjectCaptureStack stack{};
    int source_a=1,source_b=2;
    ProjectCaptureRecord completed{};
    CHECK(!stack.Top());
    CHECK(!stack.Finish(completed));
    CHECK(stack.Begin(&source_a,true));
    auto* outer=stack.Top();
    CHECK(outer && outer->original_point==&source_a);
    outer->source_caller_rva=0x1234u;
    outer->source_epoch=71u;
    outer->source_xyz={1.f,2.f,3.f};
    outer->capture=true;
    outer->matrix_identity=0x1008u;
    outer->matrix_fingerprint=0x12345678u;
    outer->geometry_stream_pointer=0x10000u;
    outer->geometry_command_word=0x789ABC12u;
    outer->geometry_command_valid=true;
    CHECK(stack.Begin(&source_b,false));
    auto* inner=stack.Top();
    CHECK(inner && inner->original_point==&source_b);
    inner->source_caller_rva=0x5678u;
    inner->capture=true;
    // The original x86 EAX may be a return value, not its input pointer.
    // Finishing each nested original call restores the right saved input.
    CHECK(stack.Finish(completed));
    CHECK(completed.original_point==&source_b);
    CHECK(completed.source_caller_rva==0x5678u);
    CHECK(!completed.emit_log);
    CHECK(stack.Top()->original_point==&source_a);
    CHECK(stack.Finish(completed));
    CHECK(completed.original_point==&source_a);
    CHECK(completed.source_caller_rva==0x1234u);
    CHECK(completed.source_epoch==71u);
    CHECK(OriginalCpuProjectorEpochStable(71u,71u));
    CHECK(!OriginalCpuProjectorEpochStable(71u,72u));
    CHECK(!OriginalCpuProjectorEpochStable(0u,0u));
    CHECK(!OriginalCpuProjectorEpochStable(0u,71u));
    CHECK(completed.emit_log && completed.capture);
    CHECK(completed.matrix_fingerprint==0x12345678u);
    CHECK(completed.geometry_stream_pointer==0x10000u);
    CHECK(completed.geometry_command_word==0x789ABC12u);
    CHECK(completed.geometry_command_valid);
    // Actual x86 original CPU ProjectVertexToScreen evidence must be
    // available at BOTH ends of the same call. Missing geometry command
    // or unknown matrix may not enter the 64-triangle XR source fitter.
    CHECK(OriginalCpuProjectorRecordSafeToJoin(completed,false,false));
    CHECK(OriginalCpuProjectorSourceGapFlags(completed,false,false)==0u);
    CHECK(OriginalCpuProjectorSourceGapFlags(completed,true,false)==2u);
    CHECK(OriginalCpuProjectorSourceGapFlags(completed,false,true)==4u);
    CHECK(OriginalCpuProjectorSourceGapFlags(completed,true,true)==6u);
    // Genuine x86 projector enters epoch 71, but original Present/Reset
    // completes before its return (or during the post-capture fence).
    CHECK(OriginalCpuProjectorSourceGapFlags(completed,false,false,true)==8u);
    CHECK(OriginalCpuProjectorSourceGapFlags(completed,true,true,true)==14u);
    CHECK(!OriginalCpuProjectorRecordSafeToJoin(completed,false,false,true));
    CHECK(!OriginalCpuProjectorRecordSafeToJoin(completed,true,false));
    CHECK(!OriginalCpuProjectorRecordSafeToJoin(completed,false,true));
    auto unavailable=completed;
    unavailable.geometry_command_valid=false;
    CHECK(!OriginalCpuProjectorRecordSafeToJoin(unavailable,false,false));
    CHECK(OriginalCpuProjectorSourceGapFlags(unavailable,false,false)==4u);
    unavailable=completed;
    unavailable.geometry_stream_pointer=0u;
    CHECK(!OriginalCpuProjectorRecordSafeToJoin(unavailable,false,false));
    unavailable=completed;
    unavailable.matrix_identity=0u;
    CHECK(!OriginalCpuProjectorRecordSafeToJoin(unavailable,false,false));
    unavailable=completed;
    unavailable.matrix_fingerprint=0u;
    CHECK(!OriginalCpuProjectorRecordSafeToJoin(unavailable,false,false));
    unavailable=completed;
    unavailable.source_epoch=0u;
    CHECK(OriginalCpuProjectorSourceGapFlags(unavailable,false,false)==8u);
    CHECK(!OriginalCpuProjectorRecordSafeToJoin(unavailable,false,false));
    unavailable=completed;
    unavailable.source_caller_rva=0u;
    CHECK(!OriginalCpuProjectorRecordSafeToJoin(unavailable,false,false));
    CHECK(OriginalCpuProjectorSourceGapFlags(unavailable,false,false)==1u);
    unavailable=completed;
    unavailable.geometry_command_word=0u; // genuine zero is allowed
    CHECK(OriginalCpuProjectorRecordSafeToJoin(unavailable,false,false));
    unavailable.geometry_command_valid=false;
    CHECK(!OriginalCpuProjectorRecordSafeToJoin(unavailable,false,false));
    CHECK(!stack.Top());
    CHECK(completed.source_xyz[2]==3.f);
    CHECK(stack.Depth()==0u);
    // Any overflow chain must consume its own matching unwinds and
    // never pop a still-active parent frame or attach false provenance.
    for(unsigned i=0;i<ProjectCaptureStack::kMaxDepth;++i)
        CHECK(stack.Begin(&source_a,i==0u));
    CHECK(!stack.Begin(&source_b,true));
    CHECK(!stack.Begin(&source_b,false));
    CHECK(!stack.Top());
    CHECK(!stack.Finish(completed));
    CHECK(!stack.Finish(completed));
    CHECK(stack.Depth()==ProjectCaptureStack::kMaxDepth);
    for(unsigned i=0;i<ProjectCaptureStack::kMaxDepth;++i){
        CHECK(stack.Finish(completed));
        CHECK(completed.original_point==&source_a);
    }
    CHECK(stack.Depth()==0u);
    CHECK(!stack.Finish(completed));
    CHECK(stack.Begin(nullptr,false));
    CHECK(stack.Finish(completed) && !completed.original_point);
    // Startup with native renderer alone, without the passive probe flag,
    // still needs the original D3D9 boundary and signed CPU projector.
    CHECK(!NeedOriginalD3D9Hooks(false,false));
    CHECK(NeedOriginalD3D9Hooks(true,false));
    CHECK(NeedOriginalD3D9Hooks(false,true));
    CHECK(NeedOriginalD3D9Hooks(true,true));
    CHECK(!CaptureOriginalCpuProjector(false,false,true));
    CHECK(!CaptureOriginalCpuProjector(false,true,false));
    CHECK(CaptureOriginalCpuProjector(false,true,true));
    CHECK(CaptureOriginalCpuProjector(true,false,true));
    CHECK(!OriginalCpuProjectorProofInputsReady(false,true,true,false,true));
    CHECK(!OriginalCpuProjectorProofInputsReady(false,true,true,true,false));
    CHECK(!OriginalCpuProjectorProofInputsReady(false,true,false,true,true));
    CHECK(OriginalCpuProjectorProofInputsReady(false,true,true,true,true));
    CHECK(OriginalCpuProjectorProofInputsReady(true,false,true,true,true));
    return 0;
}
