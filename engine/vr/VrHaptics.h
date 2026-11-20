#ifndef HAPTICS_API_H
#define HAPTICS_API_H

#ifdef ANDROID
#include <jni.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Initialization
#ifdef ANDROID
void VR_Haptics_OnCreate(jobject context);
#endif
void VR_Haptics_OnDestroy(void);
void VR_Haptics_Disable(void);
void VR_Haptics_Enable(void);

// Event methods
void VR_Haptics_Event(const char *event, int position, int flags, int intensity, float angle, float yHeight);
void VR_Haptics_UpdateEvent(const char *event, int intensity, float angle);
void VR_Haptics_StopEvent(const char *event);
void VR_Haptics_EndFrame(void);

#ifdef __cplusplus
}
#endif

#endif // HAPTICS_API_H
