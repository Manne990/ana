#include "invaders_internal.h"

static int invaders_draw_slot_index(void)
{
    return demo_ticks & (INVADERS_DRAW_SLOTS - 1);
}

static int invaders_bullet_should_draw(
    InvadersBullet bullet,
    int bullet_width,
    int bullet_height)
{
    (void)bullet_width;
    (void)bullet_height;

    return bullet.active;
}

static void invaders_fill_rect_black(ANA_Rect rect)
{
    rect = ana_rect_align_x8(rect, 0, ANA_DEFAULT_WIDTH);
    ana_fill_rect(0u, rect.x, rect.y, rect.w, rect.h);
}

static void invaders_repair_shields_overlap(int slot, ANA_Rect rect)
{
    ANA_Rect cell_rect;
    int shield;
    int row;
    int col;
    int dirty_mask;

    dirty_mask = shield_dirty_slots[slot];
    for (shield = 0; shield < SHIELD_COUNT; shield++) {
        if ((dirty_mask & (1 << shield)) != 0) {
            continue;
        }

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
    return ana_rect_make(
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
    int min_row;
    int max_row;
    int min_col;
    int max_col;

    if (invader_image == 0 || invaders_remaining <= 0) {
        return;
    }

    formation_rect = ana_rect_make(
        invader_formation_x,
        invader_formation_y,
        INVADER_FORMATION_WIDTH,
        INVADER_FORMATION_HEIGHT);
    if (!ana_rect_intersects(rect, formation_rect)) {
        return;
    }

    min_col =
        (rect.x - invader_formation_x - INVADER_WIDTH) / INVADER_SPACING_X;
    max_col =
        (rect.x + rect.w - invader_formation_x) / INVADER_SPACING_X;
    min_row =
        (rect.y - invader_formation_y - INVADER_HEIGHT) / INVADER_SPACING_Y;
    max_row =
        (rect.y + rect.h - invader_formation_y) / INVADER_SPACING_Y;

    if (min_col < 0) {
        min_col = 0;
    }
    if (min_row < 0) {
        min_row = 0;
    }
    if (max_col >= INVADER_COLUMNS) {
        max_col = INVADER_COLUMNS - 1;
    }
    if (max_row >= INVADER_ROWS) {
        max_row = INVADER_ROWS - 1;
    }

    if (min_col > max_col || min_row > max_row) {
        return;
    }

    enemy_rect.w = INVADER_WIDTH;
    enemy_rect.h = INVADER_HEIGHT;
    for (row = min_row; row <= max_row; row++) {
        enemy_y = invaders_enemy_y(row);
        enemy_rect.y = enemy_y;

        for (col = min_col; col <= max_col; col++) {
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
    rect = ana_rect_align_x8(rect, 0, ANA_DEFAULT_WIDTH);
    invaders_fill_rect_black(rect);
    if (!formation_dirty_slots[slot]) {
        invaders_repair_formation_overlap(rect);
    }
    invaders_repair_shields_overlap(slot, rect);

    return invaders_rect_overlaps_active_explosion(rect);
}

static int invaders_clear_movers_from_state(
    int slot,
    const InvadersDrawSlot* draw_slot)
{
    ANA_Rect rect;
    int bullet_width;
    int bullet_height;
    int redraw_explosions;
    int i;

    if (draw_slot == NULL) {
        return 0;
    }

    bullet_width = bullet_image != 0 ? ana_image_width(bullet_image) : 2;
    bullet_height = bullet_image != 0 ? ana_image_height(bullet_image) : 6;
    redraw_explosions = 0;

    rect = ana_bob_previous_rect(&draw_slot->player_bob);
    if (!ana_rect_is_empty(rect)) {
        if (invaders_clear_previous_rect(slot, rect)) {
            redraw_explosions = 1;
        }
    }

    for (i = 0; i < PLAYER_BULLET_SLOTS; i++) {
        if (draw_slot->bullets[i].active) {
            rect = ana_rect_make(
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
            rect = ana_rect_make(
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
        rect = ana_rect_align_x8(
            removed_enemy_rects[slot][i],
            0,
            ANA_DEFAULT_WIDTH);
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
            ana_rect_make(
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
    int dirty_mask;

    dirty_mask = shield_dirty_slots[slot];
    if (dirty_mask == 0) {
        return;
    }

    for (shield = 0; shield < SHIELD_COUNT; shield++) {
        if ((dirty_mask & (1 << shield)) == 0) {
            continue;
        }

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

static int invaders_dirty_shields_overlap_formation(int slot)
{
    ANA_Rect formation_rect;
    int shield;
    int dirty_mask;

    dirty_mask = shield_dirty_slots[slot];
    if (dirty_mask == 0 || invaders_remaining <= 0) {
        return 0;
    }

    formation_rect = ana_rect_make(
        invader_formation_x,
        invader_formation_y,
        INVADER_FORMATION_WIDTH,
        INVADER_FORMATION_HEIGHT);

    for (shield = 0; shield < SHIELD_COUNT; shield++) {
        if ((dirty_mask & (1 << shield)) == 0) {
            continue;
        }

        if (ana_rect_intersects(formation_rect, invaders_shield_rect(shield))) {
            return 1;
        }
    }

    return 0;
}

static void invaders_init_hud_labels(InvadersDrawSlot* draw_slot)
{
    if (draw_slot->hud_labels_ready) {
        return;
    }

    ana_label_init(
        &draw_slot->score_label,
        hud_font,
        HUD_SCORE_X,
        HUD_SCORE_Y,
        HUD_SCORE_CLEAR_WIDTH);
    ana_label_init(
        &draw_slot->lives_label,
        hud_font,
        HUD_LIVES_X,
        HUD_LIVES_Y,
        HUD_LIVES_CLEAR_WIDTH);
    ana_label_init(
        &draw_slot->status_label,
        hud_font,
        HUD_STATUS_X,
        HUD_STATUS_Y,
        HUD_STATUS_CLEAR_WIDTH);

    draw_slot->score_label.color = 5u;
    draw_slot->lives_label.color = 5u;
    draw_slot->status_label.color = 5u;
    draw_slot->hud_labels_ready = 1;
}

static char* invaders_copy_text(char* out, const char* text)
{
    while (*text != '\0') {
        *out++ = *text++;
    }

    return out;
}

static char* invaders_write_int(char* out, int value)
{
    char reversed[12];
    int count;
    int i;
    unsigned int magnitude;

    if (value < 0) {
        *out++ = '-';
        magnitude = (unsigned int)(-(value + 1)) + 1u;
    } else {
        magnitude = (unsigned int)value;
    }

    count = 0;
    do {
        reversed[count++] = (char)('0' + (magnitude % 10u));
        magnitude /= 10u;
    } while (magnitude != 0u && count < (int)sizeof(reversed));

    for (i = count - 1; i >= 0; i--) {
        *out++ = reversed[i];
    }

    return out;
}

static void invaders_make_hud_number_text(
    char* out,
    const char* label,
    int value)
{
    out = invaders_copy_text(out, label);
    out = invaders_write_int(out, value);
    *out = '\0';
}

static void invaders_make_hud_status_text(char* out, const char* status)
{
    out = invaders_copy_text(out, "STATUS ");
    out = invaders_copy_text(out, status);
    *out = '\0';
}

static void invaders_draw_hud_slot(int slot)
{
    InvadersDrawSlot* draw_slot;
    char score_text[32];
    char lives_text[32];
    char status_text[32];
    const char* status;

    if (hud_font == 0) {
        return;
    }

    draw_slot = &draw_slots[slot];
    status = invaders_status_text();
    invaders_init_hud_labels(draw_slot);

    invaders_make_hud_number_text(score_text, "SCORE ", score);
    invaders_make_hud_number_text(lives_text, "LIVES ", lives);
    invaders_make_hud_status_text(status_text, status);

    ana_label_set_text(&draw_slot->score_label, score_text);
    ana_label_set_text(&draw_slot->lives_label, lives_text);
    ana_label_set_text(&draw_slot->status_label, status_text);
    ana_label_draw_if_dirty(&draw_slot->score_label);
    ana_label_draw_if_dirty(&draw_slot->lives_label);
    ana_label_draw_if_dirty(&draw_slot->status_label);
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

    ana_bob_init(&draw_slot->player_bob, player_image);
    ana_bob_set_position(&draw_slot->player_bob, player_x, player_y);
    ana_bob_set_visible(
        &draw_slot->player_bob,
        game_state == INVADERS_STATE_PLAYING && player_image != 0);
    ana_bob_commit(&draw_slot->player_bob);

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

void invaders_draw(void)
{
    int i;
    int frame;
    int slot;
    int redraw_explosions;
    ANA_Bob player_bob;

    slot = invaders_draw_slot_index();
    redraw_explosions = 0;

    if (full_clear_slots[slot]) {
        ana_fill_rect(0u, 0, 0, ANA_DEFAULT_WIDTH, ANA_DEFAULT_HEIGHT);
        memset(&draw_slots[slot], 0, sizeof(draw_slots[slot]));
        formation_drawn_slots[slot] = 0;
        formation_dirty_slots[slot] = 1;
        shield_dirty_slots[slot] = SHIELD_DIRTY_ALL;
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
    if (formation_drawn_slots[slot] &&
            invaders_dirty_shields_overlap_formation(slot)) {
        formation_dirty_slots[slot] = 1;
    }
    if (formation_dirty_slots[slot]) {
        redraw_explosions = 1;
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
        ana_bob_init(&player_bob, player_image);
        ana_bob_set_position(&player_bob, player_x, player_y);
        ana_bob_draw(&player_bob);
    }

    invaders_store_draw_slot(slot);
}
