/* Shared desktop HUD plane geometry. Row-major matrices, OpenXR coordinates. */
#ifndef XASH_XR_UI_H
#define XASH_XR_UI_H
#include <math.h>

static inline void XRUI_Projection(float m[4][4], float pixelsX, float pixelsY,
    float distance, float width, const float eye[3], const float q[4], const float fov[4])
{
    float x=q[0], y=q[1], z=q[2], w=q[3];
    float r[3][3] = {{1-2*(y*y+z*z), 2*(x*y-z*w), 2*(x*z+y*w)},
        {2*(x*y+z*w), 1-2*(x*x+z*z), 2*(y*z-x*w)},
        {2*(x*z-y*w), 2*(y*z+x*w), 1-2*(x*x+y*y)}};
    float height = width * pixelsY / pixelsX;
    float origin[3] = {-width/2-eye[0], height/2-eye[1], -distance-eye[2]};
    for (int col=0; col<4; ++col) {
        float v[3] = {0,0,0};
        for (int axis=0; axis<3; ++axis) {
            if (col==0) v[axis] = r[0][axis] * width/pixelsX;
            if (col==1) v[axis] = -r[1][axis] * height/pixelsY;
            if (col==3) for (int j=0;j<3;++j) v[axis] += r[j][axis]*origin[j];
        }
        m[0][col] = 2*v[0]/(fov[1]-fov[0]) + (fov[1]+fov[0])*v[2]/(fov[1]-fov[0]);
        m[1][col] = 2*v[1]/(fov[3]-fov[2]) + (fov[3]+fov[2])*v[2]/(fov[3]-fov[2]);
        m[2][col] = -(100.01f/99.99f)*v[2] - (col==3 ? 2.0f/99.99f : 0);
        m[3][col] = -v[2];
    }
}

/* Map an eye's world-projected reticle into this plane's pixel coordinates. */
static inline int XRUI_PixelFromNDC(const float m[4][4], float sx, float sy, float* px, float* py)
{
    float a=m[0][0]-sx*m[3][0], b=m[0][1]-sx*m[3][1], c=sx*m[3][3]-m[0][3];
    float d=m[1][0]-sy*m[3][0], e=m[1][1]-sy*m[3][1], f=sy*m[3][3]-m[1][3];
    float determinant=a*e-b*d;
    if (fabsf(determinant)<1e-12f) return 0;
    *px=(c*e-b*f)/determinant; *py=(a*f-c*d)/determinant;
    return isfinite(*px) && isfinite(*py);
}
#endif
