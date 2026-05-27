/* Rendering for AMAze: tile drawing, simple HUD digits, actor primitives, and
   retained redraw state for direct-present Amiga builds. */

#include "amaze_internal.h"

#if defined(ANA_TARGET_AMIGA) && defined(ANA_AMIGA_DIRECT_PRESENT)
#define AMAZE_DRAW_SLOTS 1
#else
#define AMAZE_DRAW_SLOTS 2
#endif

typedef struct AMAzeDrawSlot {
    AMAzeActor player;
    AMAzeActor chasers[AMAZE_CHASERS];
    int actors_valid;
    int full_redraw;
    int status_valid;
    int score;
    int lives;
    int power_ticks;
} AMAzeDrawSlot;

static const unsigned char amaze_digit_rows[10][5] = {
    {7, 5, 5, 5, 7},
    {2, 6, 2, 2, 7},
    {7, 1, 7, 4, 7},
    {7, 1, 7, 1, 7},
    {5, 5, 7, 1, 1},
    {7, 4, 7, 1, 7},
    {7, 4, 7, 5, 7},
    {7, 1, 2, 2, 2},
    {7, 5, 7, 5, 7},
    {7, 5, 7, 1, 7}
};

static AMAzeDrawSlot amaze_draw_slots[AMAZE_DRAW_SLOTS];

void amaze_render_request_full_redraw(void)
{
    int i;

    for (i = 0; i < AMAZE_DRAW_SLOTS; ++i) {
        amaze_draw_slots[i].full_redraw = 1;
        amaze_draw_slots[i].actors_valid = 0;
        amaze_draw_slots[i].status_valid = 0;
    }
}

static void amaze_draw_businessman(const AMAzeActor* actor)
{
    int x;
    int y;

    x = AMAZE_ORIGIN_X + actor->tx * AMAZE_TILE;
    y = AMAZE_ORIGIN_Y + actor->ty * AMAZE_TILE;
    if (amaze_businessman_image != 0) {
        ana_draw_image(amaze_businessman_image, x, y);
        return;
    }

    ana_fill_rect(AMAZE_SKIN_COLOR, x + 2, y + 1, 4, 2);
    ana_fill_rect(AMAZE_SUIT_COLOR, x + 1, y + 3, 6, 4);
    ana_fill_rect(5, x + 3, y + 3, 2, 2);
    ana_fill_rect(AMAZE_TIE_COLOR, x + 4, y + 4, 1, 3);
    ana_fill_rect(AMAZE_SUIT_COLOR, x + 2, y + 7, 1, 1);
    ana_fill_rect(AMAZE_SUIT_COLOR, x + 5, y + 7, 1, 1);
}

static void amaze_draw_collector(const AMAzeActor* actor)
{
    ANA_Image image;
    int x;
    int y;

    x = AMAZE_ORIGIN_X + actor->tx * AMAZE_TILE;
    y = AMAZE_ORIGIN_Y + actor->ty * AMAZE_TILE;
    if (actor->color == AMAZE_CHASER_FLEE_COLOR) {
        image = amaze_taxman_blue_image;
    } else if (actor->color == 3) {
        image = amaze_taxman_red_image;
    } else if (actor->color == 4) {
        image = amaze_taxman_pink_image;
    } else {
        image = amaze_taxman_cyan_image;
    }

    if (image != 0) {
        ana_draw_image(image, x, y);
        return;
    }

    ana_fill_rect(actor->color, x + 1, y + 2, 6, 5);
    ana_fill_rect(5, x + 2, y + 1, 4, 2);
    ana_fill_rect(AMAZE_SUIT_COLOR, x + 2, y + 6, 1, 1);
    ana_fill_rect(AMAZE_SUIT_COLOR, x + 5, y + 6, 1, 1);
    if (actor->color != AMAZE_CHASER_FLEE_COLOR) {
        ana_fill_rect(AMAZE_TIE_COLOR, x + 4, y + 3, 1, 3);
    }
}

static int amaze_draw_slot_index(void)
{
    ANA_RenderStats stats;

    if (AMAZE_DRAW_SLOTS <= 1) {
        return 0;
    }

    stats = ana_render_stats();
    return (int)(stats.frames & (AMAZE_DRAW_SLOTS - 1));
}

static int amaze_actor_changed(const AMAzeActor* previous, const AMAzeActor* current)
{
    return previous->tx != current->tx ||
            previous->ty != current->ty ||
            previous->color != current->color;
}

static int amaze_actors_changed(const AMAzeDrawSlot* slot)
{
    int i;

    if (!slot->actors_valid ||
            amaze_actor_changed(&slot->player, &amaze_player)) {
        return 1;
    }

    for (i = 0; i < AMAZE_CHASERS; ++i) {
        if (amaze_actor_changed(&slot->chasers[i], &amaze_chasers[i])) {
            return 1;
        }
    }

    return 0;
}

static void amaze_store_draw_slot(AMAzeDrawSlot* slot)
{
    int i;

    slot->player = amaze_player;
    for (i = 0; i < AMAZE_CHASERS; ++i) {
        slot->chasers[i] = amaze_chasers[i];
    }

    slot->actors_valid = 1;
    slot->full_redraw = 0;
    slot->status_valid = 1;
    slot->score = amaze_score;
    slot->lives = amaze_lives;
    slot->power_ticks = amaze_power_ticks;
}

static void amaze_draw_tile(int tx, int ty)
{
    int px;
    int py;

    px = AMAZE_ORIGIN_X + tx * AMAZE_TILE;
    py = AMAZE_ORIGIN_Y + ty * AMAZE_TILE;

    ana_fill_rect(0, px, py, AMAZE_TILE, AMAZE_TILE);
    if (amaze_map[ty][tx] == '#') {
        ana_fill_rect(1, px, py, AMAZE_TILE, AMAZE_TILE);
    } else if (amaze_map[ty][tx] == 'o') {
        if (amaze_gold_bag_image != 0) {
            ana_draw_image(amaze_gold_bag_image, px, py);
        } else {
            ana_fill_rect(2, px + 2, py + 3, 4, 4);
            ana_fill_rect(2, px + 3, py + 1, 2, 2);
            ana_fill_rect(5, px + 3, py + 4, 2, 1);
        }
    } else if (amaze_map[ty][tx] == '.') {
        if (amaze_coin_image != 0) {
            ana_draw_image(amaze_coin_image, px, py);
        } else {
            ana_fill_rect(2, px + 3, py + 3, 2, 2);
        }
    }
}

static void amaze_draw_maze(void)
{
    int x;
    int y;

    for (y = 0; y < AMAZE_MAP_H; ++y) {
        for (x = 0; x < AMAZE_MAP_W; ++x) {
            amaze_draw_tile(x, y);
        }
    }
}

static void amaze_draw_digit(int digit, int x, int y, int color)
{
    int row;
    int col;
    unsigned char bits;

    if (digit < 0 || digit > 9) {
        return;
    }

    for (row = 0; row < 5; ++row) {
        bits = amaze_digit_rows[digit][row];
        for (col = 0; col < 3; ++col) {
            if ((bits & (1 << (2 - col))) != 0) {
                ana_fill_rect(color, x + col * 2, y + row * 2, 2, 2);
            }
        }
    }
}

static void amaze_draw_number(int value, int x, int y, int color)
{
    int digits[6];
    int count;
    int i;

    if (value < 0) {
        value = 0;
    }

    if (value == 0) {
        amaze_draw_digit(0, x, y, color);
        return;
    }

    count = 0;
    while (value > 0 && count < 6) {
        digits[count] = value % 10;
        value /= 10;
        ++count;
    }

    for (i = count - 1; i >= 0; --i) {
        amaze_draw_digit(digits[i], x, y, color);
        x += 8;
    }
}

static void amaze_draw_status(void)
{
    int i;

    ana_fill_rect(0, 8, 10, 150, 16);
    ana_fill_rect(0, 230, 10, 78, 16);

    ana_fill_rect(5, 12, 12, 40, 3);
    amaze_draw_number(amaze_score, 58, 10, 2);

    for (i = 0; i < amaze_lives; ++i) {
        ana_fill_rect(2, 236 + i * 14, 14, 10, 6);
    }

    if (amaze_power_ticks > 0) {
        ana_fill_rect(AMAZE_CHASER_FLEE_COLOR, 140, 12, 12, 6);
    }
}

static void amaze_draw_actors(void)
{
    int i;

    amaze_draw_businessman(&amaze_player);
    for (i = 0; i < AMAZE_CHASERS; ++i) {
        amaze_draw_collector(&amaze_chasers[i]);
    }
}

static void amaze_draw_previous_actor_tiles(const AMAzeDrawSlot* slot)
{
    int i;

    amaze_draw_tile(slot->player.tx, slot->player.ty);
    for (i = 0; i < AMAZE_CHASERS; ++i) {
        amaze_draw_tile(slot->chasers[i].tx, slot->chasers[i].ty);
    }
}

static void amaze_draw_full_slot(AMAzeDrawSlot* slot)
{
    ana_clear(0);
    amaze_draw_status();
    amaze_draw_maze();
    amaze_draw_actors();
    amaze_store_draw_slot(slot);
}

static void amaze_draw_retained_slot(AMAzeDrawSlot* slot)
{
    if (slot->full_redraw || !slot->actors_valid) {
        amaze_draw_full_slot(slot);
        return;
    }

    if (!slot->status_valid ||
            slot->score != amaze_score ||
            slot->lives != amaze_lives ||
            (slot->power_ticks == 0) != (amaze_power_ticks == 0)) {
        amaze_draw_status();
    }

    if (amaze_actors_changed(slot)) {
        amaze_draw_previous_actor_tiles(slot);
        amaze_draw_actors();
    }

    amaze_store_draw_slot(slot);
}

void amaze_draw(void)
{
    int slot;

    slot = amaze_draw_slot_index();
    amaze_draw_retained_slot(&amaze_draw_slots[slot]);
}
