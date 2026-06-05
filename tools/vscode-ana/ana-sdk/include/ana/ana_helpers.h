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

typedef struct ANA_RectList {
    ANA_Rect* rects;
    int count;
    int capacity;
    ANA_Rect bounds;
    int bounds_enabled;
    int merge_slack;
} ANA_RectList;

typedef struct ANA_Timer {
    int ticks;
    int interval;
} ANA_Timer;

typedef struct ANA_Camera {
    int x;
    int y;
    int view_x;
    int view_y;
    int view_w;
    int view_h;
    int world_w;
    int world_h;
    int snap_x;
    int snap_y;
} ANA_Camera;

ANA_Rect ana_rect_make(int x, int y, int w, int h);
ANA_Rect ana_rect_clip(ANA_Rect rect, ANA_Rect bounds);
ANA_Rect ana_rect_union(ANA_Rect a, ANA_Rect b);
ANA_Rect ana_rect_align_x8(ANA_Rect rect, int min_x, int max_x);
int ana_rect_is_empty(ANA_Rect rect);
int ana_rect_contains(ANA_Rect outer, ANA_Rect inner);
int ana_rect_intersects(ANA_Rect a, ANA_Rect b);
int ana_rect_should_merge(ANA_Rect a, ANA_Rect b, int merge_slack);
void ana_rect_list_init(ANA_RectList* list, ANA_Rect* rects, int capacity);
void ana_rect_list_set_bounds(ANA_RectList* list, ANA_Rect bounds);
void ana_rect_list_set_merge_slack(ANA_RectList* list, int merge_slack);
void ana_rect_list_clear(ANA_RectList* list);
int ana_rect_list_count(const ANA_RectList* list);
ANA_Rect ana_rect_list_rect(const ANA_RectList* list, int index);
void ana_rect_list_add(ANA_RectList* list, ANA_Rect rect);
void ana_rect_list_add_padded(ANA_RectList* list, ANA_Rect rect, int padding);
int ana_clamp_int(int value, int min_value, int max_value);
ANA_Rect ana_tile_rect_for_world_rect(
    ANA_Rect world_rect,
    int tile_width,
    int tile_height);
void ana_path_join(
    char* path,
    int capacity,
    const char* root,
    const char* name);
void ana_camera_init(
    ANA_Camera* camera,
    int view_x,
    int view_y,
    int view_w,
    int view_h,
    int world_w,
    int world_h);
void ana_camera_set_snap(ANA_Camera* camera, int snap_x, int snap_y);
void ana_camera_set_position(ANA_Camera* camera, int x, int y);
void ana_camera_scroll_by(ANA_Camera* camera, int dx, int dy);
void ana_camera_follow_rect(
    ANA_Camera* camera,
    ANA_Rect target,
    int margin_x,
    int margin_y);
ANA_Rect ana_camera_world_view(const ANA_Camera* camera);
ANA_Rect ana_camera_world_to_screen_rect(
    const ANA_Camera* camera,
    ANA_Rect world_rect);
void ana_timer_reset(ANA_Timer* timer, int interval);
int ana_timer_tick(ANA_Timer* timer);

#ifdef __cplusplus
}
#endif

#endif
