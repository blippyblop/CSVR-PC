/* Keep native window-system headers out of the shared engine interfaces. */
#if !defined(ANDROID) && !defined(_WIN32)
#include <EGL/egl.h>
#include <GL/glx.h>
#define XR_USE_PLATFORM_EGL 1
#define XR_USE_PLATFORM_XLIB 1
#elif !defined(ANDROID)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <unknwn.h>
#define XR_USE_PLATFORM_WIN32 1
#endif
#include "VrBase.h"
#include "VrRenderer.h"
#include "VrInput.h"
#ifndef ANDROID
#include <SDL.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#include <unistd.h>
#endif

static bool vr_platform[VR_PLATFORM_MAX];
static engine_t vr_engine;
int vr_initialized = 0;

static bool g_hasAndroidCreateInstanceExt = false;
static bool g_hasPerfSettingsExt = false;
static bool g_hasAndroidThreadSettingsExt = false;
static bool g_hasDisplayRefreshRateExt = false;

XrResult VR_CheckResult(XrResult result, const char* operation) {
	if (XR_FAILED(result)) {
		char message[XR_MAX_RESULT_STRING_SIZE];
		if (vr_engine.appState.Instance)
			xrResultToString(vr_engine.appState.Instance, result, message);
		else snprintf(message, sizeof(message), "result %d", result);
		ALOGE("OpenXR %s: %s\n", operation, message);
	}
	return result;
}

bool VR_IsEnabled(void) { return vr_initialized != 0; }

#ifndef ANDROID
static bool desktopEgl;
#endif

static bool XR_ExtensionSupported(const char* name,
                                 const XrExtensionProperties* props,
                                 uint32_t count)
{
	for (uint32_t i = 0; i < count; ++i) {
		if (strcmp(props[i].extensionName, name) == 0) {
			return true;
		}
	}
	return false;
}

void VR_Init( void* system, const char* name, int version ) {
	if (vr_initialized)
		return;

	ovrApp_Clear(&vr_engine.appState);

#ifdef ANDROID
	PFN_xrInitializeLoaderKHR xrInitializeLoaderKHR;
	xrGetInstanceProcAddr(XR_NULL_HANDLE, "xrInitializeLoaderKHR", (PFN_xrVoidFunction*)&xrInitializeLoaderKHR);
	if (xrInitializeLoaderKHR != NULL) {
		ovrJava* java = (ovrJava*)system;
		XrLoaderInitInfoAndroidKHR loaderInitializeInfo;
		memset(&loaderInitializeInfo, 0, sizeof(loaderInitializeInfo));
		loaderInitializeInfo.type = XR_TYPE_LOADER_INIT_INFO_ANDROID_KHR;
		loaderInitializeInfo.next = NULL;
		loaderInitializeInfo.applicationVM = java->Vm;
		loaderInitializeInfo.applicationContext = java->ActivityObject;
		xrInitializeLoaderKHR((XrLoaderInitInfoBaseHeaderKHR*)&loaderInitializeInfo);
	}
#endif

	uint32_t runtimeExtCount = 0;
	if (XR_FAILED(OXR(xrEnumerateInstanceExtensionProperties(NULL, 0, &runtimeExtCount, NULL)))) {
		ALOGE("No OpenXR runtime is available. Use -novr for desktop mode.\n");
		exit(1);
	}

	XrExtensionProperties* runtimeExtProps = calloc(runtimeExtCount, sizeof(*runtimeExtProps));
	if (!runtimeExtProps) exit(1);
	for (uint32_t i = 0; i < runtimeExtCount; ++i) {
		memset(&runtimeExtProps[i], 0, sizeof(XrExtensionProperties));
		runtimeExtProps[i].type = XR_TYPE_EXTENSION_PROPERTIES;
		runtimeExtProps[i].next = NULL;
	}
	OXR(xrEnumerateInstanceExtensionProperties(NULL, runtimeExtCount, &runtimeExtCount, runtimeExtProps));

	int extensionsCount = 0;
	const char* extensions[32];

	if (XR_ExtensionSupported(XR_KHR_COMPOSITION_LAYER_CYLINDER_EXTENSION_NAME, runtimeExtProps, runtimeExtCount)) {
		extensions[extensionsCount++] = XR_KHR_COMPOSITION_LAYER_CYLINDER_EXTENSION_NAME;
		VR_SetPlatformFLag(VR_PLATFORM_EXTENSION_CYLINDER, true);
	}

#ifdef ANDROID
	if (XR_ExtensionSupported(XR_KHR_OPENGL_ES_ENABLE_EXTENSION_NAME, runtimeExtProps, runtimeExtCount)) {
		extensions[extensionsCount++] = XR_KHR_OPENGL_ES_ENABLE_EXTENSION_NAME;
	} else {
		ALOGE("OpenXR runtime does not support %s", XR_KHR_OPENGL_ES_ENABLE_EXTENSION_NAME);
		exit(1);
	}

	g_hasAndroidCreateInstanceExt =
			VR_GetPlatformFlag(VR_PLATFORM_EXTENSION_INSTANCE) &&
			XR_ExtensionSupported(XR_KHR_ANDROID_CREATE_INSTANCE_EXTENSION_NAME, runtimeExtProps, runtimeExtCount);
	if (g_hasAndroidCreateInstanceExt) {
		extensions[extensionsCount++] = XR_KHR_ANDROID_CREATE_INSTANCE_EXTENSION_NAME;
	}

	g_hasPerfSettingsExt =
			VR_GetPlatformFlag(VR_PLATFORM_EXTENSION_PERFORMANCE) &&
			XR_ExtensionSupported(XR_EXT_PERFORMANCE_SETTINGS_EXTENSION_NAME, runtimeExtProps, runtimeExtCount);
	if (g_hasPerfSettingsExt) {
		extensions[extensionsCount++] = XR_EXT_PERFORMANCE_SETTINGS_EXTENSION_NAME;
	}

	g_hasAndroidThreadSettingsExt =
			VR_GetPlatformFlag(VR_PLATFORM_EXTENSION_PERFORMANCE) &&
			XR_ExtensionSupported(XR_KHR_ANDROID_THREAD_SETTINGS_EXTENSION_NAME, runtimeExtProps, runtimeExtCount);
	if (g_hasAndroidThreadSettingsExt) {
		extensions[extensionsCount++] = XR_KHR_ANDROID_THREAD_SETTINGS_EXTENSION_NAME;
	} else if (VR_GetPlatformFlag(VR_PLATFORM_EXTENSION_PERFORMANCE)) {
		ALOGE("Runtime does not support %s, skipping it (prevents xrCreateInstance failure).",
			  XR_KHR_ANDROID_THREAD_SETTINGS_EXTENSION_NAME);
	}

	g_hasDisplayRefreshRateExt =
			VR_GetPlatformFlag(VR_PLATFORM_EXTENSION_REFRESH) &&
			XR_ExtensionSupported(XR_FB_DISPLAY_REFRESH_RATE_EXTENSION_NAME, runtimeExtProps, runtimeExtCount);
	if (g_hasDisplayRefreshRateExt) {
		extensions[extensionsCount++] = XR_FB_DISPLAY_REFRESH_RATE_EXTENSION_NAME;
	}
#else
	if (!XR_ExtensionSupported(XR_KHR_OPENGL_ENABLE_EXTENSION_NAME, runtimeExtProps, runtimeExtCount)) {
		ALOGE("OpenXR runtime has no desktop OpenGL support.\n"); exit(1);
	}
	extensions[extensionsCount++] = XR_KHR_OPENGL_ENABLE_EXTENSION_NAME;
#ifndef _WIN32
	desktopEgl = eglGetCurrentContext() != EGL_NO_CONTEXT;
	if (desktopEgl) {
		if (!XR_ExtensionSupported(XR_MNDX_EGL_ENABLE_EXTENSION_NAME, runtimeExtProps, runtimeExtCount)) {
			ALOGE("This runtime cannot accept EGL contexts (XR_MNDX_egl_enable). "
			      "Use a compatible runtime, or opt into SDL X11/GLX.\n"); exit(1);
		}
		extensions[extensionsCount++] = XR_MNDX_EGL_ENABLE_EXTENSION_NAME;
	}
#endif
#endif
	free(runtimeExtProps);

	XrApplicationInfo appInfo;
	memset(&appInfo, 0, sizeof(appInfo));
	strcpy(appInfo.applicationName, name);
	strcpy(appInfo.engineName, name);
	appInfo.applicationVersion = version;
	appInfo.engineVersion = version;
	appInfo.apiVersion = XR_CURRENT_API_VERSION;

	XrInstanceCreateInfo instanceCreateInfo;
	memset(&instanceCreateInfo, 0, sizeof(instanceCreateInfo));
	instanceCreateInfo.type = XR_TYPE_INSTANCE_CREATE_INFO;
	instanceCreateInfo.next = NULL;
	instanceCreateInfo.createFlags = 0;
	instanceCreateInfo.applicationInfo = appInfo;
	instanceCreateInfo.enabledApiLayerCount = 0;
	instanceCreateInfo.enabledApiLayerNames = NULL;
	instanceCreateInfo.enabledExtensionCount = (uint32_t)extensionsCount;
	instanceCreateInfo.enabledExtensionNames = extensions;

#ifdef ANDROID
	XrInstanceCreateInfoAndroidKHR instanceCreateInfoAndroid = {XR_TYPE_INSTANCE_CREATE_INFO_ANDROID_KHR};
	if (g_hasAndroidCreateInstanceExt) {
		ovrJava* java = (ovrJava*)system;
		instanceCreateInfoAndroid.applicationVM = java->Vm;
		instanceCreateInfoAndroid.applicationActivity = java->ActivityObject;
		instanceCreateInfo.next = (XrBaseInStructure*)&instanceCreateInfoAndroid;
	}
#endif

	XrResult initResult;
	OXR(initResult = xrCreateInstance(&instanceCreateInfo, &vr_engine.appState.Instance));
	if (initResult != XR_SUCCESS) {
		ALOGE("Failed to create XR instance: %d.", initResult);
		exit(1);
	}

	XrInstanceProperties instanceInfo;
	instanceInfo.type = XR_TYPE_INSTANCE_PROPERTIES;
	instanceInfo.next = NULL;
	OXR(xrGetInstanceProperties(vr_engine.appState.Instance, &instanceInfo));
	ALOGV(
			"Runtime %s: Version : %u.%u.%u",
			instanceInfo.runtimeName,
			XR_VERSION_MAJOR(instanceInfo.runtimeVersion),
			XR_VERSION_MINOR(instanceInfo.runtimeVersion),
			XR_VERSION_PATCH(instanceInfo.runtimeVersion));

	XrSystemGetInfo systemGetInfo;
	memset(&systemGetInfo, 0, sizeof(systemGetInfo));
	systemGetInfo.type = XR_TYPE_SYSTEM_GET_INFO;
	systemGetInfo.next = NULL;
	systemGetInfo.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;

	XrSystemId systemId;
	OXR(initResult = xrGetSystem(vr_engine.appState.Instance, &systemGetInfo, &systemId));
	if (initResult != XR_SUCCESS) {
		ALOGE("Failed to get system.");
		exit(1);
	}

#ifdef ANDROID
	PFN_xrGetOpenGLESGraphicsRequirementsKHR pfnGetOpenGLESGraphicsRequirementsKHR = NULL;
	OXR(xrGetInstanceProcAddr(
			vr_engine.appState.Instance,
			"xrGetOpenGLESGraphicsRequirementsKHR",
			(PFN_xrVoidFunction*)(&pfnGetOpenGLESGraphicsRequirementsKHR)));

	XrGraphicsRequirementsOpenGLESKHR graphicsRequirements = {};
	graphicsRequirements.type = XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_ES_KHR;
	OXR(pfnGetOpenGLESGraphicsRequirementsKHR(vr_engine.appState.Instance, systemId, &graphicsRequirements));
#else
	PFN_xrGetOpenGLGraphicsRequirementsKHR getRequirements = NULL;
	OXR(xrGetInstanceProcAddr(vr_engine.appState.Instance, "xrGetOpenGLGraphicsRequirementsKHR", (PFN_xrVoidFunction*)&getRequirements));
	XrGraphicsRequirementsOpenGLKHR requirements = {XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_KHR};
	if (!getRequirements || XR_FAILED(OXR(getRequirements(vr_engine.appState.Instance, systemId, &requirements)))) exit(1);
	int major = 0, minor = 0;
	SDL_GL_GetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, &major);
	SDL_GL_GetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, &minor);
	XrVersion glVersion = XR_MAKE_VERSION(major, minor, 0);
	if (glVersion < requirements.minApiVersionSupported || glVersion > requirements.maxApiVersionSupported) {
		ALOGE("The current OpenGL context does not meet the OpenXR runtime requirements.\n"); exit(1);
	}
#endif

#ifdef ANDROID
	vr_engine.appState.MainThreadTid = gettid();
#endif
	vr_engine.appState.SystemId = systemId;
	vr_initialized = 1;
}

void VR_Destroy( engine_t* engine ) {
	if (engine == &vr_engine) {
		if (!vr_initialized) return;
		VR_DestroyRenderer(engine);
		IN_VRDestroy(engine);
		VR_LeaveVR(engine);
		xrDestroyInstance(engine->appState.Instance);
		ovrApp_Destroy(&engine->appState);
		memset(vr_platform, 0, sizeof(vr_platform));
		vr_initialized = 0;
	}
}

void VR_EnterVR( engine_t* engine ) {

	if (engine->appState.Session) {
		ALOGE("VR_EnterVR called with existing session");
		return;
	}

	XrSessionCreateInfo sessionCreateInfo = {};
#ifdef ANDROID
	XrGraphicsBindingOpenGLESAndroidKHR graphicsBindingGL = {};
	memset(&sessionCreateInfo, 0, sizeof(sessionCreateInfo));
	graphicsBindingGL.type = XR_TYPE_GRAPHICS_BINDING_OPENGL_ES_ANDROID_KHR;
	graphicsBindingGL.display = eglGetCurrentDisplay();
	graphicsBindingGL.config = NULL;
	graphicsBindingGL.context = eglGetCurrentContext();
	sessionCreateInfo.next = &graphicsBindingGL;
#elif defined(_WIN32)
	XrGraphicsBindingOpenGLWin32KHR binding = {XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR};
	binding.hDC = wglGetCurrentDC();
	binding.hGLRC = wglGetCurrentContext();
	sessionCreateInfo.next = &binding;
#else
	XrGraphicsBindingEGLMNDX eglBinding = {XR_TYPE_GRAPHICS_BINDING_EGL_MNDX};
	XrGraphicsBindingOpenGLXlibKHR glxBinding = {XR_TYPE_GRAPHICS_BINDING_OPENGL_XLIB_KHR};
	if (desktopEgl) {
		eglBinding.getProcAddress = (PFN_xrEglGetProcAddressMNDX)eglGetProcAddress;
		eglBinding.display = eglGetCurrentDisplay();
		eglBinding.context = eglGetCurrentContext();
		EGLint id = 0, count = 0;
		eglQueryContext(eglBinding.display, eglBinding.context, EGL_CONFIG_ID, &id);
		EGLint attributes[] = {EGL_CONFIG_ID, id, EGL_NONE};
		if (!eglChooseConfig(eglBinding.display, attributes, &eglBinding.config, 1, &count) || count != 1) exit(1);
		sessionCreateInfo.next = &eglBinding;
	} else {
		glxBinding.xDisplay = glXGetCurrentDisplay();
		glxBinding.glxContext = glXGetCurrentContext();
		glxBinding.glxDrawable = glXGetCurrentDrawable();
		int id = 0, count = 0;
		glXQueryContext(glxBinding.xDisplay, glxBinding.glxContext, GLX_FBCONFIG_ID, &id);
		GLXFBConfig* configs = glXGetFBConfigs(glxBinding.xDisplay, DefaultScreen(glxBinding.xDisplay), &count);
		for (int i = 0; i < count; ++i) {
			int current = 0; glXGetFBConfigAttrib(glxBinding.xDisplay, configs[i], GLX_FBCONFIG_ID, &current);
			if (current == id) {
				glxBinding.glxFBConfig = configs[i];
				XVisualInfo* visual = glXGetVisualFromFBConfig(glxBinding.xDisplay, configs[i]);
				if (visual) { glxBinding.visualid = visual->visualid; XFree(visual); }
				break;
			}
		}
		XFree(configs);
		if (!glxBinding.glxFBConfig) exit(1);
		sessionCreateInfo.next = &glxBinding;
	}
#endif
	sessionCreateInfo.type = XR_TYPE_SESSION_CREATE_INFO;
	sessionCreateInfo.createFlags = 0;
	sessionCreateInfo.systemId = engine->appState.SystemId;

	XrResult initResult;
	OXR(initResult = xrCreateSession(engine->appState.Instance, &sessionCreateInfo, &engine->appState.Session));
	if (initResult != XR_SUCCESS) {
		ALOGE("Failed to create XR session: %d.", initResult);
		exit(1);
	}

	XrReferenceSpaceCreateInfo spaceCreateInfo = {};
	spaceCreateInfo.type = XR_TYPE_REFERENCE_SPACE_CREATE_INFO;
	spaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
	spaceCreateInfo.poseInReferenceSpace.orientation.w = 1.0f;
	OXR(xrCreateReferenceSpace(engine->appState.Session, &spaceCreateInfo, &engine->appState.HeadSpace));
	engine->appState.ViewportConfig.type = XR_TYPE_VIEW_CONFIGURATION_PROPERTIES;
	engine->appState.ViewportConfig.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
#ifdef ANDROID
	engine->appState.RenderThreadTid = gettid();
#endif

#ifdef ANDROID
	if (VR_GetPlatformFlag(VR_PLATFORM_EXTENSION_PERFORMANCE)) {
		if (g_hasPerfSettingsExt) {
			XrPerfSettingsLevelEXT cpuPerfLevel = XR_PERF_SETTINGS_LEVEL_BOOST_EXT;
			XrPerfSettingsLevelEXT gpuPerfLevel = XR_PERF_SETTINGS_LEVEL_BOOST_EXT;

			PFN_xrPerfSettingsSetPerformanceLevelEXT pfnPerfSettingsSetPerformanceLevelEXT = NULL;
			OXR(xrGetInstanceProcAddr(
					engine->appState.Instance,
					"xrPerfSettingsSetPerformanceLevelEXT",
					(PFN_xrVoidFunction*)(&pfnPerfSettingsSetPerformanceLevelEXT)));

			if (pfnPerfSettingsSetPerformanceLevelEXT) {
				OXR(pfnPerfSettingsSetPerformanceLevelEXT(engine->appState.Session, XR_PERF_SETTINGS_DOMAIN_CPU_EXT, cpuPerfLevel));
				OXR(pfnPerfSettingsSetPerformanceLevelEXT(engine->appState.Session, XR_PERF_SETTINGS_DOMAIN_GPU_EXT, gpuPerfLevel));
			} else {
				ALOGE("xrPerfSettingsSetPerformanceLevelEXT is NULL (runtime mismatch), skipping.");
			}
		}

		if (g_hasAndroidThreadSettingsExt) {
			PFN_xrSetAndroidApplicationThreadKHR pfnSetAndroidApplicationThreadKHR = NULL;
			OXR(xrGetInstanceProcAddr(
					engine->appState.Instance,
					"xrSetAndroidApplicationThreadKHR",
					(PFN_xrVoidFunction*)(&pfnSetAndroidApplicationThreadKHR)));

			if (pfnSetAndroidApplicationThreadKHR) {
				OXR(pfnSetAndroidApplicationThreadKHR(engine->appState.Session,
				                                     XR_ANDROID_THREAD_TYPE_APPLICATION_MAIN_KHR,
				                                     engine->appState.MainThreadTid));
				OXR(pfnSetAndroidApplicationThreadKHR(engine->appState.Session,
				                                     XR_ANDROID_THREAD_TYPE_RENDERER_MAIN_KHR,
				                                     engine->appState.RenderThreadTid));
			} else {
				ALOGE("xrSetAndroidApplicationThreadKHR is NULL (runtime mismatch), skipping.");
			}
		}
	}
#endif
}

void VR_LeaveVR( engine_t* engine ) {
	if (engine->appState.Session) {
		OXR(xrDestroySpace(engine->appState.HeadSpace));

		if (engine->appState.StageSpace != XR_NULL_HANDLE) {
			OXR(xrDestroySpace(engine->appState.StageSpace));
		}
		if (engine->appState.FakeStageSpace) OXR(xrDestroySpace(engine->appState.FakeStageSpace));
		engine->appState.CurrentSpace = XR_NULL_HANDLE;
		OXR(xrDestroySession(engine->appState.Session));
		engine->appState.Session = XR_NULL_HANDLE;
	}
}

engine_t* VR_GetEngine( void ) {
	return &vr_engine;
}

bool VR_GetPlatformFlag(enum VRPlatformFlag flag) {
	return vr_platform[flag];
}

void VR_SetPlatformFLag(enum VRPlatformFlag flag, bool value) {
	vr_platform[flag] = value;
}
