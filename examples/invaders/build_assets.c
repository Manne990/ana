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

#define IMAGE_HEADER_SIZE 20
#define SOUND_HEADER_SIZE 20
#define SOUND_RATE 8000
#define SOUND_FLAG_SIGNED_8BIT 1
#define FIRE_SOUND_SAMPLES 160
#define EXPLOSION_SOUND_SAMPLES 320
#define STEP_SOUND_SAMPLES 160
#define GAME_OVER_SOUND_SAMPLES 480
#define SOUND_SIZE(samples) (SOUND_HEADER_SIZE + (samples))

#define PLAYER_WIDTH 16
#define PLAYER_HEIGHT 10
#define BULLET_WIDTH 2
#define BULLET_HEIGHT 6
#define INVADER_WIDTH 16
#define INVADER_HEIGHT 8
#define INVADER_FRAMES 2
#define INVADER_COLOR 3
#define EXPLOSION_WIDTH 16
#define EXPLOSION_HEIGHT 10
#define EXPLOSION_FRAMES 2

#define IMAGE_SIZE(width, height, frames, bitplanes, masked) \
    (IMAGE_HEADER_SIZE + \
        ((frames) * ((((width) + 7) / 8) * (height)) * \
            ((bitplanes) + ((masked) ? 1 : 0))))

static const char* const player_rows[PLAYER_HEIGHT] = {
    "......55........",
    ".....5555.......",
    "....555555......",
    "...55555555.....",
    "..5555555555....",
    ".555555555555...",
    "55555555555555..",
    ".555555555555...",
    "..5333333335....",
    "....22..22......"
};

static const char* const bullet_rows[BULLET_HEIGHT] = {
    "11",
    "11",
    "11",
    "11",
    "11",
    "11"
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

static unsigned char player_image_data[
    IMAGE_SIZE(PLAYER_WIDTH, PLAYER_HEIGHT, 1, 4, 1)];
static unsigned char bullet_image_data[
    IMAGE_SIZE(BULLET_WIDTH, BULLET_HEIGHT, 1, 1, 1)];
static unsigned char invader_image_data[
    IMAGE_SIZE(INVADER_WIDTH, INVADER_HEIGHT, INVADER_FRAMES, 4, 0)];
static unsigned char explosion_image_data[
    IMAGE_SIZE(EXPLOSION_WIDTH, EXPLOSION_HEIGHT, EXPLOSION_FRAMES, 4, 1)];
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

static void write_image_header(
    unsigned char* bytes,
    int width,
    int height,
    int frames,
    int bitplanes,
    int masked)
{
    bytes[0] = 'A';
    bytes[1] = 'N';
    bytes[2] = 'A';
    bytes[3] = 'I';
    bytes[4] = 'M';
    bytes[5] = 'G';
    bytes[6] = '0';
    bytes[7] = '1';
    write_u16_le(bytes + 8, width);
    write_u16_le(bytes + 10, height);
    write_u16_le(bytes + 12, frames);
    bytes[14] = (unsigned char)bitplanes;
    bytes[15] = masked ? 1u : 0u;
    bytes[16] = 0u;
    bytes[17] = 0u;
    bytes[18] = 0u;
    bytes[19] = 0u;
}

static void set_planar_bit(unsigned char* bytes, int row_bytes, int x, int y)
{
    bytes[(y * row_bytes) + (x >> 3)] =
        (unsigned char)(
            bytes[(y * row_bytes) + (x >> 3)] |
            (0x80u >> (x & 7)));
}

static unsigned char color_from_char(char ch, unsigned char fallback)
{
    if (ch >= '0' && ch <= '9') {
        return (unsigned char)(ch - '0');
    }

    if (ch >= 'A' && ch <= 'F') {
        return (unsigned char)(10 + (ch - 'A'));
    }

    if (ch >= 'a' && ch <= 'f') {
        return (unsigned char)(10 + (ch - 'a'));
    }

    return fallback;
}

static void write_color_frame(
    unsigned char* image_data,
    int width,
    int height,
    int bitplanes,
    int frame,
    const char* const* rows,
    unsigned char fallback_color,
    int masked)
{
    unsigned char* frame_base;
    unsigned char* mask;
    unsigned char* planes;
    unsigned char color;
    int row_bytes;
    int plane_size;
    int frame_size;
    int x;
    int y;
    int plane;

    row_bytes = (width + 7) / 8;
    plane_size = row_bytes * height;
    frame_size = plane_size * (bitplanes + (masked ? 1 : 0));
    frame_base = image_data + IMAGE_HEADER_SIZE + (frame * frame_size);
    mask = masked ? frame_base : NULL;
    planes = masked ? frame_base + plane_size : frame_base;
    memset(frame_base, 0, (size_t)frame_size);

    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            if (rows[y][x] != '.' && rows[y][x] != ' ') {
                color = color_from_char(rows[y][x], fallback_color);
                if (mask != NULL) {
                    set_planar_bit(mask, row_bytes, x, y);
                }
                for (plane = 0; plane < bitplanes; plane++) {
                    if ((color & (1u << plane)) != 0u) {
                        set_planar_bit(
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

static void build_image_data(void)
{
    write_image_header(player_image_data, PLAYER_WIDTH, PLAYER_HEIGHT, 1, 4, 1);
    write_color_frame(
        player_image_data,
        PLAYER_WIDTH,
        PLAYER_HEIGHT,
        4,
        0,
        player_rows,
        5u,
        1);

    write_image_header(bullet_image_data, BULLET_WIDTH, BULLET_HEIGHT, 1, 1, 1);
    write_color_frame(
        bullet_image_data,
        BULLET_WIDTH,
        BULLET_HEIGHT,
        1,
        0,
        bullet_rows,
        1u,
        1);

    write_image_header(
        invader_image_data,
        INVADER_WIDTH,
        INVADER_HEIGHT,
        INVADER_FRAMES,
        4,
        0);
    write_color_frame(
        invader_image_data,
        INVADER_WIDTH,
        INVADER_HEIGHT,
        4,
        0,
        invader_frame_0,
        INVADER_COLOR,
        0);
    write_color_frame(
        invader_image_data,
        INVADER_WIDTH,
        INVADER_HEIGHT,
        4,
        1,
        invader_frame_1,
        INVADER_COLOR,
        0);

    write_image_header(
        explosion_image_data,
        EXPLOSION_WIDTH,
        EXPLOSION_HEIGHT,
        EXPLOSION_FRAMES,
        4,
        1);
    write_color_frame(
        explosion_image_data,
        EXPLOSION_WIDTH,
        EXPLOSION_HEIGHT,
        4,
        0,
        explosion_frame_0,
        5u,
        1);
    write_color_frame(
        explosion_image_data,
        EXPLOSION_WIDTH,
        EXPLOSION_HEIGHT,
        4,
        1,
        explosion_frame_1,
        2u,
        1);
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

static void build_fire_sound(void)
{
    unsigned char* out;
    int i;
    int amp;
    int period;
    int sample;

    write_sound_header(fire_sound_data, FIRE_SOUND_SAMPLES, 42, 2);
    out = fire_sound_data + SOUND_HEADER_SIZE;
    for (i = 0; i < FIRE_SOUND_SAMPLES; i++) {
        amp = 58 - (i / 2);
        if (amp < 8) {
            amp = 8;
        }
        period = 6 - (i / 24);
        if (period < 2) {
            period = 2;
        }
        sample = ((i / period) & 1) ? amp : -amp;
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
        54,
        4);
    out = explosion_sound_data + SOUND_HEADER_SIZE;
    state = 0x2468ace1UL;
    for (i = 0; i < EXPLOSION_SOUND_SAMPLES; i++) {
        state = (state * 1103515245UL) + 12345UL;
        amp = 72 - ((i * 68) / EXPLOSION_SOUND_SAMPLES);
        sample = (int)((state >> 16) & 0x7fu) - 64;
        sample = (sample * amp) / 64;
        out[i] = signed_sample(sample);
    }
}

static void build_step_sound(void)
{
    unsigned char* out;
    int i;
    int sample;

    write_sound_header(step_sound_data, STEP_SOUND_SAMPLES, 34, 1);
    out = step_sound_data + SOUND_HEADER_SIZE;
    for (i = 0; i < STEP_SOUND_SAMPLES; i++) {
        sample = ((i / 8) & 1) ? 42 : -42;
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

    write_sound_header(game_over_sound_data, GAME_OVER_SOUND_SAMPLES, 52, 5);
    out = game_over_sound_data + SOUND_HEADER_SIZE;
    for (i = 0; i < GAME_OVER_SOUND_SAMPLES; i++) {
        amp = 58 - (i / 8);
        if (amp < 20) {
            amp = 20;
        }
        period = 5 + (i / 32);
        sample = ((i / period) & 1) ? amp : -amp;
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
    build_image_data();
    build_hud_font_data();
    build_sound_data();

    return
        write_asset(
            dir,
            "player.anaimg",
            player_image_data,
            (long)sizeof(player_image_data)) &&
        write_asset(
            dir,
            "bullet.anaimg",
            bullet_image_data,
            (long)sizeof(bullet_image_data)) &&
        write_asset(
            dir,
            "invader.anaimg",
            invader_image_data,
            (long)sizeof(invader_image_data)) &&
        write_asset(
            dir,
            "explosion.anaimg",
            explosion_image_data,
            (long)sizeof(explosion_image_data)) &&
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
