#include "ActionSetDisplayPanel.h"

#include <openxr/openxr.h>
#include <sstream>
#include "Render/BitmapFont.h"

ActionSetDisplayPanel::ActionSetDisplayPanel(
    std::string title,
    XrSession session,
    XrInstance instance,
    OVRFW::TinyUI* ui,
    OVR::Vector3f topLeftLocation)
    : Session{session}, Instance{instance}, ui_{ui}, topLeftLocation_{topLeftLocation} {
    ui_->AddLabel(
        title, GetNextLabelLocation() + OVR::Vector3f{0, kHeaderHeight_, 0.00}, {widthPx_, 45.0f});
}

void ActionSetDisplayPanel::AddBoolAction(XrAction action, const char* actionName) {
    auto actionStateLabel = CreateActionLabel(actionName);
    boolActions_.push_back({action, actionStateLabel});
}

void ActionSetDisplayPanel::AddFloatAction(XrAction action, const char* actionName) {
    auto actionStateLabel = CreateActionLabel(actionName);
    floatActions_.push_back({action, actionStateLabel});
}

void ActionSetDisplayPanel::AddVec2Action(XrAction action, const char* actionName) {
    auto actionStateLabel = CreateActionLabel(actionName);
    vec2Actions_.push_back({action, actionStateLabel});
}

void ActionSetDisplayPanel::AddPoseAction(XrAction action, const char* actionName) {
    auto actionStateLabel = CreateActionLabel(actionName);
    poseActions_.push_back({action, actionStateLabel});
}

VRMenuObject* ActionSetDisplayPanel::CreateActionLabel(const char* actionName) {
    auto label = ui_->AddLabel(actionName, GetNextLabelLocation(), {widthPx_, 45.0f});
    auto stateLabel = ui_->AddLabel("state", GetNextStateLabelLocation(), {widthPx_, 250.0f});

    OVRFW::VRMenuFontParms fontParams{};
    fontParams.Scale = 0.5f;
    fontParams.AlignHoriz = OVRFW::HORIZONTAL_LEFT;
    label->SetFontParms(fontParams);
    label->SetTextLocalPosition({-0.45f * width_, 0, 0});
    stateLabel->SetFontParms(fontParams);
    stateLabel->SetTextLocalPosition({-0.45f * width_, 0, 0});

    label->SetColor({0.2, 0.2, 0.2, 1.0});
    elements_++;
    return stateLabel;
}

OVR::Vector3f ActionSetDisplayPanel::GetNextLabelLocation() {
    return topLeftLocation_ +
        OVR::Vector3f{width_ * 0.5f, -elements_ * kElementGap_ - kHeaderHeight_, 0.01};
}

OVR::Vector3f ActionSetDisplayPanel::GetNextStateLabelLocation() {
    return GetNextLabelLocation() + OVR::Vector3f{0.0, -kElementGap_ * 0.5f, 0.0};
}

void ActionSetDisplayPanel::Update() {
    for (auto& pair : boolActions_) {
        XrAction action = pair.first;
        VRMenuObject* label = pair.second;
        std::string bindingText = ListBoundSources(action);

        XrActionStateGetInfo getInfo{XR_TYPE_ACTION_STATE_GET_INFO};
        getInfo.action = action;
        getInfo.subactionPath = XR_NULL_PATH;
        XrActionStateBoolean state{XR_TYPE_ACTION_STATE_BOOLEAN};
        OXR(xrGetActionStateBoolean(Session, &getInfo, &state));

        label->SetText(
            "currentState: %s | changedSinceLastSync: %s\n"
            "isActive: %s     | lastChangeTime: %ldms\n" +
                bindingText,
            state.currentState ? "True " : "False",
            state.changedSinceLastSync ? "True " : "False",
            state.isActive ? "True " : "False",
            state.lastChangeTime / (1000 * 1000)); // convert from ns to ms
        label->SetSelected(state.currentState);
    }
    for (auto& pair : floatActions_) {
        XrAction action = pair.first;
        VRMenuObject* label = pair.second;
        std::string bindingText = ListBoundSources(action);

        XrActionStateGetInfo getInfo{XR_TYPE_ACTION_STATE_GET_INFO};
        getInfo.action = action;
        getInfo.subactionPath = XR_NULL_PATH;
        XrActionStateFloat state{XR_TYPE_ACTION_STATE_FLOAT};
        OXR(xrGetActionStateFloat(Session, &getInfo, &state));

        label->SetText(
            "currentState: %0.3f | changedSinceLastSync: %s\n"
            "isActive: %s     | lastChangeTime: %ldms\n" +
                bindingText,
            state.currentState,
            state.changedSinceLastSync ? "True " : "False",
            state.isActive ? "True " : "False",
            state.lastChangeTime / (1000 * 1000)); // convert from ns to ms
    }

    for (auto& pair : vec2Actions_) {
        XrAction action = pair.first;
        VRMenuObject* label = pair.second;
        std::string bindingText = ListBoundSources(action);

        XrActionStateGetInfo getInfo{XR_TYPE_ACTION_STATE_GET_INFO};
        getInfo.action = action;
        getInfo.subactionPath = XR_NULL_PATH;
        XrActionStateVector2f state{XR_TYPE_ACTION_STATE_VECTOR2F};
        OXR(xrGetActionStateVector2f(Session, &getInfo, &state));

        label->SetText(
            "currentState: (%0.3f, %0.3f) | changedSinceLastSync: %s\n"
            "isActive: %s     | lastChangeTime: %ldms\n" +
                bindingText,
            state.currentState.x,
            state.currentState.y,
            state.changedSinceLastSync ? "True " : "False",
            state.isActive ? "True " : "False",
            state.lastChangeTime / (1000 * 1000)); // convert from ns to ms
    }

    for (auto& pair : poseActions_) {
        XrAction action = pair.first;
        VRMenuObject* label = pair.second;
        std::string bindingText = ListBoundSources(action);

        XrActionStateGetInfo getInfo{XR_TYPE_ACTION_STATE_GET_INFO};
        getInfo.action = action;
        getInfo.subactionPath = XR_NULL_PATH;
        XrActionStatePose state{XR_TYPE_ACTION_STATE_POSE};
        OXR(xrGetActionStatePose(Session, &getInfo, &state));

        label->SetText("isActive: %s\n" + bindingText, state.isActive ? "True " : "False");

        // TODO: Add calls to xrLocateSpace to get the actual data
    }
}

std::string ActionSetDisplayPanel::ListBoundSources(XrAction action) {
    XrBoundSourcesForActionEnumerateInfo enumerateInfo{
        XR_TYPE_BOUND_SOURCES_FOR_ACTION_ENUMERATE_INFO};
    enumerateInfo.action = action;
    uint32_t sourcesCount = 0;

    std::stringstream bindingText;

    OXR(xrEnumerateBoundSourcesForAction(Session, &enumerateInfo, 0, &sourcesCount, nullptr));
    std::vector<XrPath> boundSources(sourcesCount);
    OXR(xrEnumerateBoundSourcesForAction(
        Session, &enumerateInfo, boundSources.size(), &sourcesCount, boundSources.data()));

    for (XrPath sourcePath : boundSources) {
        uint32_t pathLength;
        // OXR(xrPathToString(Instance, sourcePath, 0, &pathLength, nullptr));
        std::vector<char> pathString(XR_MAX_PATH_LENGTH);
        OXR(xrPathToString(
            Instance, sourcePath, pathString.size(), &pathLength, pathString.data()));

        XrInputSourceLocalizedNameGetInfo sni{XR_TYPE_INPUT_SOURCE_LOCALIZED_NAME_GET_INFO};
        sni.sourcePath = sourcePath;
        sni.whichComponents = XR_INPUT_SOURCE_LOCALIZED_NAME_USER_PATH_BIT |
            XR_INPUT_SOURCE_LOCALIZED_NAME_COMPONENT_BIT |
            XR_INPUT_SOURCE_LOCALIZED_NAME_INTERACTION_PROFILE_BIT;
        uint32_t sourceNameLength;
        OXR(xrGetInputSourceLocalizedName(Session, &sni, 0, &sourceNameLength, nullptr));
        std::vector<char> sourceName(sourceNameLength);
        OXR(xrGetInputSourceLocalizedName(
            Session, &sni, sourceName.size(), &sourceNameLength, sourceName.data()));

        bindingText << "\nBinding: " << pathString.data() << "\n(" << sourceName.data() << ")\n";
    }
    return std::string(bindingText.str());
}
