#include <stdio.h>
#include <stdlib.h>
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

#define SOUND_HEADER_SIZE 20
#define SOUND_RATE 8000
#define SOUND_FLAG_SIGNED_8BIT 1
#define FIRE_SOUND_SAMPLES 160
#define EXPLOSION_SOUND_SAMPLES 320
#define STEP_SOUND_SAMPLES 120
#define GAME_OVER_SOUND_SAMPLES 480
#define SOUND_SIZE(samples) (SOUND_HEADER_SIZE + (samples))

static const unsigned char hud_font_glyphs
    [HUD_FONT_CHAR_COUNT][HUD_FONT_HEIGHT] = {
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
static unsigned char fire_sound_data[SOUND_SIZE(FIRE_SOUND_SAMPLES)];
static unsigned char explosion_sound_data[SOUND_SIZE(EXPLOSION_SOUND_SAMPLES)];
static unsigned char step_sound_data[SOUND_SIZE(STEP_SOUND_SAMPLES)];
static unsigned char game_over_sound_data[SOUND_SIZE(GAME_OVER_SOUND_SAMPLES)];

static void write_u16_le(unsigned char* bytes, int value)
{
    bytes[0] = (unsigned char)(value & 0xff);
    bytes[1] = (unsigned char)((value >> 8) & 0xff);
}

static void write_u32_le(unsigned char* bytes, long value)
{
    bytes[0] = (unsigned char)(value & 0xff);
    bytes[1] = (unsigned char)((value >> 8) & 0xff);
    bytes[2] = (unsigned char)((value >> 16) & 0xff);
    bytes[3] = (unsigned char)((value >> 24) & 0xff);
}

static void build_hud_font_data(void)
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
    write_u16_le(hud_font_data + 8, HUD_FONT_WIDTH);
    write_u16_le(hud_font_data + 10, HUD_FONT_HEIGHT);
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
    write_u16_le(hud_font_data + 24, HUD_FONT_WIDTH);
    write_u16_le(hud_font_data + 26, HUD_FONT_HEIGHT);
    write_u16_le(hud_font_data + 28, HUD_FONT_CHAR_COUNT);
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

static void write_sound_header(
    unsigned char* bytes,
    int sample_count,
    int volume,
    int priority)
{
    bytes[0] = 'A';
    bytes[1] = 'N';
    bytes[2] = 'A';
    bytes[3] = 'S';
    bytes[4] = 'N';
    bytes[5] = 'D';
    bytes[6] = '0';
    bytes[7] = '1';
    write_u16_le(bytes + 8, SOUND_RATE);
    write_u32_le(bytes + 10, (long)sample_count);
    bytes[14] = (unsigned char)volume;
    bytes[15] = (unsigned char)priority;
    bytes[16] = SOUND_FLAG_SIGNED_8BIT;
    bytes[17] = 0u;
    bytes[18] = 0u;
    bytes[19] = 0u;
}

static unsigned char signed_sample(int sample)
{
    if (sample > 127) {
        sample = 127;
    }

    if (sample < -128) {
        sample = -128;
    }

    return (unsigned char)(sample & 0xff);
}

static int enveloped_sample(
    int sample,
    int index,
    int sample_count,
    int attack_samples,
    int release_samples)
{
    int scale;
    int release_index;

    scale = 64;
    if (attack_samples > 0 && index < attack_samples) {
        scale = (index * 64) / attack_samples;
    }

    release_index = sample_count - index;
    if (release_samples > 0 && release_index < release_samples) {
        int release_scale;

        release_scale = (release_index * 64) / release_samples;
        if (release_scale < scale) {
            scale = release_scale;
        }
    }

    return (sample * scale) / 64;
}

static void build_fire_sound(void)
{
    unsigned char* out;
    int i;
    int amp;
    int period;
    int sample;

    write_sound_header(fire_sound_data, FIRE_SOUND_SAMPLES, 36, 2);
    out = fire_sound_data + SOUND_HEADER_SIZE;
    for (i = 0; i < FIRE_SOUND_SAMPLES; i++) {
        amp = 46 - (i / 3);
        if (amp < 5) {
            amp = 5;
        }
        period = 6 - (i / 24);
        if (period < 2) {
            period = 2;
        }
        sample = ((i / period) & 1) ? amp : -amp;
        sample = enveloped_sample(sample, i, FIRE_SOUND_SAMPLES, 10, 34);
        out[i] = signed_sample(sample);
    }
}

static void build_explosion_sound(void)
{
    unsigned char* out;
    unsigned long state;
    int i;
    int amp;
    int sample;

    write_sound_header(
        explosion_sound_data,
        EXPLOSION_SOUND_SAMPLES,
        44,
        4);
    out = explosion_sound_data + SOUND_HEADER_SIZE;
    state = 0x2468ace1UL;
    for (i = 0; i < EXPLOSION_SOUND_SAMPLES; i++) {
        state = (state * 1103515245UL) + 12345UL;
        amp = 54 - ((i * 50) / EXPLOSION_SOUND_SAMPLES);
        sample = (int)((state >> 16) & 0x7fu) - 64;
        sample = (sample * amp) / 64;
        sample = enveloped_sample(sample, i, EXPLOSION_SOUND_SAMPLES, 4, 80);
        out[i] = signed_sample(sample);
    }
}

static void build_step_sound(void)
{
    unsigned char* out;
    int i;
    int sample;

    write_sound_header(step_sound_data, STEP_SOUND_SAMPLES, 24, 1);
    out = step_sound_data + SOUND_HEADER_SIZE;
    for (i = 0; i < STEP_SOUND_SAMPLES; i++) {
        sample = ((i / 10) & 1) ? 28 : -28;
        sample = enveloped_sample(sample, i, STEP_SOUND_SAMPLES, 6, 44);
        out[i] = signed_sample(sample);
    }
}

static void build_game_over_sound(void)
{
    unsigned char* out;
    int i;
    int amp;
    int period;
    int sample;

    write_sound_header(game_over_sound_data, GAME_OVER_SOUND_SAMPLES, 42, 5);
    out = game_over_sound_data + SOUND_HEADER_SIZE;
    for (i = 0; i < GAME_OVER_SOUND_SAMPLES; i++) {
        amp = 46 - (i / 10);
        if (amp < 14) {
            amp = 14;
        }
        period = 5 + (i / 32);
        sample = ((i / period) & 1) ? amp : -amp;
        sample = enveloped_sample(sample, i, GAME_OVER_SOUND_SAMPLES, 10, 96);
        out[i] = signed_sample(sample);
    }
}

static void build_sound_data(void)
{
    build_fire_sound();
    build_explosion_sound();
    build_step_sound();
    build_game_over_sound();
}

static char* asset_path(const char* dir, const char* name)
{
    char* path;
    size_t dir_len;
    size_t name_len;
    int needs_slash;

    dir_len = strlen(dir);
    name_len = strlen(name);
    needs_slash = dir_len > 0u && dir[dir_len - 1u] != '/';
    path = (char*)malloc(dir_len + (needs_slash ? 1u : 0u) + name_len + 1u);
    if (path == NULL) {
        return NULL;
    }

    strcpy(path, dir);
    if (needs_slash) {
        strcat(path, "/");
    }
    strcat(path, name);
    return path;
}

static int write_asset(
    const char* dir,
    const char* name,
    const unsigned char* bytes,
    long size)
{
    FILE* file;
    char* path;
    size_t written;

    path = asset_path(dir, name);
    if (path == NULL) {
        fprintf(stderr, "Out of memory while writing %s\n", name);
        return 0;
    }

    file = fopen(path, "wb");
    if (file == NULL) {
        fprintf(stderr, "Could not open %s\n", path);
        free(path);
        return 0;
    }

    written = fwrite(bytes, 1u, (size_t)size, file);
    if (fclose(file) != 0 || written != (size_t)size) {
        fprintf(stderr, "Could not write %s\n", path);
        free(path);
        return 0;
    }

    printf("Wrote %s\n", path);
    free(path);
    return 1;
}

static int write_all_assets(const char* dir)
{
    build_hud_font_data();
    build_sound_data();

    return
        write_asset(
            dir,
            "hud.anafnt",
            hud_font_data,
            (long)sizeof(hud_font_data)) &&
        write_asset(
            dir,
            "fire.anasnd",
            fire_sound_data,
            (long)sizeof(fire_sound_data)) &&
        write_asset(
            dir,
            "explosion.anasnd",
            explosion_sound_data,
            (long)sizeof(explosion_sound_data)) &&
        write_asset(
            dir,
            "step.anasnd",
            step_sound_data,
            (long)sizeof(step_sound_data)) &&
        write_asset(
            dir,
            "gameover.anasnd",
            game_over_sound_data,
            (long)sizeof(game_over_sound_data));
}

int main(int argc, char** argv)
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <output-dir>\n", argv[0]);
        return 1;
    }

    return write_all_assets(argv[1]) ? 0 : 1;
}
