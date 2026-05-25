#include "ana/ana_helpers.h"

static int ana_positive_interval(int interval)
{
    return interval > 0 ? interval : 1;
}

static int ana_floor_x8(int value)
{
    int remainder;

    remainder = value % 8;
    if (remainder < 0) {
        remainder += 8;
    }

    return value - remainder;
}

static int ana_ceil_x8(int value)
{
    int remainder;

    remainder = value % 8;
    if (remainder < 0) {
        remainder += 8;
    }
    if (remainder == 0) {
        return value;
    }

    return value + (8 - remainder);
}

ANA_Rect ana_rect_make(int x, int y, int w, int h)
{
    ANA_Rect rect;

    rect.x = x;
    rect.y = y;
    rect.w = w;
    rect.h = h;

    return rect;
}

int ana_rect_is_empty(ANA_Rect rect)
{
    return rect.w <= 0 || rect.h <= 0;
}

ANA_Rect ana_rect_clip(ANA_Rect rect, ANA_Rect bounds)
{
    int left;
    int top;
    int right;
    int bottom;
    int bounds_right;
    int bounds_bottom;

    if (ana_rect_is_empty(rect) || ana_rect_is_empty(bounds)) {
        return ana_rect_make(rect.x, rect.y, 0, 0);
    }

    right = rect.x + rect.w;
    bottom = rect.y + rect.h;
    bounds_right = bounds.x + bounds.w;
    bounds_bottom = bounds.y + bounds.h;

    left = rect.x > bounds.x ? rect.x : bounds.x;
    top = rect.y > bounds.y ? rect.y : bounds.y;
    right = right < bounds_right ? right : bounds_right;
    bottom = bottom < bounds_bottom ? bottom : bounds_bottom;

    if (right <= left || bottom <= top) {
        return ana_rect_make(left, top, 0, 0);
    }

    return ana_rect_make(left, top, right - left, bottom - top);
}

ANA_Rect ana_rect_union(ANA_Rect a, ANA_Rect b)
{
    int left;
    int top;
    int right;
    int bottom;
    int b_right;
    int b_bottom;

    if (ana_rect_is_empty(a)) {
        return b;
    }
    if (ana_rect_is_empty(b)) {
        return a;
    }

    right = a.x + a.w;
    bottom = a.y + a.h;
    b_right = b.x + b.w;
    b_bottom = b.y + b.h;

    left = a.x < b.x ? a.x : b.x;
    top = a.y < b.y ? a.y : b.y;
    right = right > b_right ? right : b_right;
    bottom = bottom > b_bottom ? bottom : b_bottom;

    return ana_rect_make(left, top, right - left, bottom - top);
}

int ana_rect_contains(ANA_Rect outer, ANA_Rect inner)
{
    if (ana_rect_is_empty(outer) || ana_rect_is_empty(inner)) {
        return 0;
    }

    return inner.x >= outer.x &&
        inner.y >= outer.y &&
        inner.x + inner.w <= outer.x + outer.w &&
        inner.y + inner.h <= outer.y + outer.h;
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

ANA_Rect ana_rect_align_x8(ANA_Rect rect, int min_x, int max_x)
{
    int tmp;
    int left;
    int right;

    if (min_x > max_x) {
        tmp = min_x;
        min_x = max_x;
        max_x = tmp;
    }

    if (ana_rect_is_empty(rect) || min_x == max_x) {
        return ana_rect_make(rect.x, rect.y, 0, 0);
    }

    left = ana_floor_x8(rect.x);
    right = ana_ceil_x8(rect.x + rect.w);

    if (left < min_x) {
        left = min_x;
    }
    if (right > max_x) {
        right = max_x;
    }

    if (right <= left) {
        return ana_rect_make(left, rect.y, 0, 0);
    }

    return ana_rect_make(left, rect.y, right - left, rect.h);
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
