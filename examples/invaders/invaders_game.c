#include "invaders_internal.h"

unsigned char invader_alive[INVADER_ROWS][INVADER_COLUMNS];
InvadersExplosion explosions[EXPLOSION_SLOTS];
InvadersBullet player_bullets[PLAYER_BULLET_SLOTS];
InvadersBullet alien_bullets[ALIEN_BULLET_SLOTS];
InvadersDrawSlot draw_slots[INVADERS_DRAW_SLOTS];
ANA_Rect removed_enemy_rects
    [INVADERS_DRAW_SLOTS][INVADERS_REMOVED_ENEMY_SLOTS];
int removed_enemy_counts[INVADERS_DRAW_SLOTS];
int formation_dirty_slots[INVADERS_DRAW_SLOTS];
int formation_drawn_slots[INVADERS_DRAW_SLOTS];
int formation_slot_x[INVADERS_DRAW_SLOTS];
int formation_slot_y[INVADERS_DRAW_SLOTS];
int formation_row_min_slots[INVADERS_DRAW_SLOTS][INVADER_ROWS];
int formation_row_max_slots[INVADERS_DRAW_SLOTS][INVADER_ROWS];
int shield_dirty_slots[INVADERS_DRAW_SLOTS];
int full_clear_slots[INVADERS_DRAW_SLOTS];
unsigned char shield_cells[SHIELD_COUNT][SHIELD_ROWS][SHIELD_COLUMNS];
int player_x = 152;
int player_y = 220;
int invader_formation_x = INVADER_START_X;
int invader_formation_y = INVADER_START_Y;
int invader_direction = 1;
int invader_frame = 0;
ANA_Timer invader_move_timer;
ANA_Timer alien_fire_timer;
int invaders_remaining = 0;
int score = 0;
int lives = 3;
InvadersGameState game_state = INVADERS_STATE_TITLE;
int demo_ticks = 0;
int invaders_assets_loaded = 0;
unsigned long invaders_rng_state = 0x13579bdfUL;

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
        shield_dirty_slots[i] = SHIELD_DIRTY_ALL;
        removed_enemy_counts[i] = 0;
        draw_slots[i].hud_labels_ready = 0;
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
        shield_dirty_slots[i] = SHIELD_DIRTY_ALL;
    }
}

static void invaders_mark_shield_dirty(int shield)
{
    int i;
    int bit;

    if (shield < 0 || shield >= SHIELD_COUNT) {
        return;
    }

    bit = 1 << shield;
    for (i = 0; i < INVADERS_DRAW_SLOTS; i++) {
        shield_dirty_slots[i] |= bit;
    }
}

int invaders_shield_x(int shield)
{
    return SHIELD_START_X + (shield * SHIELD_SPACING_X);
}

ANA_Rect invaders_shield_cell_rect(int shield, int row, int col)
{
    ANA_Rect rect;

    rect.x = invaders_shield_x(shield) + (col * SHIELD_CELL_SIZE);
    rect.y = SHIELD_Y + (row * SHIELD_CELL_SIZE);
    rect.w = SHIELD_CELL_SIZE;
    rect.h = SHIELD_CELL_SIZE;

    return rect;
}

ANA_Rect invaders_shield_rect(int shield)
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
        shield_dirty_slots[i] = SHIELD_DIRTY_ALL;
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
    ana_stop_music();

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
    invaders_play_theme_music();

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
    invaders_play_theme_music();

    game_state = INVADERS_STATE_CLEAR;
    invaders_reset_player_bullets();
    invaders_reset_alien_bullets();
    invaders_request_full_clear();
}

static void invaders_enter_game_over(void)
{
    invaders_play_theme_music();

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

int invaders_enemy_x(int col)
{
    return invader_formation_x + (col * INVADER_SPACING_X);
}

int invaders_enemy_y(int row)
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

void invaders_update_explosions(void)
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

int invaders_rect_overlaps_alive_shield(ANA_Rect rect)
{
    ANA_Rect cell_rect;
    int shield;
    int shield_x;
    int row;
    int col;
    int min_row;
    int max_row;
    int min_col;
    int max_col;

    for (shield = 0; shield < SHIELD_COUNT; shield++) {
        if (!ana_rect_intersects(rect, invaders_shield_rect(shield))) {
            continue;
        }

        shield_x = invaders_shield_x(shield);
        min_col = rect.x <= shield_x ?
            0 :
            (rect.x - shield_x) / SHIELD_CELL_SIZE;
        max_col = rect.x + rect.w >= shield_x + SHIELD_WIDTH ?
            SHIELD_COLUMNS - 1 :
            ((rect.x + rect.w - 1) - shield_x) / SHIELD_CELL_SIZE;
        min_row = rect.y <= SHIELD_Y ?
            0 :
            (rect.y - SHIELD_Y) / SHIELD_CELL_SIZE;
        max_row = rect.y + rect.h >= SHIELD_Y + SHIELD_HEIGHT ?
            SHIELD_ROWS - 1 :
            ((rect.y + rect.h - 1) - SHIELD_Y) / SHIELD_CELL_SIZE;

        for (row = min_row; row <= max_row; row++) {
            for (col = min_col; col <= max_col; col++) {
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
    int shield_x;
    int row;
    int col;
    int min_row;
    int max_row;
    int min_col;
    int max_col;
    int damaged;

    damaged = 0;
    for (shield = 0; shield < SHIELD_COUNT; shield++) {
        if (!ana_rect_intersects(rect, invaders_shield_rect(shield))) {
            continue;
        }

        shield_x = invaders_shield_x(shield);
        min_col = rect.x <= shield_x ?
            0 :
            (rect.x - shield_x) / SHIELD_CELL_SIZE;
        max_col = rect.x + rect.w >= shield_x + SHIELD_WIDTH ?
            SHIELD_COLUMNS - 1 :
            ((rect.x + rect.w - 1) - shield_x) / SHIELD_CELL_SIZE;
        min_row = rect.y <= SHIELD_Y ?
            0 :
            (rect.y - SHIELD_Y) / SHIELD_CELL_SIZE;
        max_row = rect.y + rect.h >= SHIELD_Y + SHIELD_HEIGHT ?
            SHIELD_ROWS - 1 :
            ((rect.y + rect.h - 1) - SHIELD_Y) / SHIELD_CELL_SIZE;

        for (row = min_row; row <= max_row; row++) {
            for (col = min_col; col <= max_col; col++) {
                if (!shield_cells[shield][row][col]) {
                    continue;
                }

                cell_rect = invaders_shield_cell_rect(shield, row, col);
                if (ana_rect_intersects(rect, cell_rect)) {
                    shield_cells[shield][row][col] = 0u;
                    damaged = 1;
                    invaders_mark_shield_dirty(shield);
                    if (stop_after_first) {
                        return 1;
                    }
                }
            }
        }
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

    if (invader_formation_y + INVADER_FORMATION_HEIGHT <= SHIELD_Y) {
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

void invaders_update_alien_fire(int bullet_width)
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

void invaders_update_player_bullets(int bullet_height)
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

void invaders_update_alien_bullets(int bullet_height)
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

const char* invaders_status_text(void)
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

void invaders_init(void)
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

void invaders_load(void)
{
    invaders_assets_loaded = invaders_load_assets();
}

void invaders_update(ANA_Time time)
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

void invaders_shutdown(void)
{
    invaders_free_assets();
}

int invaders_assets_are_loaded(void)
{
    return invaders_assets_loaded;
}
