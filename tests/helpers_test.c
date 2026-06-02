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
    assert(ana_rect_should_merge(
        ana_rect_make(0, 0, 4, 4),
        ana_rect_make(4, 0, 4, 4),
        0));
    assert(!ana_rect_should_merge(
        ana_rect_make(0, 0, 4, 4),
        ana_rect_make(10, 0, 4, 4),
        0));

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

static void test_rect_list(void)
{
    ANA_Rect storage[4];
    ANA_RectList list;
    ANA_Rect rect;

    ana_rect_list_init(&list, storage, 4);
    ana_rect_list_set_bounds(&list, ana_rect_make(0, 0, 100, 100));
    ana_rect_list_set_merge_slack(&list, 0);

    ana_rect_list_add(&list, ana_rect_make(10, 10, 10, 10));
    assert(ana_rect_list_count(&list) == 1);
    rect = ana_rect_list_rect(&list, 0);
    assert(rect.x == 10);
    assert(rect.y == 10);
    assert(rect.w == 10);
    assert(rect.h == 10);

    ana_rect_list_add(&list, ana_rect_make(12, 12, 3, 3));
    assert(ana_rect_list_count(&list) == 1);

    ana_rect_list_add(&list, ana_rect_make(18, 10, 10, 10));
    assert(ana_rect_list_count(&list) == 1);
    rect = ana_rect_list_rect(&list, 0);
    assert(rect.x == 10);
    assert(rect.y == 10);
    assert(rect.w == 18);
    assert(rect.h == 10);

    ana_rect_list_add_padded(&list, ana_rect_make(98, 98, 10, 10), 2);
    assert(ana_rect_list_count(&list) == 2);
    rect = ana_rect_list_rect(&list, 1);
    assert(rect.x == 96);
    assert(rect.y == 96);
    assert(rect.w == 4);
    assert(rect.h == 4);

    ana_rect_list_clear(&list);
    assert(ana_rect_list_count(&list) == 0);
}

static void test_tile_rect_helpers(void)
{
    ANA_Rect tiles;

    tiles = ana_tile_rect_for_world_rect(ana_rect_make(0, 0, 16, 16), 16, 16);
    assert(tiles.x == 0);
    assert(tiles.y == 0);
    assert(tiles.w == 1);
    assert(tiles.h == 1);

    tiles = ana_tile_rect_for_world_rect(ana_rect_make(15, 31, 2, 2), 16, 16);
    assert(tiles.x == 0);
    assert(tiles.y == 1);
    assert(tiles.w == 2);
    assert(tiles.h == 2);

    tiles = ana_tile_rect_for_world_rect(ana_rect_make(-2, -1, 4, 2), 16, 16);
    assert(tiles.x == -1);
    assert(tiles.y == -1);
    assert(tiles.w == 2);
    assert(tiles.h == 2);

    tiles = ana_tile_rect_for_world_rect(ana_rect_make(0, 0, 0, 4), 16, 16);
    assert(ana_rect_is_empty(tiles));
}

static void test_path_join(void)
{
    char path[16];

    ana_path_join(path, (int)sizeof(path), "assets/", "theme.mod");
    assert(path[0] == 'a');
    assert(path[7] == 't');
    assert(path[14] == 'o');
    assert(path[15] == '\0');

    ana_path_join(path, 5, "abc", "def");
    assert(path[0] == 'a');
    assert(path[1] == 'b');
    assert(path[2] == 'c');
    assert(path[3] == 'd');
    assert(path[4] == '\0');
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

static void test_camera(void)
{
    ANA_Camera camera;
    ANA_Rect rect;

    ana_camera_init(&camera, 0, 16, 320, 240, 1024, 256);
    assert(camera.x == 0);
    assert(camera.y == 0);
    assert(camera.view_x == 0);
    assert(camera.view_y == 16);
    assert(camera.view_w == 320);
    assert(camera.view_h == 240);
    assert(camera.world_w == 1024);
    assert(camera.world_h == 256);
    assert(camera.snap_x == 1);
    assert(camera.snap_y == 1);

    ana_camera_set_position(&camera, 200, 5);
    assert(camera.x == 200);
    assert(camera.y == 5);

    ana_camera_set_position(&camera, 900, 99);
    assert(camera.x == 704);
    assert(camera.y == 16);

    ana_camera_set_snap(&camera, 8, 4);
    ana_camera_set_position(&camera, 123, 9);
    assert(camera.x == 120);
    assert(camera.y == 8);

    ana_camera_scroll_by(&camera, 5, 4);
    assert(camera.x == 120);
    assert(camera.y == 12);

    rect = ana_camera_world_view(&camera);
    assert(rect.x == 120);
    assert(rect.y == 12);
    assert(rect.w == 320);
    assert(rect.h == 240);

    rect = ana_camera_world_to_screen_rect(
        &camera,
        ana_rect_make(130, 20, 10, 8));
    assert(rect.x == 10);
    assert(rect.y == 24);
    assert(rect.w == 10);
    assert(rect.h == 8);

    ana_camera_set_position(&camera, 703, 15);
    assert(camera.x == 696);
    assert(camera.y == 12);
    ana_camera_set_position(&camera, 704, 16);
    assert(camera.x == 704);
    assert(camera.y == 16);

    ana_camera_init(&camera, 4, 8, 100, 50, 300, 100);
    ana_camera_follow_rect(&camera, ana_rect_make(130, 30, 10, 8), 30, 10);
    assert(camera.x == 70);
    assert(camera.y == 0);
    ana_camera_follow_rect(&camera, ana_rect_make(20, 70, 10, 8), 30, 10);
    assert(camera.x == 0);
    assert(camera.y == 38);
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
    test_rect_list();
    test_tile_rect_helpers();
    test_path_join();
    test_clamp_int();
    test_camera();
    test_timer();

    return 0;
}
