/* Verify the actual Windows XR context supports the legacy engine renderer. */
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <SDL_opengl.h>
#include "VrDesktopGL.h"
#include <stdio.h>
#include <stdlib.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%s:%d: %s (%s)\n", __FILE__, __LINE__, #x, SDL_GetError()); exit(1); } } while (0)
int main(void) {
    SDL_SetMainReady();
    CHECK(SDL_Init(SDL_INIT_VIDEO) == 0);
    VR_DesktopGLAttributes();
    SDL_Window* window = SDL_CreateWindow("CSVR Windows XR GL context", 0, 0, 64, 64, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
    CHECK(window);
    SDL_GLContext context = SDL_GL_CreateContext(window);
    CHECK(context);
    const char* version = (const char*)glGetString(GL_VERSION);
    int major = 0, minor = 0, profile = 0;
    CHECK(version && sscanf(version, "%d.%d", &major, &minor) == 2);
    CHECK(major > 4 || (major == 4 && minor >= 5));
    glGetIntegerv(GL_CONTEXT_PROFILE_MASK, &profile);
    CHECK(profile & GL_CONTEXT_COMPATIBILITY_PROFILE_BIT);
    glViewport(0, 0, 64, 64);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    glColor3f(1, 0, 0);
    glBegin(GL_TRIANGLES);
    glVertex2f(-1, -1); glVertex2f(1, -1); glVertex2f(0, 1);
    glEnd();
    unsigned char pixel[4] = {0};
    glReadPixels(32, 32, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    CHECK(glGetError() == GL_NO_ERROR && pixel[0] >= 240 && pixel[1] < 10 && pixel[2] < 10);
    printf("PASS: real Windows OpenGL %s compatibility context and fixed-function triangle readback\n", version);
    SDL_GL_DeleteContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
