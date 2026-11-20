/* Runs the production frame/layer code against a deterministic OpenXR runtime.
 * No headset, window, proprietary game data, or loader is required. */
#include "VrRenderer.h"
#include "VrInput.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); exit(1); } } while (0)
static engine_t engine;
static bool flags[VR_PLATFORM_MAX];
static bool renderFrame = true;
static bool validTracking = true;
static int begins, ends, acquires, inputs, destroys, expectedLayer;
static XrFovf eyeFovs[2] = {{-0.9f, 0.7f, 0.8f, -0.6f}, {-0.7f, 0.9f, 0.75f, -0.65f}};

engine_t* VR_GetEngine(void) { return &engine; }
bool VR_GetPlatformFlag(enum VRPlatformFlag flag) { return flags[flag]; }
void VR_SetPlatformFLag(enum VRPlatformFlag flag, bool value) { flags[flag] = value; }
XrResult VR_CheckResult(XrResult result, const char* operation) { CHECK(XR_SUCCEEDED(result)); return result; }
int ovrApp_HandleXrEvents(ovrApp* app) { return 0; }
void IN_VRInputFrame(engine_t* e) { ++inputs; }
void ovrRenderer_Create(XrSession session, ovrRenderer* r, bool multiview, int w, int h, int samples) {
    CHECK(w == 1280 && h == 1024);
    for (int eye = 0; eye < 2; ++eye) {
        r->FrameBuffer[eye].Width = w; r->FrameBuffer[eye].Height = h;
        r->FrameBuffer[eye].ColorSwapChain.Handle = (XrSwapchain)(uintptr_t)(eye + 1);
        r->FrameBuffer[eye].TextureSwapChainLength = 10;
    }
}
void ovrRenderer_Destroy(ovrRenderer* r) { ++destroys; }
void ovrFramebuffer_Acquire(ovrFramebuffer* fb) { CHECK(engine.frameStarted); fb->TextureSwapChainIndex = 7; ++acquires; }
void ovrFramebuffer_Resolve(ovrFramebuffer* fb) {}
void ovrFramebuffer_Release(ovrFramebuffer* fb) {}
void ovrFramebuffer_SetCurrent(ovrFramebuffer* fb) {}
void ovrFramebuffer_SetNone(void) {}
void ovrFramebuffer_Mirror(ovrFramebuffer* fb) {}
void ovrRenderer_MouseCursor(ovrRenderer* r, int x, int y, int sx, int sy) {}

XRAPI_ATTR XrResult XRAPI_CALL xrEnumerateViewConfigurations(XrInstance i, XrSystemId s, uint32_t cap, uint32_t* count, XrViewConfigurationType* types) {
    *count = 1; if (cap) types[0] = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO; return XR_SUCCESS;
}
XRAPI_ATTR XrResult XRAPI_CALL xrGetViewConfigurationProperties(XrInstance i, XrSystemId s, XrViewConfigurationType type, XrViewConfigurationProperties* p) {
    p->viewConfigurationType = type; p->fovMutable = false; return XR_SUCCESS;
}
XRAPI_ATTR XrResult XRAPI_CALL xrEnumerateViewConfigurationViews(XrInstance i, XrSystemId s, XrViewConfigurationType t, uint32_t cap, uint32_t* count, XrViewConfigurationView* views) {
    *count = 2;
    if (cap) for (int n = 0; n < 2; ++n) {
        views[n].recommendedImageRectWidth = 1280; views[n].recommendedImageRectHeight = 1024;
        views[n].maxImageRectWidth = 4096; views[n].maxImageRectHeight = 4096;
    }
    return XR_SUCCESS;
}
XRAPI_ATTR XrResult XRAPI_CALL xrEnumerateReferenceSpaces(XrSession s, uint32_t cap, uint32_t* count, XrReferenceSpaceType* spaces) {
    *count = 1; if (cap) spaces[0] = XR_REFERENCE_SPACE_TYPE_LOCAL; return XR_SUCCESS;
}
XRAPI_ATTR XrResult XRAPI_CALL xrCreateReferenceSpace(XrSession s, const XrReferenceSpaceCreateInfo* info, XrSpace* space) { *space = (XrSpace)(uintptr_t)3; return XR_SUCCESS; }
XRAPI_ATTR XrResult XRAPI_CALL xrDestroySpace(XrSpace s) { return XR_SUCCESS; }
XRAPI_ATTR XrResult XRAPI_CALL xrGetReferenceSpaceBoundsRect(XrSession s, XrReferenceSpaceType t, XrExtent2Df* b) { b->width = b->height = 2; return XR_SUCCESS; }
XRAPI_ATTR XrResult XRAPI_CALL xrLocateSpace(XrSpace s, XrSpace base, XrTime time, XrSpaceLocation* loc) {
    loc->pose = XrPosef_Identity(); loc->pose.position.y = 1.7f;
    loc->locationFlags = XR_SPACE_LOCATION_POSITION_VALID_BIT | XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
    return XR_SUCCESS;
}
XRAPI_ATTR XrResult XRAPI_CALL xrWaitFrame(XrSession s, const XrFrameWaitInfo* info, XrFrameState* state) {
    CHECK(!engine.frameStarted);
    state->predictedDisplayTime = 123456789; state->predictedDisplayPeriod = 11111111;
    state->shouldRender = renderFrame; return XR_SUCCESS;
}
XRAPI_ATTR XrResult XRAPI_CALL xrBeginFrame(XrSession s, const XrFrameBeginInfo* info) { ++begins; return XR_SUCCESS; }
XRAPI_ATTR XrResult XRAPI_CALL xrLocateViews(XrSession s, const XrViewLocateInfo* info, XrViewState* state, uint32_t cap, uint32_t* count, XrView* views) {
    CHECK(info->displayTime == 123456789); *count = 2;
    state->viewStateFlags = validTracking ? XR_VIEW_STATE_POSITION_VALID_BIT | XR_VIEW_STATE_ORIENTATION_VALID_BIT : 0;
    for (int n = 0; n < 2; ++n) {
        views[n].pose = XrPosef_Identity(); views[n].pose.position.x = n ? 0.04f : -0.04f;
        views[n].pose.position.y = 1.7f; views[n].fov = eyeFovs[n];
    }
    return XR_SUCCESS;
}
XRAPI_ATTR XrResult XRAPI_CALL xrEndFrame(XrSession s, const XrFrameEndInfo* info) {
    CHECK(info->displayTime == 123456789);
    CHECK(info->environmentBlendMode == XR_ENVIRONMENT_BLEND_MODE_OPAQUE);
    ++ends;
    if (!renderFrame || expectedLayer == 0) { CHECK(info->layerCount == 0); return XR_SUCCESS; }
    CHECK(info->layerCount == 1);
    CHECK(info->layers[0]->type == expectedLayer);
    if (expectedLayer == XR_TYPE_COMPOSITION_LAYER_PROJECTION) {
        const XrCompositionLayerProjection* layer = (const XrCompositionLayerProjection*)info->layers[0];
        CHECK(layer->viewCount == 2);
        for (int eye = 0; eye < 2; ++eye) {
            CHECK(memcmp(&layer->views[eye].fov, &eyeFovs[eye], sizeof(XrFovf)) == 0);
            CHECK(layer->views[eye].subImage.imageRect.extent.width == 1280);
            CHECK(layer->views[eye].subImage.imageRect.extent.height == 1024);
            CHECK(fabsf(layer->views[eye].pose.position.x - (eye ? 0.04f : -0.04f)) < 1e-5f);
        }
    } else if (expectedLayer == XR_TYPE_COMPOSITION_LAYER_QUAD) {
        const XrCompositionLayerQuad* q = (const XrCompositionLayerQuad*)info->layers[0];
        CHECK(q->size.width > 0 && q->size.height > 0);
        CHECK(fabsf(q->size.width/q->size.height - 1.25f) < 1e-5f);
    }
    return XR_SUCCESS;
}
XRAPI_ATTR XrResult XRAPI_CALL xrGetInstanceProcAddr(XrInstance i, const char* name, PFN_xrVoidFunction* f) { *f = NULL; return XR_ERROR_FUNCTION_UNSUPPORTED; }

int main(void) {
    engine.appState.Instance = (XrInstance)(uintptr_t)1;
    engine.appState.Session = (XrSession)(uintptr_t)2;
    engine.appState.SessionActive = true;
    VR_SetConfigFloat(VR_CONFIG_CANVAS_ASPECT, 4.0f/3.0f);
    VR_InitRenderer(&engine, false);
    renderFrame = false;
    CHECK(!VR_InitFrame(&engine));
    CHECK(begins == 1 && ends == 1 && inputs == 0 && acquires == 0 && !engine.frameStarted);
    renderFrame = true;
    CHECK(VR_InitFrame(&engine));
    CHECK(VR_GetRefreshRate() == 90);
	XrPosef located = VR_GetView(0);
	VR_InitRenderer(&engine, false);
	CHECK(destroys == 0 && !VR_GetConfig(VR_CONFIG_VIEWPORT_VALID));
	XrPosef afterResize = VR_GetView(0);
	CHECK(memcmp(&located, &afterResize, sizeof(located)) == 0);
    CHECK(fabsf(VR_GetHead().position.x) < 1e-6f);
    VR_SetConfig(VR_CONFIG_MODE, VR_MODE_STEREO_6DOF);
    expectedLayer = XR_TYPE_COMPOSITION_LAYER_PROJECTION;
    for (int eye = 0; eye < 2; ++eye) { VR_BeginFrame(&engine, eye); VR_EndFrame(&engine, eye); }
    VR_FinishFrame(&engine);
    CHECK(begins == 2 && ends == 2 && acquires == 2 && inputs == 1 && !engine.frameStarted);
    CHECK(engine.appState.Renderer.FrameBuffer[0].TextureSwapChainIndex == 7);
    CHECK(VR_InitFrame(&engine));
    VR_SetConfig(VR_CONFIG_MODE, VR_MODE_MONO_SCREEN);
    VR_SetConfigFloat(VR_CONFIG_CANVAS_DISTANCE, 5);
    expectedLayer = XR_TYPE_COMPOSITION_LAYER_QUAD;
    for (int eye = 0; eye < 2; ++eye) { VR_BeginFrame(&engine, eye); VR_EndFrame(&engine, eye); }
    VR_FinishFrame(&engine);
    validTracking = false; expectedLayer = 0;
    CHECK(!VR_InitFrame(&engine));
    CHECK(begins == 4 && ends == 4 && acquires == 4 && inputs == 2 && !engine.frameStarted);
    VR_DestroyRenderer(&engine); VR_DestroyRenderer(&engine);
    CHECK(destroys == 1);
    puts("PASS: empty frames, one begin/end per frame, runtime eye FOV/pose, rectangular swapchains, quad fallback, image ownership, teardown");
    return 0;
}
