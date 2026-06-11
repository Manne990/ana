#include "ana.h"

#ifdef PROBE_SYNTHETIC_INPUT
#include "ana_internal.h"
#endif

#include <stdio.h>

#ifdef ANA_TARGET_AMIGA
#include <dos/dos.h>
#include <exec/types.h>
#include <hardware/cia.h>
#include <hardware/custom.h>
#include <proto/dos.h>
#include <string.h>

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
#ifndef PROBE_RUNTIME_TICKS
#ifdef ANA_TARGET_AMIGA
#define PROBE_RUNTIME_TICKS 1500
#else
#define PROBE_RUNTIME_TICKS 3
#endif
#endif

#define PROBE_RESULT_FILE "ana_input_probe_result.txt"

static ANA_InputDebug probe_debug;
static int probe_tick;
static int probe_update_count;
static int probe_seen_c;
static int probe_seen_q;
static int probe_seen_escape;
static int probe_seen_space;
static int probe_seen_x;
static int probe_seen_left;
static int probe_seen_right;
static int probe_seen_up;
static int probe_seen_down;
static int probe_seen_raw;
static int probe_seen_vanilla;
static int probe_seen_backend0;
static int probe_seen_backend1;
static int probe_last_event_count;
static int probe_wrote_final;
static int probe_synthetic_ok;
static int probe_synthetic_step;

#ifdef ANA_TARGET_AMIGA
static void probe_write_text(BPTR file, const char* text)
{
    Write(file, (APTR)text, (LONG)strlen(text));
}

static void probe_write_unsigned_value(BPTR file, unsigned int value)
{
    char digits[12];
    int count;
    int i;

    count = 0;
    do {
        digits[count] = (char)('0' + (value % 10u));
        count++;
        value /= 10u;
    } while (value > 0u && count < (int)sizeof(digits));

    for (i = count - 1; i >= 0; i--) {
        Write(file, (APTR)&digits[i], 1);
    }
}

static void probe_write_string_line(BPTR file, const char* label, const char* value)
{
    probe_write_text(file, label);
    probe_write_text(file, "=");
    probe_write_text(file, value);
    probe_write_text(file, "\n");
}

static void probe_write_int_line(BPTR file, const char* label, int value)
{
    probe_write_text(file, label);
    probe_write_text(file, "=");
    if (value < 0) {
        probe_write_text(file, "-");
        probe_write_unsigned_value(file, (unsigned int)(0 - value));
    } else {
        probe_write_unsigned_value(file, (unsigned int)value);
    }
    probe_write_text(file, "\n");
}

static void probe_write_uint_line(BPTR file, const char* label, unsigned int value)
{
    probe_write_text(file, label);
    probe_write_text(file, "=");
    probe_write_unsigned_value(file, value);
    probe_write_text(file, "\n");
}
#endif

static void probe_write_result(const char* phase, const char* result_name)
{
#ifdef ANA_TARGET_AMIGA
    static const char* paths[] = {
        "DH0:" PROBE_RESULT_FILE,
        "a1200:" PROBE_RESULT_FILE,
        "a500:" PROBE_RESULT_FILE,
        "RAM:" PROBE_RESULT_FILE,
        "PROGDIR:" PROBE_RESULT_FILE,
        NULL};
    BPTR file;
    int i;

    file = (BPTR)0;
    for (i = 0; paths[i] != NULL; i++) {
        file = Open((STRPTR)paths[i], MODE_NEWFILE);
        if (file != (BPTR)0) {
            break;
        }
    }
    if (file == (BPTR)0) {
        printf("ANA input probe could not write result file.\n");
        return;
    }

    probe_write_int_line(file, "ana_input_probe_result", 1);
    probe_write_string_line(file, "phase", phase);
    probe_write_string_line(file, "result", result_name);
    probe_write_int_line(file, "tick", probe_tick);
    probe_write_int_line(file, "update_count", probe_update_count);
    probe_write_int_line(file, "runtime_ticks", PROBE_RUNTIME_TICKS);
    probe_write_int_line(file, "event_count", probe_debug.event_count);
    probe_write_int_line(file, "raw_count", probe_debug.raw_count);
    probe_write_int_line(file, "vanilla_count", probe_debug.vanilla_count);
    probe_write_int_line(file, "matrix_count", probe_debug.matrix_count);
    probe_write_int_line(file, "matrix_ready", probe_debug.matrix_ready);
    probe_write_int_line(file, "last_class", probe_debug.last_class);
    probe_write_int_line(file, "last_code", probe_debug.last_code);
    probe_write_int_line(file, "last_key", probe_debug.last_key);
    probe_write_int_line(file, "last_is_down", probe_debug.last_is_down);
    probe_write_uint_line(file, "seen_key_bits", probe_debug.seen_key_bits);
    probe_write_uint_line(file, "current0", probe_debug.current_state[0]);
    probe_write_uint_line(file, "backend0", probe_debug.backend_state[0]);
    probe_write_uint_line(file, "current1", probe_debug.current_state[1]);
    probe_write_uint_line(file, "backend1", probe_debug.backend_state[1]);
    probe_write_int_line(file, "key_c_down", probe_debug.key_c_down);
    probe_write_int_line(file, "key_q_down", probe_debug.key_q_down);
    probe_write_int_line(file, "key_escape_down", probe_debug.key_escape_down);
    probe_write_int_line(file, "key_space_down", probe_debug.key_space_down);
    probe_write_int_line(file, "key_x_down", probe_debug.key_x_down);
    probe_write_int_line(file, "quit_requested", probe_debug.quit_requested);
    probe_write_int_line(file, "seen_c", probe_seen_c);
    probe_write_int_line(file, "seen_q", probe_seen_q);
    probe_write_int_line(file, "seen_escape", probe_seen_escape);
    probe_write_int_line(file, "seen_space", probe_seen_space);
    probe_write_int_line(file, "seen_x", probe_seen_x);
    probe_write_int_line(file, "seen_left", probe_seen_left);
    probe_write_int_line(file, "seen_right", probe_seen_right);
    probe_write_int_line(file, "seen_up", probe_seen_up);
    probe_write_int_line(file, "seen_down", probe_seen_down);
    probe_write_int_line(file, "seen_raw", probe_seen_raw);
    probe_write_int_line(file, "seen_vanilla", probe_seen_vanilla);
    probe_write_int_line(file, "seen_backend0", probe_seen_backend0);
    probe_write_int_line(file, "seen_backend1", probe_seen_backend1);
    probe_write_int_line(file, "synthetic_input",
#ifdef PROBE_SYNTHETIC_INPUT
        1
#else
        0
#endif
    );
    probe_write_int_line(file, "synthetic_ok", probe_synthetic_ok);
    probe_write_int_line(file, "write_complete", 1);

    Close(file);
    printf("ANA input probe wrote result to %s.\n", paths[i]);
#else
    (void)phase;
    (void)result_name;
#endif
}

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
    probe_seen_left = 0;
    probe_seen_right = 0;
    probe_seen_up = 0;
    probe_seen_down = 0;
    probe_seen_raw = 0;
    probe_seen_vanilla = 0;
    probe_seen_backend0 = 0;
    probe_seen_backend1 = 0;
    probe_last_event_count = 0;
    probe_wrote_final = 0;
    probe_synthetic_ok = 1;
    probe_synthetic_step = 0;
    probe_update_count = 0;
}

#ifdef PROBE_SYNTHETIC_INPUT
static void probe_expect_down(
    ANA_InputDevice device,
    ANA_InputAction action,
    ANA_Key key)
{
    ANA_InputDebug debug;

    ana_input_debug_snapshot(&debug);
    if (!ana_input_action(device, action)) {
        probe_synthetic_ok = 0;
    }
    if (key == ANA_KEY_C && !debug.key_c_down) {
        probe_synthetic_ok = 0;
    }
    if (key == ANA_KEY_Q && !debug.key_q_down) {
        probe_synthetic_ok = 0;
    }
    if (key == ANA_KEY_ESCAPE && !debug.key_escape_down) {
        probe_synthetic_ok = 0;
    }
    if (key == ANA_KEY_SPACE && !debug.key_space_down) {
        probe_synthetic_ok = 0;
    }
    if (key == ANA_KEY_X && !debug.key_x_down) {
        probe_synthetic_ok = 0;
    }
}

static void probe_expect_clear(
    ANA_InputDevice device,
    ANA_InputAction action,
    ANA_Key key)
{
    ANA_InputDebug debug;

    ana_input_debug_snapshot(&debug);
    if (ana_input_action(device, action)) {
        probe_synthetic_ok = 0;
    }
    if (key == ANA_KEY_C && debug.key_c_down) {
        probe_synthetic_ok = 0;
    }
    if (key == ANA_KEY_Q && debug.key_q_down) {
        probe_synthetic_ok = 0;
    }
    if (key == ANA_KEY_ESCAPE && debug.key_escape_down) {
        probe_synthetic_ok = 0;
    }
    if (key == ANA_KEY_SPACE && debug.key_space_down) {
        probe_synthetic_ok = 0;
    }
    if (key == ANA_KEY_X && debug.key_x_down) {
        probe_synthetic_ok = 0;
    }
}

static void probe_synthetic_update(void)
{
    switch (probe_update_count) {
    case 2:
        if (ana_input_key_from_amiga_raw_code(0x40) != ANA_KEY_SPACE) {
            probe_synthetic_ok = 0;
        }
        ana_input_handle_amiga_raw_key_event(0x40, 1);
        probe_synthetic_step++;
        break;
    case 3:
        probe_expect_down(
            ANA_INPUT_DEVICE_0,
            ANA_ACTION_1,
            ANA_KEY_SPACE);
        probe_synthetic_step++;
        break;
    case 4:
        probe_expect_down(
            ANA_INPUT_DEVICE_0,
            ANA_ACTION_1,
            ANA_KEY_SPACE);
        ana_input_handle_amiga_raw_key_event(0x40, 0);
        probe_synthetic_step++;
        break;
    case 5:
        probe_expect_clear(
            ANA_INPUT_DEVICE_0,
            ANA_ACTION_1,
            ANA_KEY_UNKNOWN);
        if (ana_input_key_from_amiga_raw_code(0x32) != ANA_KEY_X) {
            probe_synthetic_ok = 0;
        }
        ana_input_handle_amiga_raw_key_event(0x32, 1);
        probe_synthetic_step++;
        break;
    case 6:
        probe_expect_down(
            ANA_INPUT_DEVICE_0,
            ANA_ACTION_2,
            ANA_KEY_X);
        probe_synthetic_step++;
        break;
    case 7:
        probe_expect_down(
            ANA_INPUT_DEVICE_0,
            ANA_ACTION_2,
            ANA_KEY_X);
        ana_input_handle_amiga_raw_key_event(0x32, 0);
        probe_synthetic_step++;
        break;
    case 8:
        probe_expect_clear(
            ANA_INPUT_DEVICE_0,
            ANA_ACTION_2,
            ANA_KEY_UNKNOWN);
        if (ana_input_key_from_amiga_raw_code(0x33) != ANA_KEY_C) {
            probe_synthetic_ok = 0;
        }
        ana_input_handle_amiga_raw_key_event(0x33, 1);
        probe_synthetic_step++;
        break;
    case 9:
        probe_expect_down(
            ANA_INPUT_DEVICE_0,
            ANA_ACTION_4,
            ANA_KEY_C);
        probe_synthetic_step++;
        break;
    case 10:
        probe_expect_down(
            ANA_INPUT_DEVICE_0,
            ANA_ACTION_4,
            ANA_KEY_C);
        ana_input_handle_amiga_raw_key_event(0x33, 0);
        probe_synthetic_step++;
        break;
    case 11:
        probe_expect_clear(
            ANA_INPUT_DEVICE_0,
            ANA_ACTION_4,
            ANA_KEY_UNKNOWN);
        if (ana_input_key_from_amiga_raw_code(0x10) != ANA_KEY_Q) {
            probe_synthetic_ok = 0;
        }
        ana_input_handle_amiga_raw_key_event(0x10, 1);
        probe_synthetic_step++;
        break;
    case 12:
        probe_expect_down(
            ANA_INPUT_DEVICE_0,
            ANA_ACTION_3,
            ANA_KEY_Q);
        probe_synthetic_step++;
        break;
    case 13:
        probe_expect_down(
            ANA_INPUT_DEVICE_0,
            ANA_ACTION_3,
            ANA_KEY_Q);
        ana_input_handle_amiga_raw_key_event(0x10, 0);
        probe_synthetic_step++;
        break;
    case 14:
        if (ana_input_key_from_amiga_raw_code(0x45) != ANA_KEY_ESCAPE) {
            probe_synthetic_ok = 0;
        }
        if (ana_input_key_from_amiga_vanilla_code(0x1b) !=
                ANA_KEY_ESCAPE) {
            probe_synthetic_ok = 0;
        }
        if (ana_input_key_from_amiga_vanilla_code('q') != ANA_KEY_Q) {
            probe_synthetic_ok = 0;
        }
        if (ana_input_key_from_amiga_vanilla_code('C') != ANA_KEY_C) {
            probe_synthetic_ok = 0;
        }
        if (ana_input_key_from_amiga_vanilla_code('X') != ANA_KEY_X) {
            probe_synthetic_ok = 0;
        }
        if (ana_input_key_from_amiga_vanilla_code(' ') != ANA_KEY_SPACE) {
            probe_synthetic_ok = 0;
        }
        if (ana_input_handle_amiga_vanilla_key_event('X') != ANA_KEY_X) {
            probe_synthetic_ok = 0;
        }
        probe_synthetic_step++;
        break;
    case 15:
        probe_expect_down(
            ANA_INPUT_DEVICE_0,
            ANA_ACTION_2,
            ANA_KEY_UNKNOWN);
        probe_synthetic_step++;
        break;
    case 16:
        probe_expect_down(
            ANA_INPUT_DEVICE_0,
            ANA_ACTION_2,
            ANA_KEY_UNKNOWN);
        probe_synthetic_step++;
        break;
    case 17:
        probe_expect_clear(
            ANA_INPUT_DEVICE_0,
            ANA_ACTION_2,
            ANA_KEY_UNKNOWN);
        if (ana_input_key_from_amiga_raw_code(0x4f) != ANA_KEY_LEFT) {
            probe_synthetic_ok = 0;
        }
        if (ana_input_key_from_amiga_raw_code(0x4e) != ANA_KEY_RIGHT) {
            probe_synthetic_ok = 0;
        }
        if (ana_input_key_from_amiga_raw_code(0x4c) != ANA_KEY_UP) {
            probe_synthetic_ok = 0;
        }
        if (ana_input_key_from_amiga_raw_code(0x4d) != ANA_KEY_DOWN) {
            probe_synthetic_ok = 0;
        }
        probe_synthetic_step++;
        break;
    default:
        break;
    }
}
#endif

static void probe_update(ANA_Time time)
{
    probe_tick = time.tick;
    probe_update_count++;

#ifdef PROBE_SYNTHETIC_INPUT
    probe_synthetic_update();
#endif

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
        } else if (probe_debug.last_key == ANA_KEY_LEFT ||
                probe_debug.last_key == ANA_KEY_A) {
            probe_seen_left = 1;
        } else if (probe_debug.last_key == ANA_KEY_RIGHT ||
                probe_debug.last_key == ANA_KEY_D) {
            probe_seen_right = 1;
        } else if (probe_debug.last_key == ANA_KEY_UP ||
                probe_debug.last_key == ANA_KEY_W) {
            probe_seen_up = 1;
        } else if (probe_debug.last_key == ANA_KEY_DOWN ||
                probe_debug.last_key == ANA_KEY_S) {
            probe_seen_down = 1;
        }
    }

    if ((probe_debug.seen_key_bits & (1u << ANA_KEY_C)) != 0u) {
        probe_seen_c = 1;
    }
    if ((probe_debug.seen_key_bits & (1u << ANA_KEY_Q)) != 0u) {
        probe_seen_q = 1;
    }
    if ((probe_debug.seen_key_bits & (1u << ANA_KEY_ESCAPE)) != 0u) {
        probe_seen_escape = 1;
    }
    if ((probe_debug.seen_key_bits & (1u << ANA_KEY_SPACE)) != 0u) {
        probe_seen_space = 1;
    }
    if ((probe_debug.seen_key_bits & (1u << ANA_KEY_X)) != 0u) {
        probe_seen_x = 1;
    }
    if ((probe_debug.seen_key_bits &
            ((1u << ANA_KEY_LEFT) | (1u << ANA_KEY_A))) != 0u) {
        probe_seen_left = 1;
    }
    if ((probe_debug.seen_key_bits &
            ((1u << ANA_KEY_RIGHT) | (1u << ANA_KEY_D))) != 0u) {
        probe_seen_right = 1;
    }
    if ((probe_debug.seen_key_bits &
            ((1u << ANA_KEY_UP) | (1u << ANA_KEY_W))) != 0u) {
        probe_seen_up = 1;
    }
    if ((probe_debug.seen_key_bits &
            ((1u << ANA_KEY_DOWN) | (1u << ANA_KEY_S))) != 0u) {
        probe_seen_down = 1;
    }

    if (ana_input_direction(ANA_INPUT_DEVICE_0, ANA_INPUT_LEFT)) {
        probe_seen_left = 1;
    }
    if (ana_input_direction(ANA_INPUT_DEVICE_0, ANA_INPUT_RIGHT)) {
        probe_seen_right = 1;
    }
    if (ana_input_direction(ANA_INPUT_DEVICE_0, ANA_INPUT_UP)) {
        probe_seen_up = 1;
    }
    if (ana_input_direction(ANA_INPUT_DEVICE_0, ANA_INPUT_DOWN)) {
        probe_seen_down = 1;
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

    if (probe_update_count >= PROBE_RUNTIME_TICKS) {
        if (probe_synthetic_step < 16) {
            probe_synthetic_ok = 0;
        }
        probe_write_result("pre_quit", "ANA_OK");
        probe_wrote_final = 1;
        ana_quit();
    }
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

    y += PROBE_CHAR_H;
    probe_draw_text("DIR", 8, y, PROBE_COLOR_TEXT);
    probe_draw_bool("L", probe_seen_left, 48, y);
    probe_draw_bool("R", probe_seen_right, 88, y);
    probe_draw_bool("U", probe_seen_up, 128, y);
    probe_draw_bool("D", probe_seen_down, 168, y);
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
    probe_draw_seen_sources(172);
    probe_draw_registers(188);
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
    probe_write_result("main_start", "START");

    result = ana_run(&game);
    if (!probe_wrote_final) {
        probe_write_result("post_run", ana_result_name(result));
    }

    printf("ANA input probe finished with %s.\n", ana_result_name(result));
    printf("Type input_probe to run it again.\n");

    return result == ANA_OK ? 0 : 1;
}
