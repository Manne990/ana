#include "ana/ana_helpers.h"

static int ana_positive_interval(int interval)
{
    return interval > 0 ? interval : 1;
}

int ana_rect_intersects(ANA_Rect a, ANA_Rect b)
{
    if (a.w <= 0 || a.h <= 0 || b.w <= 0 || b.h <= 0) {
        return 0;
    }

    return a.x < b.x + b.w &&
        b.x < a.x + a.w &&
        a.y < b.y + b.h &&
        b.y < a.y + a.h;
}

int ana_clamp_int(int value, int min_value, int max_value)
{
    int tmp;

    if (min_value > max_value) {
        tmp = min_value;
        min_value = max_value;
        max_value = tmp;
    }

    if (value < min_value) {
        return min_value;
    }

    if (value > max_value) {
        return max_value;
    }

    return value;
}

void ana_timer_reset(ANA_Timer* timer, int interval)
{
    if (timer == 0) {
        return;
    }

    timer->ticks = 0;
    timer->interval = ana_positive_interval(interval);
}

int ana_timer_tick(ANA_Timer* timer)
{
    if (timer == 0) {
        return 0;
    }

    timer->interval = ana_positive_interval(timer->interval);
    timer->ticks++;
    if (timer->ticks >= timer->interval) {
        timer->ticks = 0;
        return 1;
    }

    return 0;
}
