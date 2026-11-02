#pragma once
#include <array>
#include <cstdint>
#include <string_view>

namespace m2vr::ffbui {
struct Binding {
    std::wstring_view key;
    std::wstring_view label;
    int initial;
    int min;
    int max;
};
// Read from upstream Generic DD v2 Model2 input sources (Daytona/Rally/Indy).
// Values are SDL button/axis indices, NOT scancodes or FFB torque controls.
inline constexpr auto kBindings=std::to_array<Binding>({
    {L"InputDeviceWheelSteeringAxis",L"Steering axis",0,0,31},
    {L"InputDeviceWheelAcclAxis",L"Accelerator axis",0,0,31},
    {L"InputDeviceWheelBrakeAxis",L"Brake axis",0,0,31},
    {L"InputDeviceWheelReverseAxis",L"Reverse pedal axes",0,0,1},
    {L"InputDeviceCombinedPedals",L"Combined pedals",0,0,1},
    {L"SteeringDeadzone",L"Steering deadzone",0,0,100},
    {L"PedalDeadzone",L"Pedal deadzone",0,0,100},
    {L"StartButton",L"Start button",0,0,127},
    {L"ShiftUpButton",L"Shift up",0,0,127},
    {L"ShiftDownButton",L"Shift down",0,0,127},
    {L"Shift1Button",L"Gear 1",0,0,127},
    {L"Shift2Button",L"Gear 2",0,0,127},
    {L"Shift3Button",L"Gear 3",0,0,127},
    {L"Shift4Button",L"Gear 4",0,0,127},
    {L"VR1Button",L"View 1",0,0,127},
    {L"VR2Button",L"View 2 (where supported)",0,0,127},
    {L"VR3Button",L"View 3 (where supported)",0,0,127},
    {L"VR4Button",L"View 4 (where supported)",0,0,127},
    {L"Coin1Button",L"Coin 1",0,0,127},
    {L"Coin2Button",L"Coin 2",0,0,127},
    {L"TestButton",L"Test",0,0,127},
    {L"ServiceButton",L"Service",0,0,127},
    {L"ExitButton",L"Exit",0,0,127},
});
constexpr bool Valid(const Binding& b,int v) noexcept {
    return v>=b.min && v<=b.max;
}
constexpr bool UniqueKeys() noexcept {
    for(std::size_t i=0;i<kBindings.size();++i)
        for(std::size_t j=i+1;j<kBindings.size();++j)
            if(kBindings[i].key==kBindings[j].key)return false;
    return true;
}
static_assert(UniqueKeys());
static_assert(Valid(kBindings[0],0));
static_assert(!Valid(kBindings[0],32));
static_assert(!Valid(kBindings[7],128));
} // namespace m2vr::ffbui
