/* Dirty-rect retained renderer for ANA Invaders. Tracks previous object
   positions and redraw dependencies to keep Amiga frame updates small. */

#include "invaders_internal.h"

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

void invaders_render_reset(void)
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

void invaders_render_request_full_clear(void)
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

void invaders_render_mark_formation_dirty(void)
{
    int i;

    for (i = 0; i < INVADERS_DRAW_SLOTS; i++) {
        formation_dirty_slots[i] = 1;
        removed_enemy_counts[i] = 0;
    }
}

void invaders_render_mark_shields_dirty(void)
{
    int i;

    for (i = 0; i < INVADERS_DRAW_SLOTS; i++) {
        shield_dirty_slots[i] = SHIELD_DIRTY_ALL;
    }
}

void invaders_render_mark_shield_dirty(int shield)
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

void invaders_render_queue_removed_enemy_rect(int x, int y)
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

static int invaders_draw_slot_index(void)
{
    ANA_RenderStats stats;

    if (INVADERS_DRAW_SLOTS <= 1) {
        return 0;
    }

    stats = ana_render_stats();
    return (int)(stats.frames & (INVADERS_DRAW_SLOTS - 1));
}

static int invaders_bullet_should_draw(InvadersBullet bullet)
{
    return bullet.active;
}

static void invaders_fill_rect_black(ANA_Rect rect)
{
    ana_retained_clear_rect_x8(rect, 0u, 0, ANA_DEFAULT_WIDTH, 0, 0);
}

static void invaders_repair_shields_overlap(int slot, ANA_Rect rect)
{
    ANA_Rect shield_rect;
    ANA_Rect cell_rect;
    int shield;
    int shield_x;
    int row;
    int col;
    int min_row;
    int max_row;
    int min_col;
    int max_col;
    int dirty_mask;

    dirty_mask = shield_dirty_slots[slot];
    for (shield = 0; shield < SHIELD_COUNT; shield++) {
        if ((dirty_mask & (1 << shield)) != 0) {
            continue;
        }

        shield_rect = invaders_shield_rect(shield);
        if (!ana_rect_intersects(rect, shield_rect)) {
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

    if (ana_rect_is_empty(rect)) {
        return 0;
    }

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
    return ana_bob_is_unchanged(&draw_slot->explosion_bobs[index]);
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

static int invaders_rect_overlaps_formation_layer(int slot, ANA_Rect rect)
{
    ANA_Rect formation_rect;

    if (ana_rect_is_empty(rect) ||
            formation_dirty_slots[slot] ||
            !formation_drawn_slots[slot] ||
            invader_image == 0 ||
            invaders_remaining <= 0) {
        return 0;
    }

    formation_rect = ana_rect_make(
        invader_formation_x,
        invader_formation_y,
        INVADER_FORMATION_WIDTH,
        INVADER_FORMATION_HEIGHT);

    return ana_rect_intersects(rect, formation_rect);
}

static int invaders_rect_overlaps_shield_layer(int slot, ANA_Rect rect)
{
    int dirty_mask;
    int shield;

    if (ana_rect_is_empty(rect)) {
        return 0;
    }

    dirty_mask = shield_dirty_slots[slot];
    for (shield = 0; shield < SHIELD_COUNT; shield++) {
        if ((dirty_mask & (1 << shield)) != 0) {
            continue;
        }

        if (ana_rect_intersects(rect, invaders_shield_rect(shield))) {
            return 1;
        }
    }

    return 0;
}

static void invaders_repair_formation_layer(ANA_Rect rect, void* user_data)
{
    int slot;

    slot = *(int*)user_data;
    if (!formation_dirty_slots[slot]) {
        invaders_repair_formation_overlap(rect);
    }
}

static void invaders_repair_shields_layer(ANA_Rect rect, void* user_data)
{
    int slot;

    slot = *(int*)user_data;
    invaders_repair_shields_overlap(slot, rect);
}

static int invaders_clear_previous_bob(int slot, const ANA_Bob* bob)
{
    ANA_RetainedLayer layers[2];
    ANA_Rect rect;
    int layer_count;
    int layer_slot;

    if (bob == NULL) {
        return 0;
    }

    rect = ana_rect_align_x8(
        ana_bob_previous_rect(bob),
        0,
        ANA_DEFAULT_WIDTH);
    if (ana_rect_is_empty(rect)) {
        return 0;
    }

    layer_slot = slot;
    layer_count = 0;
    if (invaders_rect_overlaps_formation_layer(slot, rect)) {
        layers[layer_count].redraw = invaders_repair_formation_layer;
        layers[layer_count].user_data = &layer_slot;
        layer_count++;
    }
    if (invaders_rect_overlaps_shield_layer(slot, rect)) {
        layers[layer_count].redraw = invaders_repair_shields_layer;
        layers[layer_count].user_data = &layer_slot;
        layer_count++;
    }

    rect = ana_bob_clear_previous_masked_x8_with_layers(
        bob,
        0,
        ANA_DEFAULT_WIDTH,
        layers,
        layer_count);

    return invaders_rect_overlaps_active_explosion(rect);
}

static int invaders_clear_movers_from_state(
    int slot,
    const InvadersDrawSlot* draw_slot)
{
    int redraw_explosions;
    int i;

    if (draw_slot == NULL) {
        return 0;
    }

    redraw_explosions = 0;

    if (!ana_bob_is_unchanged(&draw_slot->player_bob)) {
        if (invaders_clear_previous_bob(slot, &draw_slot->player_bob)) {
            redraw_explosions = 1;
        }
    }

    for (i = 0; i < PLAYER_BULLET_SLOTS; i++) {
        if (!ana_bob_is_unchanged(&draw_slot->player_bullet_bobs[i])) {
            if (invaders_clear_previous_bob(
                    slot,
                    &draw_slot->player_bullet_bobs[i])) {
                redraw_explosions = 1;
            }
        }
    }

    for (i = 0; i < ALIEN_BULLET_SLOTS; i++) {
        if (!ana_bob_is_unchanged(&draw_slot->alien_bullet_bobs[i])) {
            if (invaders_clear_previous_bob(
                    slot,
                    &draw_slot->alien_bullet_bobs[i])) {
                redraw_explosions = 1;
            }
        }
    }

    for (i = 0; i < EXPLOSION_SLOTS; i++) {
        if (!ana_bob_is_unchanged(&draw_slot->explosion_bobs[i])) {
            if (invaders_clear_previous_bob(
                    slot,
                    &draw_slot->explosion_bobs[i])) {
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

static void invaders_configure_bob(
    ANA_Bob* bob,
    ANA_Image image,
    int frame,
    int x,
    int y,
    int visible)
{
    if (bob == NULL) {
        return;
    }

    ana_bob_set_image(bob, image);
    ana_bob_set_frame(bob, frame);
    ana_bob_set_position(bob, x, y);
    ana_bob_set_visible(bob, visible && image != 0);
    bob->clear_color = 0u;
}

static void invaders_prepare_draw_slot(int slot)
{
    InvadersDrawSlot* draw_slot;
    int i;

    draw_slot = &draw_slots[slot];

    invaders_configure_bob(
        &draw_slot->player_bob,
        player_image,
        0,
        player_x,
        player_y,
        game_state == INVADERS_STATE_PLAYING);

    for (i = 0; i < PLAYER_BULLET_SLOTS; i++) {
        invaders_configure_bob(
            &draw_slot->player_bullet_bobs[i],
            bullet_image,
            0,
            player_bullets[i].x,
            player_bullets[i].y,
            game_state == INVADERS_STATE_PLAYING &&
                invaders_bullet_should_draw(player_bullets[i]));
    }

    for (i = 0; i < ALIEN_BULLET_SLOTS; i++) {
        invaders_configure_bob(
            &draw_slot->alien_bullet_bobs[i],
            bullet_image,
            0,
            alien_bullets[i].x,
            alien_bullets[i].y,
            game_state == INVADERS_STATE_PLAYING &&
                invaders_bullet_should_draw(alien_bullets[i]));
    }

    for (i = 0; i < EXPLOSION_SLOTS; i++) {
        invaders_configure_bob(
            &draw_slot->explosion_bobs[i],
            explosion_image,
            invaders_explosion_frame_for_age(explosions[i].age),
            explosions[i].x,
            explosions[i].y,
            game_state == INVADERS_STATE_PLAYING && explosions[i].active);
    }
}

static void invaders_commit_draw_slot(int slot)
{
    InvadersDrawSlot* draw_slot;
    int i;

    draw_slot = &draw_slots[slot];

    ana_bob_commit(&draw_slot->player_bob);
    for (i = 0; i < PLAYER_BULLET_SLOTS; i++) {
        ana_bob_commit(&draw_slot->player_bullet_bobs[i]);
    }
    for (i = 0; i < ALIEN_BULLET_SLOTS; i++) {
        ana_bob_commit(&draw_slot->alien_bullet_bobs[i]);
    }
    for (i = 0; i < EXPLOSION_SLOTS; i++) {
        ana_bob_commit(&draw_slot->explosion_bobs[i]);
    }
}

void invaders_draw(void)
{
    InvadersDrawSlot* draw_slot;
    int i;
    int slot;
    int redraw_explosions;

    slot = invaders_draw_slot_index();
    draw_slot = &draw_slots[slot];
    redraw_explosions = 0;

    if (full_clear_slots[slot]) {
        ana_clear(0u);
        memset(draw_slot, 0, sizeof(*draw_slot));
        formation_drawn_slots[slot] = 0;
        formation_dirty_slots[slot] = 1;
        shield_dirty_slots[slot] = SHIELD_DIRTY_ALL;
        full_clear_slots[slot] = 0;
        redraw_explosions = 1;
    }

    invaders_prepare_draw_slot(slot);
    if (!redraw_explosions) {
        redraw_explosions = invaders_clear_previous_movers(slot);
    }
    invaders_draw_hud_slot(slot);
    if (game_state != INVADERS_STATE_PLAYING) {
        invaders_draw_state_message();
        invaders_commit_draw_slot(slot);
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

    for (i = 0; i < EXPLOSION_SLOTS; i++) {
        if (draw_slot->explosion_bobs[i].visible) {
            if (!redraw_explosions &&
                    invaders_explosion_unchanged_for_slot(draw_slot, i)) {
                continue;
            }

            ana_bob_draw(&draw_slot->explosion_bobs[i]);
        }
    }

    for (i = 0; i < PLAYER_BULLET_SLOTS; i++) {
        if (draw_slot->player_bullet_bobs[i].visible) {
            ana_bob_draw(&draw_slot->player_bullet_bobs[i]);
        }
    }

    for (i = 0; i < ALIEN_BULLET_SLOTS; i++) {
        if (draw_slot->alien_bullet_bobs[i].visible) {
            ana_bob_draw(&draw_slot->alien_bullet_bobs[i]);
        }
    }

    ana_bob_draw(&draw_slot->player_bob);

    invaders_commit_draw_slot(slot);
}
