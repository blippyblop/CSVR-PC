/* Exercise production VR_Init against runtimes with different API ceilings. */
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <SDL_opengl.h>
#ifndef _WIN32
#include <EGL/egl.h>
#endif
#include "VrBase.h"
#include "VrDesktopGL.h"
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); exit(1); } } while (0)
static XrVersion runtimeVersion;
static unsigned creates, requirements;
static int requestedMajor, requestedMinor, attributes, attributeReads;
static const char* glVersion = "4.6 fixture";
static bool missingGetString;

/* PE linking retains other exported entry points; this fixture must not use them. */
void VR_DestroyRenderer(engine_t* engine) { CHECK(!"unexpected renderer teardown"); }
void IN_VRDestroy(engine_t* engine) { CHECK(!"unexpected input teardown"); }
void ovrApp_Destroy(ovrApp* app) { CHECK(!"unexpected app teardown"); }
#define UNUSED_XR(name, args) XRAPI_ATTR XrResult XRAPI_CALL name args { CHECK(!"unexpected OpenXR call: " #name); return XR_ERROR_FUNCTION_UNSUPPORTED; }
UNUSED_XR(xrDestroyInstance, (XrInstance instance))
UNUSED_XR(xrDestroySpace, (XrSpace space))
UNUSED_XR(xrDestroySession, (XrSession session))
UNUSED_XR(xrCreateSession, (XrInstance instance, const XrSessionCreateInfo* info, XrSession* session))
UNUSED_XR(xrCreateReferenceSpace, (XrSession session, const XrReferenceSpaceCreateInfo* info, XrSpace* space))

void ovrApp_Clear(ovrApp* app) { memset(app, 0, sizeof(*app)); }
#ifndef _WIN32
EGLContext EGLAPIENTRY eglGetCurrentContext(void) { return EGL_NO_CONTEXT; }
#endif
int SDLCALL SDL_GL_GetAttribute(SDL_GLattr attr, int* value) {
    CHECK(attr == SDL_GL_CONTEXT_MAJOR_VERSION || attr == SDL_GL_CONTEXT_MINOR_VERSION);
    ++attributeReads;
    *value = attr == SDL_GL_CONTEXT_MAJOR_VERSION ? requestedMajor : requestedMinor; return 0;
}
int SDLCALL SDL_GL_SetAttribute(SDL_GLattr attr, int value) {
    ++attributes;
    switch (attr) {
    case SDL_GL_CONTEXT_MAJOR_VERSION: requestedMajor = value; break;
    case SDL_GL_CONTEXT_MINOR_VERSION: requestedMinor = value; break;
    case SDL_GL_CONTEXT_PROFILE_MASK: CHECK(value == SDL_GL_CONTEXT_PROFILE_COMPATIBILITY); break;
    case SDL_GL_MULTISAMPLEBUFFERS:
    case SDL_GL_MULTISAMPLESAMPLES: CHECK(value == 0); break;
    default: CHECK(!"unexpected context attribute");
    }
    return 0;
}
static const GLubyte* APIENTRY getString(GLenum name) {
    CHECK(name == GL_VERSION); return (const GLubyte*)glVersion;
}
void* SDLCALL SDL_GL_GetProcAddress(const char* name) {
    CHECK(!strcmp(name, "glGetString")); return missingGetString ? NULL : (void*)getString;
}
XRAPI_ATTR XrResult XRAPI_CALL xrEnumerateInstanceExtensionProperties(const char* layer, uint32_t cap, uint32_t* count, XrExtensionProperties* props) {
    CHECK(!layer); *count = 1;
    if (cap) {
        CHECK(props[0].type == XR_TYPE_EXTENSION_PROPERTIES);
        strcpy(props[0].extensionName, XR_KHR_OPENGL_ENABLE_EXTENSION_NAME);
        props[0].extensionVersion = XR_KHR_opengl_enable_SPEC_VERSION;
    }
    return XR_SUCCESS;
}
XRAPI_ATTR XrResult XRAPI_CALL xrCreateInstance(const XrInstanceCreateInfo* info, XrInstance* instance) {
    ++creates;
    CHECK(info->type == XR_TYPE_INSTANCE_CREATE_INFO && !info->next);
    CHECK(!info->enabledApiLayerCount && info->enabledExtensionCount == 1);
    CHECK(!strcmp(info->enabledExtensionNames[0], XR_KHR_OPENGL_ENABLE_EXTENSION_NAME));
    CHECK(!strcmp(info->applicationInfo.applicationName, "CSVR startup test"));
    /* As required by OpenXR: reject a request for a newer major/minor API. */
    if (XR_VERSION_MAJOR(info->applicationInfo.apiVersion) > XR_VERSION_MAJOR(runtimeVersion) ||
        XR_VERSION_MINOR(info->applicationInfo.apiVersion) > XR_VERSION_MINOR(runtimeVersion))
        return XR_ERROR_API_VERSION_UNSUPPORTED;
    CHECK(info->applicationInfo.apiVersion == XR_MAKE_VERSION(1, 0, 0));
    *instance = (XrInstance)(uintptr_t)1;
    return XR_SUCCESS;
}
XRAPI_ATTR XrResult XRAPI_CALL xrGetInstanceProperties(XrInstance instance, XrInstanceProperties* props) {
    CHECK(instance == (XrInstance)(uintptr_t)1);
    props->runtimeVersion = runtimeVersion; strcpy(props->runtimeName, "Version ceiling fixture");
    return XR_SUCCESS;
}
XRAPI_ATTR XrResult XRAPI_CALL xrGetSystem(XrInstance instance, const XrSystemGetInfo* info, XrSystemId* system) {
    CHECK(instance == (XrInstance)(uintptr_t)1);
    CHECK(info->formFactor == XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY);
    *system = 7; return XR_SUCCESS;
}
static XRAPI_ATTR XrResult XRAPI_CALL getRequirements(XrInstance instance, XrSystemId system, XrGraphicsRequirementsOpenGLKHR* req) {
    CHECK(system == 7 && req->type == XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_KHR);
    ++requirements;
    /* OpenGL binding must ignore patch components; maximum is advisory. */
    req->minApiVersionSupported = XR_MAKE_VERSION(4, 5, 99);
    req->maxApiVersionSupported = XR_MAKE_VERSION(4, 5, 42);
    return XR_SUCCESS;
}
XRAPI_ATTR XrResult XRAPI_CALL xrGetInstanceProcAddr(XrInstance instance, const char* name, PFN_xrVoidFunction* function) {
    CHECK(!strcmp(name, "xrGetOpenGLGraphicsRequirementsKHR"));
    *function = (PFN_xrVoidFunction)getRequirements; return XR_SUCCESS;
}
XRAPI_ATTR XrResult XRAPI_CALL xrResultToString(XrInstance instance, XrResult result, char buffer[XR_MAX_RESULT_STRING_SIZE]) {
    snprintf(buffer, XR_MAX_RESULT_STRING_SIZE, "%d", result); return XR_SUCCESS;
}
int main(int argc, char** argv) {
    CHECK((argc == 2 || argc == 3) && (!strcmp(argv[1], "0") || !strcmp(argv[1], "1")));
    runtimeVersion = XR_MAKE_VERSION(1, atoi(argv[1]), 0);
    if (!strcmp(argv[1], "1")) glVersion = "4.5 fixture";
    if (argc == 3) {
        if (!strcmp(argv[2], "below")) glVersion = "4.4 fixture";
        else if (!strcmp(argv[2], "malformed")) glVersion = "not a GL version";
        else if (!strcmp(argv[2], "missing")) missingGetString = true;
        else CHECK(!"unknown fixture scenario");
    }
    VR_DesktopGLAttributes();
    CHECK(attributes == 5);
#ifdef _WIN32
    CHECK(requestedMajor == 4 && requestedMinor == 5);
#else
    CHECK(requestedMajor == 3 && requestedMinor == 3);
#endif
    VR_Init(NULL, "CSVR startup test", 1);
    CHECK(VR_IsEnabled() && creates == 1 && requirements == 1 && !attributeReads);
    CHECK(VR_GetEngine()->appState.SystemId == 7);
    VR_Init(NULL, "CSVR startup test", 1);
    CHECK(creates == 1);
    puts("\nPASS: production VR_Init requests OpenXR 1.0, creates one instance and validates OpenGL requirements");
    puts("PASS: compatibility context attributes, actual driver GL version, tested maximum advisory and patch components ignored");
    return 0;
}
