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
    test_clamp_int();
    test_timer();

    return 0;
}
