/* Keyboard-style menu events from a controller, independent of game input. */
#ifndef XASH_XR_MENU_H
#define XASH_XR_MENU_H
#include <math.h>
enum { XR_MENU_UP=1, XR_MENU_DOWN=2, XR_MENU_LEFT=4, XR_MENU_RIGHT=8,
    XR_MENU_CONFIRM=16, XR_MENU_BACK=32 };
typedef struct { int direction, confirm, back; double repeatAt; } xr_menu_state;

static inline unsigned XRMenu_Update(xr_menu_state* s, int active, float x, float y,
    int confirm, int back, double now)
{
    unsigned events=0;
    int dir = fabsf(y)>=fabsf(x) ? (y>0.55f ? XR_MENU_UP : y<-.55f ? XR_MENU_DOWN : 0)
        : (x>.55f ? XR_MENU_RIGHT : x<-.55f ? XR_MENU_LEFT : 0);
    if (active) {
        if (dir && dir!=s->direction) { events|=dir; s->repeatAt=now+0.4; }
        else if (dir && now>=s->repeatAt) { events|=dir; s->repeatAt=now+0.12; }
        if (confirm && !s->confirm) events|=XR_MENU_CONFIRM;
        if (back && !s->back) events|=XR_MENU_BACK;
    } else s->repeatAt=now+0.4;
    s->direction=dir; s->confirm=confirm; s->back=back;
    return events;
}
static inline int XRMenu_NextSlot(unsigned mask, int current, int direction)
{
    for (int n=0;n<10;++n) {
        current=((current-1+direction+10)%10)+1;
        if (mask & (1u<<(current-1))) return current;
    }
    return 0;
}

typedef struct { float x1,y1,x2,y2; } xr_menu_rect;
/* Geometric focus for scripted panels, rows and separate ON/OFF cells. */
static inline int XRMenu_Neighbor(const xr_menu_rect* items, int count, int current, unsigned direction)
{
    if (current<0 || current>=count) return count ? 0 : -1;
    int vertical=(direction==XR_MENU_UP || direction==XR_MENU_DOWN);
    int sign=(direction==XR_MENU_UP || direction==XR_MENU_LEFT) ? -1 : 1;
    float x=(items[current].x1+items[current].x2)*.5f, y=(items[current].y1+items[current].y2)*.5f;
    float best=1e30f;
    int result=current;
    for (int i=0;i<count;++i) {
        float dx=(items[i].x1+items[i].x2)*.5f-x, dy=(items[i].y1+items[i].y2)*.5f-y;
        float primary=sign*(vertical ? dy : dx), cross=fabsf(vertical ? dx : dy);
        if (i==current || primary<.0001f) continue;
        if (vertical && fminf(items[i].x2,items[current].x2)-fmaxf(items[i].x1,items[current].x1)<.0001f) continue;
        float score=primary+5*cross;
        if (score<best) { best=score; result=i; }
    }
    if (result==current && vertical) {
        /* Wrap within this column, never into a neighboring submenu. */
        float extreme=sign>0 ? 1e30f : -1e30f;
        for (int i=0;i<count;++i) {
            if (fminf(items[i].x2,items[current].x2)-fmaxf(items[i].x1,items[current].x1)<.0001f) continue;
            float value=(items[i].y1+items[i].y2)*.5f;
            if ((sign>0 && value<extreme) || (sign<0 && value>extreme)) { extreme=value; result=i; }
        }
    }
    return result;
}
#endif
