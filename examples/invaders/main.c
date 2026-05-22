#include "ana.h"

#include <stdio.h>
#include <string.h>

#define HUD_FONT_WIDTH 6
#define HUD_FONT_HEIGHT 7
#define HUD_FONT_FIRST_CHAR '0'
#define HUD_FONT_CHAR_COUNT 43
#define HUD_FONT_HEADER_SIZE 16
#define HUD_FONT_IMAGE_HEADER_SIZE 20
#define HUD_FONT_FRAME_SIZE (HUD_FONT_HEIGHT * 2)
#define HUD_FONT_DATA_SIZE \
    (HUD_FONT_HEADER_SIZE + HUD_FONT_IMAGE_HEADER_SIZE + \
        (HUD_FONT_CHAR_COUNT * HUD_FONT_FRAME_SIZE))
#define INVADERS_IMAGE_HEADER_SIZE 20
#define INVADERS_IMAGE_BITPLANES 4
#define INVADERS_MASKED_IMAGE_SIZE(width, height, frames) \
    (INVADERS_IMAGE_HEADER_SIZE + \
        ((frames) * ((((width) + 7) / 8) * (height)) * \
            (1 + INVADERS_IMAGE_BITPLANES)))

#define PLAYER_SPEED 2
#define BULLET_SPEED 4
#define PLAYER_BULLET_SLOTS 4
#define GAME_TICK_LIMIT 3000

#define INVADER_COLUMNS 6
#define INVADER_ROWS 3
#define INVADER_COUNT (INVADER_COLUMNS * INVADER_ROWS)
#define INVADER_WIDTH 16
#define INVADER_HEIGHT 8
#define INVADER_FRAMES 2
#define INVADER_SPACING_X 24
#define INVADER_SPACING_Y 18
#define INVADER_FORMATION_WIDTH \
    (INVADER_WIDTH + ((INVADER_COLUMNS - 1) * INVADER_SPACING_X))
#define INVADER_START_X 48
#define INVADER_START_Y 58
#define INVADER_STEP_PIXELS 2
#define INVADER_DROP_PIXELS 8
#define INVADER_MIN_X 8
#define INVADER_MAX_X (ANA_DEFAULT_WIDTH - 8)
#define INVADER_BASE_INTERVAL 18
#define INVADER_MIN_INTERVAL 6
#define INVADER_COLOR 3

#define EXPLOSION_WIDTH 16
#define EXPLOSION_HEIGHT 10
#define EXPLOSION_FRAMES 2
#define EXPLOSION_SLOTS 4
#define EXPLOSION_TICKS_PER_FRAME 5
#define EXPLOSION_TICKS (EXPLOSION_FRAMES * EXPLOSION_TICKS_PER_FRAME)

#define INVADERS_DRAW_SLOTS 2
#define INVADERS_REMOVED_ENEMY_SLOTS INVADER_COUNT
#define HUD_SCORE_X 16
#define HUD_SCORE_Y 16
#define HUD_LIVES_X 200
#define HUD_LIVES_Y 16
#define HUD_STATUS_X 16
#define HUD_STATUS_Y 30
#define HUD_SCORE_CLEAR_WIDTH 90
#define HUD_LIVES_CLEAR_WIDTH 70
#define HUD_STATUS_CLEAR_WIDTH 150
#define HUD_STATUS_TEXT_SIZE 16

typedef struct InvadersRect {
    int x;
    int y;
    int w;
    int h;
} InvadersRect;

typedef struct InvadersExplosion {
    int active;
    int x;
    int y;
    int age;
} InvadersExplosion;

typedef struct InvadersBullet {
    int active;
    int x;
    int y;
} InvadersBullet;

typedef struct InvadersDrawSlot {
    int has_player;
    int player_x;
    int player_y;
    InvadersBullet bullets[PLAYER_BULLET_SLOTS];
    InvadersExplosion explosions[EXPLOSION_SLOTS];
    int hud_drawn;
    int hud_score;
    int hud_lives;
    char hud_status[HUD_STATUS_TEXT_SIZE];
} InvadersDrawSlot;

static const unsigned char player_image_data[] = {
    0x41, 0x4e, 0x41, 0x49, 0x4d, 0x47, 0x30, 0x31,
    0x10, 0x00, 0x0a, 0x00, 0x01, 0x00, 0x04, 0x01,
    0x00, 0x00, 0x00, 0x00,
    0x03, 0x00, 0x07, 0x80, 0x0f, 0xc0, 0x1f, 0xe0,
    0x3f, 0xf0, 0x7f, 0xf8, 0xff, 0xfc, 0x7f, 0xf8,
    0x3b, 0x70, 0x04, 0x80,
    0x03, 0x00, 0x07, 0x80, 0x0f, 0xc0, 0x1f, 0xe0,
    0x3f, 0xf0, 0x7f, 0xf8, 0xff, 0xfc, 0x7f, 0xf8,
    0x38, 0x70, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x07, 0x80,
    0x0f, 0xc0, 0x1f, 0xe0, 0x3f, 0xf0, 0x10, 0x20,
    0x03, 0x00, 0x04, 0x80,
    0x03, 0x00, 0x07, 0x80, 0x0c, 0xc0, 0x18, 0x60,
    0x30, 0x30, 0x60, 0x18, 0xc0, 0x0c, 0x6f, 0xd8,
    0x38, 0x70, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00
};

static const unsigned char bullet_image_data[] = {
    0x41, 0x4e, 0x41, 0x49, 0x4d, 0x47, 0x30, 0x31,
    0x02, 0x00, 0x06, 0x00, 0x01, 0x00, 0x01, 0x01,
    0x00, 0x00, 0x00, 0x00,
    0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0,
    0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0
};

static const char* const invader_frame_0[INVADER_HEIGHT] = {
    "....##....##....",
    "...##########...",
    "..############..",
    ".###..####..###.",
    "################",
    "..##.##..##.##..",
    ".##..........##.",
    "...##......##..."
};

static const char* const invader_frame_1[INVADER_HEIGHT] = {
    "....##....##....",
    ".##############.",
    "################",
    "###..######..###",
    "################",
    "...###....###...",
    "..##..####..##..",
    "##............##"
};

static const char* const explosion_frame_0[EXPLOSION_HEIGHT] = {
    "................",
    "......#..#......",
    "...#..####..#...",
    "....########....",
    "..############..",
    "....########....",
    "...#..####..#...",
    "......#..#......",
    "................",
    "................"
};

static const char* const explosion_frame_1[EXPLOSION_HEIGHT] = {
    "....#......#....",
    ".#....#..#....#.",
    "...#........#...",
    ".....##..##.....",
    "#..##......##..#",
    ".....##..##.....",
    "...#........#...",
    ".#....#..#....#.",
    "....#......#....",
    "................"
};

static const unsigned char hud_font_glyphs[HUD_FONT_CHAR_COUNT][HUD_FONT_HEIGHT] = {
    { 0x70, 0x88, 0x98, 0xa8, 0xc8, 0x88, 0x70 },
    { 0x20, 0x60, 0x20, 0x20, 0x20, 0x20, 0x70 },
    { 0x70, 0x88, 0x08, 0x10, 0x20, 0x40, 0xf8 },
    { 0xf0, 0x08, 0x08, 0x70, 0x08, 0x08, 0xf0 },
    { 0x10, 0x30, 0x50, 0x90, 0xf8, 0x10, 0x10 },
    { 0xf8, 0x80, 0x80, 0xf0, 0x08, 0x08, 0xf0 },
    { 0x70, 0x80, 0x80, 0xf0, 0x88, 0x88, 0x70 },
    { 0xf8, 0x08, 0x10, 0x20, 0x40, 0x40, 0x40 },
    { 0x70, 0x88, 0x88, 0x70, 0x88, 0x88, 0x70 },
    { 0x70, 0x88, 0x88, 0x78, 0x08, 0x08, 0x70 },
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    { 0x70, 0x88, 0x88, 0xf8, 0x88, 0x88, 0x88 },
    { 0xf0, 0x88, 0x88, 0xf0, 0x88, 0x88, 0xf0 },
    { 0x78, 0x80, 0x80, 0x80, 0x80, 0x80, 0x78 },
    { 0xf0, 0x88, 0x88, 0x88, 0x88, 0x88, 0xf0 },
    { 0xf8, 0x80, 0x80, 0xf0, 0x80, 0x80, 0xf8 },
    { 0xf8, 0x80, 0x80, 0xf0, 0x80, 0x80, 0x80 },
    { 0x78, 0x80, 0x80, 0x98, 0x88, 0x88, 0x78 },
    { 0x88, 0x88, 0x88, 0xf8, 0x88, 0x88, 0x88 },
    { 0x70, 0x20, 0x20, 0x20, 0x20, 0x20, 0x70 },
    { 0x08, 0x08, 0x08, 0x08, 0x88, 0x88, 0x70 },
    { 0x88, 0x90, 0xa0, 0xc0, 0xa0, 0x90, 0x88 },
    { 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0xf8 },
    { 0x88, 0xd8, 0xa8, 0xa8, 0x88, 0x88, 0x88 },
    { 0x88, 0xc8, 0xa8, 0x98, 0x88, 0x88, 0x88 },
    { 0x70, 0x88, 0x88, 0x88, 0x88, 0x88, 0x70 },
    { 0xf0, 0x88, 0x88, 0xf0, 0x80, 0x80, 0x80 },
    { 0x70, 0x88, 0x88, 0x88, 0xa8, 0x90, 0x68 },
    { 0xf0, 0x88, 0x88, 0xf0, 0xa0, 0x90, 0x88 },
    { 0x78, 0x80, 0x80, 0x70, 0x08, 0x08, 0xf0 },
    { 0xf8, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20 },
    { 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x70 },
    { 0x88, 0x88, 0x88, 0x88, 0x88, 0x50, 0x20 },
    { 0x88, 0x88, 0x88, 0xa8, 0xa8, 0xa8, 0x50 },
    { 0x88, 0x88, 0x50, 0x20, 0x50, 0x88, 0x88 },
    { 0x88, 0x88, 0x50, 0x20, 0x20, 0x20, 0x20 },
    { 0xf8, 0x08, 0x10, 0x20, 0x40, 0x80, 0xf8 }
};

static unsigned char hud_font_data[HUD_FONT_DATA_SIZE];
static unsigned char invader_image_data[
    INVADERS_MASKED_IMAGE_SIZE(INVADER_WIDTH, INVADER_HEIGHT, INVADER_FRAMES)];
static unsigned char explosion_image_data[
    INVADERS_MASKED_IMAGE_SIZE(
        EXPLOSION_WIDTH,
        EXPLOSION_HEIGHT,
        EXPLOSION_FRAMES)];
static ANA_Image player_image = 0;
static ANA_Image bullet_image = 0;
static ANA_Image invader_image = 0;
static ANA_Image explosion_image = 0;
static ANA_Font hud_font = 0;
static unsigned char invader_alive[INVADER_ROWS][INVADER_COLUMNS];
static InvadersExplosion explosions[EXPLOSION_SLOTS];
static InvadersBullet player_bullets[PLAYER_BULLET_SLOTS];
static InvadersDrawSlot draw_slots[INVADERS_DRAW_SLOTS];
static InvadersRect removed_enemy_rects
    [INVADERS_DRAW_SLOTS][INVADERS_REMOVED_ENEMY_SLOTS];
static int removed_enemy_counts[INVADERS_DRAW_SLOTS];
static int formation_dirty_slots[INVADERS_DRAW_SLOTS] = { 1, 1 };
static int formation_drawn_slots[INVADERS_DRAW_SLOTS];
static int formation_slot_x[INVADERS_DRAW_SLOTS];
static int formation_slot_y[INVADERS_DRAW_SLOTS];
static int full_clear_slots[INVADERS_DRAW_SLOTS] = { 1, 1 };
static int player_x = 152;
static int player_y = 220;
static int invader_formation_x = INVADER_START_X;
static int invader_formation_y = INVADER_START_Y;
static int invader_direction = 1;
static int invader_frame = 0;
static int invader_move_timer = 0;
static int invaders_remaining = 0;
static int score = 0;
static int lives = 3;
static int game_over = 0;
static int demo_ticks = 0;
static int invaders_assets_loaded = 0;

static void invaders_write_u16_le(unsigned char* bytes, int value)
{
    bytes[0] = (unsigned char)(value & 0xff);
    bytes[1] = (unsigned char)((value >> 8) & 0xff);
}

static void invaders_write_image_header(
    unsigned char* bytes,
    int width,
    int height,
    int frames)
{
    bytes[0] = 'A';
    bytes[1] = 'N';
    bytes[2] = 'A';
    bytes[3] = 'I';
    bytes[4] = 'M';
    bytes[5] = 'G';
    bytes[6] = '0';
    bytes[7] = '1';
    invaders_write_u16_le(bytes + 8, width);
    invaders_write_u16_le(bytes + 10, height);
    invaders_write_u16_le(bytes + 12, frames);
    bytes[14] = INVADERS_IMAGE_BITPLANES;
    bytes[15] = 1u;
    bytes[16] = 0u;
    bytes[17] = 0u;
    bytes[18] = 0u;
    bytes[19] = 0u;
}

static void invaders_set_planar_bit(
    unsigned char* bytes,
    int row_bytes,
    int x,
    int y)
{
    bytes[(y * row_bytes) + (x >> 3)] =
        (unsigned char)(
            bytes[(y * row_bytes) + (x >> 3)] |
            (0x80u >> (x & 7)));
}

static void invaders_write_solid_frame(
    unsigned char* image_data,
    int width,
    int height,
    int frame,
    const char* const* rows,
    unsigned char color)
{
    unsigned char* frame_base;
    unsigned char* mask;
    unsigned char* planes;
    int row_bytes;
    int plane_size;
    int frame_size;
    int x;
    int y;
    int plane;

    row_bytes = (width + 7) / 8;
    plane_size = row_bytes * height;
    frame_size = plane_size * (1 + INVADERS_IMAGE_BITPLANES);
    frame_base =
        image_data + INVADERS_IMAGE_HEADER_SIZE + (frame * frame_size);
    mask = frame_base;
    planes = frame_base + plane_size;
    memset(frame_base, 0, (size_t)frame_size);

    color = (unsigned char)(color & 0x0f);
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            if (rows[y][x] != '.' && rows[y][x] != ' ') {
                invaders_set_planar_bit(mask, row_bytes, x, y);
                for (plane = 0; plane < INVADERS_IMAGE_BITPLANES; plane++) {
                    if ((color & (1u << plane)) != 0u) {
                        invaders_set_planar_bit(
                            planes + (plane * plane_size),
                            row_bytes,
                            x,
                            y);
                    }
                }
            }
        }
    }
}

static void invaders_build_sprite_data(void)
{
    invaders_write_image_header(
        invader_image_data,
        INVADER_WIDTH,
        INVADER_HEIGHT,
        INVADER_FRAMES);
    invaders_write_solid_frame(
        invader_image_data,
        INVADER_WIDTH,
        INVADER_HEIGHT,
        0,
        invader_frame_0,
        INVADER_COLOR);
    invaders_write_solid_frame(
        invader_image_data,
        INVADER_WIDTH,
        INVADER_HEIGHT,
        1,
        invader_frame_1,
        INVADER_COLOR);

    invaders_write_image_header(
        explosion_image_data,
        EXPLOSION_WIDTH,
        EXPLOSION_HEIGHT,
        EXPLOSION_FRAMES);
    invaders_write_solid_frame(
        explosion_image_data,
        EXPLOSION_WIDTH,
        EXPLOSION_HEIGHT,
        0,
        explosion_frame_0,
        5u);
    invaders_write_solid_frame(
        explosion_image_data,
        EXPLOSION_WIDTH,
        EXPLOSION_HEIGHT,
        1,
        explosion_frame_1,
        2u);
}

static void invaders_build_hud_font_data(void)
{
    unsigned char* out;
    int glyph;
    int row;

    hud_font_data[0] = 'A';
    hud_font_data[1] = 'N';
    hud_font_data[2] = 'A';
    hud_font_data[3] = 'F';
    hud_font_data[4] = 'N';
    hud_font_data[5] = 'T';
    hud_font_data[6] = '0';
    hud_font_data[7] = '1';
    invaders_write_u16_le(hud_font_data + 8, HUD_FONT_WIDTH);
    invaders_write_u16_le(hud_font_data + 10, HUD_FONT_HEIGHT);
    hud_font_data[12] = HUD_FONT_FIRST_CHAR;
    hud_font_data[13] = HUD_FONT_CHAR_COUNT;
    hud_font_data[14] = 0u;
    hud_font_data[15] = 0u;

    hud_font_data[16] = 'A';
    hud_font_data[17] = 'N';
    hud_font_data[18] = 'A';
    hud_font_data[19] = 'I';
    hud_font_data[20] = 'M';
    hud_font_data[21] = 'G';
    hud_font_data[22] = '0';
    hud_font_data[23] = '1';
    invaders_write_u16_le(hud_font_data + 24, HUD_FONT_WIDTH);
    invaders_write_u16_le(hud_font_data + 26, HUD_FONT_HEIGHT);
    invaders_write_u16_le(hud_font_data + 28, HUD_FONT_CHAR_COUNT);
    hud_font_data[30] = 1u;
    hud_font_data[31] = 1u;
    hud_font_data[32] = 0u;
    hud_font_data[33] = 0u;
    hud_font_data[34] = 0u;
    hud_font_data[35] = 0u;

    out = hud_font_data + HUD_FONT_HEADER_SIZE + HUD_FONT_IMAGE_HEADER_SIZE;
    for (glyph = 0; glyph < HUD_FONT_CHAR_COUNT; glyph++) {
        for (row = 0; row < HUD_FONT_HEIGHT; row++) {
            *out++ = hud_font_glyphs[glyph][row];
        }

        for (row = 0; row < HUD_FONT_HEIGHT; row++) {
            *out++ = hud_font_glyphs[glyph][row];
        }
    }
}

static void invaders_reset_explosions(void)
{
    int i;

    for (i = 0; i < EXPLOSION_SLOTS; i++) {
        explosions[i].active = 0;
        explosions[i].x = 0;
        explosions[i].y = 0;
        explosions[i].age = 0;
    }
}

static void invaders_reset_player_bullets(void)
{
    int i;

    for (i = 0; i < PLAYER_BULLET_SLOTS; i++) {
        player_bullets[i].active = 0;
        player_bullets[i].x = 0;
        player_bullets[i].y = 0;
    }
}

static void invaders_mark_formation_dirty(void)
{
    int i;

    for (i = 0; i < INVADERS_DRAW_SLOTS; i++) {
        formation_dirty_slots[i] = 1;
        removed_enemy_counts[i] = 0;
    }
}

static void invaders_reset_draw_slots(void)
{
    int i;

    memset(draw_slots, 0, sizeof(draw_slots));
    memset(removed_enemy_rects, 0, sizeof(removed_enemy_rects));
    for (i = 0; i < INVADERS_DRAW_SLOTS; i++) {
        removed_enemy_counts[i] = 0;
        formation_dirty_slots[i] = 1;
        formation_drawn_slots[i] = 0;
        formation_slot_x[i] = INVADER_START_X;
        formation_slot_y[i] = INVADER_START_Y;
        full_clear_slots[i] = 1;
    }
}

static void invaders_reset_formation(void)
{
    int row;
    int col;

    invader_formation_x = INVADER_START_X;
    invader_formation_y = INVADER_START_Y;
    invader_direction = 1;
    invader_frame = 0;
    invader_move_timer = 0;
    invaders_remaining = 0;

    for (row = 0; row < INVADER_ROWS; row++) {
        for (col = 0; col < INVADER_COLUMNS; col++) {
            invader_alive[row][col] = 1u;
            invaders_remaining++;
        }
    }

    invaders_mark_formation_dirty();
}

static void invaders_reset_game_state(void)
{
    player_x = 152;
    player_y = 220;
    score = 0;
    lives = 3;
    game_over = 0;
    demo_ticks = 0;

    invaders_reset_formation();
    invaders_reset_explosions();
    invaders_reset_player_bullets();
    invaders_reset_draw_slots();
}

static int invaders_rects_intersect(InvadersRect a, InvadersRect b)
{
    return a.x < b.x + b.w &&
        b.x < a.x + a.w &&
        a.y < b.y + b.h &&
        b.y < a.y + a.h;
}

static int invaders_enemy_x(int col)
{
    return invader_formation_x + (col * INVADER_SPACING_X);
}

static int invaders_enemy_y(int row)
{
    return invader_formation_y + (row * INVADER_SPACING_Y);
}

static void invaders_queue_removed_enemy_rect(int x, int y)
{
    InvadersRect rect;
    int slot;
    int count;

    rect.x = x;
    rect.y = y;
    rect.w = INVADER_WIDTH;
    rect.h = INVADER_HEIGHT;

    for (slot = 0; slot < INVADERS_DRAW_SLOTS; slot++) {
        count = removed_enemy_counts[slot];
        if (count < INVADERS_REMOVED_ENEMY_SLOTS) {
            removed_enemy_rects[slot][count] = rect;
            removed_enemy_counts[slot] = count + 1;
        } else {
            formation_dirty_slots[slot] = 1;
        }
    }
}

static void invaders_spawn_explosion(int x, int y)
{
    int i;
    int slot;

    slot = 0;
    for (i = 0; i < EXPLOSION_SLOTS; i++) {
        if (!explosions[i].active) {
            slot = i;
            break;
        }
    }

    explosions[slot].active = 1;
    explosions[slot].x = x;
    explosions[slot].y = y - 1;
    explosions[slot].age = 0;
}

static void invaders_update_explosions(void)
{
    int i;

    for (i = 0; i < EXPLOSION_SLOTS; i++) {
        if (explosions[i].active) {
            explosions[i].age++;
            if (explosions[i].age >= EXPLOSION_TICKS) {
                explosions[i].active = 0;
            }
        }
    }
}

static int invaders_move_interval(void)
{
    int removed;
    int interval;

    removed = INVADER_COUNT - invaders_remaining;
    interval = INVADER_BASE_INTERVAL - (removed / 2);
    if (interval < INVADER_MIN_INTERVAL) {
        interval = INVADER_MIN_INTERVAL;
    }

    return interval;
}

static void invaders_update_formation(void)
{
    int row;
    int col;
    int left;
    int right;
    int enemy_x;
    int enemy_y;

    if (game_over || invaders_remaining <= 0) {
        return;
    }

    invader_move_timer++;
    if (invader_move_timer < invaders_move_interval()) {
        return;
    }

    invader_move_timer = 0;
    left = ANA_DEFAULT_WIDTH;
    right = 0;

    for (row = 0; row < INVADER_ROWS; row++) {
        for (col = 0; col < INVADER_COLUMNS; col++) {
            if (invader_alive[row][col]) {
                enemy_x = invaders_enemy_x(col);
                if (enemy_x < left) {
                    left = enemy_x;
                }
                if (enemy_x + INVADER_WIDTH > right) {
                    right = enemy_x + INVADER_WIDTH;
                }
            }
        }
    }

    if ((invader_direction < 0 && left <= INVADER_MIN_X) ||
            (invader_direction > 0 && right >= INVADER_MAX_X)) {
        invader_direction = -invader_direction;
        invader_formation_y += INVADER_DROP_PIXELS;
    } else {
        invader_formation_x += invader_direction * INVADER_STEP_PIXELS;
    }

    invader_frame = 1 - invader_frame;
    invaders_mark_formation_dirty();
    enemy_y = invader_formation_y +
        ((INVADER_ROWS - 1) * INVADER_SPACING_Y) +
        INVADER_HEIGHT;
    if (enemy_y >= player_y - 4) {
        lives = 0;
        game_over = 1;
        invaders_reset_player_bullets();
    }
}

static int invaders_enemy_score(int row)
{
    return (INVADER_ROWS - row) * 10;
}

static void invaders_check_bullet_hits(int bullet_width, int bullet_height)
{
    InvadersRect bullet_rect;
    InvadersRect enemy_rect;
    int bullet;
    int row;
    int col;
    int enemy_x;
    int enemy_y;

    if (game_over || invaders_remaining <= 0) {
        return;
    }

    bullet_rect.w = bullet_width;
    bullet_rect.h = bullet_height;

    for (bullet = 0; bullet < PLAYER_BULLET_SLOTS; bullet++) {
        if (!player_bullets[bullet].active) {
            continue;
        }

        bullet_rect.x = player_bullets[bullet].x;
        bullet_rect.y = player_bullets[bullet].y;

        for (row = 0; row < INVADER_ROWS; row++) {
            for (col = 0; col < INVADER_COLUMNS; col++) {
                if (!invader_alive[row][col]) {
                    continue;
                }

                enemy_x = invaders_enemy_x(col);
                enemy_y = invaders_enemy_y(row);
                enemy_rect.x = enemy_x;
                enemy_rect.y = enemy_y;
                enemy_rect.w = INVADER_WIDTH;
                enemy_rect.h = INVADER_HEIGHT;

                if (invaders_rects_intersect(bullet_rect, enemy_rect)) {
                    invader_alive[row][col] = 0u;
                    invaders_remaining--;
                    player_bullets[bullet].active = 0;
                    score += invaders_enemy_score(row);
                    invaders_spawn_explosion(enemy_x, enemy_y);
                    invaders_queue_removed_enemy_rect(enemy_x, enemy_y);
                    return;
                }
            }
        }
    }
}

static int invaders_any_player_bullet_active(void)
{
    int i;

    for (i = 0; i < PLAYER_BULLET_SLOTS; i++) {
        if (player_bullets[i].active) {
            return 1;
        }
    }

    return 0;
}

static void invaders_spawn_player_bullet(
    int player_width,
    int bullet_width,
    int bullet_height)
{
    int i;

    for (i = 0; i < PLAYER_BULLET_SLOTS; i++) {
        if (!player_bullets[i].active) {
            player_bullets[i].active = 1;
            player_bullets[i].x =
                player_x + (player_width / 2) - (bullet_width / 2);
            player_bullets[i].y = player_y - bullet_height;
            return;
        }
    }
}

static void invaders_update_player_bullets(int bullet_height)
{
    int i;

    for (i = 0; i < PLAYER_BULLET_SLOTS; i++) {
        if (player_bullets[i].active) {
            player_bullets[i].y -= BULLET_SPEED;
            if (player_bullets[i].y <= -bullet_height) {
                player_bullets[i].active = 0;
            }
        }
    }
}

static const char* invaders_status_text(void)
{
    if (game_over) {
        return "GAME OVER";
    }

    if (invaders_remaining <= 0) {
        return "CLEAR";
    }

    if (invaders_any_player_bullet_active()) {
        return "SHOT";
    }

    return "READY";
}

static int invaders_draw_slot_index(void)
{
    return demo_ticks & (INVADERS_DRAW_SLOTS - 1);
}

static InvadersRect invaders_make_rect(int x, int y, int w, int h)
{
    InvadersRect rect;

    rect.x = x;
    rect.y = y;
    rect.w = w;
    rect.h = h;

    return rect;
}

static InvadersRect invaders_enemy_rect(int row, int col)
{
    return invaders_make_rect(
        invaders_enemy_x(col),
        invaders_enemy_y(row),
        INVADER_WIDTH,
        INVADER_HEIGHT);
}

static void invaders_fill_rect_black(InvadersRect rect)
{
    ana_fill_rect(0u, rect.x, rect.y, rect.w, rect.h);
}

static int invaders_explosion_frame_for_age(int age)
{
    int frame;

    frame = age / EXPLOSION_TICKS_PER_FRAME;
    if (frame >= EXPLOSION_FRAMES) {
        frame = EXPLOSION_FRAMES - 1;
    }

    return frame;
}

static InvadersRect invaders_explosion_rect(const InvadersExplosion* explosion)
{
    return invaders_make_rect(
        explosion->x,
        explosion->y,
        EXPLOSION_WIDTH,
        EXPLOSION_HEIGHT);
}

static int invaders_rect_overlaps_active_explosion(InvadersRect rect)
{
    InvadersRect explosion_rect;
    int i;

    for (i = 0; i < EXPLOSION_SLOTS; i++) {
        if (!explosions[i].active) {
            continue;
        }

        explosion_rect = invaders_explosion_rect(&explosions[i]);
        if (invaders_rects_intersect(rect, explosion_rect)) {
            return 1;
        }
    }

    return 0;
}

static int invaders_explosion_unchanged_for_slot(
    const InvadersDrawSlot* draw_slot,
    int index)
{
    const InvadersExplosion* previous;
    const InvadersExplosion* current;

    previous = &draw_slot->explosions[index];
    current = &explosions[index];

    return previous->active &&
        current->active &&
        previous->x == current->x &&
        previous->y == current->y &&
        invaders_explosion_frame_for_age(previous->age) ==
            invaders_explosion_frame_for_age(current->age);
}

static void invaders_repair_formation_overlap(InvadersRect rect)
{
    InvadersRect enemy_rect;
    int row;
    int col;

    if (invader_image == 0 || invaders_remaining <= 0) {
        return;
    }

    for (row = 0; row < INVADER_ROWS; row++) {
        for (col = 0; col < INVADER_COLUMNS; col++) {
            if (!invader_alive[row][col]) {
                continue;
            }

            enemy_rect = invaders_enemy_rect(row, col);
            if (invaders_rects_intersect(rect, enemy_rect)) {
                ana_draw_image_frame(
                    invader_image,
                    invader_frame,
                    enemy_rect.x,
                    enemy_rect.y);
            }
        }
    }
}

static void invaders_clear_previous_rect(int slot, InvadersRect rect)
{
    invaders_fill_rect_black(rect);
    if (!formation_dirty_slots[slot]) {
        invaders_repair_formation_overlap(rect);
    }
}

static int invaders_clear_previous_movers(int slot)
{
    InvadersDrawSlot* draw_slot;
    InvadersRect rect;
    int player_width;
    int player_height;
    int bullet_width;
    int bullet_height;
    int redraw_explosions;
    int i;

    draw_slot = &draw_slots[slot];
    player_width = player_image != 0 ? ana_image_width(player_image) : 16;
    player_height = player_image != 0 ? ana_image_height(player_image) : 10;
    bullet_width = bullet_image != 0 ? ana_image_width(bullet_image) : 2;
    bullet_height = bullet_image != 0 ? ana_image_height(bullet_image) : 6;
    redraw_explosions = 0;

    if (draw_slot->has_player) {
        rect = invaders_make_rect(
            draw_slot->player_x,
            draw_slot->player_y,
            player_width,
            player_height);
        invaders_clear_previous_rect(slot, rect);
        if (invaders_rect_overlaps_active_explosion(rect)) {
            redraw_explosions = 1;
        }
    }

    for (i = 0; i < PLAYER_BULLET_SLOTS; i++) {
        if (draw_slot->bullets[i].active) {
            rect = invaders_make_rect(
                draw_slot->bullets[i].x,
                draw_slot->bullets[i].y,
                bullet_width,
                bullet_height);
            invaders_clear_previous_rect(slot, rect);
            if (invaders_rect_overlaps_active_explosion(rect)) {
                redraw_explosions = 1;
            }
        }
    }

    for (i = 0; i < EXPLOSION_SLOTS; i++) {
        if (draw_slot->explosions[i].active) {
            if (invaders_explosion_unchanged_for_slot(draw_slot, i)) {
                continue;
            }

            rect = invaders_explosion_rect(&draw_slot->explosions[i]);
            invaders_clear_previous_rect(slot, rect);
            if (invaders_rect_overlaps_active_explosion(rect)) {
                redraw_explosions = 1;
            }
        }
    }

    return redraw_explosions;
}

static int invaders_process_removed_enemies(int slot)
{
    InvadersRect rect;
    int redraw_explosions;
    int i;

    if (removed_enemy_counts[slot] <= 0) {
        return 0;
    }

    if (formation_dirty_slots[slot]) {
        removed_enemy_counts[slot] = 0;
        return 0;
    }

    redraw_explosions = 0;
    for (i = 0; i < removed_enemy_counts[slot]; i++) {
        rect = removed_enemy_rects[slot][i];
        invaders_fill_rect_black(rect);
        if (invaders_rect_overlaps_active_explosion(rect)) {
            redraw_explosions = 1;
        }
    }

    removed_enemy_counts[slot] = 0;
    return redraw_explosions;
}

static void invaders_clear_formation_slot(int slot)
{
    int row;
    int x;
    int y;

    if (!formation_drawn_slots[slot]) {
        return;
    }

    x = formation_slot_x[slot];
    y = formation_slot_y[slot];
    for (row = 0; row < INVADER_ROWS; row++) {
        invaders_fill_rect_black(
            invaders_make_rect(
                x,
                y + (row * INVADER_SPACING_Y),
                INVADER_FORMATION_WIDTH,
                INVADER_HEIGHT));
    }
}

static void invaders_draw_formation_slot(int slot)
{
    int row;
    int col;

    if (!formation_dirty_slots[slot]) {
        return;
    }

    invaders_clear_formation_slot(slot);

    if (invader_image != 0) {
        for (row = 0; row < INVADER_ROWS; row++) {
            for (col = 0; col < INVADER_COLUMNS; col++) {
                if (invader_alive[row][col]) {
                    ana_draw_image_frame(
                        invader_image,
                        invader_frame,
                        invaders_enemy_x(col),
                        invaders_enemy_y(row));
                }
            }
        }
    }

    formation_dirty_slots[slot] = 0;
    formation_drawn_slots[slot] = invader_image != 0 && invaders_remaining > 0;
    formation_slot_x[slot] = invader_formation_x;
    formation_slot_y[slot] = invader_formation_y;
}

static int invaders_hud_needs_redraw(
    const InvadersDrawSlot* draw_slot,
    const char* status)
{
    return !draw_slot->hud_drawn ||
        draw_slot->hud_score != score ||
        draw_slot->hud_lives != lives ||
        strcmp(draw_slot->hud_status, status) != 0;
}

static void invaders_store_hud_state(
    InvadersDrawSlot* draw_slot,
    const char* status)
{
    draw_slot->hud_drawn = 1;
    draw_slot->hud_score = score;
    draw_slot->hud_lives = lives;
    strncpy(draw_slot->hud_status, status, HUD_STATUS_TEXT_SIZE - 1);
    draw_slot->hud_status[HUD_STATUS_TEXT_SIZE - 1] = '\0';
}

static void invaders_draw_hud_slot(int slot)
{
    InvadersDrawSlot* draw_slot;
    const char* status;

    if (hud_font == 0) {
        return;
    }

    draw_slot = &draw_slots[slot];
    status = invaders_status_text();
    if (!invaders_hud_needs_redraw(draw_slot, status)) {
        return;
    }

    ana_fill_rect(
        0u,
        HUD_SCORE_X,
        HUD_SCORE_Y,
        HUD_SCORE_CLEAR_WIDTH,
        HUD_FONT_HEIGHT);
    ana_fill_rect(
        0u,
        HUD_LIVES_X,
        HUD_LIVES_Y,
        HUD_LIVES_CLEAR_WIDTH,
        HUD_FONT_HEIGHT);
    ana_fill_rect(
        0u,
        HUD_STATUS_X,
        HUD_STATUS_Y,
        HUD_STATUS_CLEAR_WIDTH,
        HUD_FONT_HEIGHT);

    ana_draw_text(hud_font, HUD_SCORE_X, HUD_SCORE_Y, "SCORE");
    ana_draw_int(
        hud_font,
        HUD_SCORE_X + ana_text_width(hud_font, "SCORE "),
        HUD_SCORE_Y,
        score);
    ana_draw_text(hud_font, HUD_LIVES_X, HUD_LIVES_Y, "LIVES");
    ana_draw_int(
        hud_font,
        HUD_LIVES_X + ana_text_width(hud_font, "LIVES "),
        HUD_LIVES_Y,
        lives);
    ana_draw_text(hud_font, HUD_STATUS_X, HUD_STATUS_Y, "STATUS");
    ana_draw_text(
        hud_font,
        HUD_STATUS_X + ana_text_width(hud_font, "STATUS "),
        HUD_STATUS_Y,
        status);

    invaders_store_hud_state(draw_slot, status);
}

static void invaders_store_draw_slot(int slot)
{
    InvadersDrawSlot* draw_slot;
    int i;

    draw_slot = &draw_slots[slot];
    draw_slot->has_player = player_image != 0;
    draw_slot->player_x = player_x;
    draw_slot->player_y = player_y;

    for (i = 0; i < PLAYER_BULLET_SLOTS; i++) {
        draw_slot->bullets[i] = player_bullets[i];
    }

    for (i = 0; i < EXPLOSION_SLOTS; i++) {
        draw_slot->explosions[i] = explosions[i];
    }
}

static void invaders_init(void)
{
    invaders_reset_game_state();
    invaders_assets_loaded = 0;

    ana_input_clear_key_map();
    ana_input_map_key_to_direction(ANA_KEY_LEFT, ANA_INPUT_DEVICE_0, ANA_INPUT_LEFT);
    ana_input_map_key_to_direction(ANA_KEY_A, ANA_INPUT_DEVICE_0, ANA_INPUT_LEFT);
    ana_input_map_key_to_direction(ANA_KEY_RIGHT, ANA_INPUT_DEVICE_0, ANA_INPUT_RIGHT);
    ana_input_map_key_to_direction(ANA_KEY_D, ANA_INPUT_DEVICE_0, ANA_INPUT_RIGHT);
    ana_input_map_key_to_action(ANA_KEY_SPACE, ANA_INPUT_DEVICE_0, ANA_ACTION_1);
    ana_input_map_key_to_quit(ANA_KEY_ESCAPE);
}

static void invaders_load(void)
{
    invaders_build_sprite_data();
    invaders_build_hud_font_data();

    player_image = ana_load_image_data(
        player_image_data,
        (long)sizeof(player_image_data));
    bullet_image = ana_load_image_data(
        bullet_image_data,
        (long)sizeof(bullet_image_data));
    invader_image = ana_load_image_data(
        invader_image_data,
        (long)sizeof(invader_image_data));
    explosion_image = ana_load_image_data(
        explosion_image_data,
        (long)sizeof(explosion_image_data));
    hud_font = ana_load_font_data(
        hud_font_data,
        (long)sizeof(hud_font_data));
    if (hud_font != 0) {
        ana_set_font_color(hud_font, 5);
    }

    invaders_assets_loaded =
        player_image != 0 &&
        bullet_image != 0 &&
        invader_image != 0 &&
        explosion_image != 0 &&
        hud_font != 0;
}

static void invaders_update(ANA_Time time)
{
    int player_width;
    int bullet_width;
    int bullet_height;
    int action_pressed;

    demo_ticks = time.tick;
    player_width = player_image != 0 ? ana_image_width(player_image) : 16;
    bullet_width = bullet_image != 0 ? ana_image_width(bullet_image) : 2;
    bullet_height = bullet_image != 0 ? ana_image_height(bullet_image) : 6;
    action_pressed = ana_input_action_pressed(
        ANA_INPUT_DEVICE_0,
        ANA_ACTION_1);

    if (ana_input_direction(ANA_INPUT_DEVICE_0, ANA_INPUT_LEFT)) {
        player_x -= PLAYER_SPEED;
    }

    if (ana_input_direction(ANA_INPUT_DEVICE_0, ANA_INPUT_RIGHT)) {
        player_x += PLAYER_SPEED;
    }

    if (player_x < 0) {
        player_x = 0;
    }

    if (player_x > ANA_DEFAULT_WIDTH - player_width) {
        player_x = ANA_DEFAULT_WIDTH - player_width;
    }

    if ((game_over || invaders_remaining <= 0) && action_pressed) {
        invaders_reset_game_state();
    } else if (action_pressed && !game_over) {
        invaders_spawn_player_bullet(
            player_width,
            bullet_width,
            bullet_height);
    }

    invaders_update_player_bullets(bullet_height);
    invaders_check_bullet_hits(bullet_width, bullet_height);
    invaders_update_formation();
    invaders_update_explosions();

    if (ana_quit_requested()) {
        ana_quit();
    }

    if (time.tick >= GAME_TICK_LIMIT) {
        ana_quit();
    }
}

static void invaders_draw(void)
{
    int i;
    int frame;
    int slot;
    int redraw_explosions;

    slot = invaders_draw_slot_index();
    redraw_explosions = 0;

    if (full_clear_slots[slot]) {
        ana_fill_rect(0u, 0, 0, ANA_DEFAULT_WIDTH, ANA_DEFAULT_HEIGHT);
        memset(&draw_slots[slot], 0, sizeof(draw_slots[slot]));
        formation_drawn_slots[slot] = 0;
        formation_dirty_slots[slot] = 1;
        full_clear_slots[slot] = 0;
        redraw_explosions = 1;
    } else {
        redraw_explosions = invaders_clear_previous_movers(slot);
    }

    invaders_draw_hud_slot(slot);
    if (invaders_process_removed_enemies(slot)) {
        redraw_explosions = 1;
    }
    if (formation_dirty_slots[slot]) {
        redraw_explosions = 1;
    }
    invaders_draw_formation_slot(slot);

    if (explosion_image != 0) {
        for (i = 0; i < EXPLOSION_SLOTS; i++) {
            if (explosions[i].active) {
                if (!redraw_explosions &&
                        invaders_explosion_unchanged_for_slot(
                            &draw_slots[slot],
                            i)) {
                    continue;
                }

                frame = invaders_explosion_frame_for_age(explosions[i].age);
                ana_draw_image_frame(
                    explosion_image,
                    frame,
                    explosions[i].x,
                    explosions[i].y);
            }
        }
    }

    if (bullet_image != 0) {
        for (i = 0; i < PLAYER_BULLET_SLOTS; i++) {
            if (player_bullets[i].active) {
                ana_draw_image(
                    bullet_image,
                    player_bullets[i].x,
                    player_bullets[i].y);
            }
        }
    }

    if (player_image != 0) {
        ana_draw_image(player_image, player_x, player_y);
    }

    invaders_store_draw_slot(slot);
}

static void invaders_shutdown(void)
{
    ana_free_font(hud_font);
    ana_free_image(explosion_image);
    ana_free_image(invader_image);
    ana_free_image(bullet_image);
    ana_free_image(player_image);
    hud_font = 0;
    explosion_image = 0;
    invader_image = 0;
    bullet_image = 0;
    player_image = 0;
}

#ifdef ANA_INVADERS_DEBUG_STATS
static long invaders_average_us(
    long total_perf_ticks,
    long frames,
    long perf_ticks_per_second)
{
    long avg_ticks;

    if (total_perf_ticks <= 0L || frames <= 0L ||
            perf_ticks_per_second <= 0L) {
        return 0L;
    }

    avg_ticks = total_perf_ticks / frames;
    if (perf_ticks_per_second >= 1000L) {
        return (avg_ticks * 1000L) / (perf_ticks_per_second / 1000L);
    }

    return (avg_ticks * 1000000L) / perf_ticks_per_second;
}

static void invaders_print_ms(long microseconds)
{
    printf("%ld.%03ld", microseconds / 1000L, microseconds % 1000L);
}

static void invaders_print_run_stats(void)
{
    ANA_RunStats stats;
    ANA_RenderStats render_stats;
    long elapsed_seconds_x100;
    long dirty_rects_per_frame_x100;
    long converted_pixels_per_frame;
    long planar_clear_pixels_per_frame;
    long chunky_clear_pixels_per_frame;
    long input_us;
    long update_us;
    long draw_us;
    long present_us;
    long present_total_us;
    long present_clear_us;
    long present_convert_us;
    long present_flip_us;

    stats = ana_last_run_stats();
    render_stats = ana_render_stats();
    elapsed_seconds_x100 = 0L;
    dirty_rects_per_frame_x100 = 0L;
    converted_pixels_per_frame = 0L;
    planar_clear_pixels_per_frame = 0L;
    chunky_clear_pixels_per_frame = 0L;
    input_us = invaders_average_us(
        stats.input_perf_ticks,
        stats.frames,
        stats.perf_ticks_per_second);
    update_us = invaders_average_us(
        stats.update_perf_ticks,
        stats.frames,
        stats.perf_ticks_per_second);
    draw_us = invaders_average_us(
        stats.draw_perf_ticks,
        stats.frames,
        stats.perf_ticks_per_second);
    present_us = invaders_average_us(
        stats.present_perf_ticks,
        stats.frames,
        stats.perf_ticks_per_second);
    present_total_us = invaders_average_us(
        render_stats.present_total_perf_ticks,
        render_stats.frames,
        render_stats.perf_ticks_per_second);
    present_clear_us = invaders_average_us(
        render_stats.present_clear_perf_ticks,
        render_stats.frames,
        render_stats.perf_ticks_per_second);
    present_convert_us = invaders_average_us(
        render_stats.present_convert_perf_ticks,
        render_stats.frames,
        render_stats.perf_ticks_per_second);
    present_flip_us = invaders_average_us(
        render_stats.present_flip_perf_ticks,
        render_stats.frames,
        render_stats.perf_ticks_per_second);

    if (stats.ticks_per_second > 0L) {
        elapsed_seconds_x100 =
            (stats.elapsed_ticks * 100L) / stats.ticks_per_second;
    }

    if (render_stats.frames > 0L) {
        dirty_rects_per_frame_x100 =
            (render_stats.dirty_rects * 100L) / render_stats.frames;
        converted_pixels_per_frame =
            render_stats.converted_pixels / render_stats.frames;
        planar_clear_pixels_per_frame =
            render_stats.planar_clear_pixels / render_stats.frames;
        chunky_clear_pixels_per_frame =
            render_stats.chunky_clear_pixels / render_stats.frames;
    }

    printf("Frames presented: %ld\n", stats.frames);
    printf(
        "Elapsed time: %ld.%02ld sec\n",
        elapsed_seconds_x100 / 100L,
        elapsed_seconds_x100 % 100L);
    printf(
        "Average FPS: %ld.%02ld\n",
        stats.average_fps_x100 / 100L,
        stats.average_fps_x100 % 100L);
    printf(
        "Dirty rects/frame: %ld.%02ld (max %ld)\n",
        dirty_rects_per_frame_x100 / 100L,
        dirty_rects_per_frame_x100 % 100L,
        render_stats.max_dirty_rects);
    printf(
        "Converted pixels/frame: %ld (max %ld)\n",
        converted_pixels_per_frame,
        render_stats.max_converted_pixels);
    printf(
        "Planar clear pixels/frame: %ld (max %ld)\n",
        planar_clear_pixels_per_frame,
        render_stats.max_planar_clear_pixels);
    printf(
        "Chunky clear pixels/frame: %ld (max %ld)\n",
        chunky_clear_pixels_per_frame,
        render_stats.max_chunky_clear_pixels);
    printf("Avg input/update/draw/present ms: ");
    invaders_print_ms(input_us);
    printf("/");
    invaders_print_ms(update_us);
    printf("/");
    invaders_print_ms(draw_us);
    printf("/");
    invaders_print_ms(present_us);
    printf("\n");
    printf("Avg present clear/c2p/flip/total ms: ");
    invaders_print_ms(present_clear_us);
    printf("/");
    invaders_print_ms(present_convert_us);
    printf("/");
    invaders_print_ms(present_flip_us);
    printf("/");
    invaders_print_ms(present_total_us);
    printf("\n");
    printf(
        "Flip paths screen/direct: %ld/%ld\n",
        render_stats.screen_buffer_flips,
        render_stats.direct_flips);
}
#endif

static void invaders_print_run_summary(void)
{
    ANA_RunStats stats;

    stats = ana_last_run_stats();
    printf(
        "Average FPS: %ld.%02ld\n",
        stats.average_fps_x100 / 100L,
        stats.average_fps_x100 % 100L);
}

int main(void)
{
    ANA_Game game;
    int result;

    game.init = invaders_init;
    game.load = invaders_load;
    game.update = invaders_update;
    game.draw = invaders_draw;
    game.shutdown = invaders_shutdown;
    game.width = 320;
    game.height = 256;
    game.fps = 50;
    game.colors = 16;
    game.screen_mode = ANA_SCREEN_PAL_LORES;

    printf("ANA invaders started.\n");
    printf("Keyboard mapping: cursor/A-D movement, Space action, Esc quit.\n");
    printf("Move left/right and press Space/fire to shoot.\n");

    result = ana_run(&game);

    printf("ANA invaders finished with %s.\n", ana_result_name((ANA_Result)result));
    invaders_print_run_summary();
#ifdef ANA_INVADERS_DEBUG_STATS
    invaders_print_run_stats();
#endif
    printf("Player X: %d\n", player_x);
    printf("Score: %d\n", score);
    printf("Invaders remaining: %d\n", invaders_remaining);
    printf("Assets loaded: %s\n", invaders_assets_loaded ? "yes" : "no");
    printf("Type invaders to run it again.\n");

    if (result == ANA_OK && !invaders_assets_loaded) {
        return ANA_ERROR_NOT_IMPLEMENTED;
    }

    return result;
}
