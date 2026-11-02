#include "../src/ffb/binding_contract.hpp"
#include <cstdio>
#include <string_view>
using namespace m2vr::ffbui;
#define TEST(expr) do{if(!(expr)){std::fprintf(stderr,"FFB binding metadata failed line %d: %s\n",__LINE__,#expr);return 1;}}while(0)
int main(){
    TEST(kBindings.size()==23);
    TEST(UniqueKeys());
    TEST(kBindings[0].key==L"InputDeviceWheelSteeringAxis");
    TEST(kBindings[1].key==L"InputDeviceWheelAcclAxis");
    TEST(kBindings[2].key==L"InputDeviceWheelBrakeAxis");
    TEST(kBindings[7].key==L"StartButton");
    TEST(kBindings[8].key==L"ShiftUpButton");
    TEST(kBindings[9].key==L"ShiftDownButton");
    TEST(kBindings[18].key==L"Coin1Button");
    for(const auto& b:kBindings) {
        TEST(Valid(b,b.initial));
        TEST(Valid(b,b.min));
        TEST(Valid(b,b.max));
        TEST(!Valid(b,b.min-1));
        TEST(!Valid(b,b.max+1));
    }
    TEST(!Valid(kBindings[0],32));   // impossible wheel axis
    TEST(!Valid(kBindings[3],2));    // reverse is boolean
    TEST(!Valid(kBindings[8],128));  // impossible joystick button
    return 0;
}
