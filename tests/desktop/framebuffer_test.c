/* Exercise the production desktop FBO code with a real OpenGL context and
 * runtime-owned textures. OpenXR allocation/ownership is simulated. */
#include "VrFramebuffer.h"
#include "VrMath.h"
#include "xr_ui.h"
#include <SDL.h>
#include <SDL_opengl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%s:%d: %s (%s)\n", __FILE__, __LINE__, #x, SDL_GetError()); exit(1); } } while (0)
static GLuint textures[2][2];
static unsigned int chains, released, destroyed, acquisitions;
XrResult VR_CheckResult(XrResult result, const char* operation) { CHECK(XR_SUCCEEDED(result)); return result; }
XRAPI_ATTR XrResult XRAPI_CALL xrEnumerateSwapchainFormats(XrSession s, uint32_t cap, uint32_t* count, int64_t* f) {
    *count = 2; if (cap) { f[0] = GL_RGB10_A2; f[1] = GL_RGBA8; } return XR_SUCCESS;
}
XRAPI_ATTR XrResult XRAPI_CALL xrCreateSwapchain(XrSession s, const XrSwapchainCreateInfo* info, XrSwapchain* chain) {
    CHECK(chains < 2 && info->format == GL_RGBA8 && info->sampleCount == 1 && info->arraySize == 1);
    CHECK(info->width == 320 && info->height == 240);
    glGenTextures(2, textures[chains]);
    for (int i = 0; i < 2; ++i) {
        glBindTexture(GL_TEXTURE_2D, textures[chains][i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 320, 240, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }
    *chain = (XrSwapchain)(uintptr_t)(++chains); return XR_SUCCESS;
}
XRAPI_ATTR XrResult XRAPI_CALL xrEnumerateSwapchainImages(XrSwapchain chain, uint32_t cap, uint32_t* count, XrSwapchainImageBaseHeader* images) {
    *count = 2;
    if (cap) {
        XrSwapchainImageOpenGLKHR* out = (XrSwapchainImageOpenGLKHR*)images;
        for (int i = 0; i < 2; ++i) { CHECK(out[i].type == XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR); out[i].image = textures[(uintptr_t)chain - 1][i]; }
    }
    return XR_SUCCESS;
}
XRAPI_ATTR XrResult XRAPI_CALL xrAcquireSwapchainImage(XrSwapchain chain, const XrSwapchainImageAcquireInfo* info, uint32_t* index) {
    *index = acquisitions++ % 2 ? 0 : 1; return XR_SUCCESS;
}
XRAPI_ATTR XrResult XRAPI_CALL xrWaitSwapchainImage(XrSwapchain chain, const XrSwapchainImageWaitInfo* info) { CHECK(info->timeout == XR_INFINITE_DURATION); return XR_SUCCESS; }
XRAPI_ATTR XrResult XRAPI_CALL xrReleaseSwapchainImage(XrSwapchain chain, const XrSwapchainImageReleaseInfo* info) { ++released; return XR_SUCCESS; }
XRAPI_ATTR XrResult XRAPI_CALL xrDestroySwapchain(XrSwapchain chain) { glDeleteTextures(2, textures[(uintptr_t)chain-1]); ++destroyed; return XR_SUCCESS; }

int main(void) {
    CHECK(SDL_Init(SDL_INIT_VIDEO) == 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
    SDL_Window* window = SDL_CreateWindow("CSVR framebuffer test", 0, 0, 320, 240, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
    CHECK(window);
    SDL_GLContext context = SDL_GL_CreateContext(window); CHECK(context);
    ovrRenderer renderer;
    ovrRenderer_Create((XrSession)(uintptr_t)1, &renderer, true, 320, 240, 4);
    CHECK(!renderer.Multiview);
    for (int frame = 0; frame < 3; ++frame) {
        for (int eye = 0; eye < 2; ++eye) {
            ovrFramebuffer* fb = &renderer.FrameBuffer[eye];
            ovrFramebuffer_Acquire(fb); ovrFramebuffer_SetCurrent(fb);
            glViewport(0, 0, 320, 240);
            glClearColor(eye ? 0 : 1, eye ? 1 : 0, 0, 1); glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            unsigned char pixel[4] = {0};
            glReadPixels(160, 120, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
            CHECK(pixel[eye] == 255 && pixel[1-eye] == 0);
			/* A shared HUD landmark must project from the same head-space point,
			 * through asymmetric/canted views, rather than to identical pixels. */
			float offset[3]={eye ? .0325f : -.0325f, 0, 0};
			XrQuaternionf q=XrQuaternionf_CreateFromVectorAngle((XrVector3f){0,1,0}, frame==1 ? (eye ? -.15f : .15f) : 0);
			float orientation[4]={q.x,q.y,q.z,q.w};
			float fov[4]={eye ? -.8f : -1.3f, eye ? 1.3f : .8f, -1.1f, .9f};
			float hud[4][4], columnMajor[16];
			XRUI_Projection(hud, 320, 240, 2, 1.6f, offset, orientation, fov);
			for (int row=0;row<4;++row) for (int col=0;col<4;++col) columnMajor[col*4+row]=hud[row][col];
			glMatrixMode(GL_PROJECTION); glLoadMatrixf(columnMajor);
			glMatrixMode(GL_MODELVIEW); glLoadIdentity();
			glDisable(GL_TEXTURE_2D); glDisable(GL_DEPTH_TEST); glDisable(GL_BLEND);
			glColor4f(1,1,1,1);
			glBegin(GL_QUADS);
			glVertex2f(155,115); glVertex2f(165,115); glVertex2f(165,125); glVertex2f(155,125);
			glEnd();
			XrVector3f point=XrQuaternionf_Rotate(XrQuaternionf_Inverse(q),(XrVector3f){-offset[0],0,-2});
			float sx=(2*point.x/-point.z-fov[1]-fov[0])/(fov[1]-fov[0]);
			float sy=(2*point.y/-point.z-fov[3]-fov[2])/(fov[3]-fov[2]);
			glReadPixels((int)((sx+1)*160), (int)((sy+1)*120), 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
			CHECK(pixel[0]==255 && pixel[1]==255 && pixel[2]==255);
			float px,py;
			CHECK(XRUI_PixelFromNDC(hud,sx,sy,&px,&py));
			CHECK(fabsf(px-160)<.001f && fabsf(py-120)<.001f);
            ovrFramebuffer_Resolve(fb); ovrFramebuffer_Release(fb);
        }
    }
    CHECK(released == 6 && acquisitions == 6);
    ovrFramebuffer_SetNone(); ovrRenderer_Destroy(&renderer);
    CHECK(destroyed == 2 && renderer.FrameBuffer[0].ColorSwapChain.Handle == XR_NULL_HANDLE);
    CHECK(glGetError() == GL_NO_ERROR);
    SDL_GL_DeleteContext(context); SDL_DestroyWindow(window); SDL_Quit();
    puts("PASS: real GL FBOs and image lifetime; stereo HUD landmarks with asymmetric/canted views and reticle inverse projection");
    return 0;
}
