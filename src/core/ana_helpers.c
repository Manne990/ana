#include "ana/ana_helpers.h"

static int ana_positive_interval(int interval)
{
    return interval > 0 ? interval : 1;
}

static int ana_positive_snap(int snap)
{
    return snap > 1 ? snap : 1;
}

static int ana_floor_to_snap(int value, int snap)
{
    int remainder;

    snap = ana_positive_snap(snap);
    if (snap <= 1) {
        return value;
    }

    remainder = value % snap;
    if (remainder < 0) {
        remainder += snap;
    }

    return value - remainder;
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

static int ana_camera_max_x(const ANA_Camera* camera)
{
    int max_x;

    if (camera == 0) {
        return 0;
    }

    max_x = camera->world_w - camera->view_w;
    return max_x > 0 ? max_x : 0;
}

static int ana_camera_max_y(const ANA_Camera* camera)
{
    int max_y;

    if (camera == 0) {
        return 0;
    }

    max_y = camera->world_h - camera->view_h;
    return max_y > 0 ? max_y : 0;
}

void ana_camera_init(
    ANA_Camera* camera,
    int view_x,
    int view_y,
    int view_w,
    int view_h,
    int world_w,
    int world_h)
{
    if (camera == 0) {
        return;
    }

    camera->x = 0;
    camera->y = 0;
    camera->view_x = view_x;
    camera->view_y = view_y;
    camera->view_w = view_w > 0 ? view_w : 0;
    camera->view_h = view_h > 0 ? view_h : 0;
    camera->world_w = world_w > 0 ? world_w : camera->view_w;
    camera->world_h = world_h > 0 ? world_h : camera->view_h;
    camera->snap_x = 1;
    camera->snap_y = 1;
}

void ana_camera_set_snap(ANA_Camera* camera, int snap_x, int snap_y)
{
    if (camera == 0) {
        return;
    }

    camera->snap_x = ana_positive_snap(snap_x);
    camera->snap_y = ana_positive_snap(snap_y);
    ana_camera_set_position(camera, camera->x, camera->y);
}

void ana_camera_set_position(ANA_Camera* camera, int x, int y)
{
    int max_x;
    int max_y;

    if (camera == 0) {
        return;
    }

    max_x = ana_camera_max_x(camera);
    max_y = ana_camera_max_y(camera);
    x = ana_clamp_int(x, 0, max_x);
    y = ana_clamp_int(y, 0, max_y);

    if (camera->snap_x > 1 && x != max_x) {
        x = ana_floor_to_snap(x, camera->snap_x);
    }
    if (camera->snap_y > 1 && y != max_y) {
        y = ana_floor_to_snap(y, camera->snap_y);
    }

    camera->x = x;
    camera->y = y;
}

void ana_camera_scroll_by(ANA_Camera* camera, int dx, int dy)
{
    if (camera == 0) {
        return;
    }

    ana_camera_set_position(camera, camera->x + dx, camera->y + dy);
}

void ana_camera_follow_rect(
    ANA_Camera* camera,
    ANA_Rect target,
    int margin_x,
    int margin_y)
{
    int x;
    int y;

    if (camera == 0) {
        return;
    }

    if (margin_x < 0) {
        margin_x = 0;
    }
    if (margin_y < 0) {
        margin_y = 0;
    }

    x = camera->x;
    y = camera->y;

    if (target.x < x + margin_x) {
        x = target.x - margin_x;
    } else if (target.x + target.w > x + camera->view_w - margin_x) {
        x = target.x + target.w - camera->view_w + margin_x;
    }

    if (target.y < y + margin_y) {
        y = target.y - margin_y;
    } else if (target.y + target.h > y + camera->view_h - margin_y) {
        y = target.y + target.h - camera->view_h + margin_y;
    }

    ana_camera_set_position(camera, x, y);
}

ANA_Rect ana_camera_world_view(const ANA_Camera* camera)
{
    if (camera == 0) {
        return ana_rect_make(0, 0, 0, 0);
    }

    return ana_rect_make(camera->x, camera->y, camera->view_w, camera->view_h);
}

ANA_Rect ana_camera_world_to_screen_rect(
    const ANA_Camera* camera,
    ANA_Rect world_rect)
{
    if (camera == 0) {
        return ana_rect_make(world_rect.x, world_rect.y, world_rect.w, world_rect.h);
    }

    return ana_rect_make(
        world_rect.x - camera->x + camera->view_x,
        world_rect.y - camera->y + camera->view_y,
        world_rect.w,
        world_rect.h);
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
