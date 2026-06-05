#include "ana.h"

#include <stdio.h>

#ifdef ANA_TARGET_AMIGA
#include <exec/types.h>
#include <hardware/cia.h>
#include <hardware/custom.h>

extern struct CIA ciaa;
extern struct Custom custom;
#endif

#define PROBE_COLOR_BG 0
#define PROBE_COLOR_TEXT 1
#define PROBE_COLOR_ON 5
#define PROBE_COLOR_OFF 8
#define PROBE_TEXT_SCALE 2
#define PROBE_CHAR_W 8
#define PROBE_CHAR_H 12
#define PROBE_RUNTIME_TICKS 1500

static ANA_InputDebug probe_debug;
static int probe_tick;
static int probe_seen_c;
static int probe_seen_q;
static int probe_seen_escape;
static int probe_seen_space;
static int probe_seen_x;
static int probe_seen_raw;
static int probe_seen_vanilla;
static int probe_seen_backend0;
static int probe_seen_backend1;
static int probe_last_event_count;

static unsigned char probe_glyph_bits(char ch, int row)
{
    switch (ch) {
    case '0': {
        static const unsigned char bits[5] = {7, 5, 5, 5, 7};
        return bits[row];
    }
    case '1': {
        static const unsigned char bits[5] = {2, 6, 2, 2, 7};
        return bits[row];
    }
    case '2': {
        static const unsigned char bits[5] = {7, 1, 7, 4, 7};
        return bits[row];
    }
    case '3': {
        static const unsigned char bits[5] = {7, 1, 7, 1, 7};
        return bits[row];
    }
    case '4': {
        static const unsigned char bits[5] = {5, 5, 7, 1, 1};
        return bits[row];
    }
    case '5': {
        static const unsigned char bits[5] = {7, 4, 7, 1, 7};
        return bits[row];
    }
    case '6': {
        static const unsigned char bits[5] = {7, 4, 7, 5, 7};
        return bits[row];
    }
    case '7': {
        static const unsigned char bits[5] = {7, 1, 1, 2, 2};
        return bits[row];
    }
    case '8': {
        static const unsigned char bits[5] = {7, 5, 7, 5, 7};
        return bits[row];
    }
    case '9': {
        static const unsigned char bits[5] = {7, 5, 7, 1, 7};
        return bits[row];
    }
    case 'A': {
        static const unsigned char bits[5] = {7, 5, 7, 5, 5};
        return bits[row];
    }
    case 'B': {
        static const unsigned char bits[5] = {6, 5, 6, 5, 6};
        return bits[row];
    }
    case 'C': {
        static const unsigned char bits[5] = {7, 4, 4, 4, 7};
        return bits[row];
    }
    case 'D': {
        static const unsigned char bits[5] = {6, 5, 5, 5, 6};
        return bits[row];
    }
    case 'E': {
        static const unsigned char bits[5] = {7, 4, 6, 4, 7};
        return bits[row];
    }
    case 'F': {
        static const unsigned char bits[5] = {7, 4, 6, 4, 4};
        return bits[row];
    }
    case 'G': {
        static const unsigned char bits[5] = {7, 4, 5, 5, 7};
        return bits[row];
    }
    case 'H': {
        static const unsigned char bits[5] = {5, 5, 7, 5, 5};
        return bits[row];
    }
    case 'I': {
        static const unsigned char bits[5] = {7, 2, 2, 2, 7};
        return bits[row];
    }
    case 'J': {
        static const unsigned char bits[5] = {1, 1, 1, 5, 7};
        return bits[row];
    }
    case 'K': {
        static const unsigned char bits[5] = {5, 5, 6, 5, 5};
        return bits[row];
    }
    case 'L': {
        static const unsigned char bits[5] = {4, 4, 4, 4, 7};
        return bits[row];
    }
    case 'M': {
        static const unsigned char bits[5] = {5, 7, 7, 5, 5};
        return bits[row];
    }
    case 'N': {
        static const unsigned char bits[5] = {5, 7, 7, 7, 5};
        return bits[row];
    }
    case 'O': {
        static const unsigned char bits[5] = {7, 5, 5, 5, 7};
        return bits[row];
    }
    case 'P': {
        static const unsigned char bits[5] = {7, 5, 7, 4, 4};
        return bits[row];
    }
    case 'Q': {
        static const unsigned char bits[5] = {7, 5, 5, 7, 1};
        return bits[row];
    }
    case 'R': {
        static const unsigned char bits[5] = {7, 5, 7, 6, 5};
        return bits[row];
    }
    case 'S': {
        static const unsigned char bits[5] = {7, 4, 7, 1, 7};
        return bits[row];
    }
    case 'T': {
        static const unsigned char bits[5] = {7, 2, 2, 2, 2};
        return bits[row];
    }
    case 'U': {
        static const unsigned char bits[5] = {5, 5, 5, 5, 7};
        return bits[row];
    }
    case 'V': {
        static const unsigned char bits[5] = {5, 5, 5, 5, 2};
        return bits[row];
    }
    case 'W': {
        static const unsigned char bits[5] = {5, 5, 7, 7, 5};
        return bits[row];
    }
    case 'X': {
        static const unsigned char bits[5] = {5, 5, 2, 5, 5};
        return bits[row];
    }
    case 'Y': {
        static const unsigned char bits[5] = {5, 5, 2, 2, 2};
        return bits[row];
    }
    case 'Z': {
        static const unsigned char bits[5] = {7, 1, 2, 4, 7};
        return bits[row];
    }
    case '-': {
        static const unsigned char bits[5] = {0, 0, 7, 0, 0};
        return bits[row];
    }
    default:
        return 0;
    }
}

static void probe_draw_char(char ch, int x, int y, unsigned char color)
{
    int row;
    int col;
    unsigned char bits;

    for (row = 0; row < 5; row++) {
        bits = probe_glyph_bits(ch, row);
        for (col = 0; col < 3; col++) {
            if ((bits & (1u << (2 - col))) != 0u) {
                ana_fill_rect(
                    color,
                    x + col * PROBE_TEXT_SCALE,
                    y + row * PROBE_TEXT_SCALE,
                    PROBE_TEXT_SCALE,
                    PROBE_TEXT_SCALE);
            }
        }
    }
}

static void probe_draw_text(const char* text, int x, int y, unsigned char color)
{
    while (*text != '\0') {
        if (*text != ' ') {
            probe_draw_char(*text, x, y, color);
        }
        x += PROBE_CHAR_W;
        text++;
    }
}

static void probe_draw_hex(
    unsigned int value,
    int digits,
    int x,
    int y,
    unsigned char color)
{
    int i;
    int shift;
    int nibble;
    char ch;

    for (i = 0; i < digits; i++) {
        shift = (digits - 1 - i) * 4;
        nibble = (int)((value >> shift) & 0x0fu);
        ch = (char)(nibble < 10 ? '0' + nibble : 'A' + nibble - 10);
        probe_draw_char(ch, x + i * PROBE_CHAR_W, y, color);
    }
}

static void probe_draw_dec(
    int value,
    int x,
    int y,
    unsigned char color)
{
    char digits[8];
    int count;
    int i;
    int v;

    if (value < 0) {
        value = 0;
    }

    v = value;
    count = 0;
    do {
        digits[count] = (char)('0' + (v % 10));
        count++;
        v /= 10;
    } while (v > 0 && count < (int)sizeof(digits));

    for (i = count - 1; i >= 0; i--) {
        probe_draw_char(digits[i], x, y, color);
        x += PROBE_CHAR_W;
    }
}

static void probe_draw_bool(
    const char* label,
    int is_on,
    int x,
    int y)
{
    probe_draw_text(label, x, y, PROBE_COLOR_TEXT);
    probe_draw_char(
        is_on ? '1' : '0',
        x + 24,
        y,
        is_on ? PROBE_COLOR_ON : PROBE_COLOR_OFF);
}

static void probe_init(void)
{
    ANA_Color palette[ANA_DEFAULT_COLORS];
    int i;

    for (i = 0; i < ANA_DEFAULT_COLORS; i++) {
        palette[i].r = 0;
        palette[i].g = 0;
        palette[i].b = 0;
    }

    palette[PROBE_COLOR_TEXT].r = 255;
    palette[PROBE_COLOR_TEXT].g = 255;
    palette[PROBE_COLOR_TEXT].b = 255;
    palette[PROBE_COLOR_ON].r = 255;
    palette[PROBE_COLOR_ON].g = 220;
    palette[PROBE_COLOR_ON].b = 0;
    palette[PROBE_COLOR_OFF].r = 90;
    palette[PROBE_COLOR_OFF].g = 90;
    palette[PROBE_COLOR_OFF].b = 120;
    palette[10].r = 0;
    palette[10].g = 160;
    palette[10].b = 255;

    ana_set_palette(palette, ANA_DEFAULT_COLORS);

    ana_input_clear_key_map();
    ana_input_map_default_keys(ANA_INPUT_DEVICE_0);
    ana_input_unmap_key_from_quit(ANA_KEY_ESCAPE);
    ana_input_map_key_to_action(ANA_KEY_X, ANA_INPUT_DEVICE_0, ANA_ACTION_2);
    ana_input_map_key_to_action(ANA_KEY_Q, ANA_INPUT_DEVICE_0, ANA_ACTION_3);
    ana_input_map_key_to_action(ANA_KEY_C, ANA_INPUT_DEVICE_0, ANA_ACTION_4);

    probe_seen_c = 0;
    probe_seen_q = 0;
    probe_seen_escape = 0;
    probe_seen_space = 0;
    probe_seen_x = 0;
    probe_seen_raw = 0;
    probe_seen_vanilla = 0;
    probe_seen_backend0 = 0;
    probe_seen_backend1 = 0;
    probe_last_event_count = 0;
}

static void probe_update(ANA_Time time)
{
    probe_tick = time.tick;
    ana_input_debug_snapshot(&probe_debug);

    if (probe_debug.raw_count > 0) {
        probe_seen_raw = 1;
    }
    if (probe_debug.vanilla_count > 0) {
        probe_seen_vanilla = 1;
    }
    if (probe_debug.backend_state[0] != 0u) {
        probe_seen_backend0 = 1;
    }
    if (probe_debug.backend_state[1] != 0u) {
        probe_seen_backend1 = 1;
    }

    if (probe_debug.event_count != probe_last_event_count) {
        probe_last_event_count = probe_debug.event_count;
        if (probe_debug.last_key == ANA_KEY_C) {
            probe_seen_c = 1;
        } else if (probe_debug.last_key == ANA_KEY_Q) {
            probe_seen_q = 1;
        } else if (probe_debug.last_key == ANA_KEY_ESCAPE) {
            probe_seen_escape = 1;
        } else if (probe_debug.last_key == ANA_KEY_SPACE) {
            probe_seen_space = 1;
        } else if (probe_debug.last_key == ANA_KEY_X) {
            probe_seen_x = 1;
        }
    }

    if (ana_input_action(ANA_INPUT_DEVICE_0, ANA_ACTION_1)) {
        probe_seen_space = 1;
    }
    if (ana_input_action(ANA_INPUT_DEVICE_0, ANA_ACTION_2)) {
        probe_seen_x = 1;
    }
    if (ana_input_action(ANA_INPUT_DEVICE_0, ANA_ACTION_3)) {
        probe_seen_q = 1;
    }
    if (ana_input_action(ANA_INPUT_DEVICE_0, ANA_ACTION_4)) {
        probe_seen_c = 1;
    }

#ifdef ANA_TARGET_AMIGA
    if (time.tick >= PROBE_RUNTIME_TICKS) {
        ana_quit();
    }
#else
    if (time.tick >= 3) {
        ana_quit();
    }
#endif
}

static void probe_draw_input_state(int y)
{
    probe_draw_text("S0", 8, y, PROBE_COLOR_TEXT);
    probe_draw_hex(probe_debug.current_state[0], 2, 32, y, 10);
    probe_draw_text("B0", 72, y, PROBE_COLOR_TEXT);
    probe_draw_hex(probe_debug.backend_state[0], 2, 96, y, 10);
    probe_draw_text("S1", 136, y, PROBE_COLOR_TEXT);
    probe_draw_hex(probe_debug.current_state[1], 2, 160, y, 10);
    probe_draw_text("B1", 200, y, PROBE_COLOR_TEXT);
    probe_draw_hex(probe_debug.backend_state[1], 2, 224, y, 10);
}

static void probe_draw_key_state(int y)
{
    probe_draw_text("DOWN", 8, y, PROBE_COLOR_TEXT);
    probe_draw_bool("C", probe_debug.key_c_down, 48, y);
    probe_draw_bool("Q", probe_debug.key_q_down, 88, y);
    probe_draw_bool("E", probe_debug.key_escape_down, 128, y);
    probe_draw_bool("SP", probe_debug.key_space_down, 168, y);
    probe_draw_bool("X", probe_debug.key_x_down, 224, y);
}

static void probe_draw_seen_state(int y)
{
    probe_draw_text("SEEN", 8, y, PROBE_COLOR_TEXT);
    probe_draw_bool("C", probe_seen_c, 48, y);
    probe_draw_bool("Q", probe_seen_q, 88, y);
    probe_draw_bool("E", probe_seen_escape, 128, y);
    probe_draw_bool("SP", probe_seen_space, 168, y);
    probe_draw_bool("X", probe_seen_x, 224, y);
}

static void probe_draw_seen_sources(int y)
{
    probe_draw_text("SRC", 8, y, PROBE_COLOR_TEXT);
    probe_draw_bool("R", probe_seen_raw, 48, y);
    probe_draw_bool("V", probe_seen_vanilla, 88, y);
    probe_draw_bool("B0", probe_seen_backend0, 128, y);
    probe_draw_bool("B1", probe_seen_backend1, 184, y);
}

static void probe_draw_event_state(int y)
{
    probe_draw_text("EV", 8, y, PROBE_COLOR_TEXT);
    probe_draw_dec(probe_debug.event_count, 32, y, 10);
    probe_draw_text("R", 96, y, PROBE_COLOR_TEXT);
    probe_draw_dec(probe_debug.raw_count, 112, y, 10);
    probe_draw_text("V", 160, y, PROBE_COLOR_TEXT);
    probe_draw_dec(probe_debug.vanilla_count, 176, y, 10);
    probe_draw_text("M", 224, y, PROBE_COLOR_TEXT);
    probe_draw_dec(probe_debug.matrix_count, 240, y, 10);

    y += PROBE_CHAR_H;
    probe_draw_text("CL", 8, y, PROBE_COLOR_TEXT);
    probe_draw_hex((unsigned int)probe_debug.last_class, 4, 32, y, 10);
    probe_draw_text("CO", 96, y, PROBE_COLOR_TEXT);
    probe_draw_hex((unsigned int)probe_debug.last_code, 2, 120, y, 10);
    probe_draw_text("K", 160, y, PROBE_COLOR_TEXT);
    probe_draw_hex((unsigned int)probe_debug.last_key, 2, 176, y, 10);
    probe_draw_text("DN", 216, y, PROBE_COLOR_TEXT);
    probe_draw_char(
        probe_debug.last_is_down ? '1' : '0',
        240,
        y,
        probe_debug.last_is_down ? PROBE_COLOR_ON : PROBE_COLOR_OFF);
    probe_draw_text("MR", 264, y, PROBE_COLOR_TEXT);
    probe_draw_char(
        probe_debug.matrix_ready ? '1' : '0',
        288,
        y,
        probe_debug.matrix_ready ? PROBE_COLOR_ON : PROBE_COLOR_OFF);
}

static void probe_draw_registers(int y)
{
#ifdef ANA_TARGET_AMIGA
    probe_draw_text("PRA", 8, y, PROBE_COLOR_TEXT);
    probe_draw_hex((unsigned int)ciaa.ciapra, 2, 40, y, 10);
    probe_draw_text("J0", 80, y, PROBE_COLOR_TEXT);
    probe_draw_hex((unsigned int)custom.joy0dat, 4, 104, y, 10);
    probe_draw_text("J1", 168, y, PROBE_COLOR_TEXT);
    probe_draw_hex((unsigned int)custom.joy1dat, 4, 192, y, 10);

    y += PROBE_CHAR_H;
    probe_draw_text("POT", 8, y, PROBE_COLOR_TEXT);
    probe_draw_hex((unsigned int)custom.potinp, 4, 40, y, 10);
#else
    probe_draw_text("HOST", 8, y, PROBE_COLOR_TEXT);
#endif
}

static void probe_draw(void)
{
    ana_clear(PROBE_COLOR_BG);
    probe_draw_text("ANA INPUT PROBE", 8, 8, PROBE_COLOR_TEXT);
    probe_draw_text("ARROWS SPACE X Q C ESC", 8, 24, PROBE_COLOR_TEXT);
    probe_draw_text("AUTO QUIT", 8, 40, PROBE_COLOR_OFF);
    probe_draw_dec(PROBE_RUNTIME_TICKS - probe_tick, 88, 40, PROBE_COLOR_OFF);

    probe_draw_event_state(60);
    probe_draw_input_state(88);
    probe_draw_key_state(112);
    probe_draw_seen_state(136);
    probe_draw_seen_sources(160);
    probe_draw_registers(176);
}

int main(void)
{
    ANA_Game game = {0};
    ANA_Result result;

    game.init = probe_init;
    game.update = probe_update;
    game.draw = probe_draw;
    game.width = ANA_DEFAULT_WIDTH;
    game.height = ANA_DEFAULT_HEIGHT;
    game.fps = ANA_DEFAULT_FPS;
    game.colors = ANA_DEFAULT_COLORS;
    game.screen_mode = ANA_SCREEN_PAL_LORES;
    game.render_mode = ANA_RENDER_DIRTY;
    game.debug_stats = 1;
    game.warmup_frames = 1;

    printf("ANA input probe started.\n");
    printf("Shows keyboard events, ANA states, and Amiga joystick registers.\n");

    result = ana_run(&game);

    printf("ANA input probe finished with %s.\n", ana_result_name(result));
    printf("Type input_probe to run it again.\n");

    return result == ANA_OK ? 0 : 1;
}
