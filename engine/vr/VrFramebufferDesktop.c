/* Desktop OpenGL swapchains. Android keeps its GLES implementation. */
#ifndef ANDROID
#include "VrFramebuffer.h"
#include <SDL.h>
#include <SDL_opengl.h>
#include <SDL_opengl_glext.h>
#include <stdlib.h>
#include <string.h>

static PFNGLBLITFRAMEBUFFERPROC blitFramebuffer;
static PFNGLGENFRAMEBUFFERSPROC genFramebuffers;
static PFNGLDELETEFRAMEBUFFERSPROC deleteFramebuffers;
static PFNGLBINDFRAMEBUFFERPROC bindFramebuffer;
static PFNGLFRAMEBUFFERTEXTURE2DPROC framebufferTexture2D;
static PFNGLCHECKFRAMEBUFFERSTATUSPROC checkFramebufferStatus;
static PFNGLGENRENDERBUFFERSPROC genRenderbuffers;
static PFNGLDELETERENDERBUFFERSPROC deleteRenderbuffers;
static PFNGLBINDRENDERBUFFERPROC bindRenderbuffer;
static PFNGLRENDERBUFFERSTORAGEPROC renderbufferStorage;
static PFNGLFRAMEBUFFERRENDERBUFFERPROC framebufferRenderbuffer;

static void LoadGL(void) {
#define LOAD_GL(member, type, name) \
    member = (type)SDL_GL_GetProcAddress(name); \
    if (!member) { ALOGE("Required OpenGL function unavailable: %s\n", name); exit(1); }
    LOAD_GL(blitFramebuffer, PFNGLBLITFRAMEBUFFERPROC, "glBlitFramebuffer");
    LOAD_GL(genFramebuffers, PFNGLGENFRAMEBUFFERSPROC, "glGenFramebuffers");
    LOAD_GL(deleteFramebuffers, PFNGLDELETEFRAMEBUFFERSPROC, "glDeleteFramebuffers");
    LOAD_GL(bindFramebuffer, PFNGLBINDFRAMEBUFFERPROC, "glBindFramebuffer");
    LOAD_GL(framebufferTexture2D, PFNGLFRAMEBUFFERTEXTURE2DPROC, "glFramebufferTexture2D");
    LOAD_GL(checkFramebufferStatus, PFNGLCHECKFRAMEBUFFERSTATUSPROC, "glCheckFramebufferStatus");
    LOAD_GL(genRenderbuffers, PFNGLGENRENDERBUFFERSPROC, "glGenRenderbuffers");
    LOAD_GL(deleteRenderbuffers, PFNGLDELETERENDERBUFFERSPROC, "glDeleteRenderbuffers");
    LOAD_GL(bindRenderbuffer, PFNGLBINDRENDERBUFFERPROC, "glBindRenderbuffer");
    LOAD_GL(renderbufferStorage, PFNGLRENDERBUFFERSTORAGEPROC, "glRenderbufferStorage");
    LOAD_GL(framebufferRenderbuffer, PFNGLFRAMEBUFFERRENDERBUFFERPROC, "glFramebufferRenderbuffer");
#undef LOAD_GL
}

void GLCheckErrors(const char* file, int line) {
    GLenum error;
    while ((error = glGetError()) != GL_NO_ERROR)
        ALOGE("OpenGL error %u at %s:%d\n", error, file, line);
}

static void Require(XrResult result) {
    if (XR_FAILED(result)) exit(1);
}

static void CreateFramebuffer(XrSession session, ovrFramebuffer* fb, int width, int height) {
    uint32_t count = 0;
    Require(OXR(xrEnumerateSwapchainFormats(session, 0, &count, NULL)));
    int64_t* formats = calloc(count, sizeof(*formats));
    if (!formats) exit(1);
    Require(OXR(xrEnumerateSwapchainFormats(session, count, &count, formats)));
    int64_t format = 0;
    const int64_t preferred[] = {GL_SRGB8_ALPHA8, GL_RGBA8};
    for (unsigned int p = 0; p < sizeof(preferred)/sizeof(preferred[0]) && !format; ++p)
        for (uint32_t i = 0; i < count; ++i)
            if (formats[i] == preferred[p]) { format = formats[i]; break; }
    free(formats);
    if (!format) { ALOGE("Runtime has no compatible RGBA8 OpenGL swapchain format.\n"); exit(1); }
    XrSwapchainCreateInfo info = {XR_TYPE_SWAPCHAIN_CREATE_INFO};
    info.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
    info.format = format;
    info.sampleCount = info.faceCount = info.arraySize = info.mipCount = 1;
    info.width = width; info.height = height;
    Require(OXR(xrCreateSwapchain(session, &info, &fb->ColorSwapChain.Handle)));
    fb->Width = fb->ColorSwapChain.Width = width;
    fb->Height = fb->ColorSwapChain.Height = height;
    fb->Multisamples = 1;
    Require(OXR(xrEnumerateSwapchainImages(fb->ColorSwapChain.Handle, 0, &fb->TextureSwapChainLength, NULL)));
    uint32_t length = fb->TextureSwapChainLength;
    fb->ColorSwapChainImage = calloc(length, sizeof(*fb->ColorSwapChainImage));
    fb->FrameBuffers = calloc(length, sizeof(*fb->FrameBuffers));
    fb->DepthBuffers = calloc(length, sizeof(*fb->DepthBuffers));
    if (!length || !fb->ColorSwapChainImage || !fb->FrameBuffers || !fb->DepthBuffers) exit(1);
    for (uint32_t i = 0; i < length; ++i)
        fb->ColorSwapChainImage[i].type = XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR;
    Require(OXR(xrEnumerateSwapchainImages(fb->ColorSwapChain.Handle, length, &length,
        (XrSwapchainImageBaseHeader*)fb->ColorSwapChainImage)));
    genFramebuffers(length, fb->FrameBuffers);
    genRenderbuffers(length, fb->DepthBuffers);
    for (uint32_t i = 0; i < length; ++i) {
        bindFramebuffer(GL_FRAMEBUFFER, fb->FrameBuffers[i]);
        framebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fb->ColorSwapChainImage[i].image, 0);
        bindRenderbuffer(GL_RENDERBUFFER, fb->DepthBuffers[i]);
        renderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
        framebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, fb->DepthBuffers[i]);
        framebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, fb->DepthBuffers[i]);
        if (checkFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            ALOGE("Desktop OpenXR framebuffer is incomplete.\n"); exit(1);
        }
    }
    bindRenderbuffer(GL_RENDERBUFFER, 0);
    bindFramebuffer(GL_FRAMEBUFFER, 0);
}

void ovrRenderer_Clear(ovrRenderer* renderer) { memset(renderer, 0, sizeof(*renderer)); }
void ovrRenderer_Create(XrSession session, ovrRenderer* renderer, bool multiview, int width, int height, int samples) {
    LoadGL();
    ovrRenderer_Clear(renderer);
    for (int eye = 0; eye < ovrMaxNumEyes; ++eye)
        CreateFramebuffer(session, &renderer->FrameBuffer[eye], width, height);
}
void ovrRenderer_Destroy(ovrRenderer* renderer) {
    for (int eye = 0; eye < ovrMaxNumEyes; ++eye) {
        ovrFramebuffer* fb = &renderer->FrameBuffer[eye];
        if (fb->FrameBuffers) deleteFramebuffers(fb->TextureSwapChainLength, fb->FrameBuffers);
        if (fb->DepthBuffers) deleteRenderbuffers(fb->TextureSwapChainLength, fb->DepthBuffers);
        if (fb->ColorSwapChain.Handle) OXR(xrDestroySwapchain(fb->ColorSwapChain.Handle));
        free(fb->FrameBuffers); free(fb->DepthBuffers); free(fb->ColorSwapChainImage);
    }
    ovrRenderer_Clear(renderer);
}
void ovrFramebuffer_SetCurrent(ovrFramebuffer* fb) {
    bindFramebuffer(GL_FRAMEBUFFER, fb->FrameBuffers[fb->TextureSwapChainIndex]);
}
void ovrFramebuffer_SetNone(void) { bindFramebuffer(GL_FRAMEBUFFER, 0); }
void ovrFramebuffer_Acquire(ovrFramebuffer* fb) {
    XrSwapchainImageAcquireInfo acquire = {XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
    XrSwapchainImageWaitInfo wait = {XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
    wait.timeout = XR_INFINITE_DURATION;
    Require(OXR(xrAcquireSwapchainImage(fb->ColorSwapChain.Handle, &acquire, &fb->TextureSwapChainIndex)));
    Require(OXR(xrWaitSwapchainImage(fb->ColorSwapChain.Handle, &wait)));
}
void ovrFramebuffer_Mirror(ovrFramebuffer* fb) {
    /* A Wayland mirror can wait for a desktop frame callback while the XR
     * compositor is waiting for xrEndFrame. Keep this optional on desktop. */
    const char* enabled = getenv("CSVR_XR_MIRROR");
    if (!enabled || strcmp(enabled, "1") != 0) return;
    SDL_Window* window = SDL_GL_GetCurrentWindow();
    int width = 0, height = 0;
    if (!window) return;
    SDL_GL_GetDrawableSize(window, &width, &height);
    if (width <= 0 || height <= 0) return;
    glPushAttrib(GL_COLOR_BUFFER_BIT | GL_ENABLE_BIT | GL_SCISSOR_BIT);
    glDisable(GL_SCISSOR_TEST);
    bindFramebuffer(GL_READ_FRAMEBUFFER, fb->FrameBuffers[fb->TextureSwapChainIndex]);
    bindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    blitFramebuffer(0, 0, fb->Width, fb->Height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    SDL_GL_SwapWindow(window);
    glPopAttrib();
    bindFramebuffer(GL_FRAMEBUFFER, fb->FrameBuffers[fb->TextureSwapChainIndex]);
}
void ovrFramebuffer_Resolve(ovrFramebuffer* fb) {
    /* Explicit diagnostic readback of the image actually released to OpenXR. */
    static unsigned long resolves;
    const char* prefix = getenv("CSVR_XR_CAPTURE_PREFIX");
    const char* at = getenv("CSVR_XR_CAPTURE_FRAME");
    unsigned long frame = at ? strtoul(at, NULL, 10) : 600;
    unsigned long current = resolves++;
    if (prefix && current / 2 == frame) {
        char path[2048];
        snprintf(path, sizeof(path), "%s-%s.ppm", prefix, current % 2 ? "right" : "left");
        FILE* file = fopen(path, "wb");
        unsigned char* pixels = malloc((size_t)fb->Width * fb->Height * 3);
        if (file && pixels) {
            GLint pack, buffer;
            glGetIntegerv(GL_PACK_ALIGNMENT, &pack);
            glGetIntegerv(GL_READ_BUFFER, &buffer);
            glPixelStorei(GL_PACK_ALIGNMENT, 1);
            glReadBuffer(GL_COLOR_ATTACHMENT0);
            glReadPixels(0, 0, fb->Width, fb->Height, GL_RGB, GL_UNSIGNED_BYTE, pixels);
            fprintf(file, "P6\n%d %d\n255\n", fb->Width, fb->Height);
            for (int y = fb->Height - 1; y >= 0; --y)
                fwrite(pixels + (size_t)y * fb->Width * 3, 3, fb->Width, file);
            glPixelStorei(GL_PACK_ALIGNMENT, pack);
            glReadBuffer(buffer);
            printf("OpenXR eye readback: %s (%dx%d)\n", path, fb->Width, fb->Height);
        }
        if (file) fclose(file);
        free(pixels);
    }
    glFlush();
}
void ovrFramebuffer_Release(ovrFramebuffer* fb) {
    XrSwapchainImageReleaseInfo release = {XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
    Require(OXR(xrReleaseSwapchainImage(fb->ColorSwapChain.Handle, &release)));
}
void ovrRenderer_MouseCursor(ovrRenderer* renderer, int x, int y, int sx, int sy) {
    glPushAttrib(GL_COLOR_BUFFER_BIT | GL_ENABLE_BIT | GL_SCISSOR_BIT | GL_VIEWPORT_BIT);
    glEnable(GL_SCISSOR_TEST); glScissor(x, y, sx, sy);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glClearColor(1, 1, 1, 1); glClear(GL_COLOR_BUFFER_BIT);
    glPopAttrib();
}
#endif
