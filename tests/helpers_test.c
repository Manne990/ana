#include "ana.h"

#include <assert.h>

static void test_rect_intersections(void)
{
    ANA_Rect a;
    ANA_Rect b;

    a.x = 10;
    a.y = 10;
    a.w = 8;
    a.h = 8;

    b.x = 17;
    b.y = 17;
    b.w = 8;
    b.h = 8;
    assert(ana_rect_intersects(a, b));

    b.x = 18;
    b.y = 10;
    assert(!ana_rect_intersects(a, b));

    b.x = 10;
    b.y = 18;
    assert(!ana_rect_intersects(a, b));

    b.x = 12;
    b.y = 12;
    b.w = 0;
    b.h = 8;
    assert(!ana_rect_intersects(a, b));
}

static void test_rect_helpers(void)
{
    ANA_Rect rect;
    ANA_Rect bounds;
    ANA_Rect clipped;
    ANA_Rect merged;
    ANA_Rect aligned;

    rect = ana_rect_make(3, 4, 5, 6);
    assert(rect.x == 3);
    assert(rect.y == 4);
    assert(rect.w == 5);
    assert(rect.h == 6);
    assert(!ana_rect_is_empty(rect));
    assert(ana_rect_is_empty(ana_rect_make(0, 0, 0, 5)));
    assert(ana_rect_is_empty(ana_rect_make(0, 0, 5, -1)));

    bounds = ana_rect_make(0, 0, 10, 10);
    clipped = ana_rect_clip(ana_rect_make(-2, 5, 6, 7), bounds);
    assert(clipped.x == 0);
    assert(clipped.y == 5);
    assert(clipped.w == 4);
    assert(clipped.h == 5);

    clipped = ana_rect_clip(ana_rect_make(20, 20, 4, 4), bounds);
    assert(ana_rect_is_empty(clipped));

    merged = ana_rect_union(
        ana_rect_make(1, 2, 3, 4),
        ana_rect_make(4, 1, 2, 2));
    assert(merged.x == 1);
    assert(merged.y == 1);
    assert(merged.w == 5);
    assert(merged.h == 5);

    assert(ana_rect_contains(bounds, ana_rect_make(2, 2, 4, 4)));
    assert(!ana_rect_contains(bounds, ana_rect_make(8, 8, 4, 4)));
    assert(!ana_rect_contains(bounds, ana_rect_make(8, 8, 0, 4)));

    aligned = ana_rect_align_x8(ana_rect_make(5, 7, 9, 3), 0, 320);
    assert(aligned.x == 0);
    assert(aligned.y == 7);
    assert(aligned.w == 16);
    assert(aligned.h == 3);

    aligned = ana_rect_align_x8(ana_rect_make(317, 0, 8, 2), 0, 320);
    assert(aligned.x == 312);
    assert(aligned.w == 8);

    aligned = ana_rect_align_x8(ana_rect_make(10, 0, 4, 2), 64, 0);
    assert(aligned.x == 8);
    assert(aligned.w == 8);

    aligned = ana_rect_align_x8(ana_rect_make(-3, 0, 6, 2), -16, 16);
    assert(aligned.x == -8);
    assert(aligned.w == 16);
}

static void test_clamp_int(void)
{
    assert(ana_clamp_int(5, 0, 10) == 5);
    assert(ana_clamp_int(-2, 0, 10) == 0);
    assert(ana_clamp_int(12, 0, 10) == 10);
    assert(ana_clamp_int(5, 10, 0) == 5);
    assert(ana_clamp_int(-2, 10, 0) == 0);
    assert(ana_clamp_int(12, 10, 0) == 10);
}

static void test_timer(void)
{
    ANA_Timer timer;

    ana_timer_reset(&timer, 3);
    assert(timer.ticks == 0);
    assert(timer.interval == 3);
    assert(!ana_timer_tick(&timer));
    assert(!ana_timer_tick(&timer));
    assert(ana_timer_tick(&timer));
    assert(timer.ticks == 0);

    timer.interval = 2;
    assert(!ana_timer_tick(&timer));
    assert(ana_timer_tick(&timer));

    ana_timer_reset(&timer, 0);
    assert(timer.interval == 1);
    assert(ana_timer_tick(&timer));

    ana_timer_reset(0, 3);
    assert(!ana_timer_tick(0));
}

int main(void)
{
    test_rect_intersections();
    test_rect_helpers();
    test_clamp_int();
    test_timer();

    return 0;
}
