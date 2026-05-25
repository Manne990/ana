#ifndef ANA_HELPERS_H
#define ANA_HELPERS_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ANA_Rect {
    int x;
    int y;
    int w;
    int h;
} ANA_Rect;

typedef struct ANA_Timer {
    int ticks;
    int interval;
} ANA_Timer;

ANA_Rect ana_rect_make(int x, int y, int w, int h);
ANA_Rect ana_rect_clip(ANA_Rect rect, ANA_Rect bounds);
ANA_Rect ana_rect_union(ANA_Rect a, ANA_Rect b);
ANA_Rect ana_rect_align_x8(ANA_Rect rect, int min_x, int max_x);
int ana_rect_is_empty(ANA_Rect rect);
int ana_rect_contains(ANA_Rect outer, ANA_Rect inner);
int ana_rect_intersects(ANA_Rect a, ANA_Rect b);
int ana_clamp_int(int value, int min_value, int max_value);
void ana_timer_reset(ANA_Timer* timer, int interval);
int ana_timer_tick(ANA_Timer* timer);

#ifdef __cplusplus
}
#endif

#endif
