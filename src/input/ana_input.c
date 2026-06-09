#include "ana/ana_input.h"

#include "ana_internal.h"

#include <stddef.h>

#ifdef ANA_TARGET_AMIGA
#include <clib/alib_protos.h>
#include <devices/keyboard.h>
#include <exec/io.h>
#include <exec/ports.h>
#include <exec/types.h>
#include <hardware/cia.h>
#include <hardware/custom.h>
#include <intuition/intuition.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#endif

#ifdef ANA_TARGET_AMIGA
#define ANA_AMIGA_RAWKEY_RELEASE 0x80
#ifndef ANA_AMIGA_KEYBOARD_MATRIX_POLL_INTERVAL
#define ANA_AMIGA_KEYBOARD_MATRIX_POLL_INTERVAL 1
#endif
#define ANA_AMIGA_KEY_MATRIX_BYTES 16L
#define ANA_AMIGA_KEY_MATRIX_RAW_CODES 0x80
#define ANA_AMIGA_CIAA_FIRE0 0x40u
#define ANA_AMIGA_CIAA_FIRE1 0x80u
#define ANA_AMIGA_POTGO_DATLX 0x0100u
#define ANA_AMIGA_POTGO_OUTLX 0x0200u
#define ANA_AMIGA_POTGO_DATLY 0x0400u
#define ANA_AMIGA_POTGO_OUTLY 0x0800u
#define ANA_AMIGA_POTGO_DATRX 0x1000u
#define ANA_AMIGA_POTGO_OUTRX 0x2000u
#define ANA_AMIGA_POTGO_DATRY 0x4000u
#define ANA_AMIGA_POTGO_OUTRY 0x8000u
#define ANA_AMIGA_POTGO_BUTTONS_0 \
    (ANA_AMIGA_POTGO_DATLX | ANA_AMIGA_POTGO_OUTLX | \
        ANA_AMIGA_POTGO_DATLY | ANA_AMIGA_POTGO_OUTLY)
#define ANA_AMIGA_POTGO_BUTTONS_1 \
    (ANA_AMIGA_POTGO_DATRX | ANA_AMIGA_POTGO_OUTRX | \
        ANA_AMIGA_POTGO_DATRY | ANA_AMIGA_POTGO_OUTRY)
#define ANA_AMIGA_POTINP_FIRE2_0 ANA_AMIGA_POTGO_DATLY
#define ANA_AMIGA_POTINP_FIRE2_1 ANA_AMIGA_POTGO_DATRY
#define ANA_AMIGA_POTINP_FIRE3_0 ANA_AMIGA_POTGO_DATLX
#define ANA_AMIGA_POTINP_FIRE3_1 ANA_AMIGA_POTGO_DATRX

extern struct Custom custom;
extern struct CIA ciaa;

typedef struct ANA_AmigaRawKeyMapEntry {
    unsigned char raw_code;
    ANA_Key key;
} ANA_AmigaRawKeyMapEntry;

static const ANA_AmigaRawKeyMapEntry ana_amiga_raw_key_map[] = {
    {0x4f, ANA_KEY_LEFT},
    {0x4e, ANA_KEY_RIGHT},
    {0x4c, ANA_KEY_UP},
    {0x4d, ANA_KEY_DOWN},
    {0x40, ANA_KEY_SPACE},
    {0x44, ANA_KEY_RETURN},
    {0x45, ANA_KEY_ESCAPE},
    {0x10, ANA_KEY_Q},
    {0x20, ANA_KEY_A},
    {0x22, ANA_KEY_D},
    {0x11, ANA_KEY_W},
    {0x21, ANA_KEY_S},
    {0x31, ANA_KEY_Z},
    {0x32, ANA_KEY_X},
    {0x33, ANA_KEY_C},
    {0x34, ANA_KEY_V},
    {0x63, ANA_KEY_CTRL}
};
#endif

static unsigned int ana_current_input[ANA_INPUT_DEVICES];
static unsigned int ana_previous_input[ANA_INPUT_DEVICES];
static unsigned int ana_pending_input[ANA_INPUT_DEVICES];
static unsigned int ana_backend_input[ANA_INPUT_DEVICES];
static unsigned int ana_event_input[ANA_INPUT_DEVICES];
static int ana_pending_key_state[ANA_KEY_COUNT];
static unsigned int ana_key_direction_map[ANA_KEY_COUNT][ANA_INPUT_DEVICES];
static unsigned int ana_key_action_map[ANA_KEY_COUNT][ANA_INPUT_DEVICES];
static int ana_key_quit_map[ANA_KEY_COUNT];
static unsigned int ana_action_quit_map[ANA_INPUT_DEVICES];
static int ana_current_quit_requested = 0;
static int ana_pending_quit_requested = 0;
static int ana_event_quit_requested = 0;
static int ana_debug_event_count = 0;
static int ana_debug_raw_count = 0;
static int ana_debug_vanilla_count = 0;
static int ana_debug_matrix_count = 0;
static int ana_debug_matrix_ready = 0;
static int ana_debug_last_class = 0;
static int ana_debug_last_code = 0;
static int ana_debug_last_key = 0;
static int ana_debug_last_is_down = 0;
#ifdef ANA_TARGET_AMIGA
static struct MsgPort* ana_keyboard_port = NULL;
static struct IOStdReq* ana_keyboard_io = NULL;
static int ana_keyboard_open = 0;
static int ana_keyboard_failed = 0;
static int ana_keyboard_matrix_poll_countdown = 0;
static unsigned char ana_keyboard_matrix[ANA_AMIGA_KEY_MATRIX_BYTES];
static int ana_keyboard_matrix_key_state[ANA_KEY_COUNT];
static int ana_keyboard_raw_key_state[ANA_KEY_COUNT];
#endif

static unsigned int ana_input_direction_mask(ANA_InputDirection direction)
{
    switch (direction) {
    case ANA_INPUT_LEFT:
        return ANA_INPUT_LEFT_MASK;
    case ANA_INPUT_RIGHT:
        return ANA_INPUT_RIGHT_MASK;
    case ANA_INPUT_UP:
        return ANA_INPUT_UP_MASK;
    case ANA_INPUT_DOWN:
        return ANA_INPUT_DOWN_MASK;
    default:
        return 0u;
    }
}

static unsigned int ana_input_action_mask(ANA_InputAction action)
{
    switch (action) {
    case ANA_ACTION_1:
        return ANA_ACTION_1_MASK;
    case ANA_ACTION_2:
        return ANA_ACTION_2_MASK;
    case ANA_ACTION_3:
        return ANA_ACTION_3_MASK;
    case ANA_ACTION_4:
        return ANA_ACTION_4_MASK;
    default:
        return 0u;
    }
}

static int ana_valid_device(ANA_InputDevice device)
{
    return device >= 0 && device < ANA_INPUT_DEVICES;
}

static int ana_valid_key(ANA_Key key)
{
    return key > ANA_KEY_UNKNOWN && key < ANA_KEY_COUNT;
}

static int ana_input_mask_down(ANA_InputDevice device, unsigned int mask)
{
    if (!ana_valid_device(device) || mask == 0u) {
        return 0;
    }

    return (ana_current_input[device] & mask) != 0u;
}

static int ana_input_mask_pressed(ANA_InputDevice device, unsigned int mask)
{
    if (!ana_valid_device(device) || mask == 0u) {
        return 0;
    }

    return ((ana_current_input[device] & mask) != 0u) &&
        ((ana_previous_input[device] & mask) == 0u);
}

static int ana_input_mask_released(ANA_InputDevice device, unsigned int mask)
{
    if (!ana_valid_device(device) || mask == 0u) {
        return 0;
    }

    return ((ana_current_input[device] & mask) == 0u) &&
        ((ana_previous_input[device] & mask) != 0u);
}

static unsigned int ana_input_mapped_key_state(ANA_InputDevice device)
{
    ANA_Key key;
    unsigned int state;

    state = 0u;

    for (key = ANA_KEY_UNKNOWN; key < ANA_KEY_COUNT; key++) {
        if (ana_pending_key_state[key]) {
            state |= ana_key_direction_map[key][device];
            state |= ana_key_action_map[key][device];
        }
    }

    return state;
}

static int ana_input_mapped_quit_requested(void)
{
    ANA_Key key;
    ANA_InputDevice device;

    for (key = ANA_KEY_UNKNOWN; key < ANA_KEY_COUNT; key++) {
        if (ana_pending_key_state[key] && ana_key_quit_map[key]) {
            return 1;
        }
    }

    for (device = ANA_INPUT_DEVICE_0;
            device < ANA_INPUT_DEVICES;
            device++) {
        if ((ana_current_input[device] & ana_action_quit_map[device]) != 0u) {
            return 1;
        }
    }

    return 0;
}

unsigned int ana_input_state_from_amiga_joydat(
    unsigned short joydat,
    int fire_down,
    int second_button_down,
    int third_button_down)
{
    unsigned int state;

    state = 0u;

    if ((joydat & 0x0200u) != 0u) {
        state |= ANA_INPUT_LEFT_MASK;
    }

    if ((joydat & 0x0002u) != 0u) {
        state |= ANA_INPUT_RIGHT_MASK;
    }

    if ((((joydat >> 9) ^ (joydat >> 8)) & 0x0001u) != 0u) {
        state |= ANA_INPUT_UP_MASK;
    }

    if ((((joydat >> 1) ^ joydat) & 0x0001u) != 0u) {
        state |= ANA_INPUT_DOWN_MASK;
    }

    if (fire_down) {
        state |= ANA_ACTION_1_MASK;
    }

    if (second_button_down) {
        state |= ANA_ACTION_2_MASK;
    }

    if (third_button_down) {
        state |= ANA_ACTION_3_MASK;
    }

    return state;
}

int ana_input_key_matrix_raw_down(
    const unsigned char* matrix,
    int length,
    int raw_code)
{
    int byte_index;
    int bit_index;

    if (matrix == NULL || length <= 0 ||
            raw_code < 0 || raw_code >= 0x80) {
        return 0;
    }

    byte_index = raw_code >> 3;
    bit_index = raw_code & 7;
    if (byte_index >= length) {
        return 0;
    }

    return (matrix[byte_index] & (unsigned char)(1u << bit_index)) != 0u;
}

#ifdef ANA_TARGET_AMIGA
static void ana_input_pulse_key(ANA_Key key)
{
    ANA_InputDevice device;

    if (!ana_valid_key(key)) {
        return;
    }

    for (device = ANA_INPUT_DEVICE_0;
            device < ANA_INPUT_DEVICES;
            device++) {
        ana_event_input[device] |= ana_key_direction_map[key][device];
        ana_event_input[device] |= ana_key_action_map[key][device];
    }

    if (ana_key_quit_map[key]) {
        ana_event_quit_requested = 1;
    }
}

static void ana_input_debug_record_event(
    int class_id,
    int code,
    ANA_Key key,
    int is_down)
{
    ana_debug_event_count++;
    ana_debug_last_class = class_id;
    ana_debug_last_code = code;
    ana_debug_last_key = (int)key;
    ana_debug_last_is_down = is_down != 0;
}

static ANA_Key ana_amiga_key_from_raw_code(int code)
{
    int i;

    for (i = 0;
            i < (int)(sizeof(ana_amiga_raw_key_map) /
                sizeof(ana_amiga_raw_key_map[0]));
            i++) {
        if ((int)ana_amiga_raw_key_map[i].raw_code == code) {
            return ana_amiga_raw_key_map[i].key;
        }
    }

    return ANA_KEY_UNKNOWN;
}

static ANA_Key ana_amiga_key_from_vanilla_code(int code)
{
    switch (code) {
    case ' ':
        return ANA_KEY_SPACE;
    case '\r':
    case '\n':
        return ANA_KEY_RETURN;
    case 0x1b:
        return ANA_KEY_ESCAPE;
    case 'q':
    case 'Q':
        return ANA_KEY_Q;
    case 'a':
    case 'A':
        return ANA_KEY_A;
    case 'd':
    case 'D':
        return ANA_KEY_D;
    case 'w':
    case 'W':
        return ANA_KEY_W;
    case 's':
    case 'S':
        return ANA_KEY_S;
    case 'z':
    case 'Z':
        return ANA_KEY_Z;
    case 'x':
    case 'X':
        return ANA_KEY_X;
    case 'c':
    case 'C':
        return ANA_KEY_C;
    case 'v':
    case 'V':
        return ANA_KEY_V;
    default:
        return ANA_KEY_UNKNOWN;
    }
}

static int ana_amiga_keyboard_open(void)
{
    if (ana_keyboard_open) {
        return 1;
    }

    if (ana_keyboard_failed) {
        return 0;
    }

    ana_keyboard_port = CreatePort(NULL, 0L);
    if (ana_keyboard_port == NULL) {
        ana_keyboard_failed = 1;
        return 0;
    }

    ana_keyboard_io = (struct IOStdReq*)CreateExtIO(
        ana_keyboard_port,
        (LONG)sizeof(struct IOStdReq));
    if (ana_keyboard_io == NULL) {
        DeletePort(ana_keyboard_port);
        ana_keyboard_port = NULL;
        ana_keyboard_failed = 1;
        return 0;
    }

    if (OpenDevice(
            (CONST_STRPTR)"keyboard.device",
            0L,
            (struct IORequest*)ana_keyboard_io,
            0L) != 0) {
        DeleteExtIO((struct IORequest*)ana_keyboard_io);
        DeletePort(ana_keyboard_port);
        ana_keyboard_io = NULL;
        ana_keyboard_port = NULL;
        ana_keyboard_failed = 1;
        return 0;
    }

    ana_keyboard_open = 1;
    return 1;
}

static int ana_amiga_poll_keyboard_matrix(void)
{
    int length;
    int is_down;
    int i;
    ANA_Key key;
    int raw_code;

#if ANA_AMIGA_KEYBOARD_MATRIX_POLL_INTERVAL <= 0
    ana_debug_matrix_ready = 0;
    return 0;
#else
    if (ana_keyboard_matrix_poll_countdown > 0) {
        ana_keyboard_matrix_poll_countdown--;
        return ana_debug_matrix_ready;
    }
    ana_keyboard_matrix_poll_countdown =
        ANA_AMIGA_KEYBOARD_MATRIX_POLL_INTERVAL - 1;
#endif

    if (!ana_amiga_keyboard_open()) {
        ana_debug_matrix_ready = 0;
        for (key = ANA_KEY_UNKNOWN; key < ANA_KEY_COUNT; key++) {
            ana_keyboard_matrix_key_state[key] = 0;
        }
        return 0;
    }

    ana_keyboard_io->io_Command = KBD_READMATRIX;
    ana_keyboard_io->io_Data = (APTR)ana_keyboard_matrix;
    ana_keyboard_io->io_Length = ANA_AMIGA_KEY_MATRIX_BYTES;
    ana_keyboard_io->io_Actual = 0L;
    if (DoIO((struct IORequest*)ana_keyboard_io) != 0 ||
            ana_keyboard_io->io_Error != 0) {
        ana_debug_matrix_ready = 0;
        for (key = ANA_KEY_UNKNOWN; key < ANA_KEY_COUNT; key++) {
            ana_keyboard_matrix_key_state[key] = 0;
        }
        return 0;
    }

    ana_debug_matrix_count++;
    ana_debug_matrix_ready = 1;
    length = (int)ana_keyboard_io->io_Actual;
    if (length <= 0 || length > (int)ANA_AMIGA_KEY_MATRIX_BYTES) {
        length = (int)ANA_AMIGA_KEY_MATRIX_BYTES;
    }

    for (i = 0;
            i < (int)(sizeof(ana_amiga_raw_key_map) /
                sizeof(ana_amiga_raw_key_map[0]));
            i++) {
        raw_code = (int)ana_amiga_raw_key_map[i].raw_code;
        key = ana_amiga_raw_key_map[i].key;

        is_down = ana_input_key_matrix_raw_down(
            ana_keyboard_matrix,
            length,
            raw_code);
        if (is_down && !ana_keyboard_matrix_key_state[key]) {
            ana_input_pulse_key(key);
        }
        ana_keyboard_matrix_key_state[key] = is_down;
    }

    return 1;
}

static void ana_amiga_sync_keyboard_key_state(void)
{
    ANA_Key key;

    for (key = ANA_KEY_UNKNOWN; key < ANA_KEY_COUNT; key++) {
        ana_pending_key_state[key] =
            ana_keyboard_matrix_key_state[key] ||
            ana_keyboard_raw_key_state[key];
    }
}

static void ana_amiga_poll_joysticks(void)
{
    unsigned char ciaa_port_a;
    unsigned short potinp;

    ciaa_port_a = ciaa.ciapra;
    custom.potgo = ANA_AMIGA_POTGO_BUTTONS_0 | ANA_AMIGA_POTGO_BUTTONS_1;
    potinp = custom.potinp;
    ana_backend_input[ANA_INPUT_DEVICE_0] =
        ana_input_state_from_amiga_joydat(
            custom.joy1dat,
            (ciaa_port_a & ANA_AMIGA_CIAA_FIRE1) == 0u,
            (potinp & ANA_AMIGA_POTINP_FIRE2_1) == 0u,
            (potinp & ANA_AMIGA_POTINP_FIRE3_1) == 0u);
    ana_backend_input[ANA_INPUT_DEVICE_1] =
        ana_input_state_from_amiga_joydat(
            custom.joy0dat,
            (ciaa_port_a & ANA_AMIGA_CIAA_FIRE0) == 0u,
            (potinp & ANA_AMIGA_POTINP_FIRE2_0) == 0u,
            (potinp & ANA_AMIGA_POTINP_FIRE3_0) == 0u);
}

static void ana_input_poll_backend(void)
{
    struct Window* window;
    struct IntuiMessage* message;
    ANA_Key key;
    int code;
    int is_down;

    ana_amiga_poll_joysticks();
    ana_amiga_poll_keyboard_matrix();

    window = (struct Window*)ana_gfx_native_window();
    if (window == NULL || window->UserPort == NULL) {
        ana_amiga_sync_keyboard_key_state();
        return;
    }

    if ((window->Flags & WFLG_WINDOWACTIVE) == 0) {
        if (window->WScreen != NULL) {
            ScreenToFront(window->WScreen);
        }
        WindowToFront(window);
        ActivateWindow(window);
    }

    while ((message = (struct IntuiMessage*)GetMsg(window->UserPort)) != NULL) {
        if (message->Class == IDCMP_RAWKEY) {
            code = (int)(message->Code & 0x7f);
            is_down = (message->Code & ANA_AMIGA_RAWKEY_RELEASE) == 0;
            key = ana_amiga_key_from_raw_code(code);
            ana_debug_raw_count++;
            ana_input_debug_record_event(
                (int)message->Class,
                code,
                key,
                is_down);

            if (ana_valid_key(key)) {
                ana_keyboard_raw_key_state[key] = is_down;
                if (is_down) {
                    ana_input_pulse_key(key);
                }
            }
        } else if (message->Class == IDCMP_VANILLAKEY) {
            key = ana_amiga_key_from_vanilla_code((int)message->Code);
            ana_debug_vanilla_count++;
            ana_input_debug_record_event(
                (int)message->Class,
                (int)message->Code,
                key,
                1);
            if (ana_valid_key(key)) {
                ana_input_pulse_key(key);
            }
        }

        ReplyMsg((struct Message*)message);
    }

    ana_amiga_sync_keyboard_key_state();
}
#else
static void ana_input_poll_backend(void)
{
}
#endif

void ana_input_shutdown(void)
{
#ifdef ANA_TARGET_AMIGA
    if (ana_keyboard_io != NULL) {
        if (ana_keyboard_open) {
            if (!CheckIO((struct IORequest*)ana_keyboard_io)) {
                AbortIO((struct IORequest*)ana_keyboard_io);
                WaitIO((struct IORequest*)ana_keyboard_io);
            }
            CloseDevice((struct IORequest*)ana_keyboard_io);
        }
        DeleteExtIO((struct IORequest*)ana_keyboard_io);
        ana_keyboard_io = NULL;
    }

    if (ana_keyboard_port != NULL) {
        DeletePort(ana_keyboard_port);
        ana_keyboard_port = NULL;
    }

    ana_keyboard_open = 0;
    ana_keyboard_failed = 0;
#endif
}

void ana_input_reset(void)
{
    int i;

    for (i = 0; i < ANA_INPUT_DEVICES; i++) {
        ana_current_input[i] = 0u;
        ana_previous_input[i] = 0u;
        ana_pending_input[i] = 0u;
        ana_backend_input[i] = 0u;
        ana_event_input[i] = 0u;
        ana_action_quit_map[i] = 0u;
    }

    for (i = 0; i < ANA_KEY_COUNT; i++) {
        ana_pending_key_state[i] = 0;
#ifdef ANA_TARGET_AMIGA
        ana_keyboard_matrix_key_state[i] = 0;
        ana_keyboard_raw_key_state[i] = 0;
#endif
    }

    ana_input_clear_key_map();
    ana_input_map_default_keys(ANA_INPUT_DEVICE_0);

    ana_current_quit_requested = 0;
    ana_pending_quit_requested = 0;
    ana_event_quit_requested = 0;
    ana_debug_event_count = 0;
    ana_debug_raw_count = 0;
    ana_debug_vanilla_count = 0;
    ana_debug_matrix_count = 0;
    ana_debug_matrix_ready = 0;
    ana_debug_last_class = 0;
    ana_debug_last_code = 0;
    ana_debug_last_key = 0;
    ana_debug_last_is_down = 0;

#ifdef ANA_TARGET_AMIGA
    ana_amiga_keyboard_open();
    ana_keyboard_matrix_poll_countdown = 0;
#endif
}

void ana_input_update(void)
{
    ana_input_poll_backend();
    ana_input_advance_without_poll();
}

void ana_input_advance_without_poll(void)
{
    int i;

    for (i = 0; i < ANA_INPUT_DEVICES; i++) {
        ana_previous_input[i] = ana_current_input[i];
        ana_current_input[i] = ana_pending_input[i] |
            ana_backend_input[i] |
            ana_event_input[i] |
            ana_input_mapped_key_state((ANA_InputDevice)i);
        ana_event_input[i] = 0u;
    }

    ana_current_quit_requested = ana_pending_quit_requested ||
        ana_event_quit_requested ||
        ana_input_mapped_quit_requested();
    ana_event_quit_requested = 0;
}

int ana_input_direction(ANA_InputDevice device, ANA_InputDirection direction)
{
    return ana_input_mask_down(device, ana_input_direction_mask(direction));
}

int ana_input_direction_pressed(ANA_InputDevice device, ANA_InputDirection direction)
{
    return ana_input_mask_pressed(device, ana_input_direction_mask(direction));
}

int ana_input_direction_released(ANA_InputDevice device, ANA_InputDirection direction)
{
    return ana_input_mask_released(device, ana_input_direction_mask(direction));
}

int ana_input_action(ANA_InputDevice device, ANA_InputAction action)
{
    return ana_input_mask_down(device, ana_input_action_mask(action));
}

int ana_input_action_pressed(ANA_InputDevice device, ANA_InputAction action)
{
    return ana_input_mask_pressed(device, ana_input_action_mask(action));
}

int ana_input_action_released(ANA_InputDevice device, ANA_InputAction action)
{
    return ana_input_mask_released(device, ana_input_action_mask(action));
}

int ana_quit_requested(void)
{
    return ana_current_quit_requested;
}

void ana_input_debug_snapshot(ANA_InputDebug* debug)
{
    int i;

    if (debug == NULL) {
        return;
    }

    debug->event_count = ana_debug_event_count;
    debug->raw_count = ana_debug_raw_count;
    debug->vanilla_count = ana_debug_vanilla_count;
    debug->matrix_count = ana_debug_matrix_count;
    debug->matrix_ready = ana_debug_matrix_ready;
    debug->last_class = ana_debug_last_class;
    debug->last_code = ana_debug_last_code;
    debug->last_key = ana_debug_last_key;
    debug->last_is_down = ana_debug_last_is_down;
    for (i = 0; i < ANA_INPUT_DEVICES; i++) {
        debug->current_state[i] = ana_current_input[i];
        debug->backend_state[i] = ana_backend_input[i];
    }
    debug->key_c_down = ana_pending_key_state[ANA_KEY_C];
    debug->key_q_down = ana_pending_key_state[ANA_KEY_Q];
    debug->key_escape_down = ana_pending_key_state[ANA_KEY_ESCAPE];
    debug->key_space_down = ana_pending_key_state[ANA_KEY_SPACE];
    debug->key_x_down = ana_pending_key_state[ANA_KEY_X];
    debug->quit_requested = ana_current_quit_requested;
}

void ana_input_clear_key_map(void)
{
    int key;
    int device;

    for (device = 0; device < ANA_INPUT_DEVICES; device++) {
        ana_action_quit_map[device] = 0u;
    }

    for (key = 0; key < ANA_KEY_COUNT; key++) {
        ana_key_quit_map[key] = 0;

        for (device = 0; device < ANA_INPUT_DEVICES; device++) {
            ana_key_direction_map[key][device] = 0u;
            ana_key_action_map[key][device] = 0u;
        }
    }

    ana_key_quit_map[ANA_KEY_ESCAPE] = 1;
}

void ana_input_map_key_to_direction(
    ANA_Key key,
    ANA_InputDevice device,
    ANA_InputDirection direction)
{
    unsigned int mask;

    if (!ana_valid_key(key) || !ana_valid_device(device)) {
        return;
    }

    mask = ana_input_direction_mask(direction);
    if (mask == 0u) {
        return;
    }

    ana_key_direction_map[key][device] |= mask;
}

void ana_input_map_key_to_action(
    ANA_Key key,
    ANA_InputDevice device,
    ANA_InputAction action)
{
    unsigned int mask;

    if (!ana_valid_key(key) || !ana_valid_device(device)) {
        return;
    }

    mask = ana_input_action_mask(action);
    if (mask == 0u) {
        return;
    }

    ana_key_action_map[key][device] |= mask;
}

void ana_input_map_key_to_quit(ANA_Key key)
{
    if (!ana_valid_key(key)) {
        return;
    }

    ana_key_quit_map[key] = 1;
}

void ana_input_unmap_key_from_quit(ANA_Key key)
{
    if (!ana_valid_key(key)) {
        return;
    }

    ana_key_quit_map[key] = 0;
}

void ana_input_map_action_to_quit(
    ANA_InputDevice device,
    ANA_InputAction action)
{
    unsigned int mask;

    if (!ana_valid_device(device)) {
        return;
    }

    mask = ana_input_action_mask(action);
    if (mask == 0u) {
        return;
    }

    ana_action_quit_map[device] |= mask;
}

void ana_input_map_default_keys(ANA_InputDevice device)
{
    if (!ana_valid_device(device)) {
        return;
    }

    ana_input_map_key_to_direction(ANA_KEY_LEFT, device, ANA_INPUT_LEFT);
    ana_input_map_key_to_direction(ANA_KEY_A, device, ANA_INPUT_LEFT);
    ana_input_map_key_to_direction(ANA_KEY_RIGHT, device, ANA_INPUT_RIGHT);
    ana_input_map_key_to_direction(ANA_KEY_D, device, ANA_INPUT_RIGHT);
    ana_input_map_key_to_direction(ANA_KEY_UP, device, ANA_INPUT_UP);
    ana_input_map_key_to_direction(ANA_KEY_W, device, ANA_INPUT_UP);
    ana_input_map_key_to_direction(ANA_KEY_DOWN, device, ANA_INPUT_DOWN);
    ana_input_map_key_to_direction(ANA_KEY_S, device, ANA_INPUT_DOWN);

    ana_input_map_key_to_action(ANA_KEY_SPACE, device, ANA_ACTION_1);
    ana_input_map_key_to_action(ANA_KEY_CTRL, device, ANA_ACTION_1);
    ana_input_map_key_to_action(ANA_KEY_X, device, ANA_ACTION_2);
    ana_input_map_key_to_action(ANA_KEY_Z, device, ANA_ACTION_2);
    ana_input_map_key_to_action(ANA_KEY_C, device, ANA_ACTION_3);
    ana_input_map_key_to_action(ANA_KEY_V, device, ANA_ACTION_3);
    ana_input_map_key_to_action(ANA_KEY_RETURN, device, ANA_ACTION_4);
}

void ana_input_set_pending_state(ANA_InputDevice device, unsigned int state)
{
    if (!ana_valid_device(device)) {
        return;
    }

    ana_pending_input[device] = state;
}

void ana_input_set_pending_key_state(ANA_Key key, int is_down)
{
    if (!ana_valid_key(key)) {
        return;
    }

    ana_pending_key_state[key] = is_down != 0;
}

void ana_input_set_pending_quit(int requested)
{
    ana_pending_quit_requested = requested != 0;
}
