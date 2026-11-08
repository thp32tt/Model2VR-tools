#include "emulator_menu_controller.hpp"
#include "../src/common/model2_menu_controller_ray.hpp"
#include "../src/common/stereo_frame_ipc.h"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <vector>
namespace m2vr::host {
namespace {
// Interaction-profile suggestions are optional. Missing controller support
// must NEVER prevent desktop-mouse input or the ordinary OpenXR image.
bool Path(XrInstance instance,const char* text,XrPath& result){
    result=XR_NULL_PATH;
    return XR_SUCCEEDED(xrStringToPath(instance,text,&result)) &&
           result!=XR_NULL_PATH;
}
bool Suggest(XrInstance instance,XrAction aim,XrAction select,
             const char* profile,const char* left_aim,
             const char* right_aim,const char* left_click,
             const char* right_click){
    XrPath profile_path=XR_NULL_PATH,paths[4]{};
    if(!Path(instance,profile,profile_path) ||
       !Path(instance,left_aim,paths[0]) ||
       !Path(instance,right_aim,paths[1]) ||
       !Path(instance,left_click,paths[2]) ||
       !Path(instance,right_click,paths[3]))return false;
    const XrActionSuggestedBinding bindings[4]{
        {aim,paths[0]},{aim,paths[1]},
        {select,paths[2]},{select,paths[3]}
    };
    XrInteractionProfileSuggestedBinding suggested{
        XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
    suggested.interactionProfile=profile_path;
    suggested.suggestedBindings=bindings;
    suggested.countSuggestedBindings=4;
    return XR_SUCCEEDED(xrSuggestInteractionProfileBindings(instance,&suggested));
}
} // namespace

bool EmulatorMenuController::Initialize(XrInstance instance,XrSession session){
    Shutdown();
    if(GetPrivateProfileIntW(L"probe",L"quest_menu_controller_enabled",
                             1,L".\\model2vr_probe.ini")==0){
        std::cout<<"Model2VR XR emulator menu controller disabled by INI\n";
        return false;
    }
    instance_=instance;
    XrActionSetCreateInfo set_info{XR_TYPE_ACTION_SET_CREATE_INFO};
    std::strncpy(set_info.actionSetName,"emulator_menu",XR_MAX_ACTION_SET_NAME_SIZE-1);
    std::strncpy(set_info.localizedActionSetName,"Emulator menu",XR_MAX_LOCALIZED_ACTION_SET_NAME_SIZE-1);
    if(XR_FAILED(xrCreateActionSet(instance,&set_info,&set_))){Shutdown();return false;}
    if(!Path(instance,"/user/hand/left",hand_paths_[0]) ||
       !Path(instance,"/user/hand/right",hand_paths_[1])){
        Shutdown();return false;
    }
    XrActionCreateInfo action{XR_TYPE_ACTION_CREATE_INFO};
    action.countSubactionPaths=2;
    action.subactionPaths=hand_paths_.data();
    action.actionType=XR_ACTION_TYPE_POSE_INPUT;
    std::strncpy(action.actionName,"menu_aim",XR_MAX_ACTION_NAME_SIZE-1);
    std::strncpy(action.localizedActionName,"Menu aim",XR_MAX_LOCALIZED_ACTION_NAME_SIZE-1);
    if(XR_FAILED(xrCreateAction(set_,&action,&aim_))){Shutdown();return false;}
    action={XR_TYPE_ACTION_CREATE_INFO};
    action.countSubactionPaths=2;
    action.subactionPaths=hand_paths_.data();
    action.actionType=XR_ACTION_TYPE_BOOLEAN_INPUT;
    std::strncpy(action.actionName,"menu_select",XR_MAX_ACTION_NAME_SIZE-1);
    std::strncpy(action.localizedActionName,"Menu select",XR_MAX_LOCALIZED_ACTION_NAME_SIZE-1);
    if(XR_FAILED(xrCreateAction(set_,&action,&select_))){Shutdown();return false;}
    for(std::size_t i=0;i<2;++i){
        XrActionSpaceCreateInfo info{XR_TYPE_ACTION_SPACE_CREATE_INFO};
        info.action=aim_;
        info.subactionPath=hand_paths_[i];
        info.poseInActionSpace.orientation.w=1.f;
        if(XR_FAILED(xrCreateActionSpace(session,&info,&aim_spaces_[i]))){
            Shutdown();return false;
        }
    }
    // Oculus/Meta Touch (Quest 3, VDXR) and KHR simple-controller fallback.
    // A given runtime is allowed to reject unsupported profiles.
    const bool touch_trigger=Suggest(instance,aim_,select_,
        "/interaction_profiles/oculus/touch_controller",
        "/user/hand/left/input/aim/pose",
        "/user/hand/right/input/aim/pose",
        "/user/hand/left/input/trigger/value",
        "/user/hand/right/input/trigger/value");
    // Some runtimes reject an analog trigger-to-boolean binding. Preserve
    // usable HMD menu selection with Touch X(left)/A(right) click inputs.
    // Never replace a successfully installed trigger mapping.
    const bool touch_button_fallback=!touch_trigger &&
        Suggest(instance,aim_,select_,
        "/interaction_profiles/oculus/touch_controller",
        "/user/hand/left/input/aim/pose",
        "/user/hand/right/input/aim/pose",
        "/user/hand/left/input/x/click",
        "/user/hand/right/input/a/click");
    const bool touch=touch_trigger || touch_button_fallback;
    const bool simple=Suggest(instance,aim_,select_,
        "/interaction_profiles/khr/simple_controller",
        "/user/hand/left/input/aim/pose",
        "/user/hand/right/input/aim/pose",
        "/user/hand/left/input/select/click",
        "/user/hand/right/input/select/click");
    if(!touch && !simple){Shutdown();return false;}
    XrSessionActionSetsAttachInfo attach{XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};
    attach.countActionSets=1;attach.actionSets=&set_;
    if(XR_FAILED(xrAttachSessionActionSets(session,&attach))){Shutdown();return false;}
    active_=true;
    std::cout<<"Model2VR XR desktop menu ray: ENABLED touch="
             <<touch<<" touch_trigger="<<touch_trigger
             <<" touch_button_fallback="<<touch_button_fallback
             <<" simple="<<simple
             <<" (only foreground emulator HWND, never gameplay)\n";
    return true;
}
void EmulatorMenuController::Shutdown() noexcept {
    active_=false;
    ray_on_menu_=false;
    for(auto& space:aim_spaces_){
        if(space!=XR_NULL_HANDLE)xrDestroySpace(space);
        space=XR_NULL_HANDLE;
    }
    if(select_!=XR_NULL_HANDLE)xrDestroyAction(select_);
    if(aim_!=XR_NULL_HANDLE)xrDestroyAction(aim_);
    if(set_!=XR_NULL_HANDLE)xrDestroyActionSet(set_);
    select_=aim_=XR_NULL_HANDLE;
    set_=XR_NULL_HANDLE;
    instance_=XR_NULL_HANDLE;
    hand_paths_={XR_NULL_PATH,XR_NULL_PATH};
}
void EmulatorMenuController::Poll(
    XrSession session,XrSpace local_space,XrTime predicted_time,
    bool focused,bool menu_visible,std::uint32_t pid,
    const monoquad::Placement& quad,EmulatorWindowCapture& capture,
    std::uint64_t submitted_menu_capture_serial){
    capture.SetControllerReticle(false,0,0);
    ray_on_menu_=false;
    if(!active_ || !focused)return;
    const XrActiveActionSet set{set_,XR_NULL_PATH};
    XrActionsSyncInfo sync{XR_TYPE_ACTIONS_SYNC_INFO};
    sync.countActiveActionSets=1;
    sync.activeActionSets=&set;
    if(XR_FAILED(xrSyncActions(session,&sync)))return;
    if(!menu_visible || !capture.Ready() || !pid || !capture.Window())return;
    const auto desktop=capture.SourceBounds();
    if(!desktop.Valid())return;
    const auto width=desktop.width;
    const auto height=desktop.height;
    // The visual reticle follows the predicted display pose even on a
    // trigger frame. Separately, OS activation uses the ray sampled at the
    // real action event time, never the later display-time hover location.
    std::array<menuray::Hit,2> hover_hits{},press_hits{};
    const auto locate_hit=[&](int hand,XrTime at)->menuray::Hit {
        XrSpaceLocation location{XR_TYPE_SPACE_LOCATION};
        if(XR_FAILED(xrLocateSpace(aim_spaces_[hand],local_space,
                                    at,&location)))return {};
        constexpr XrSpaceLocationFlags tracked=
            XR_SPACE_LOCATION_POSITION_VALID_BIT |
            XR_SPACE_LOCATION_ORIENTATION_VALID_BIT |
            XR_SPACE_LOCATION_POSITION_TRACKED_BIT |
            XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT;
        if((location.locationFlags&tracked)!=tracked)return {};
        const auto& p=location.pose;
        const menuray::Ray ray{
            {p.position.x,p.position.y,p.position.z},
            {p.orientation.x,p.orientation.y,p.orientation.z,p.orientation.w}
        };
        return menuray::Intersect(ray,quad,
            static_cast<std::uint32_t>(width),
            static_cast<std::uint32_t>(height),
            transport::kWidth,transport::kHeight);
    };
    for(int hand=1;hand>=0;--hand){
        XrActionStateGetInfo state_get{XR_TYPE_ACTION_STATE_GET_INFO};
        state_get.action=aim_;
        state_get.subactionPath=hand_paths_[hand];
        XrActionStatePose pose{XR_TYPE_ACTION_STATE_POSE};
        if(XR_FAILED(xrGetActionStatePose(session,&state_get,&pose)) ||
           !pose.isActive)continue;
        state_get.action=select_;
        XrActionStateBoolean button{XR_TYPE_ACTION_STATE_BOOLEAN};
        const bool press_edge=
            XR_SUCCEEDED(xrGetActionStateBoolean(session,&state_get,&button)) &&
            button.isActive && button.currentState && button.changedSinceLastSync;
        const bool press_fresh=press_edge && menuray::FreshPressTime(
            static_cast<std::int64_t>(predicted_time),
            static_cast<std::int64_t>(button.lastChangeTime));
        hover_hits[hand]=locate_hit(hand,predicted_time);
        // Invalid/untracked historical pose MUST NOT fall back to the
        // visually current ray for a click. Equal timestamps may reuse it.
        if(press_fresh){
            press_hits[hand]=button.lastChangeTime==predicted_time
                ?hover_hits[hand]:locate_hit(hand,button.lastChangeTime);
        }
    }
    const auto targets=menuray::SelectMenuTargets(
        hover_hits[0].valid,hover_hits[1].valid,
        press_hits[0].valid,press_hits[1].valid);
    if(targets.hover_hand>=0){
        ray_on_menu_=true;
        const auto& hover=hover_hits[targets.hover_hand];
        capture.SetControllerReticle(true,hover.texture_x,hover.texture_y);
    }
    if(targets.click_hand>=0){
        const int hand=targets.click_hand;
        const auto& hit=press_hits[hand];
        // Poll runs BEFORE this frame's D3D11 upload and xrEndFrame. Its
        // current capture might therefore be newer than the visible quad.
        const bool submitted=desktopmenu::PresentedCaptureMatches(
            capture.Serial(),submitted_menu_capture_serial);
        const bool sent=submitted &&
            capture.DeliverControllerClick(pid,hit.window_x,hit.window_y);
        std::cout<<"Model2VR XR emulator menu click: "
                 <<(sent?"SENT":submitted?"BLOCKED_FOREGROUND_OR_INJECTION":
                         "BLOCKED_UNSUBMITTED_XR_MENU_SNAPSHOT")
                 <<" hand="<<(hand?"right":"left")
                 <<" desktop_x="<<hit.window_x
                 <<" desktop_y="<<hit.window_y
                 <<" current_capture_serial="<<capture.Serial()
                 <<" submitted_capture_serial="<<submitted_menu_capture_serial
                 <<"\n";
    }
}
} // namespace m2vr::host
