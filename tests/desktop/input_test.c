/* Production action polling and pose serialization, with deterministic inputs. */
#include "VrInput.h"
#include "VrMath.h"
#include "xr_menu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); exit(1); } } while (0)
#define HANDLE(n) ((XrAction)(uintptr_t)(n))
extern XrAction handPoseLeftAction, handPoseRightAction, indexLeftAction, indexRightAction;
extern XrAction gripLeftAction, gripRightAction, moveOnLeftJoystickAction, moveOnRightJoystickAction;
extern XrPath leftHandPath, rightHandPath;
extern XrSpace leftControllerAimSpace, rightControllerAimSpace;
extern float Q_atof(const char*);
static engine_t engine;
static bool active = true, poseValid = true;
/* These entry points must never be reached by the polling-only fixture. */
#define UNUSED_XR(name, args) XRAPI_ATTR XrResult XRAPI_CALL name args { CHECK(!"unexpected OpenXR call: " #name); return XR_ERROR_FUNCTION_UNSUPPORTED; }
UNUSED_XR(xrApplyHapticFeedback, (XrSession s, const XrHapticActionInfo* i, const XrHapticBaseHeader* h))
UNUSED_XR(xrStopHapticFeedback, (XrSession s, const XrHapticActionInfo* i))
UNUSED_XR(xrStringToPath, (XrInstance i, const char* text, XrPath* p))
UNUSED_XR(xrCreateActionSet, (XrInstance i, const XrActionSetCreateInfo* c, XrActionSet* s))
UNUSED_XR(xrCreateAction, (XrActionSet s, const XrActionCreateInfo* i, XrAction* a))
UNUSED_XR(xrAttachSessionActionSets, (XrSession s, const XrSessionActionSetsAttachInfo* i))
UNUSED_XR(xrEnumerateBoundSourcesForAction, (XrSession s, const XrBoundSourcesForActionEnumerateInfo* i, uint32_t cap, uint32_t* count, XrPath* p))
UNUSED_XR(xrGetInputSourceLocalizedName, (XrSession s, const XrInputSourceLocalizedNameGetInfo* i, uint32_t cap, uint32_t* count, char* text))
UNUSED_XR(xrPathToString, (XrInstance i, XrPath p, uint32_t cap, uint32_t* count, char* text))
UNUSED_XR(xrSuggestInteractionProfileBindings, (XrInstance i, const XrInteractionProfileSuggestedBinding* b))
UNUSED_XR(xrDestroySpace, (XrSpace s))
UNUSED_XR(xrDestroyActionSet, (XrActionSet s))
engine_t* VR_GetEngine(void) { return &engine; }
XrResult VR_CheckResult(XrResult r, const char* op) { CHECK(XR_SUCCEEDED(r)); return r; }
XRAPI_ATTR XrResult XRAPI_CALL xrSyncActions(XrSession s, const XrActionsSyncInfo* i) { return XR_SUCCESS; }
XRAPI_ATTR XrResult XRAPI_CALL xrCreateActionSpace(XrSession s, const XrActionSpaceCreateInfo* i, XrSpace* p) { *p = (XrSpace)i->action; return XR_SUCCESS; }
XRAPI_ATTR XrResult XRAPI_CALL xrGetActionStatePose(XrSession s, const XrActionStateGetInfo* i, XrActionStatePose* state) {
    CHECK(i->subactionPath == (i->action == handPoseLeftAction ? leftHandPath : rightHandPath));
    state->isActive = active; return XR_SUCCESS;
}
XRAPI_ATTR XrResult XRAPI_CALL xrGetActionStateBoolean(XrSession s, const XrActionStateGetInfo* i, XrActionStateBoolean* state) {
    state->isActive = active;
    state->currentState = i->action == indexRightAction; return XR_SUCCESS;
}
XRAPI_ATTR XrResult XRAPI_CALL xrGetActionStateFloat(XrSession s, const XrActionStateGetInfo* i, XrActionStateFloat* state) {
    state->isActive = active; state->currentState = i->action == gripLeftAction ? 0.9f : 0.1f; return XR_SUCCESS;
}
XRAPI_ATTR XrResult XRAPI_CALL xrGetActionStateVector2f(XrSession s, const XrActionStateGetInfo* i, XrActionStateVector2f* state) {
    state->isActive = active;
    state->currentState = i->action == moveOnLeftJoystickAction ? (XrVector2f){0.2f, 0.8f} : (XrVector2f){-0.9f, 0.1f};
    return XR_SUCCESS;
}
XRAPI_ATTR XrResult XRAPI_CALL xrLocateSpace(XrSpace s, XrSpace base, XrTime t, XrSpaceLocation* loc) {
    CHECK(t == 123);
    loc->locationFlags = poseValid ? XR_SPACE_LOCATION_POSITION_VALID_BIT | XR_SPACE_LOCATION_ORIENTATION_VALID_BIT : 0;
    loc->pose = XrPosef_Identity(); loc->pose.position.x = 0.25f; return XR_SUCCESS;
}
int main(void) {
	/* A trigger that opened a menu must be released before it can confirm. */
	xr_menu_state nav={0};
	CHECK(!XRMenu_Update(&nav, 0, 0, 0, 1, 0, 0));
	CHECK(!XRMenu_Update(&nav, 1, 0, 0, 1, 0, 0.01));
	CHECK(!XRMenu_Update(&nav, 1, 0, 0, 0, 0, 0.02));
	CHECK(XRMenu_Update(&nav, 1, 0, 0, 1, 0, 0.03)==XR_MENU_CONFIRM);
	CHECK(!XRMenu_Update(&nav, 1, 0, 0, 1, 0, 0.04));
	CHECK(XRMenu_Update(&nav, 1, 0, -1, 0, 0, 0.05)==XR_MENU_DOWN);
	CHECK(!XRMenu_Update(&nav, 1, 0, -1, 0, 0, 0.20));
	CHECK(XRMenu_Update(&nav, 1, 0, -1, 0, 0, 0.46)==XR_MENU_DOWN);
	CHECK(XRMenu_Update(&nav, 1, 1, 0, 0, 0, 0.47)==XR_MENU_RIGHT);
	CHECK(XRMenu_Update(&nav, 1, 0, 0, 0, 1, 0.48)==XR_MENU_BACK);
	CHECK(!XRMenu_Update(&nav, 0, 0, 1, 1, 1, 0.49));
	unsigned slots=(1u<<0)|(1u<<4)|(1u<<9);
	CHECK(XRMenu_NextSlot(slots, 0, 1)==1);
	CHECK(XRMenu_NextSlot(slots, 1, 1)==5);
	CHECK(XRMenu_NextSlot(slots, 5, 1)==10);
	CHECK(XRMenu_NextSlot(slots, 10, 1)==1);
	CHECK(XRMenu_NextSlot(slots, 1, -1)==10);
	CHECK(XRMenu_NextSlot(0, 1, 1)==0);
	xr_menu_rect panels[]={{0,.1f,.2f,.2f},{0,.2f,.2f,.3f},
		{.2f,.1f,.4f,.2f},{.2f,.2f,.3f,.3f},{.3f,.2f,.4f,.3f},{0,.3f,.2f,.4f}};
	CHECK(XRMenu_Neighbor(panels,6,0,XR_MENU_DOWN)==1);
	CHECK(XRMenu_Neighbor(panels,6,0,XR_MENU_UP)==5);
	CHECK(XRMenu_Neighbor(panels,6,5,XR_MENU_DOWN)==0);
	CHECK(XRMenu_Neighbor(panels,6,1,XR_MENU_RIGHT)==3);
	CHECK(XRMenu_Neighbor(panels,6,3,XR_MENU_RIGHT)==4);
	CHECK(XRMenu_Neighbor(panels,6,4,XR_MENU_LEFT)==3);
	CHECK(XRMenu_Neighbor(panels,6,0,XR_MENU_LEFT)==0);
	CHECK(XRMenu_Neighbor(panels,0,-1,XR_MENU_DOWN)==-1);
    /* Tiny offsets previously became metres after %g -> Q_atof round trips. */
    const float values[] = {-0.0325f, 0.0325f, 7.21775e-9f, -8.96398e-9f, 1e-12f, 0, -0.9425f, 179.999f};
    for (unsigned int i = 0; i < sizeof(values)/sizeof(values[0]); ++i) {
        char text[32]; VR_FormatPoseValue(text, values[i]);
        CHECK(!strchr(text, 'e') && !strchr(text, 'E'));
        CHECK(fabsf(Q_atof(text) - values[i]) < 1e-7f);
    }
    CHECK(Q_atof("7.21775e-9") > 7); /* The actual engine parser's constraint. */
    engine.appState.Focused = true; engine.predictedDisplayTime = 123;
    handPoseLeftAction = HANDLE(1); handPoseRightAction = HANDLE(2);
    indexLeftAction = HANDLE(3); indexRightAction = HANDLE(4);
    gripLeftAction = HANDLE(5); gripRightAction = HANDLE(6);
    moveOnLeftJoystickAction = HANDLE(7); moveOnRightJoystickAction = HANDLE(8);
    leftHandPath = 10; rightHandPath = 11;
    IN_VRInputFrame(&engine);
    CHECK(IN_VRIsActive(0) && IN_VRIsActive(1));
    CHECK(IN_VRGetButtonState(0) == (ovrButton_GripTrigger | ovrButton_Up));
    CHECK(IN_VRGetButtonState(1) == (ovrButton_Trigger | ovrButton_Left));
    CHECK(fabsf(IN_VRGetJoystickState(0).y - 0.8f) < 1e-6f);
    CHECK(IN_VRGetPose(1).position.x == 0.25f);
    IN_VRSetSwapThumbsticks(true); IN_VRInputFrame(&engine);
    CHECK(fabsf(IN_VRGetJoystickState(0).x + 0.9f) < 1e-6f);
    poseValid = false;
    CHECK(IN_VRGetPose(1).orientation.w == 1 && !IN_VRIsActive(1));
    active = false; IN_VRInputFrame(&engine);
    CHECK(!IN_VRIsActive(0) && !IN_VRIsActive(1));
    CHECK(!IN_VRGetButtonState(0) && !IN_VRGetButtonState(1));
    CHECK(IN_VRGetJoystickState(0).x == 0 && IN_VRGetJoystickState(1).y == 0);
    active = true; engine.appState.Focused = false; IN_VRInputFrame(&engine);
    CHECK(!IN_VRGetButtonState(0) && !IN_VRGetButtonState(1));
    puts("PASS: engine pose serialization, physical hand mapping, grip/trigger, stick swap, tracking/focus loss; menu navigation, repeat, trigger latch and sparse numeric slots");
}
