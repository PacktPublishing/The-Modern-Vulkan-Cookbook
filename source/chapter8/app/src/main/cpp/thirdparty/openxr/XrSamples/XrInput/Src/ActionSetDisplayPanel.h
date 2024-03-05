#include <openxr/openxr.h>

#include "XrApp.h"
#include "Input/TinyUI.h"
#include "GUI/VRMenuObject.h"

using OVRFW::VRMenuObject;
class ActionSetDisplayPanel {
   public:
    ActionSetDisplayPanel(
        std::string title,
        XrSession session,
        XrInstance instance,
        OVRFW::TinyUI* ui,
        OVR::Vector3f topLeftLocation);
    void AddBoolAction(XrAction action, const char* actionName);
    void AddFloatAction(XrAction action, const char* actionName);
    void AddVec2Action(XrAction action, const char* actionName);
    void AddPoseAction(XrAction action, const char* actionName);

    void Update();

   private:
    VRMenuObject* CreateActionLabel(const char* actionName);
    std::string ListBoundSources(XrAction action);
    OVR::Vector3f GetNextLabelLocation();
    OVR::Vector3f GetNextStateLabelLocation();

    // OVRFW::VRMenuObject* backgroundPane_{};
    std::vector<std::pair<XrAction, VRMenuObject*>> boolActions_{};
    std::vector<std::pair<XrAction, VRMenuObject*>> floatActions_{};
    std::vector<std::pair<XrAction, VRMenuObject*>> vec2Actions_{};
    std::vector<std::pair<XrAction, VRMenuObject*>> poseActions_{};
    XrSession Session;
    XrInstance Instance;
    OVRFW::TinyUI* ui_;

    OVR::Vector3f topLeftLocation_;
    int elements_{0};
    static constexpr float kHeaderHeight_{0.15};
    static constexpr float kElementGap_{0.65};

    static constexpr float widthPx_{600};
    static constexpr float heightPx_{500};
    static constexpr float width_{widthPx_ * VRMenuObject::DEFAULT_TEXEL_SCALE};
    static constexpr float height_{heightPx_ * VRMenuObject::DEFAULT_TEXEL_SCALE};
};
