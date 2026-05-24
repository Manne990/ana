#include "ana.h"
#include "invaders_assets.h"

#include <stdio.h>
#include <string.h>

#define PLAYER_SPEED 2
#define PLAYER_BULLET_SPEED 4
#define PLAYER_BULLET_SLOTS 4
#define ALIEN_BULLET_SPEED 2
#define ALIEN_BULLET_SLOTS 6
#define GAME_TICK_LIMIT 3000

#define INVADER_COLUMNS 10
#define INVADER_ROWS 5
#define INVADER_COUNT (INVADER_COLUMNS * INVADER_ROWS)
#define INVADER_WIDTH 16
#define INVADER_HEIGHT 8
#define INVADER_FRAMES 2
#define INVADER_SPACING_X 22
#define INVADER_SPACING_Y 16
#define INVADER_FORMATION_WIDTH \
    (INVADER_WIDTH + ((INVADER_COLUMNS - 1) * INVADER_SPACING_X))
#define INVADER_FORMATION_HEIGHT \
    (INVADER_HEIGHT + ((INVADER_ROWS - 1) * INVADER_SPACING_Y))
#define INVADER_START_X 52
#define INVADER_START_Y 54
#define INVADER_STEP_PIXELS 2
#define INVADER_DROP_PIXELS 8
#define INVADER_MIN_X 8
#define INVADER_MAX_X (ANA_DEFAULT_WIDTH - 8)
#define INVADER_BASE_INTERVAL 24
#define INVADER_MIN_INTERVAL 5
#define ALIEN_FIRE_BASE_INTERVAL 45
#define ALIEN_FIRE_MIN_INTERVAL 10

#define EXPLOSION_WIDTH 16
#define EXPLOSION_HEIGHT 10
#define EXPLOSION_FRAMES 2
#define EXPLOSION_SLOTS 4
#define EXPLOSION_TICKS_PER_FRAME 5
#define EXPLOSION_TICKS (EXPLOSION_FRAMES * EXPLOSION_TICKS_PER_FRAME)

#if defined(ANA_TARGET_AMIGA) && defined(ANA_AMIGA_DIRECT_PRESENT)
#define INVADERS_DRAW_SLOTS 1
#else
#define INVADERS_DRAW_SLOTS 2
#endif
#define INVADERS_REMOVED_ENEMY_SLOTS INVADER_COUNT
#define HUD_SCORE_X 16
#define HUD_SCORE_Y 16
#define HUD_LIVES_X 200
#define HUD_LIVES_Y 16
#define HUD_STATUS_X 16
#define HUD_STATUS_Y 30
#define HUD_FONT_HEIGHT 7
#define HUD_SCORE_CLEAR_WIDTH 90
#define HUD_LIVES_CLEAR_WIDTH 70
#define HUD_STATUS_CLEAR_WIDTH 150
#define HUD_STATUS_TEXT_SIZE 16

#define SHIELD_COUNT 4
#define SHIELD_COLUMNS 8
#define SHIELD_ROWS 5
#define SHIELD_CELL_SIZE 4
#define SHIELD_WIDTH (SHIELD_COLUMNS * SHIELD_CELL_SIZE)
#define SHIELD_HEIGHT (SHIELD_ROWS * SHIELD_CELL_SIZE)
#define SHIELD_START_X 30
#define SHIELD_Y 178
#define SHIELD_SPACING_X 72
#define SHIELD_COLOR 3

typedef enum InvadersGameState {
    INVADERS_STATE_TITLE,
    INVADERS_STATE_PLAYING,
    INVADERS_STATE_CLEAR,
    INVADERS_STATE_GAME_OVER
} InvadersGameState;

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
    InvadersBullet alien_bullets[ALIEN_BULLET_SLOTS];
    InvadersExplosion explosions[EXPLOSION_SLOTS];
    int hud_drawn;
    int hud_score;
    int hud_lives;
    char hud_status[HUD_STATUS_TEXT_SIZE];
} InvadersDrawSlot;

static unsigned char invader_alive[INVADER_ROWS][INVADER_COLUMNS];
static InvadersExplosion explosions[EXPLOSION_SLOTS];
static InvadersBullet player_bullets[PLAYER_BULLET_SLOTS];
static InvadersBullet alien_bullets[ALIEN_BULLET_SLOTS];
static InvadersDrawSlot draw_slots[INVADERS_DRAW_SLOTS];
static ANA_Rect removed_enemy_rects
    [INVADERS_DRAW_SLOTS][INVADERS_REMOVED_ENEMY_SLOTS];
static int removed_enemy_counts[INVADERS_DRAW_SLOTS];
static int formation_dirty_slots[INVADERS_DRAW_SLOTS];
static int formation_drawn_slots[INVADERS_DRAW_SLOTS];
static int formation_slot_x[INVADERS_DRAW_SLOTS];
static int formation_slot_y[INVADERS_DRAW_SLOTS];
static int formation_row_min_slots[INVADERS_DRAW_SLOTS][INVADER_ROWS];
static int formation_row_max_slots[INVADERS_DRAW_SLOTS][INVADER_ROWS];
static int shield_dirty_slots[INVADERS_DRAW_SLOTS];
static int full_clear_slots[INVADERS_DRAW_SLOTS];
static unsigned char shield_cells[SHIELD_COUNT][SHIELD_ROWS][SHIELD_COLUMNS];
static int player_x = 152;
static int player_y = 220;
static int invader_formation_x = INVADER_START_X;
static int invader_formation_y = INVADER_START_Y;
static int invader_direction = 1;
static int invader_frame = 0;
static ANA_Timer invader_move_timer;
static ANA_Timer alien_fire_timer;
static int invaders_remaining = 0;
static int score = 0;
static int lives = 3;
static InvadersGameState game_state = INVADERS_STATE_TITLE;
static int demo_ticks = 0;
static int invaders_assets_loaded = 0;
static unsigned long invaders_rng_state = 0x13579bdfUL;

static const unsigned char shield_pattern[SHIELD_ROWS][SHIELD_COLUMNS] = {
    { 0u, 1u, 1u, 1u, 1u, 1u, 1u, 0u },
    { 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u },
    { 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u },
    { 1u, 1u, 1u, 0u, 0u, 1u, 1u, 1u },
    { 1u, 1u, 0u, 0u, 0u, 0u, 1u, 1u }
};

static void invaders_damage_shields_for_formation(void);

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

static void invaders_reset_alien_bullets(void)
{
    int i;

    for (i = 0; i < ALIEN_BULLET_SLOTS; i++) {
        alien_bullets[i].active = 0;
        alien_bullets[i].x = 0;
        alien_bullets[i].y = 0;
    }
}

static unsigned int invaders_random(void)
{
    invaders_rng_state =
        (invaders_rng_state * 1103515245UL) + 12345UL;
    return (unsigned int)((invaders_rng_state >> 16) & 0x7fffu);
}

static void invaders_request_full_clear(void)
{
    int i;

    for (i = 0; i < INVADERS_DRAW_SLOTS; i++) {
        full_clear_slots[i] = 1;
        formation_dirty_slots[i] = 1;
        shield_dirty_slots[i] = 1;
        removed_enemy_counts[i] = 0;
        draw_slots[i].hud_drawn = 0;
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

static void invaders_mark_shields_dirty(void)
{
    int i;

    for (i = 0; i < INVADERS_DRAW_SLOTS; i++) {
        shield_dirty_slots[i] = 1;
    }
}

static int invaders_shield_x(int shield)
{
    return SHIELD_START_X + (shield * SHIELD_SPACING_X);
}

static ANA_Rect invaders_shield_cell_rect(int shield, int row, int col)
{
    ANA_Rect rect;

    rect.x = invaders_shield_x(shield) + (col * SHIELD_CELL_SIZE);
    rect.y = SHIELD_Y + (row * SHIELD_CELL_SIZE);
    rect.w = SHIELD_CELL_SIZE;
    rect.h = SHIELD_CELL_SIZE;

    return rect;
}

static ANA_Rect invaders_shield_rect(int shield)
{
    ANA_Rect rect;

    rect.x = invaders_shield_x(shield);
    rect.y = SHIELD_Y;
    rect.w = SHIELD_WIDTH;
    rect.h = SHIELD_HEIGHT;

    return rect;
}

static void invaders_reset_shields(void)
{
    int shield;
    int row;
    int col;

    for (shield = 0; shield < SHIELD_COUNT; shield++) {
        for (row = 0; row < SHIELD_ROWS; row++) {
            for (col = 0; col < SHIELD_COLUMNS; col++) {
                shield_cells[shield][row][col] = shield_pattern[row][col];
            }
        }
    }

    invaders_mark_shields_dirty();
}

static void invaders_reset_draw_slots(void)
{
    int i;
    int row;

    memset(draw_slots, 0, sizeof(draw_slots));
    memset(removed_enemy_rects, 0, sizeof(removed_enemy_rects));
    for (i = 0; i < INVADERS_DRAW_SLOTS; i++) {
        removed_enemy_counts[i] = 0;
        formation_dirty_slots[i] = 1;
        formation_drawn_slots[i] = 0;
        formation_slot_x[i] = INVADER_START_X;
        formation_slot_y[i] = INVADER_START_Y;
        for (row = 0; row < INVADER_ROWS; row++) {
            formation_row_min_slots[i][row] = INVADER_COLUMNS;
            formation_row_max_slots[i][row] = -1;
        }
        shield_dirty_slots[i] = 1;
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
    ana_timer_reset(&invader_move_timer, INVADER_BASE_INTERVAL);
    ana_timer_reset(&alien_fire_timer, ALIEN_FIRE_BASE_INTERVAL);
    invaders_remaining = 0;

    for (row = 0; row < INVADER_ROWS; row++) {
        for (col = 0; col < INVADER_COLUMNS; col++) {
            invader_alive[row][col] = 1u;
            invaders_remaining++;
        }
    }

    invaders_mark_formation_dirty();
}

static void invaders_start_new_game(void)
{
    player_x = 152;
    player_y = 220;
    score = 0;
    lives = 3;
    game_state = INVADERS_STATE_PLAYING;
    demo_ticks = 0;

    invaders_reset_formation();
    invaders_reset_shields();
    invaders_reset_explosions();
    invaders_reset_player_bullets();
    invaders_reset_alien_bullets();
    invaders_reset_draw_slots();
}

static void invaders_enter_title(void)
{
    player_x = 152;
    player_y = 220;
    score = 0;
    lives = 3;
    game_state = INVADERS_STATE_TITLE;
    demo_ticks = 0;

    invaders_reset_formation();
    invaders_reset_shields();
    invaders_reset_explosions();
    invaders_reset_player_bullets();
    invaders_reset_alien_bullets();
    invaders_reset_draw_slots();
}

static void invaders_enter_clear(void)
{
    game_state = INVADERS_STATE_CLEAR;
    invaders_reset_player_bullets();
    invaders_reset_alien_bullets();
    invaders_request_full_clear();
}

static void invaders_enter_game_over(void)
{
    game_state = INVADERS_STATE_GAME_OVER;
    invaders_reset_player_bullets();
    invaders_reset_alien_bullets();
    ana_play_sound(game_over_sound);
    invaders_request_full_clear();
}

static void invaders_lose_life(void)
{
    if (lives > 0) {
        lives--;
    }

    if (lives <= 0) {
        invaders_enter_game_over();
        return;
    }

    player_x = 152;
    invaders_reset_player_bullets();
    invaders_reset_alien_bullets();
    invaders_reset_explosions();
    ana_play_sound(explosion_sound);
    invaders_request_full_clear();
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
    ANA_Rect rect;
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
    ana_play_sound(explosion_sound);
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
    interval =
        (INVADER_BASE_INTERVAL * 100) /
        (100 + (removed * 5));
    return ana_clamp_int(interval, INVADER_MIN_INTERVAL, INVADER_BASE_INTERVAL);
}

static int invaders_fire_interval(void)
{
    int removed;
    int interval;

    removed = INVADER_COUNT - invaders_remaining;
    interval =
        (ALIEN_FIRE_BASE_INTERVAL * 100) /
        (100 + (removed * 6));
    return ana_clamp_int(
        interval,
        ALIEN_FIRE_MIN_INTERVAL,
        ALIEN_FIRE_BASE_INTERVAL);
}

static int invaders_update_formation(void)
{
    int row;
    int col;
    int left;
    int right;
    int enemy_x;
    int enemy_y;

    if (game_state != INVADERS_STATE_PLAYING || invaders_remaining <= 0) {
        return 0;
    }

    invader_move_timer.interval = invaders_move_interval();
    if (!ana_timer_tick(&invader_move_timer)) {
        return 0;
    }

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

    ana_play_sound(step_sound);
    invader_frame = 1 - invader_frame;
    invaders_damage_shields_for_formation();
    invaders_mark_formation_dirty();
    enemy_y = invader_formation_y +
        ((INVADER_ROWS - 1) * INVADER_SPACING_Y) +
        INVADER_HEIGHT;
    if (enemy_y >= player_y - 4) {
        invaders_enter_game_over();
        return 1;
    }

    return 0;
}

static int invaders_enemy_score(int row)
{
    return (INVADER_ROWS - row) * 10;
}

static int invaders_rect_overlaps_alive_shield(ANA_Rect rect)
{
    ANA_Rect cell_rect;
    int shield;
    int row;
    int col;

    for (shield = 0; shield < SHIELD_COUNT; shield++) {
        if (!ana_rect_intersects(rect, invaders_shield_rect(shield))) {
            continue;
        }

        for (row = 0; row < SHIELD_ROWS; row++) {
            for (col = 0; col < SHIELD_COLUMNS; col++) {
                if (!shield_cells[shield][row][col]) {
                    continue;
                }

                cell_rect = invaders_shield_cell_rect(shield, row, col);
                if (ana_rect_intersects(rect, cell_rect)) {
                    return 1;
                }
            }
        }
    }

    return 0;
}

static int invaders_damage_shield_cells_at_rect(ANA_Rect rect, int stop_after_first)
{
    ANA_Rect cell_rect;
    int shield;
    int row;
    int col;
    int damaged;

    damaged = 0;
    for (shield = 0; shield < SHIELD_COUNT; shield++) {
        if (!ana_rect_intersects(rect, invaders_shield_rect(shield))) {
            continue;
        }

        for (row = 0; row < SHIELD_ROWS; row++) {
            for (col = 0; col < SHIELD_COLUMNS; col++) {
                if (!shield_cells[shield][row][col]) {
                    continue;
                }

                cell_rect = invaders_shield_cell_rect(shield, row, col);
                if (ana_rect_intersects(rect, cell_rect)) {
                    shield_cells[shield][row][col] = 0u;
                    damaged = 1;
                    if (stop_after_first) {
                        invaders_mark_shields_dirty();
                        return 1;
                    }
                }
            }
        }
    }

    if (damaged) {
        invaders_mark_shields_dirty();
    }

    return damaged;
}

static int invaders_damage_shield_at_rect(ANA_Rect rect)
{
    return invaders_damage_shield_cells_at_rect(rect, 1);
}

static void invaders_damage_shields_for_formation(void)
{
    ANA_Rect enemy_rect;
    int row;
    int col;

    if (invaders_remaining <= 0) {
        return;
    }

    enemy_rect.w = INVADER_WIDTH;
    enemy_rect.h = INVADER_HEIGHT;
    for (row = 0; row < INVADER_ROWS; row++) {
        enemy_rect.y = invaders_enemy_y(row);
        for (col = 0; col < INVADER_COLUMNS; col++) {
            if (!invader_alive[row][col]) {
                continue;
            }

            enemy_rect.x = invaders_enemy_x(col);
            invaders_damage_shield_cells_at_rect(enemy_rect, 0);
        }
    }
}

static void invaders_check_shield_hits(
    InvadersBullet* bullets,
    int slots,
    int bullet_width,
    int bullet_height)
{
    ANA_Rect bullet_rect;
    int i;

    bullet_rect.w = bullet_width;
    bullet_rect.h = bullet_height;

    for (i = 0; i < slots; i++) {
        if (!bullets[i].active) {
            continue;
        }

        bullet_rect.x = bullets[i].x;
        bullet_rect.y = bullets[i].y;
        if (invaders_damage_shield_at_rect(bullet_rect)) {
            bullets[i].active = 0;
        }
    }
}

static void invaders_check_bullet_hits(int bullet_width, int bullet_height)
{
    int bullet;
    int row;
    int col;
    int enemy_x;
    int enemy_y;
    ANA_Rect bullet_rect;
    ANA_Rect enemy_rect;

    if (game_state != INVADERS_STATE_PLAYING || invaders_remaining <= 0) {
        return;
    }

    bullet_rect.w = bullet_width;
    bullet_rect.h = bullet_height;
    enemy_rect.w = INVADER_WIDTH;
    enemy_rect.h = INVADER_HEIGHT;

    for (bullet = 0; bullet < PLAYER_BULLET_SLOTS; bullet++) {
        if (!player_bullets[bullet].active) {
            continue;
        }

        bullet_rect.x = player_bullets[bullet].x;
        bullet_rect.y = player_bullets[bullet].y;

        for (row = 0; row < INVADER_ROWS; row++) {
            enemy_y = invaders_enemy_y(row);
            enemy_rect.y = enemy_y;

            for (col = 0; col < INVADER_COLUMNS; col++) {
                if (!invader_alive[row][col]) {
                    continue;
                }

                enemy_x = invaders_enemy_x(col);
                enemy_rect.x = enemy_x;
                if (ana_rect_intersects(bullet_rect, enemy_rect)) {
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
            ana_play_sound(fire_sound);
            return;
        }
    }
}

static int invaders_collect_alien_shooters(
    int* shooter_rows,
    int* shooter_cols,
    int max_shooters)
{
    int row;
    int col;
    int count;

    count = 0;
    for (col = 0; col < INVADER_COLUMNS; col++) {
        for (row = INVADER_ROWS - 1; row >= 0; row--) {
            if (invader_alive[row][col]) {
                if (count < max_shooters) {
                    shooter_rows[count] = row;
                    shooter_cols[count] = col;
                    count++;
                }
                break;
            }
        }
    }

    return count;
}

static void invaders_spawn_alien_bullet(int bullet_width)
{
    int shooter_rows[INVADER_COLUMNS];
    int shooter_cols[INVADER_COLUMNS];
    int shooter_count;
    int shooter;
    int bullet;
    int row;
    int col;

    if (game_state != INVADERS_STATE_PLAYING || invaders_remaining <= 0) {
        return;
    }

    bullet = -1;
    for (shooter = 0; shooter < ALIEN_BULLET_SLOTS; shooter++) {
        if (!alien_bullets[shooter].active) {
            bullet = shooter;
            break;
        }
    }

    if (bullet < 0) {
        return;
    }

    shooter_count = invaders_collect_alien_shooters(
        shooter_rows,
        shooter_cols,
        INVADER_COLUMNS);
    if (shooter_count <= 0) {
        return;
    }

    shooter = (int)(invaders_random() % (unsigned int)shooter_count);
    row = shooter_rows[shooter];
    col = shooter_cols[shooter];

    alien_bullets[bullet].active = 1;
    alien_bullets[bullet].x =
        invaders_enemy_x(col) + (INVADER_WIDTH / 2) - (bullet_width / 2);
    alien_bullets[bullet].y = invaders_enemy_y(row) + INVADER_HEIGHT;
}

static void invaders_update_alien_fire(int bullet_width)
{
    if (game_state != INVADERS_STATE_PLAYING || invaders_remaining <= 0) {
        return;
    }

    alien_fire_timer.interval = invaders_fire_interval();
    if (!ana_timer_tick(&alien_fire_timer)) {
        return;
    }

    alien_fire_timer.ticks = (int)(invaders_random() % 4u);
    invaders_spawn_alien_bullet(bullet_width);
}

static void invaders_update_player_bullets(int bullet_height)
{
    int i;

    for (i = 0; i < PLAYER_BULLET_SLOTS; i++) {
        if (player_bullets[i].active) {
            player_bullets[i].y -= PLAYER_BULLET_SPEED;
            if (player_bullets[i].y <= -bullet_height) {
                player_bullets[i].active = 0;
            }
        }
    }
}

static void invaders_update_alien_bullets(int bullet_height)
{
    int i;

    for (i = 0; i < ALIEN_BULLET_SLOTS; i++) {
        if (alien_bullets[i].active) {
            alien_bullets[i].y += ALIEN_BULLET_SPEED;
            if (alien_bullets[i].y >= ANA_DEFAULT_HEIGHT) {
                alien_bullets[i].active = 0;
            }
        }
    }

    (void)bullet_height;
}

static int invaders_player_was_hit(
    int player_width,
    int player_height,
    int bullet_width,
    int bullet_height)
{
    ANA_Rect player_rect;
    ANA_Rect bullet_rect;
    int i;

    if (game_state != INVADERS_STATE_PLAYING || invaders_remaining <= 0) {
        return 0;
    }

    player_rect.x = player_x;
    player_rect.y = player_y;
    player_rect.w = player_width;
    player_rect.h = player_height;
    bullet_rect.w = bullet_width;
    bullet_rect.h = bullet_height;

    for (i = 0; i < ALIEN_BULLET_SLOTS; i++) {
        if (!alien_bullets[i].active) {
            continue;
        }

        bullet_rect.x = alien_bullets[i].x;
        bullet_rect.y = alien_bullets[i].y;
        if (ana_rect_intersects(player_rect, bullet_rect)) {
            alien_bullets[i].active = 0;
            return 1;
        }
    }

    return 0;
}

static int invaders_any_alien_bullet_active(void)
{
    int i;

    for (i = 0; i < ALIEN_BULLET_SLOTS; i++) {
        if (alien_bullets[i].active) {
            return 1;
        }
    }

    return 0;
}

static const char* invaders_status_text(void)
{
    if (game_state == INVADERS_STATE_TITLE) {
        return "PRESS FIRE";
    }

    if (game_state == INVADERS_STATE_GAME_OVER) {
        return "GAME OVER";
    }

    if (game_state == INVADERS_STATE_CLEAR) {
        return "CLEAR";
    }

    if (invaders_any_player_bullet_active()) {
        return "SHOT";
    }

    if (invaders_any_alien_bullet_active()) {
        return "DANGER";
    }

    return "READY";
}

static int invaders_draw_slot_index(void)
{
    return demo_ticks & (INVADERS_DRAW_SLOTS - 1);
}

static ANA_Rect invaders_make_rect(int x, int y, int w, int h)
{
    ANA_Rect rect;

    rect.x = x;
    rect.y = y;
    rect.w = w;
    rect.h = h;

    return rect;
}

static ANA_Rect invaders_align_rect_for_dirty(ANA_Rect rect)
{
    int max_x;

    max_x = rect.x + rect.w;
    rect.x &= ~7;
    max_x = (max_x + 7) & ~7;
    if (rect.x < 0) {
        rect.x = 0;
    }
    if (max_x > ANA_DEFAULT_WIDTH) {
        max_x = ANA_DEFAULT_WIDTH;
    }
    rect.w = max_x - rect.x;

    return rect;
}

static int invaders_rect_overlaps_alive_enemy(ANA_Rect rect)
{
    ANA_Rect enemy;
    int row;
    int col;

    if (invaders_remaining <= 0) {
        return 0;
    }

    enemy.w = INVADER_WIDTH;
    enemy.h = INVADER_HEIGHT;
    for (row = 0; row < INVADER_ROWS; row++) {
        enemy.y = invaders_enemy_y(row);
        for (col = 0; col < INVADER_COLUMNS; col++) {
            if (!invader_alive[row][col]) {
                continue;
            }

            enemy.x = invaders_enemy_x(col);
            if (ana_rect_intersects(rect, enemy)) {
                return 1;
            }
        }
    }

    return 0;
}

static int invaders_bullet_should_draw(
    InvadersBullet bullet,
    int bullet_width,
    int bullet_height)
{
    ANA_Rect rect;

    if (!bullet.active) {
        return 0;
    }

    rect = invaders_make_rect(
        bullet.x,
        bullet.y,
        bullet_width,
        bullet_height);

    return !invaders_rect_overlaps_alive_enemy(rect) &&
        !invaders_rect_overlaps_alive_shield(rect);
}

static void invaders_fill_rect_black(ANA_Rect rect)
{
    rect = invaders_align_rect_for_dirty(rect);
    ana_fill_rect(0u, rect.x, rect.y, rect.w, rect.h);
}

static void invaders_repair_shields_overlap(ANA_Rect rect)
{
    ANA_Rect cell_rect;
    int shield;
    int row;
    int col;

    for (shield = 0; shield < SHIELD_COUNT; shield++) {
        if (!ana_rect_intersects(rect, invaders_shield_rect(shield))) {
            continue;
        }

        for (row = 0; row < SHIELD_ROWS; row++) {
            for (col = 0; col < SHIELD_COLUMNS; col++) {
                if (!shield_cells[shield][row][col]) {
                    continue;
                }

                cell_rect = invaders_shield_cell_rect(shield, row, col);
                if (ana_rect_intersects(rect, cell_rect)) {
                    ana_fill_rect(
                        SHIELD_COLOR,
                        cell_rect.x,
                        cell_rect.y,
                        cell_rect.w,
                        cell_rect.h);
                }
            }
        }
    }
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

static ANA_Rect invaders_explosion_rect(const InvadersExplosion* explosion)
{
    return invaders_make_rect(
        explosion->x,
        explosion->y,
        EXPLOSION_WIDTH,
        EXPLOSION_HEIGHT);
}

static int invaders_rect_overlaps_active_explosion(ANA_Rect rect)
{
    ANA_Rect explosion_rect;
    int i;

    for (i = 0; i < EXPLOSION_SLOTS; i++) {
        if (!explosions[i].active) {
            continue;
        }

        explosion_rect = invaders_explosion_rect(&explosions[i]);
        if (ana_rect_intersects(rect, explosion_rect)) {
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

static void invaders_repair_formation_overlap(ANA_Rect rect)
{
    ANA_Rect formation_rect;
    ANA_Rect enemy_rect;
    int row;
    int col;
    int enemy_x;
    int enemy_y;

    if (invader_image == 0 || invaders_remaining <= 0) {
        return;
    }

    formation_rect = invaders_make_rect(
        invader_formation_x,
        invader_formation_y,
        INVADER_FORMATION_WIDTH,
        INVADER_FORMATION_HEIGHT);
    if (!ana_rect_intersects(rect, formation_rect)) {
        return;
    }

    enemy_rect.w = INVADER_WIDTH;
    enemy_rect.h = INVADER_HEIGHT;
    for (row = 0; row < INVADER_ROWS; row++) {
        enemy_y = invaders_enemy_y(row);
        enemy_rect.y = enemy_y;

        for (col = 0; col < INVADER_COLUMNS; col++) {
            if (!invader_alive[row][col]) {
                continue;
            }

            enemy_x = invaders_enemy_x(col);
            enemy_rect.x = enemy_x;
            if (ana_rect_intersects(rect, enemy_rect)) {
                ana_draw_image_frame(
                    invader_image,
                    invader_frame,
                    enemy_x,
                    enemy_y);
            }
        }
    }
}

static int invaders_clear_previous_rect(int slot, ANA_Rect rect)
{
    rect = invaders_align_rect_for_dirty(rect);
    invaders_fill_rect_black(rect);
    if (!formation_dirty_slots[slot]) {
        invaders_repair_formation_overlap(rect);
    }
    if (!shield_dirty_slots[slot]) {
        invaders_repair_shields_overlap(rect);
    }

    return invaders_rect_overlaps_active_explosion(rect);
}

static int invaders_clear_movers_from_state(
    int slot,
    const InvadersDrawSlot* draw_slot)
{
    ANA_Rect rect;
    int player_width;
    int player_height;
    int bullet_width;
    int bullet_height;
    int redraw_explosions;
    int i;

    if (draw_slot == NULL) {
        return 0;
    }

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
        if (invaders_clear_previous_rect(slot, rect)) {
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
            if (invaders_clear_previous_rect(slot, rect)) {
                redraw_explosions = 1;
            }
        }
    }

    for (i = 0; i < ALIEN_BULLET_SLOTS; i++) {
        if (draw_slot->alien_bullets[i].active) {
            rect = invaders_make_rect(
                draw_slot->alien_bullets[i].x,
                draw_slot->alien_bullets[i].y,
                bullet_width,
                bullet_height);
            if (invaders_clear_previous_rect(slot, rect)) {
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
            if (invaders_clear_previous_rect(slot, rect)) {
                redraw_explosions = 1;
            }
        }
    }

    return redraw_explosions;
}

static int invaders_clear_previous_movers(int slot)
{
    return invaders_clear_movers_from_state(slot, &draw_slots[slot]);
}

static int invaders_process_removed_enemies(int slot)
{
    ANA_Rect rect;
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
        rect = invaders_align_rect_for_dirty(removed_enemy_rects[slot][i]);
        invaders_fill_rect_black(rect);
        if (invaders_rect_overlaps_active_explosion(rect)) {
            redraw_explosions = 1;
        }
    }

    removed_enemy_counts[slot] = 0;
    return redraw_explosions;
}

static void invaders_clear_formation_state(
    int formation_drawn,
    int formation_x,
    int formation_y,
    const int* row_min,
    const int* row_max)
{
    int row;
    int min_col;
    int max_col;
    int width;

    if (!formation_drawn || row_min == NULL || row_max == NULL) {
        return;
    }

    for (row = 0; row < INVADER_ROWS; row++) {
        min_col = row_min[row];
        max_col = row_max[row];
        if (min_col > max_col) {
            continue;
        }

        width =
            INVADER_WIDTH +
            ((max_col - min_col) * INVADER_SPACING_X);
        invaders_fill_rect_black(
            invaders_make_rect(
                formation_x + (min_col * INVADER_SPACING_X),
                formation_y + (row * INVADER_SPACING_Y),
                width,
                INVADER_HEIGHT));
    }
}

static void invaders_clear_formation_slot(int slot)
{
    invaders_clear_formation_state(
        formation_drawn_slots[slot],
        formation_slot_x[slot],
        formation_slot_y[slot],
        formation_row_min_slots[slot],
        formation_row_max_slots[slot]);
}

static void invaders_draw_formation_slot(int slot)
{
    int row;
    int col;
    int row_min[INVADER_ROWS];
    int row_max[INVADER_ROWS];

    for (row = 0; row < INVADER_ROWS; row++) {
        row_min[row] = INVADER_COLUMNS;
        row_max[row] = -1;
    }

    if (!formation_dirty_slots[slot]) {
        return;
    }

    if (invader_image != 0) {
        for (row = 0; row < INVADER_ROWS; row++) {
            for (col = 0; col < INVADER_COLUMNS; col++) {
                if (invader_alive[row][col]) {
                    if (col < row_min[row]) {
                        row_min[row] = col;
                    }
                    if (col > row_max[row]) {
                        row_max[row] = col;
                    }
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
    for (row = 0; row < INVADER_ROWS; row++) {
        formation_row_min_slots[slot][row] = row_min[row];
        formation_row_max_slots[slot][row] = row_max[row];
    }
}

static void invaders_draw_shields_slot(int slot)
{
    ANA_Rect shield_rect;
    int shield;
    int row;
    int col;
    int run_start;

    if (!shield_dirty_slots[slot]) {
        return;
    }

    for (shield = 0; shield < SHIELD_COUNT; shield++) {
        shield_rect = invaders_shield_rect(shield);
        invaders_fill_rect_black(shield_rect);

        for (row = 0; row < SHIELD_ROWS; row++) {
            run_start = -1;
            for (col = 0; col <= SHIELD_COLUMNS; col++) {
                if (col < SHIELD_COLUMNS &&
                        shield_cells[shield][row][col]) {
                    if (run_start < 0) {
                        run_start = col;
                    }
                    continue;
                }

                if (run_start >= 0) {
                    ana_fill_rect(
                        SHIELD_COLOR,
                        invaders_shield_x(shield) +
                            (run_start * SHIELD_CELL_SIZE),
                        SHIELD_Y + (row * SHIELD_CELL_SIZE),
                        (col - run_start) * SHIELD_CELL_SIZE,
                        SHIELD_CELL_SIZE);
                    run_start = -1;
                }
            }
        }
    }

    shield_dirty_slots[slot] = 0;
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

static void invaders_draw_centered_text(int y, const char* text)
{
    int x;

    if (hud_font == 0 || text == NULL) {
        return;
    }

    x = (ANA_DEFAULT_WIDTH - ana_text_width(hud_font, text)) / 2;
    ana_draw_text(hud_font, x, y, text);
}

static void invaders_draw_state_message(void)
{
    if (game_state == INVADERS_STATE_PLAYING) {
        return;
    }

    if (game_state == INVADERS_STATE_TITLE) {
        invaders_draw_centered_text(96, "ANA INVADERS");
        invaders_draw_centered_text(116, "PRESS FIRE");
        return;
    }

    if (game_state == INVADERS_STATE_CLEAR) {
        invaders_draw_centered_text(96, "CLEAR");
        invaders_draw_centered_text(116, "PRESS FIRE");
        return;
    }

    invaders_draw_centered_text(96, "GAME OVER");
    invaders_draw_centered_text(116, "PRESS FIRE");
}

static void invaders_store_draw_slot(int slot)
{
    InvadersDrawSlot* draw_slot;
    int bullet_width;
    int bullet_height;
    int i;

    draw_slot = &draw_slots[slot];
    bullet_width = bullet_image != 0 ? ana_image_width(bullet_image) : 2;
    bullet_height = bullet_image != 0 ? ana_image_height(bullet_image) : 6;
    draw_slot->has_player =
        game_state == INVADERS_STATE_PLAYING && player_image != 0;
    draw_slot->player_x = player_x;
    draw_slot->player_y = player_y;

    for (i = 0; i < PLAYER_BULLET_SLOTS; i++) {
        draw_slot->bullets[i] = player_bullets[i];
        if (game_state != INVADERS_STATE_PLAYING) {
            draw_slot->bullets[i].active = 0;
        }
        if (!invaders_bullet_should_draw(
                draw_slot->bullets[i],
                bullet_width,
                bullet_height)) {
            draw_slot->bullets[i].active = 0;
        }
    }

    for (i = 0; i < ALIEN_BULLET_SLOTS; i++) {
        draw_slot->alien_bullets[i] = alien_bullets[i];
        if (game_state != INVADERS_STATE_PLAYING) {
            draw_slot->alien_bullets[i].active = 0;
        }
        if (!invaders_bullet_should_draw(
                draw_slot->alien_bullets[i],
                bullet_width,
                bullet_height)) {
            draw_slot->alien_bullets[i].active = 0;
        }
    }

    for (i = 0; i < EXPLOSION_SLOTS; i++) {
        draw_slot->explosions[i] = explosions[i];
        if (game_state != INVADERS_STATE_PLAYING) {
            draw_slot->explosions[i].active = 0;
        }
    }
}

static void invaders_init(void)
{
    invaders_enter_title();
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
    invaders_assets_loaded = invaders_load_assets();
}

static void invaders_update(ANA_Time time)
{
    int player_width;
    int player_height;
    int bullet_width;
    int bullet_height;
    int action_pressed;

    demo_ticks = time.tick;
    player_width = player_image != 0 ? ana_image_width(player_image) : 16;
    player_height = player_image != 0 ? ana_image_height(player_image) : 10;
    bullet_width = bullet_image != 0 ? ana_image_width(bullet_image) : 2;
    bullet_height = bullet_image != 0 ? ana_image_height(bullet_image) : 6;
    action_pressed = ana_input_action_pressed(
        ANA_INPUT_DEVICE_0,
        ANA_ACTION_1);

    if (game_state == INVADERS_STATE_TITLE) {
        if (action_pressed) {
            invaders_start_new_game();
        }
        goto invaders_update_done;
    }

    if (game_state == INVADERS_STATE_CLEAR ||
            game_state == INVADERS_STATE_GAME_OVER) {
        if (action_pressed) {
            invaders_start_new_game();
        }
        goto invaders_update_done;
    }

    if (ana_input_direction(ANA_INPUT_DEVICE_0, ANA_INPUT_LEFT)) {
        player_x -= PLAYER_SPEED;
    }

    if (ana_input_direction(ANA_INPUT_DEVICE_0, ANA_INPUT_RIGHT)) {
        player_x += PLAYER_SPEED;
    }

    player_x = ana_clamp_int(player_x, 0, ANA_DEFAULT_WIDTH - player_width);

    if (action_pressed) {
        invaders_spawn_player_bullet(
            player_width,
            bullet_width,
            bullet_height);
    }

    invaders_update_player_bullets(bullet_height);
    invaders_update_alien_bullets(bullet_height);
    invaders_check_shield_hits(
        player_bullets,
        PLAYER_BULLET_SLOTS,
        bullet_width,
        bullet_height);
    invaders_check_shield_hits(
        alien_bullets,
        ALIEN_BULLET_SLOTS,
        bullet_width,
        bullet_height);
    invaders_check_bullet_hits(bullet_width, bullet_height);
    if (invaders_remaining <= 0) {
        invaders_enter_clear();
        goto invaders_update_done;
    }

    invaders_update_alien_fire(bullet_width);
    if (invaders_player_was_hit(
            player_width,
            player_height,
            bullet_width,
            bullet_height)) {
        invaders_lose_life();
        goto invaders_update_done;
    }

    if (invaders_update_formation()) {
        goto invaders_update_done;
    }

    invaders_update_explosions();

invaders_update_done:
    if (ana_quit_requested()) {
        ana_quit();
    }

#ifndef ANA_TARGET_AMIGA
    if (time.tick >= GAME_TICK_LIMIT) {
        ana_quit();
    }
#endif
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
        shield_dirty_slots[slot] = 1;
        full_clear_slots[slot] = 0;
        redraw_explosions = 1;
    } else {
        redraw_explosions = invaders_clear_previous_movers(slot);
    }

    invaders_draw_hud_slot(slot);
    if (game_state != INVADERS_STATE_PLAYING) {
        invaders_draw_state_message();
        invaders_store_draw_slot(slot);
        return;
    }

    if (invaders_process_removed_enemies(slot)) {
        redraw_explosions = 1;
    }
    if (shield_dirty_slots[slot] && formation_drawn_slots[slot]) {
        formation_dirty_slots[slot] = 1;
    }
    if (formation_dirty_slots[slot]) {
        redraw_explosions = 1;
        shield_dirty_slots[slot] = 1;
        invaders_clear_formation_slot(slot);
    }
    invaders_draw_shields_slot(slot);
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
        int bullet_width;
        int bullet_height;

        bullet_width = ana_image_width(bullet_image);
        bullet_height = ana_image_height(bullet_image);
        for (i = 0; i < PLAYER_BULLET_SLOTS; i++) {
            if (invaders_bullet_should_draw(
                    player_bullets[i],
                    bullet_width,
                    bullet_height)) {
                ana_draw_image(
                    bullet_image,
                    player_bullets[i].x,
                    player_bullets[i].y);
            }
        }
    }

    if (bullet_image != 0) {
        int bullet_width;
        int bullet_height;

        bullet_width = ana_image_width(bullet_image);
        bullet_height = ana_image_height(bullet_image);
        for (i = 0; i < ALIEN_BULLET_SLOTS; i++) {
            if (invaders_bullet_should_draw(
                    alien_bullets[i],
                    bullet_width,
                    bullet_height)) {
                ana_draw_image(
                    bullet_image,
                    alien_bullets[i].x,
                    alien_bullets[i].y);
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
    invaders_free_assets();
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
