#include "../src/probe/d3d9_primary_device_owner.hpp"
#include <cstdio>
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"D3D9 primary device ownership line %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(){
    m2vr::drawprobe::OriginalD3D9DeviceOwner owner{};
    int primary=1,secondary=2,recreated=3;
    CHECK(!owner.Bound());
    CHECK(!owner.Owns(&primary));
    CHECK(!owner.BindOnce(nullptr));
    CHECK(owner.BindOnce(&primary));
    CHECK(owner.Bound());
    CHECK(owner.Owns(&primary));
    CHECK(!owner.Owns(&secondary));
    CHECK(!owner.BindOnce(&secondary));
    CHECK(!owner.BindOnce(&primary));
    CHECK(!owner.Owns(&recreated));
    CHECK(!owner.Owns(nullptr));
    return 0;
}
