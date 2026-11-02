// Synthetic passive COM Draw caller sampling; requires no emulator/ROM/HMD.
#include "../src/probe/draw_origin_gate.hpp"
#include <cstdint>
#include <cstdio>
#define CHECK(expr) do{if(!(expr)){std::fprintf(stderr, "origin gate failure at line %d: %s\n",__LINE__,#expr);return 1;}}while(0)
int main(){
    using m2vr::drawprobe::DrawOriginGate;
    DrawOriginGate gate{};
    CHECK(!gate.ShouldObserve(0,0x100u,0));
    CHECK(!gate.ShouldObserve(5,0x100u,0));
    CHECK(gate.ShouldObserve(1,0x100u,0)); // first family draw
    CHECK(!gate.ShouldObserve(1,0x100u,1)); // same caller not a new witness
    CHECK(gate.ShouldObserve(1,0x100u,1024)); // periodic safety net
    CHECK(gate.ShouldObserve(1,0x200u,2)); // new origin is not lost
    CHECK(!gate.ShouldObserve(1,0u,3)); // outside executable
    CHECK(gate.ShouldObserve(1,0u,2048)); // still periodic
    CHECK(gate.ShouldObserve(2,0x100u,1)); // family isolation
    CHECK(gate.ShouldObserve(3,0x100u,0));
    CHECK(gate.ShouldObserve(4,0x100u,0));
    // The unique source budget is process-bounded. Any hash collisions
    // may suppress individual candidates, but never periodic tracing.
    unsigned additional=0;
    for(std::uint32_t i=1;i<=2000;++i)
        if(gate.ShouldObserve(1,0x400000u+i*17u,1u))++additional;
    CHECK(additional<=DrawOriginGate::kMaxNewSources);
    CHECK(additional>=16);
    CHECK(!gate.ShouldObserve(1,0x100u,5001));
    CHECK(gate.ShouldObserve(1,0x100u,5120)); // exact ordinal multiple
    return 0;
}
