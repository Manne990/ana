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

int ana_rect_intersects(ANA_Rect a, ANA_Rect b);
int ana_clamp_int(int value, int min_value, int max_value);
void ana_timer_reset(ANA_Timer* timer, int interval);
int ana_timer_tick(ANA_Timer* timer);

#ifdef __cplusplus
}
#endif

#endif
