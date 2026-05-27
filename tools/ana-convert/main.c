#include "ana.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wunused-function"
#endif
#define STBI_ONLY_PNG
#define STBI_NO_LINEAR
#define STBI_NO_HDR
#define STB_IMAGE_IMPLEMENTATION
#include "vendor/stb_image.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#define ANA_CONVERT_MAX_COLORS 16
#define ANA_CONVERT_MAX_DIMENSION 4096
#define ANA_CONVERT_MAX_TOKENS 32
#define ANA_CONVERT_MAX_MANIFEST_PALETTES 16
#define ANA_CONVERT_IMAGE_FLAG_MASKED 0x01u
#define ANA_CONVERT_FONT_HEADER_SIZE 16
#define ANA_CONVERT_SOUND_HEADER_SIZE 20
#define ANA_CONVERT_SOUND_DEFAULT_RATE 8000
#define ANA_CONVERT_SOUND_MIN_RATE 1000
#define ANA_CONVERT_SOUND_MAX_RATE 30000
#define ANA_CONVERT_SOUND_MAX_SAMPLES 131070L
#define ANA_CONVERT_SOUND_FLAG_SIGNED_8BIT 0x01u

typedef enum ANA_SoundWave {
    ANA_SOUND_WAVE_NONE = 0,
    ANA_SOUND_WAVE_SQUARE,
    ANA_SOUND_WAVE_NOISE
} ANA_SoundWave;

typedef struct ANA_ConvertColor {
    int r;
    int g;
    int b;
    int a;
} ANA_ConvertColor;

typedef struct ANA_SourceImage {
    int width;
    int height;
    ANA_ConvertColor* pixels;
} ANA_SourceImage;

typedef struct ANA_ImageOptions {
    const char* input_path;
    const char* output_path;
    const char* palette_path;
    int colors;
    int frame_width;
    int frame_height;
    int has_transparent;
    ANA_ConvertColor transparent;
} ANA_ImageOptions;

typedef struct ANA_FontOptions {
    ANA_ImageOptions image;
    int char_width;
    int char_height;
    int first_char;
    int char_count;
} ANA_FontOptions;

typedef struct ANA_SoundOptions {
    const char* input_path;
    const char* output_path;
    ANA_SoundWave wave;
    int sample_rate;
    int sample_count;
    int volume;
    int priority;
    int attack_samples;
    int release_samples;
    int amp_start;
    int amp_end;
    int period_start;
    int period_end;
    unsigned long seed;
} ANA_SoundOptions;

typedef struct ANA_WavSource {
    int sample_rate;
    int channels;
    int bits_per_sample;
    long frame_count;
    long data_size;
    unsigned char* data;
} ANA_WavSource;

typedef struct ANA_Palette {
    ANA_ConvertColor colors[ANA_CONVERT_MAX_COLORS];
    int present[ANA_CONVERT_MAX_COLORS];
    int count;
    int max_index;
} ANA_Palette;

typedef struct ANA_PaletteOptions {
    const char* input_path;
    const char* output_path;
    int colors;
} ANA_PaletteOptions;

typedef struct ANA_ManifestPalette {
    char* name;
    char* path;
} ANA_ManifestPalette;

static void print_usage(void)
{
    printf("ana-convert %s\n", ANA_VERSION_STRING);
    printf("Usage:\n");
    printf("  ana-convert --version\n");
    printf("  ana-convert image input.png --out output.anaimg [options]\n");
    printf("  ana-convert font input.png --out output.anafnt [options]\n");
    printf("  ana-convert sound input.anasfx|input.wav --out output.anasnd [options]\n");
    printf("  ana-convert palette palette.png --out game.anapal --colors 16\n");
    printf("  ana-convert build assets.ana --out build/assets/game\n");
    printf("\n");
    printf("Image options:\n");
    printf("  --palette PATH          ANA palette file or manifest palette name\n");
    printf("  --colors N              Palette size, 1-16. Default: 16\n");
    printf("  --frame-width N         Frame width for spritesheets\n");
    printf("  --frame-height N        Frame height for spritesheets\n");
    printf("  --transparent R,G,B     Transparent color, for example 255,0,255\n");
    printf("  --transparent #RRGGBB   Transparent color as hex\n");
    printf("\n");
    printf("Font options:\n");
    printf("  --char-width N          Fixed glyph width in pixels\n");
    printf("  --char-height N         Fixed glyph height in pixels\n");
    printf("  --first-char N          First ASCII character code. Default: 32\n");
    printf("  --chars N               Number of glyph frames\n");
    printf("  plus image palette and transparent options.\n");
    printf("\n");
    printf("Sound input formats: ANA_SOUND 1 text recipes and PCM WAV.\n");
    printf("Sound options for WAV: --rate N --volume N --priority N.\n");
    printf("\n");
    printf("Image input formats: PNG and PPM P3/P6.\n");
    printf("Manifest asset types: palette, image, font, sound, music.\n");
}

static char* copy_string(const char* text)
{
    char* copy;
    size_t length;

    length = strlen(text);
    copy = (char*)malloc(length + 1u);
    if (copy == NULL) {
        return NULL;
    }

    memcpy(copy, text, length + 1u);
    return copy;
}

static int has_extension(const char* path, const char* extension)
{
    size_t path_length;
    size_t extension_length;
    const char* suffix;

    path_length = strlen(path);
    extension_length = strlen(extension);
    if (path_length < extension_length) {
        return 0;
    }

    suffix = path + path_length - extension_length;
    while (*suffix != '\0' && *extension != '\0') {
        if (tolower((unsigned char)*suffix) !=
                tolower((unsigned char)*extension)) {
            return 0;
        }
        suffix++;
        extension++;
    }

    return 1;
}

static char* path_join(const char* dir, const char* name)
{
    char* path;
    size_t dir_len;
    size_t name_len;
    int needs_slash;

    if (dir == NULL || dir[0] == '\0' || name[0] == '/') {
        return copy_string(name);
    }

    dir_len = strlen(dir);
    name_len = strlen(name);
    needs_slash = dir[dir_len - 1u] != '/';
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

static char* path_dirname(const char* path)
{
    const char* slash;
    char* dir;
    size_t length;

    slash = strrchr(path, '/');
    if (slash == NULL) {
        return copy_string(".");
    }

    length = (size_t)(slash - path);
    if (length == 0u) {
        length = 1u;
    }

    dir = (char*)malloc(length + 1u);
    if (dir == NULL) {
        return NULL;
    }

    memcpy(dir, path, length);
    dir[length] = '\0';
    return dir;
}

static char* asset_output_path(const char* output_dir, const char* name, const char* ext)
{
    char* filename;
    char* path;
    size_t name_len;
    size_t ext_len;

    name_len = strlen(name);
    ext_len = strlen(ext);
    filename = (char*)malloc(name_len + ext_len + 1u);
    if (filename == NULL) {
        return NULL;
    }

    strcpy(filename, name);
    strcat(filename, ext);
    path = path_join(output_dir, filename);
    free(filename);
    return path;
}

static int ensure_dir(const char* path)
{
    char* copy;
    char* cursor;

    if (path == NULL || path[0] == '\0') {
        fprintf(stderr, "ana-convert: invalid output directory\n");
        return 0;
    }

    copy = copy_string(path);
    if (copy == NULL) {
        fprintf(stderr, "ana-convert: out of memory while creating '%s'\n", path);
        return 0;
    }

    cursor = copy;
    if (cursor[0] == '/') {
        cursor++;
    }

    while (*cursor != '\0') {
        if (*cursor == '/') {
            *cursor = '\0';
            if (copy[0] != '\0' &&
                    mkdir(copy, 0777) != 0 &&
                    errno != EEXIST) {
                fprintf(stderr, "ana-convert: could not create directory '%s'\n", copy);
                free(copy);
                return 0;
            }
            *cursor = '/';
        }
        cursor++;
    }

    if (mkdir(copy, 0777) == 0 || errno == EEXIST) {
        free(copy);
        return 1;
    }

    fprintf(stderr, "ana-convert: could not create directory '%s'\n", path);
    free(copy);
    return 0;
}

static int copy_binary_file(const char* input_path, const char* output_path)
{
    unsigned char buffer[4096];
    FILE* input;
    FILE* output;
    size_t bytes_read;
    int ok;

    input = fopen(input_path, "rb");
    if (input == NULL) {
        fprintf(stderr, "ana-convert: could not open '%s'\n", input_path);
        return 0;
    }

    output = fopen(output_path, "wb");
    if (output == NULL) {
        fprintf(stderr, "ana-convert: could not open output '%s'\n", output_path);
        fclose(input);
        return 0;
    }

    ok = 1;
    while ((bytes_read = fread(buffer, 1u, sizeof(buffer), input)) > 0u) {
        if (fwrite(buffer, 1u, bytes_read, output) != bytes_read) {
            ok = 0;
            break;
        }
    }

    if (ferror(input)) {
        ok = 0;
    }

    if (fclose(input) != 0) {
        ok = 0;
    }

    if (fclose(output) != 0) {
        ok = 0;
    }

    if (!ok) {
        fprintf(stderr, "ana-convert: failed while copying '%s'\n", input_path);
    }

    return ok;
}

static int parse_int(const char* text, int* value)
{
    char* end;
    long parsed;

    if (text == NULL || text[0] == '\0') {
        return 0;
    }

    parsed = strtol(text, &end, 10);
    if (*end != '\0' || parsed < 0L || parsed > 32767L) {
        return 0;
    }

    *value = (int)parsed;
    return 1;
}

static int parse_required_positive_int(const char* text, int* value)
{
    if (!parse_int(text, value)) {
        return 0;
    }

    return *value > 0;
}

static int hex_digit_value(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }

    if (c >= 'a' && c <= 'f') {
        return 10 + (c - 'a');
    }

    if (c >= 'A' && c <= 'F') {
        return 10 + (c - 'A');
    }

    return -1;
}

static int parse_hex_byte(const char* text, int* value)
{
    int hi;
    int lo;

    hi = hex_digit_value(text[0]);
    lo = hex_digit_value(text[1]);

    if (hi < 0 || lo < 0) {
        return 0;
    }

    *value = (hi * 16) + lo;
    return 1;
}

static int parse_color_triplet(const char* text, ANA_ConvertColor* color)
{
    const char* comma1;
    const char* comma2;
    char buffer[16];
    int length;

    if (text == NULL || color == NULL) {
        return 0;
    }

    if (text[0] == '#') {
        if (strlen(text) != 7u) {
            return 0;
        }

        if (!parse_hex_byte(text + 1, &color->r) ||
                !parse_hex_byte(text + 3, &color->g) ||
                !parse_hex_byte(text + 5, &color->b)) {
            return 0;
        }

        color->a = 255;
        return 1;
    }

    comma1 = strchr(text, ',');
    if (comma1 == NULL) {
        return 0;
    }

    comma2 = strchr(comma1 + 1, ',');
    if (comma2 == NULL || strchr(comma2 + 1, ',') != NULL) {
        return 0;
    }

    length = (int)(comma1 - text);
    if (length <= 0 || length >= (int)sizeof(buffer)) {
        return 0;
    }
    memcpy(buffer, text, (size_t)length);
    buffer[length] = '\0';
    if (!parse_int(buffer, &color->r)) {
        return 0;
    }

    length = (int)(comma2 - comma1 - 1);
    if (length <= 0 || length >= (int)sizeof(buffer)) {
        return 0;
    }
    memcpy(buffer, comma1 + 1, (size_t)length);
    buffer[length] = '\0';
    if (!parse_int(buffer, &color->g)) {
        return 0;
    }

    if (!parse_int(comma2 + 1, &color->b)) {
        return 0;
    }

    color->a = 255;
    return color->r <= 255 && color->g <= 255 && color->b <= 255;
}

static int colors_equal(const ANA_ConvertColor* left, const ANA_ConvertColor* right)
{
    return left->r == right->r &&
        left->g == right->g &&
        left->b == right->b;
}

static void source_image_free(ANA_SourceImage* image)
{
    if (image == NULL) {
        return;
    }

    free(image->pixels);
    image->pixels = NULL;
    image->width = 0;
    image->height = 0;
}

static int ppm_skip_comment(FILE* file)
{
    int c;

    do {
        c = fgetc(file);
    } while (c != EOF && c != '\n' && c != '\r');

    return c != EOF;
}

static int ppm_read_token(FILE* file, char* token, int token_size)
{
    int c;
    int count;

    count = 0;

    for (;;) {
        c = fgetc(file);
        if (c == EOF) {
            return 0;
        }

        if (c == '#') {
            ppm_skip_comment(file);
        } else if (!isspace(c)) {
            break;
        }
    }

    while (c != EOF && !isspace(c) && c != '#') {
        if (count + 1 >= token_size) {
            return 0;
        }

        token[count] = (char)c;
        count++;
        c = fgetc(file);
    }

    if (c == '#') {
        ppm_skip_comment(file);
    }

    token[count] = '\0';
    return count > 0;
}

static int ppm_read_int_token(FILE* file, int* value)
{
    char token[64];

    if (!ppm_read_token(file, token, (int)sizeof(token))) {
        return 0;
    }

    return parse_int(token, value);
}

static int ppm_scaled_value(int value, int max_value)
{
    return (value * 255) / max_value;
}

static int read_ppm_p3_pixels(FILE* file, ANA_SourceImage* image, int max_value)
{
    int pixel_count;
    int i;
    int r;
    int g;
    int b;

    pixel_count = image->width * image->height;

    for (i = 0; i < pixel_count; i++) {
        if (!ppm_read_int_token(file, &r) ||
                !ppm_read_int_token(file, &g) ||
                !ppm_read_int_token(file, &b)) {
            return 0;
        }

        if (r > max_value || g > max_value || b > max_value) {
            return 0;
        }

        image->pixels[i].r = ppm_scaled_value(r, max_value);
        image->pixels[i].g = ppm_scaled_value(g, max_value);
        image->pixels[i].b = ppm_scaled_value(b, max_value);
        image->pixels[i].a = 255;
    }

    return 1;
}

static int read_ppm_p6_pixels(FILE* file, ANA_SourceImage* image, int max_value)
{
    int pixel_count;
    int i;
    int r;
    int g;
    int b;

    if (max_value > 255) {
        return 0;
    }

    pixel_count = image->width * image->height;

    for (i = 0; i < pixel_count; i++) {
        r = fgetc(file);
        g = fgetc(file);
        b = fgetc(file);

        if (r == EOF || g == EOF || b == EOF) {
            return 0;
        }

        if (r > max_value || g > max_value || b > max_value) {
            return 0;
        }

        image->pixels[i].r = ppm_scaled_value(r, max_value);
        image->pixels[i].g = ppm_scaled_value(g, max_value);
        image->pixels[i].b = ppm_scaled_value(b, max_value);
        image->pixels[i].a = 255;
    }

    return 1;
}

static int read_ppm(const char* path, ANA_SourceImage* image)
{
    FILE* file;
    char magic[8];
    int max_value;
    int pixel_count;
    int ok;

    image->width = 0;
    image->height = 0;
    image->pixels = NULL;

    file = fopen(path, "rb");
    if (file == NULL) {
        fprintf(stderr, "ana-convert: could not open input '%s'\n", path);
        return 0;
    }

    ok = ppm_read_token(file, magic, (int)sizeof(magic)) &&
        (strcmp(magic, "P3") == 0 || strcmp(magic, "P6") == 0) &&
        ppm_read_int_token(file, &image->width) &&
        ppm_read_int_token(file, &image->height) &&
        ppm_read_int_token(file, &max_value);

    if (!ok ||
            image->width <= 0 ||
            image->height <= 0 ||
            image->width > ANA_CONVERT_MAX_DIMENSION ||
            image->height > ANA_CONVERT_MAX_DIMENSION ||
            max_value <= 0 ||
            max_value > 255) {
        fclose(file);
        fprintf(stderr, "ana-convert: invalid or unsupported PPM file\n");
        return 0;
    }

    pixel_count = image->width * image->height;
    image->pixels = (ANA_ConvertColor*)malloc(
        sizeof(ANA_ConvertColor) * (size_t)pixel_count);
    if (image->pixels == NULL) {
        fclose(file);
        fprintf(stderr, "ana-convert: out of memory\n");
        return 0;
    }

    if (strcmp(magic, "P3") == 0) {
        ok = read_ppm_p3_pixels(file, image, max_value);
    } else {
        ok = read_ppm_p6_pixels(file, image, max_value);
    }

    fclose(file);

    if (!ok) {
        source_image_free(image);
        fprintf(stderr, "ana-convert: invalid PPM pixel data\n");
        return 0;
    }

    return 1;
}

static int read_png(const char* path, ANA_SourceImage* image)
{
    unsigned char* pixels;
    int width;
    int height;
    int channels;
    int pixel_count;
    int i;

    image->width = 0;
    image->height = 0;
    image->pixels = NULL;

    pixels = stbi_load(path, &width, &height, &channels, 4);
    (void)channels;
    if (pixels == NULL) {
        fprintf(
            stderr,
            "ana-convert: could not read PNG '%s': %s\n",
            path,
            stbi_failure_reason());
        return 0;
    }

    if (width <= 0 ||
            height <= 0 ||
            width > ANA_CONVERT_MAX_DIMENSION ||
            height > ANA_CONVERT_MAX_DIMENSION) {
        stbi_image_free(pixels);
        fprintf(stderr, "ana-convert: invalid PNG dimensions in '%s'\n", path);
        return 0;
    }

    pixel_count = width * height;
    image->pixels = (ANA_ConvertColor*)malloc(
        sizeof(ANA_ConvertColor) * (size_t)pixel_count);
    if (image->pixels == NULL) {
        stbi_image_free(pixels);
        fprintf(stderr, "ana-convert: out of memory\n");
        return 0;
    }

    image->width = width;
    image->height = height;
    for (i = 0; i < pixel_count; i++) {
        image->pixels[i].r = pixels[(i * 4) + 0];
        image->pixels[i].g = pixels[(i * 4) + 1];
        image->pixels[i].b = pixels[(i * 4) + 2];
        image->pixels[i].a = pixels[(i * 4) + 3];
    }

    stbi_image_free(pixels);
    return 1;
}

static int read_source_image(const char* path, ANA_SourceImage* image)
{
    if (has_extension(path, ".png")) {
        return read_png(path, image);
    }

    if (has_extension(path, ".ppm")) {
        return read_ppm(path, image);
    }

    fprintf(
        stderr,
        "ana-convert: unsupported image format for '%s' (expected .png or .ppm)\n",
        path);
    return 0;
}

static void palette_init(ANA_Palette* palette)
{
    int i;

    palette->count = 0;
    palette->max_index = -1;
    for (i = 0; i < ANA_CONVERT_MAX_COLORS; i++) {
        palette->present[i] = 0;
        palette->colors[i].r = 0;
        palette->colors[i].g = 0;
        palette->colors[i].b = 0;
        palette->colors[i].a = 255;
    }
}

static int palette_find_color(const ANA_Palette* palette, const ANA_ConvertColor* color)
{
    int i;

    for (i = 0; i < ANA_CONVERT_MAX_COLORS; i++) {
        if (palette->present[i] && colors_equal(&palette->colors[i], color)) {
            return i;
        }
    }

    return -1;
}

static int palette_span(const ANA_Palette* palette)
{
    if (palette->max_index < 0) {
        return 1;
    }

    return palette->max_index + 1;
}

static int palette_add_color(ANA_Palette* palette, const ANA_ConvertColor* color, int limit)
{
    int existing;
    int index;

    existing = palette_find_color(palette, color);
    if (existing >= 0) {
        return existing;
    }

    if (palette->count >= limit || palette->count >= ANA_CONVERT_MAX_COLORS) {
        return -1;
    }

    index = palette->count;
    palette->colors[index] = *color;
    palette->colors[index].a = 255;
    palette->present[index] = 1;
    palette->count++;
    if (index > palette->max_index) {
        palette->max_index = index;
    }

    return index;
}

static int palette_set_color(ANA_Palette* palette, int index, const ANA_ConvertColor* color)
{
    if (index < 0 || index >= ANA_CONVERT_MAX_COLORS || palette->present[index]) {
        return 0;
    }

    palette->colors[index] = *color;
    palette->colors[index].a = 255;
    palette->present[index] = 1;
    palette->count++;
    if (index > palette->max_index) {
        palette->max_index = index;
    }

    return 1;
}

static int color_is_transparent(
    const ANA_ImageOptions* options,
    const ANA_ConvertColor* color)
{
    if (color->a == 0) {
        return 1;
    }

    return options->has_transparent && colors_equal(color, &options->transparent);
}

static int image_has_transparency(
    const ANA_SourceImage* image,
    const ANA_ImageOptions* options)
{
    int pixel_count;
    int i;

    pixel_count = image->width * image->height;
    for (i = 0; i < pixel_count; i++) {
        if (color_is_transparent(options, &image->pixels[i])) {
            return 1;
        }
    }

    return 0;
}

static int build_palette(
    const ANA_SourceImage* image,
    const ANA_ImageOptions* options,
    ANA_Palette* palette)
{
    int pixel_count;
    int i;
    const ANA_ConvertColor* color;

    palette_init(palette);
    pixel_count = image->width * image->height;

    for (i = 0; i < pixel_count; i++) {
        color = &image->pixels[i];
        if (color_is_transparent(options, color)) {
            continue;
        }

        if (palette_add_color(palette, color, options->colors) < 0) {
            fprintf(
                stderr,
                "ana-convert: image uses more than %d non-transparent colors\n",
                options->colors);
            return 0;
        }
    }

    return 1;
}

static int bitplanes_for_color_count(int colors)
{
    if (colors <= 2) {
        return 1;
    }

    if (colors <= 4) {
        return 2;
    }

    if (colors <= 8) {
        return 3;
    }

    return 4;
}

static int write_byte(FILE* file, int value)
{
    return fputc(value & 0xff, file) != EOF;
}

static int write_u16_le(FILE* file, int value)
{
    return write_byte(file, value) && write_byte(file, value >> 8);
}

static int write_u32_le(FILE* file, long value)
{
    return write_byte(file, (int)value) &&
        write_byte(file, (int)(value >> 8)) &&
        write_byte(file, (int)(value >> 16)) &&
        write_byte(file, (int)(value >> 24));
}

static int write_palette_file(const char* path, const ANA_Palette* palette)
{
    FILE* file;
    int i;
    int ok;

    file = fopen(path, "w");
    if (file == NULL) {
        fprintf(stderr, "ana-convert: could not open palette output '%s'\n", path);
        return 0;
    }

    ok = fprintf(file, "ANA_PALETTE 1\n") >= 0;
    for (i = 0; ok && i < ANA_CONVERT_MAX_COLORS; i++) {
        if (palette->present[i]) {
            ok = fprintf(
                file,
                "%d #%02x%02x%02x\n",
                i,
                palette->colors[i].r,
                palette->colors[i].g,
                palette->colors[i].b) >= 0;
        }
    }

    if (fclose(file) != 0) {
        ok = 0;
    }

    if (!ok) {
        fprintf(stderr, "ana-convert: failed while writing palette '%s'\n", path);
        return 0;
    }

    return 1;
}

static int read_palette_file(const char* path, ANA_Palette* palette)
{
    FILE* file;
    char line[256];
    char magic[64];
    char version[64];
    char color_text[64];
    ANA_ConvertColor color;
    int index;
    int line_number;
    int saw_header;

    palette_init(palette);
    file = fopen(path, "r");
    if (file == NULL) {
        fprintf(stderr, "ana-convert: could not open palette '%s'\n", path);
        return 0;
    }

    line_number = 0;
    saw_header = 0;
    while (fgets(line, (int)sizeof(line), file) != NULL) {
        line_number++;
        if (line[0] == '\n' || line[0] == '\r' || line[0] == '\0') {
            continue;
        }

        if (!saw_header) {
            if (sscanf(line, "%63s %63s", magic, version) != 2 ||
                    strcmp(magic, "ANA_PALETTE") != 0 ||
                    strcmp(version, "1") != 0) {
                fprintf(
                    stderr,
                    "ana-convert: invalid palette header in '%s'\n",
                    path);
                fclose(file);
                return 0;
            }
            saw_header = 1;
            continue;
        }

        if (sscanf(line, "%d %63s", &index, color_text) != 2 ||
                index < 0 ||
                index >= ANA_CONVERT_MAX_COLORS ||
                !parse_color_triplet(color_text, &color) ||
                !palette_set_color(palette, index, &color)) {
            fprintf(
                stderr,
                "ana-convert: invalid palette entry in '%s' at line %d\n",
                path,
                line_number);
            fclose(file);
            return 0;
        }
    }

    if (fclose(file) != 0) {
        return 0;
    }

    if (!saw_header || palette->count <= 0) {
        fprintf(stderr, "ana-convert: palette '%s' contains no colors\n", path);
        return 0;
    }

    return 1;
}

static void set_plan_bit(unsigned char* bytes, int row_bytes, int x, int y)
{
    int offset;
    int shift;

    offset = (y * row_bytes) + (x >> 3);
    shift = 7 - (x & 7);
    bytes[offset] = (unsigned char)(bytes[offset] | (1u << shift));
}

static int write_frame(
    FILE* file,
    const char* source_path,
    const ANA_SourceImage* image,
    const ANA_ImageOptions* options,
    const ANA_Palette* palette,
    int frame_x,
    int frame_y,
    int frame_width,
    int frame_height,
    int row_bytes,
    int bitplanes,
    int has_mask)
{
    unsigned char* mask;
    unsigned char* planes;
    int plane_size;
    int x;
    int y;
    int plane;
    int src_x;
    int src_y;
    int color_index;
    int ok;
    const ANA_ConvertColor* color;

    plane_size = row_bytes * frame_height;
    mask = NULL;
    planes = NULL;

    if (has_mask) {
        mask = (unsigned char*)calloc((size_t)plane_size, 1u);
        if (mask == NULL) {
            return 0;
        }
    }

    planes = (unsigned char*)calloc((size_t)(plane_size * bitplanes), 1u);
    if (planes == NULL) {
        free(mask);
        return 0;
    }

    for (y = 0; y < frame_height; y++) {
        for (x = 0; x < frame_width; x++) {
            src_x = frame_x + x;
            src_y = frame_y + y;
            color = &image->pixels[(src_y * image->width) + src_x];

            if (has_mask && color_is_transparent(options, color)) {
                continue;
            }

            if (has_mask) {
                set_plan_bit(mask, row_bytes, x, y);
            }

            color_index = palette_find_color(palette, color);
            if (color_index < 0) {
                fprintf(
                    stderr,
                    "ana-convert: pixel %d,%d in '%s' is not in palette "
                    "(#%02x%02x%02x)\n",
                    src_x,
                    src_y,
                    source_path,
                    color->r,
                    color->g,
                    color->b);
                free(mask);
                free(planes);
                return 0;
            }

            for (plane = 0; plane < bitplanes; plane++) {
                if ((color_index & (1 << plane)) != 0) {
                    set_plan_bit(
                        planes + (plane * plane_size),
                        row_bytes,
                        x,
                        y);
                }
            }
        }
    }

    ok = 1;
    if (has_mask && fwrite(mask, 1u, (size_t)plane_size, file) !=
            (size_t)plane_size) {
        ok = 0;
    }

    if (ok && fwrite(planes, 1u, (size_t)(plane_size * bitplanes), file) !=
            (size_t)(plane_size * bitplanes)) {
        ok = 0;
    }

    free(mask);
    free(planes);

    return ok;
}

static int write_ana_image_contents(
    FILE* file,
    const ANA_SourceImage* image,
    const ANA_ImageOptions* options,
    const ANA_Palette* palette)
{
    int frame_width;
    int frame_height;
    int frames_x;
    int frames_y;
    int frame_count;
    int row_bytes;
    int bitplanes;
    int has_mask;
    int frame_x;
    int frame_y;
    int ok;

    frame_width = options->frame_width > 0 ? options->frame_width : image->width;
    frame_height = options->frame_height > 0 ? options->frame_height : image->height;

    if (frame_width <= 0 || frame_height <= 0 ||
            frame_width > ANA_DEFAULT_WIDTH ||
            frame_height > ANA_DEFAULT_HEIGHT ||
            (image->width % frame_width) != 0 ||
            (image->height % frame_height) != 0) {
        fprintf(stderr, "ana-convert: invalid frame size for source image\n");
        return 0;
    }

    frames_x = image->width / frame_width;
    frames_y = image->height / frame_height;
    frame_count = frames_x * frames_y;

    if (frame_count <= 0 || frame_count > 256) {
        fprintf(stderr, "ana-convert: too many frames\n");
        return 0;
    }

    bitplanes = bitplanes_for_color_count(palette_span(palette));
    row_bytes = (frame_width + 7) / 8;
    has_mask = image_has_transparency(image, options);

    ok = fwrite("ANAIMG01", 1u, 8u, file) == 8u &&
        write_u16_le(file, frame_width) &&
        write_u16_le(file, frame_height) &&
        write_u16_le(file, frame_count) &&
        write_byte(file, bitplanes) &&
        write_byte(file, has_mask ? ANA_CONVERT_IMAGE_FLAG_MASKED : 0) &&
        write_byte(file, 0) &&
        write_byte(file, 0) &&
        write_byte(file, 0) &&
        write_byte(file, 0);

    for (frame_y = 0; ok && frame_y < image->height; frame_y += frame_height) {
        for (frame_x = 0; ok && frame_x < image->width; frame_x += frame_width) {
            ok = write_frame(
                file,
                options->input_path,
                image,
                options,
                palette,
                frame_x,
                frame_y,
                frame_width,
                frame_height,
                row_bytes,
                bitplanes,
                has_mask);
        }
    }

    return ok;
}

static int write_ana_image(
    const char* path,
    const ANA_SourceImage* image,
    const ANA_ImageOptions* options,
    const ANA_Palette* palette)
{
    FILE* file;
    int ok;

    file = fopen(path, "wb");
    if (file == NULL) {
        fprintf(stderr, "ana-convert: could not open output '%s'\n", path);
        return 0;
    }

    ok = write_ana_image_contents(file, image, options, palette);

    if (fclose(file) != 0) {
        ok = 0;
    }

    if (!ok) {
        fprintf(stderr, "ana-convert: failed while writing '%s'\n", path);
        return 0;
    }

    return 1;
}

static int write_ana_font(
    const char* path,
    const ANA_SourceImage* image,
    const ANA_FontOptions* options,
    const ANA_Palette* palette)
{
    ANA_ImageOptions image_options;
    FILE* file;
    int frames_x;
    int frames_y;
    int frame_count;
    int ok;

    if (options->char_width <= 0 ||
            options->char_height <= 0 ||
            options->char_width > ANA_DEFAULT_WIDTH ||
            options->char_height > ANA_DEFAULT_HEIGHT ||
            (image->width % options->char_width) != 0 ||
            (image->height % options->char_height) != 0) {
        fprintf(stderr, "ana-convert: invalid font glyph size\n");
        return 0;
    }

    frames_x = image->width / options->char_width;
    frames_y = image->height / options->char_height;
    frame_count = frames_x * frames_y;

    if (options->char_count <= 0 ||
            options->first_char < 0 ||
            options->first_char + options->char_count > 128 ||
            frame_count < options->char_count) {
        fprintf(stderr, "ana-convert: invalid font character range\n");
        return 0;
    }

    image_options = options->image;
    image_options.frame_width = options->char_width;
    image_options.frame_height = options->char_height;

    file = fopen(path, "wb");
    if (file == NULL) {
        fprintf(stderr, "ana-convert: could not open output '%s'\n", path);
        return 0;
    }

    ok = fwrite("ANAFNT01", 1u, 8u, file) == 8u &&
        write_u16_le(file, options->char_width) &&
        write_u16_le(file, options->char_height) &&
        write_byte(file, options->first_char) &&
        write_byte(file, options->char_count) &&
        write_byte(file, 0) &&
        write_byte(file, 0) &&
        write_ana_image_contents(file, image, &image_options, palette);

    if (fclose(file) != 0) {
        ok = 0;
    }

    if (!ok) {
        fprintf(stderr, "ana-convert: failed while writing '%s'\n", path);
        return 0;
    }

    return 1;
}

static void image_options_init(
    ANA_ImageOptions* options,
    const char* input_path,
    const char* output_path)
{
    options->input_path = input_path;
    options->output_path = output_path;
    options->palette_path = NULL;
    options->colors = ANA_DEFAULT_COLORS;
    options->frame_width = 0;
    options->frame_height = 0;
    options->has_transparent = 0;
    options->transparent.r = 0;
    options->transparent.g = 0;
    options->transparent.b = 0;
    options->transparent.a = 255;
}

static void font_options_init(
    ANA_FontOptions* options,
    const char* input_path,
    const char* output_path)
{
    image_options_init(&options->image, input_path, output_path);
    options->char_width = 0;
    options->char_height = 0;
    options->first_char = 32;
    options->char_count = 0;
}

static int parse_image_flags(
    int argc,
    char** argv,
    int start_index,
    ANA_ImageOptions* options)
{
    int i;

    i = start_index;
    while (i < argc) {
        if (strcmp(argv[i], "--out") == 0 && i + 1 < argc) {
            options->output_path = argv[i + 1];
            i += 2;
        } else if (strcmp(argv[i], "--palette") == 0 && i + 1 < argc) {
            options->palette_path = argv[i + 1];
            i += 2;
        } else if (strcmp(argv[i], "--colors") == 0 && i + 1 < argc) {
            if (!parse_required_positive_int(argv[i + 1], &options->colors) ||
                    options->colors > ANA_CONVERT_MAX_COLORS) {
                return 0;
            }
            i += 2;
        } else if (strcmp(argv[i], "--frame-width") == 0 && i + 1 < argc) {
            if (!parse_required_positive_int(argv[i + 1], &options->frame_width)) {
                return 0;
            }
            i += 2;
        } else if (strcmp(argv[i], "--frame-height") == 0 && i + 1 < argc) {
            if (!parse_required_positive_int(argv[i + 1], &options->frame_height)) {
                return 0;
            }
            i += 2;
        } else if (strcmp(argv[i], "--transparent") == 0 && i + 1 < argc) {
            if (!parse_color_triplet(argv[i + 1], &options->transparent)) {
                return 0;
            }
            options->has_transparent = 1;
            i += 2;
        } else {
            return 0;
        }
    }

    if (options->output_path == NULL) {
        return 0;
    }

    if ((options->frame_width > 0 && options->frame_height == 0) ||
            (options->frame_height > 0 && options->frame_width == 0)) {
        return 0;
    }

    return 1;
}

static int parse_image_args(int argc, char** argv, ANA_ImageOptions* options)
{
    if (argc < 5) {
        return 0;
    }

    image_options_init(options, argv[2], NULL);
    return parse_image_flags(argc, argv, 3, options);
}

static int parse_font_flags(
    int argc,
    char** argv,
    int start_index,
    ANA_FontOptions* options)
{
    int i;

    i = start_index;
    while (i < argc) {
        if (strcmp(argv[i], "--out") == 0 && i + 1 < argc) {
            options->image.output_path = argv[i + 1];
            i += 2;
        } else if (strcmp(argv[i], "--palette") == 0 && i + 1 < argc) {
            options->image.palette_path = argv[i + 1];
            i += 2;
        } else if (strcmp(argv[i], "--colors") == 0 && i + 1 < argc) {
            if (!parse_required_positive_int(argv[i + 1], &options->image.colors) ||
                    options->image.colors > ANA_CONVERT_MAX_COLORS) {
                return 0;
            }
            i += 2;
        } else if (strcmp(argv[i], "--char-width") == 0 && i + 1 < argc) {
            if (!parse_required_positive_int(argv[i + 1], &options->char_width)) {
                return 0;
            }
            i += 2;
        } else if (strcmp(argv[i], "--char-height") == 0 && i + 1 < argc) {
            if (!parse_required_positive_int(argv[i + 1], &options->char_height)) {
                return 0;
            }
            i += 2;
        } else if (strcmp(argv[i], "--first-char") == 0 && i + 1 < argc) {
            if (!parse_int(argv[i + 1], &options->first_char)) {
                return 0;
            }
            i += 2;
        } else if (strcmp(argv[i], "--chars") == 0 && i + 1 < argc) {
            if (!parse_required_positive_int(argv[i + 1], &options->char_count)) {
                return 0;
            }
            i += 2;
        } else if (strcmp(argv[i], "--transparent") == 0 && i + 1 < argc) {
            if (!parse_color_triplet(argv[i + 1], &options->image.transparent)) {
                return 0;
            }
            options->image.has_transparent = 1;
            i += 2;
        } else {
            return 0;
        }
    }

    return options->image.output_path != NULL &&
        options->char_width > 0 &&
        options->char_height > 0 &&
        options->char_count > 0;
}

static int parse_font_args(int argc, char** argv, ANA_FontOptions* options)
{
    if (argc < 9) {
        return 0;
    }

    font_options_init(options, argv[2], NULL);
    return parse_font_flags(argc, argv, 3, options);
}

static int run_image_command(int argc, char** argv)
{
    ANA_ImageOptions options;
    ANA_SourceImage image;
    ANA_Palette palette;
    int ok;

    image.width = 0;
    image.height = 0;
    image.pixels = NULL;

    if (!parse_image_args(argc, argv, &options)) {
        print_usage();
        return 1;
    }

    ok = read_source_image(options.input_path, &image);
    if (ok) {
        if (options.palette_path != NULL) {
            ok = read_palette_file(options.palette_path, &palette);
        } else {
            ok = build_palette(&image, &options, &palette);
        }
    }
    if (ok) {
        ok = write_ana_image(options.output_path, &image, &options, &palette);
    }

    source_image_free(&image);

    if (!ok) {
        return 1;
    }

    printf(
        "Wrote %s from %s (%d colors)\n",
        options.output_path,
        options.input_path,
        palette.count);

    return 0;
}

static int run_font_options(const ANA_FontOptions* options)
{
    ANA_SourceImage image;
    ANA_Palette palette;
    int ok;

    image.width = 0;
    image.height = 0;
    image.pixels = NULL;

    ok = read_source_image(options->image.input_path, &image);
    if (ok) {
        if (options->image.palette_path != NULL) {
            ok = read_palette_file(options->image.palette_path, &palette);
        } else {
            ok = build_palette(&image, &options->image, &palette);
        }
    }
    if (ok) {
        ok = write_ana_font(options->image.output_path, &image, options, &palette);
    }

    source_image_free(&image);

    if (!ok) {
        return 1;
    }

    printf(
        "Wrote %s from %s (%d colors)\n",
        options->image.output_path,
        options->image.input_path,
        palette.count);

    return 0;
}

static int run_font_command(int argc, char** argv)
{
    ANA_FontOptions options;

    if (!parse_font_args(argc, argv, &options)) {
        print_usage();
        return 1;
    }

    return run_font_options(&options);
}

static int parse_palette_args(int argc, char** argv, ANA_PaletteOptions* options)
{
    int i;

    if (argc < 5) {
        return 0;
    }

    options->input_path = argv[2];
    options->output_path = NULL;
    options->colors = ANA_DEFAULT_COLORS;

    i = 3;
    while (i < argc) {
        if (strcmp(argv[i], "--out") == 0 && i + 1 < argc) {
            options->output_path = argv[i + 1];
            i += 2;
        } else if (strcmp(argv[i], "--colors") == 0 && i + 1 < argc) {
            if (!parse_required_positive_int(argv[i + 1], &options->colors) ||
                    options->colors > ANA_CONVERT_MAX_COLORS) {
                return 0;
            }
            i += 2;
        } else {
            return 0;
        }
    }

    return options->output_path != NULL;
}

static int run_palette_options(const ANA_PaletteOptions* options)
{
    ANA_SourceImage image;
    ANA_ImageOptions image_options;
    ANA_Palette palette;
    int ok;

    image.width = 0;
    image.height = 0;
    image.pixels = NULL;

    image_options_init(&image_options, options->input_path, NULL);
    image_options.colors = options->colors;

    ok = read_source_image(options->input_path, &image) &&
        build_palette(&image, &image_options, &palette) &&
        write_palette_file(options->output_path, &palette);

    source_image_free(&image);
    if (!ok) {
        return 1;
    }

    printf(
        "Wrote %s from %s (%d colors)\n",
        options->output_path,
        options->input_path,
        palette.count);
    return 0;
}

static int run_palette_command(int argc, char** argv)
{
    ANA_PaletteOptions options;

    if (!parse_palette_args(argc, argv, &options)) {
        print_usage();
        return 1;
    }

    return run_palette_options(&options);
}

static int parse_build_args(
    int argc,
    char** argv,
    const char** manifest_path,
    const char** output_dir)
{
    int i;

    if (argc < 5) {
        return 0;
    }

    *manifest_path = argv[2];
    *output_dir = NULL;

    i = 3;
    while (i < argc) {
        if (strcmp(argv[i], "--out") == 0 && i + 1 < argc) {
            *output_dir = argv[i + 1];
            i += 2;
        } else {
            return 0;
        }
    }

    return *output_dir != NULL;
}

static int tokenize_line(char* line, char** tokens, int max_tokens)
{
    char* token;
    int count;

    count = 0;
    token = strtok(line, " \t\r\n");
    while (token != NULL && count < max_tokens) {
        tokens[count] = token;
        count++;
        token = strtok(NULL, " \t\r\n");
    }

    return count;
}

static void sound_options_init(
    ANA_SoundOptions* options,
    const char* input_path,
    const char* output_path)
{
    options->input_path = input_path;
    options->output_path = output_path;
    options->wave = ANA_SOUND_WAVE_NONE;
    options->sample_rate = ANA_CONVERT_SOUND_DEFAULT_RATE;
    options->sample_count = 0;
    options->volume = 64;
    options->priority = 1;
    options->attack_samples = 0;
    options->release_samples = 0;
    options->amp_start = 32;
    options->amp_end = 32;
    options->period_start = 8;
    options->period_end = 8;
    options->seed = 0x2468ace1UL;
}

static int parse_sound_wave(const char* text, ANA_SoundWave* wave)
{
    if (strcmp(text, "square") == 0) {
        *wave = ANA_SOUND_WAVE_SQUARE;
        return 1;
    }

    if (strcmp(text, "noise") == 0) {
        *wave = ANA_SOUND_WAVE_NOISE;
        return 1;
    }

    return 0;
}

static int parse_sound_recipe_line(
    ANA_SoundOptions* options,
    char** tokens,
    int token_count,
    int line_number)
{
    if (strcmp(tokens[0], "wave") == 0 && token_count == 2) {
        if (!parse_sound_wave(tokens[1], &options->wave)) {
            fprintf(stderr, "ana-convert: invalid sound wave at line %d\n", line_number);
            return 0;
        }
        return 1;
    }

    if (strcmp(tokens[0], "rate") == 0 && token_count == 2) {
        if (!parse_required_positive_int(tokens[1], &options->sample_rate)) {
            fprintf(stderr, "ana-convert: invalid sound rate at line %d\n", line_number);
            return 0;
        }
        return 1;
    }

    if (strcmp(tokens[0], "samples") == 0 && token_count == 2) {
        if (!parse_required_positive_int(tokens[1], &options->sample_count)) {
            fprintf(stderr, "ana-convert: invalid sound sample count at line %d\n", line_number);
            return 0;
        }
        return 1;
    }

    if (strcmp(tokens[0], "volume") == 0 && token_count == 2) {
        if (!parse_int(tokens[1], &options->volume)) {
            fprintf(stderr, "ana-convert: invalid sound volume at line %d\n", line_number);
            return 0;
        }
        return 1;
    }

    if (strcmp(tokens[0], "priority") == 0 && token_count == 2) {
        if (!parse_int(tokens[1], &options->priority)) {
            fprintf(stderr, "ana-convert: invalid sound priority at line %d\n", line_number);
            return 0;
        }
        return 1;
    }

    if (strcmp(tokens[0], "attack") == 0 && token_count == 2) {
        if (!parse_int(tokens[1], &options->attack_samples)) {
            fprintf(stderr, "ana-convert: invalid sound attack at line %d\n", line_number);
            return 0;
        }
        return 1;
    }

    if (strcmp(tokens[0], "release") == 0 && token_count == 2) {
        if (!parse_int(tokens[1], &options->release_samples)) {
            fprintf(stderr, "ana-convert: invalid sound release at line %d\n", line_number);
            return 0;
        }
        return 1;
    }

    if (strcmp(tokens[0], "amplitude") == 0 && token_count == 3) {
        if (!parse_int(tokens[1], &options->amp_start) ||
                !parse_int(tokens[2], &options->amp_end)) {
            fprintf(stderr, "ana-convert: invalid sound amplitude at line %d\n", line_number);
            return 0;
        }
        return 1;
    }

    if (strcmp(tokens[0], "period") == 0 && token_count == 3) {
        if (!parse_required_positive_int(tokens[1], &options->period_start) ||
                !parse_required_positive_int(tokens[2], &options->period_end)) {
            fprintf(stderr, "ana-convert: invalid sound period at line %d\n", line_number);
            return 0;
        }
        return 1;
    }

    fprintf(stderr, "ana-convert: invalid sound recipe at line %d\n", line_number);
    return 0;
}

static int read_sound_recipe(const char* path, ANA_SoundOptions* options)
{
    FILE* file;
    char line[256];
    char* tokens[ANA_CONVERT_MAX_TOKENS];
    int line_number;
    int token_count;
    int saw_header;
    int ok;

    file = fopen(path, "r");
    if (file == NULL) {
        fprintf(stderr, "ana-convert: could not open sound recipe '%s'\n", path);
        return 0;
    }

    line_number = 0;
    saw_header = 0;
    ok = 1;
    while (ok && fgets(line, (int)sizeof(line), file) != NULL) {
        line_number++;
        token_count = tokenize_line(line, tokens, ANA_CONVERT_MAX_TOKENS);
        if (token_count == 0 || tokens[0][0] == '#') {
            continue;
        }

        if (!saw_header) {
            if (token_count != 2 ||
                    strcmp(tokens[0], "ANA_SOUND") != 0 ||
                    strcmp(tokens[1], "1") != 0) {
                fprintf(stderr, "ana-convert: invalid sound recipe header in '%s'\n", path);
                ok = 0;
            }
            saw_header = 1;
            continue;
        }

        ok = parse_sound_recipe_line(options, tokens, token_count, line_number);
    }

    if (fclose(file) != 0) {
        ok = 0;
    }

    if (ok && !saw_header) {
        fprintf(stderr, "ana-convert: sound recipe '%s' is empty\n", path);
        ok = 0;
    }

    return ok;
}

static int sound_lerp(int start, int end, int index, int count)
{
    if (count <= 1) {
        return start;
    }

    return start + (((end - start) * index) / (count - 1));
}

static int sound_enveloped_sample(
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

static unsigned char sound_signed_sample(int sample)
{
    if (sample > 127) {
        sample = 127;
    }

    if (sample < -128) {
        sample = -128;
    }

    return (unsigned char)(sample & 0xff);
}

static int read_exact(FILE* file, unsigned char* bytes, size_t size)
{
    return fread(bytes, 1u, size, file) == size;
}

static int read_u16_le_bytes(const unsigned char* bytes)
{
    return (int)bytes[0] | ((int)bytes[1] << 8);
}

static unsigned long read_u32_le_bytes(const unsigned char* bytes)
{
    return (unsigned long)bytes[0] |
        ((unsigned long)bytes[1] << 8) |
        ((unsigned long)bytes[2] << 16) |
        ((unsigned long)bytes[3] << 24);
}

static int skip_wav_chunk(FILE* file, unsigned long size)
{
    unsigned long padded_size;

    padded_size = size + (size & 1UL);
    if (padded_size == 0UL) {
        return 1;
    }

    return fseek(file, (long)padded_size, SEEK_CUR) == 0;
}

static void wav_source_free(ANA_WavSource* wav)
{
    free(wav->data);
    wav->data = NULL;
    wav->data_size = 0;
    wav->frame_count = 0;
}

static int read_wav_source(const char* path, ANA_WavSource* wav)
{
    FILE* file;
    unsigned char header[12];
    unsigned char chunk_header[8];
    int saw_fmt;
    int saw_data;
    int ok;

    wav->sample_rate = 0;
    wav->channels = 0;
    wav->bits_per_sample = 0;
    wav->frame_count = 0;
    wav->data_size = 0;
    wav->data = NULL;

    file = fopen(path, "rb");
    if (file == NULL) {
        fprintf(stderr, "ana-convert: could not open WAV '%s'\n", path);
        return 0;
    }

    ok = read_exact(file, header, sizeof(header)) &&
        memcmp(header, "RIFF", 4u) == 0 &&
        memcmp(header + 8, "WAVE", 4u) == 0;
    if (!ok) {
        fprintf(stderr, "ana-convert: invalid WAV header in '%s'\n", path);
        fclose(file);
        return 0;
    }

    saw_fmt = 0;
    saw_data = 0;
    while (read_exact(file, chunk_header, sizeof(chunk_header))) {
        unsigned long chunk_size;

        chunk_size = read_u32_le_bytes(chunk_header + 4);
        if (memcmp(chunk_header, "fmt ", 4u) == 0) {
            unsigned char* fmt_data;
            int audio_format;

            if (chunk_size < 16UL) {
                ok = 0;
                break;
            }

            fmt_data = (unsigned char*)malloc((size_t)chunk_size);
            if (fmt_data == NULL) {
                fprintf(stderr, "ana-convert: out of memory while reading '%s'\n", path);
                ok = 0;
                break;
            }

            ok = read_exact(file, fmt_data, (size_t)chunk_size);
            if (!ok) {
                free(fmt_data);
                break;
            }

            audio_format = read_u16_le_bytes(fmt_data);
            wav->channels = read_u16_le_bytes(fmt_data + 2);
            wav->sample_rate = (int)read_u32_le_bytes(fmt_data + 4);
            wav->bits_per_sample = read_u16_le_bytes(fmt_data + 14);
            free(fmt_data);

            if (audio_format != 1 ||
                    (wav->channels != 1 && wav->channels != 2) ||
                    (wav->bits_per_sample != 8 && wav->bits_per_sample != 16) ||
                    wav->sample_rate <= 0) {
                fprintf(
                    stderr,
                    "ana-convert: unsupported WAV format in '%s' "
                    "(expected PCM mono/stereo 8/16-bit)\n",
                    path);
                ok = 0;
                break;
            }

            saw_fmt = 1;
            if ((chunk_size & 1UL) != 0UL && fgetc(file) == EOF) {
                ok = 0;
                break;
            }
        } else if (memcmp(chunk_header, "data", 4u) == 0) {
            if (chunk_size > (unsigned long)ANA_CONVERT_SOUND_MAX_SAMPLES * 8UL) {
                fprintf(stderr, "ana-convert: WAV data too large in '%s'\n", path);
                ok = 0;
                break;
            }

            wav->data = (unsigned char*)malloc((size_t)chunk_size);
            if (wav->data == NULL) {
                fprintf(stderr, "ana-convert: out of memory while reading '%s'\n", path);
                ok = 0;
                break;
            }

            wav->data_size = (long)chunk_size;
            ok = read_exact(file, wav->data, (size_t)chunk_size);
            if (!ok) {
                break;
            }

            saw_data = 1;
            if ((chunk_size & 1UL) != 0UL && fgetc(file) == EOF) {
                ok = 0;
                break;
            }
        } else if (!skip_wav_chunk(file, chunk_size)) {
            ok = 0;
            break;
        }
    }

    if (ferror(file)) {
        ok = 0;
    }

    if (fclose(file) != 0) {
        ok = 0;
    }

    if (!ok || !saw_fmt || !saw_data) {
        fprintf(stderr, "ana-convert: could not read WAV '%s'\n", path);
        wav_source_free(wav);
        return 0;
    }

    {
        long bytes_per_frame;

        bytes_per_frame = (long)wav->channels * (long)(wav->bits_per_sample / 8);
        if (bytes_per_frame <= 0 || wav->data_size < bytes_per_frame) {
            fprintf(stderr, "ana-convert: WAV contains no sample frames in '%s'\n", path);
            wav_source_free(wav);
            return 0;
        }

        wav->frame_count = wav->data_size / bytes_per_frame;
    }

    return 1;
}

static int wav_frame_sample_16(const ANA_WavSource* wav, long frame)
{
    long offset;
    long sum;
    int bytes_per_sample;
    int channel;

    bytes_per_sample = wav->bits_per_sample / 8;
    offset = frame * (long)wav->channels * (long)bytes_per_sample;
    sum = 0;

    for (channel = 0; channel < wav->channels; ++channel) {
        const unsigned char* sample;
        int value;

        sample = wav->data + offset + (long)channel * (long)bytes_per_sample;
        if (wav->bits_per_sample == 8) {
            value = ((int)sample[0] - 128) << 8;
        } else {
            value = (int)sample[0] | ((int)sample[1] << 8);
            if (value >= 32768) {
                value -= 65536;
            }
        }

        sum += value;
    }

    return (int)(sum / wav->channels);
}

static int write_ana_sound_from_wav(
    const char* path,
    const ANA_SoundOptions* options,
    const ANA_WavSource* wav)
{
    FILE* file;
    long sample_count;
    long i;
    int ok;

    if (options->sample_rate < ANA_CONVERT_SOUND_MIN_RATE ||
            options->sample_rate > ANA_CONVERT_SOUND_MAX_RATE ||
            options->volume < 0 ||
            options->volume > 64 ||
            options->priority < 0 ||
            options->priority > 127) {
        fprintf(stderr, "ana-convert: invalid WAV sound options in '%s'\n", options->input_path);
        return 0;
    }

    sample_count = (wav->frame_count * (long)options->sample_rate) /
        (long)wav->sample_rate;
    if (sample_count <= 0 || sample_count > ANA_CONVERT_SOUND_MAX_SAMPLES) {
        fprintf(stderr, "ana-convert: converted WAV sample count is invalid in '%s'\n", options->input_path);
        return 0;
    }

    file = fopen(path, "wb");
    if (file == NULL) {
        fprintf(stderr, "ana-convert: could not open output '%s'\n", path);
        return 0;
    }

    ok = fwrite("ANASND01", 1u, 8u, file) == 8u &&
        write_u16_le(file, options->sample_rate) &&
        write_u32_le(file, sample_count) &&
        write_byte(file, options->volume) &&
        write_byte(file, options->priority) &&
        write_byte(file, ANA_CONVERT_SOUND_FLAG_SIGNED_8BIT) &&
        write_byte(file, 0) &&
        write_byte(file, 0) &&
        write_byte(file, 0);

    for (i = 0; ok && i < sample_count; ++i) {
        long source_frame;
        int sample;

        source_frame = (i * (long)wav->sample_rate) / (long)options->sample_rate;
        if (source_frame >= wav->frame_count) {
            source_frame = wav->frame_count - 1;
        }

        sample = wav_frame_sample_16(wav, source_frame) / 256;
        ok = write_byte(file, sound_signed_sample(sample));
    }

    if (fclose(file) != 0) {
        ok = 0;
    }

    if (!ok) {
        fprintf(stderr, "ana-convert: failed while writing '%s'\n", path);
        return 0;
    }

    return 1;
}

static int sound_recipe_is_valid(const ANA_SoundOptions* options)
{
    return options->wave != ANA_SOUND_WAVE_NONE &&
        options->sample_rate >= ANA_CONVERT_SOUND_MIN_RATE &&
        options->sample_rate <= ANA_CONVERT_SOUND_MAX_RATE &&
        options->sample_count > 0 &&
        options->sample_count <= ANA_CONVERT_SOUND_MAX_SAMPLES &&
        options->volume >= 0 &&
        options->volume <= 64 &&
        options->priority >= 0 &&
        options->priority <= 127 &&
        options->attack_samples >= 0 &&
        options->release_samples >= 0 &&
        options->amp_start >= 0 &&
        options->amp_start <= 127 &&
        options->amp_end >= 0 &&
        options->amp_end <= 127 &&
        options->period_start > 0 &&
        options->period_end > 0;
}

static int sound_sample_at(
    const ANA_SoundOptions* options,
    int index,
    unsigned long* seed)
{
    int amp;
    int period;
    int sample;

    amp = sound_lerp(
        options->amp_start,
        options->amp_end,
        index,
        options->sample_count);
    period = sound_lerp(
        options->period_start,
        options->period_end,
        index,
        options->sample_count);
    if (period < 1) {
        period = 1;
    }

    if (options->wave == ANA_SOUND_WAVE_NOISE) {
        *seed = (*seed * 1103515245UL) + 12345UL;
        sample = (int)((*seed >> 16) & 0x7fu) - 64;
        sample = (sample * amp) / 64;
    } else {
        sample = ((index / period) & 1) ? amp : -amp;
    }

    return sound_enveloped_sample(
        sample,
        index,
        options->sample_count,
        options->attack_samples,
        options->release_samples);
}

static int write_ana_sound(const char* path, const ANA_SoundOptions* options)
{
    FILE* file;
    unsigned long seed;
    int i;
    int ok;

    if (!sound_recipe_is_valid(options)) {
        fprintf(stderr, "ana-convert: invalid sound recipe in '%s'\n", options->input_path);
        return 0;
    }

    file = fopen(path, "wb");
    if (file == NULL) {
        fprintf(stderr, "ana-convert: could not open output '%s'\n", path);
        return 0;
    }

    ok = fwrite("ANASND01", 1u, 8u, file) == 8u &&
        write_u16_le(file, options->sample_rate) &&
        write_u32_le(file, options->sample_count) &&
        write_byte(file, options->volume) &&
        write_byte(file, options->priority) &&
        write_byte(file, ANA_CONVERT_SOUND_FLAG_SIGNED_8BIT) &&
        write_byte(file, 0) &&
        write_byte(file, 0) &&
        write_byte(file, 0);

    seed = options->seed;
    for (i = 0; ok && i < options->sample_count; i++) {
        ok = write_byte(file, sound_signed_sample(sound_sample_at(options, i, &seed)));
    }

    if (fclose(file) != 0) {
        ok = 0;
    }

    if (!ok) {
        fprintf(stderr, "ana-convert: failed while writing '%s'\n", path);
        return 0;
    }

    return 1;
}

static int run_sound_options(ANA_SoundOptions* options)
{
    if (has_extension(options->input_path, ".wav")) {
        ANA_WavSource wav;
        int result;

        if (!read_wav_source(options->input_path, &wav)) {
            return 1;
        }

        result = write_ana_sound_from_wav(options->output_path, options, &wav) ? 0 : 1;
        wav_source_free(&wav);
        if (result != 0) {
            return 1;
        }

        printf("Wrote %s from %s\n", options->output_path, options->input_path);
        return 0;
    }

    if (!read_sound_recipe(options->input_path, options)) {
        return 1;
    }

    if (!write_ana_sound(options->output_path, options)) {
        return 1;
    }

    printf("Wrote %s from %s\n", options->output_path, options->input_path);
    return 0;
}

static int parse_sound_flags(
    int argc,
    char** argv,
    int start,
    ANA_SoundOptions* options,
    int allow_out)
{
    int i;

    i = start;
    while (i < argc) {
        if (allow_out && strcmp(argv[i], "--out") == 0 && i + 1 < argc) {
            options->output_path = argv[i + 1];
            i += 2;
        } else if (strcmp(argv[i], "--rate") == 0 && i + 1 < argc) {
            if (!parse_required_positive_int(argv[i + 1], &options->sample_rate)) {
                return 0;
            }
            i += 2;
        } else if (strcmp(argv[i], "--volume") == 0 && i + 1 < argc) {
            if (!parse_int(argv[i + 1], &options->volume)) {
                return 0;
            }
            i += 2;
        } else if (strcmp(argv[i], "--priority") == 0 && i + 1 < argc) {
            if (!parse_int(argv[i + 1], &options->priority)) {
                return 0;
            }
            i += 2;
        } else {
            return 0;
        }
    }

    return 1;
}

static int parse_sound_args(int argc, char** argv, ANA_SoundOptions* options)
{
    if (argc < 5) {
        return 0;
    }

    sound_options_init(options, argv[2], NULL);

    if (!parse_sound_flags(argc, argv, 3, options, 1)) {
        return 0;
    }

    return options->output_path != NULL;
}

static int run_sound_command(int argc, char** argv)
{
    ANA_SoundOptions options;

    if (!parse_sound_args(argc, argv, &options)) {
        print_usage();
        return 1;
    }

    return run_sound_options(&options);
}

static const char* manifest_find_palette(
    const ANA_ManifestPalette* palettes,
    int palette_count,
    const char* name)
{
    int i;

    for (i = 0; i < palette_count; i++) {
        if (strcmp(palettes[i].name, name) == 0) {
            return palettes[i].path;
        }
    }

    return NULL;
}

static void manifest_free_palettes(
    ANA_ManifestPalette* palettes,
    int palette_count)
{
    int i;

    for (i = 0; i < palette_count; i++) {
        free(palettes[i].name);
        free(palettes[i].path);
        palettes[i].name = NULL;
        palettes[i].path = NULL;
    }
}

static int run_manifest_palette(
    char** tokens,
    int token_count,
    const char* manifest_dir,
    const char* output_dir,
    ANA_ManifestPalette* palettes,
    int* palette_count,
    int line_number)
{
    ANA_PaletteOptions options;
    char* input_path;
    char* output_path;
    int arg_count;
    int result;

    if (token_count < 4 ||
            *palette_count >= ANA_CONVERT_MAX_MANIFEST_PALETTES) {
        fprintf(stderr, "ana-convert: invalid palette entry at line %d\n", line_number);
        return 0;
    }

    input_path = path_join(manifest_dir, tokens[2]);
    output_path = asset_output_path(output_dir, tokens[1], ".anapal");
    if (input_path == NULL || output_path == NULL) {
        free(input_path);
        free(output_path);
        fprintf(stderr, "ana-convert: out of memory while reading manifest\n");
        return 0;
    }

    options.input_path = input_path;
    options.output_path = output_path;
    options.colors = ANA_DEFAULT_COLORS;

    arg_count = 3;
    while (arg_count < token_count) {
        if (strcmp(tokens[arg_count], "--colors") == 0 &&
                arg_count + 1 < token_count) {
            if (!parse_required_positive_int(tokens[arg_count + 1], &options.colors) ||
                    options.colors > ANA_CONVERT_MAX_COLORS) {
                fprintf(
                    stderr,
                    "ana-convert: invalid palette color count at line %d\n",
                    line_number);
                free(input_path);
                free(output_path);
                return 0;
            }
            arg_count += 2;
        } else {
            fprintf(
                stderr,
                "ana-convert: unsupported palette option at line %d\n",
                line_number);
            free(input_path);
            free(output_path);
            return 0;
        }
    }

    result = run_palette_options(&options);
    if (result != 0) {
        free(input_path);
        free(output_path);
        return 0;
    }

    palettes[*palette_count].name = copy_string(tokens[1]);
    palettes[*palette_count].path = output_path;
    if (palettes[*palette_count].name == NULL) {
        free(input_path);
        free(output_path);
        fprintf(stderr, "ana-convert: out of memory while reading manifest\n");
        return 0;
    }

    (*palette_count)++;
    free(input_path);
    return 1;
}

static int run_manifest_image(
    char** tokens,
    int token_count,
    const char* manifest_dir,
    const char* output_dir,
    const ANA_ManifestPalette* palettes,
    int palette_count,
    int line_number)
{
    ANA_ImageOptions options;
    char* input_path;
    char* output_path;
    char* palette_path;
    const char* manifest_palette_path;
    int result;

    if (token_count < 4) {
        fprintf(stderr, "ana-convert: invalid image entry at line %d\n", line_number);
        return 0;
    }

    input_path = path_join(manifest_dir, tokens[2]);
    output_path = asset_output_path(output_dir, tokens[1], ".anaimg");
    if (input_path == NULL || output_path == NULL) {
        free(input_path);
        free(output_path);
        fprintf(stderr, "ana-convert: out of memory while reading manifest\n");
        return 0;
    }

    image_options_init(&options, input_path, output_path);
    if (!parse_image_flags(token_count, tokens, 3, &options)) {
        fprintf(stderr, "ana-convert: invalid image options at line %d\n", line_number);
        free(input_path);
        free(output_path);
        return 0;
    }

    palette_path = NULL;
    if (options.palette_path != NULL) {
        manifest_palette_path = manifest_find_palette(
            palettes,
            palette_count,
            options.palette_path);
        if (manifest_palette_path != NULL) {
            options.palette_path = manifest_palette_path;
        } else if (!has_extension(options.palette_path, ".anapal")) {
            fprintf(
                stderr,
                "ana-convert: unknown palette '%s' at line %d\n",
                options.palette_path,
                line_number);
            free(input_path);
            free(output_path);
            return 0;
        } else {
            palette_path = path_join(manifest_dir, options.palette_path);
            if (palette_path == NULL) {
                free(input_path);
                free(output_path);
                fprintf(stderr, "ana-convert: out of memory while reading manifest\n");
                return 0;
            }
            options.palette_path = palette_path;
        }
    }

    {
        ANA_SourceImage image;
        ANA_Palette palette;
        int ok;

        image.width = 0;
        image.height = 0;
        image.pixels = NULL;
        ok = read_source_image(options.input_path, &image);
        if (ok) {
            if (options.palette_path != NULL) {
                ok = read_palette_file(options.palette_path, &palette);
            } else {
                ok = build_palette(&image, &options, &palette);
            }
        }
        if (ok) {
            ok = write_ana_image(options.output_path, &image, &options, &palette);
        }
        source_image_free(&image);
        result = ok ? 0 : 1;
    }

    if (result == 0) {
        printf("Wrote %s from %s\n", output_path, options.input_path);
    }
    free(palette_path);
    free(input_path);
    free(output_path);
    return result == 0;
}

static int run_manifest_font(
    char** tokens,
    int token_count,
    const char* manifest_dir,
    const char* output_dir,
    const ANA_ManifestPalette* palettes,
    int palette_count,
    int line_number)
{
    ANA_FontOptions options;
    char* input_path;
    char* output_path;
    char* palette_path;
    const char* manifest_palette_path;
    int result;

    if (token_count < 4) {
        fprintf(stderr, "ana-convert: invalid font entry at line %d\n", line_number);
        return 0;
    }

    input_path = path_join(manifest_dir, tokens[2]);
    output_path = asset_output_path(output_dir, tokens[1], ".anafnt");
    if (input_path == NULL || output_path == NULL) {
        free(input_path);
        free(output_path);
        fprintf(stderr, "ana-convert: out of memory while reading manifest\n");
        return 0;
    }

    font_options_init(&options, input_path, output_path);
    if (!parse_font_flags(token_count, tokens, 3, &options)) {
        fprintf(stderr, "ana-convert: invalid font options at line %d\n", line_number);
        free(input_path);
        free(output_path);
        return 0;
    }

    palette_path = NULL;
    if (options.image.palette_path != NULL) {
        manifest_palette_path = manifest_find_palette(
            palettes,
            palette_count,
            options.image.palette_path);
        if (manifest_palette_path != NULL) {
            options.image.palette_path = manifest_palette_path;
        } else if (!has_extension(options.image.palette_path, ".anapal")) {
            fprintf(
                stderr,
                "ana-convert: unknown palette '%s' at line %d\n",
                options.image.palette_path,
                line_number);
            free(input_path);
            free(output_path);
            return 0;
        } else {
            palette_path = path_join(manifest_dir, options.image.palette_path);
            if (palette_path == NULL) {
                free(input_path);
                free(output_path);
                fprintf(stderr, "ana-convert: out of memory while reading manifest\n");
                return 0;
            }
            options.image.palette_path = palette_path;
        }
    }

    result = run_font_options(&options);

    free(palette_path);
    free(input_path);
    free(output_path);
    return result == 0;
}

static int run_manifest_sound(
    char** tokens,
    int token_count,
    const char* manifest_dir,
    const char* output_dir,
    int line_number)
{
    ANA_SoundOptions options;
    char* input_path;
    char* output_path;
    int result;

    if (token_count < 3) {
        fprintf(stderr, "ana-convert: invalid sound entry at line %d\n", line_number);
        return 0;
    }

    input_path = path_join(manifest_dir, tokens[2]);
    output_path = asset_output_path(output_dir, tokens[1], ".anasnd");
    if (input_path == NULL || output_path == NULL) {
        free(input_path);
        free(output_path);
        fprintf(stderr, "ana-convert: out of memory while reading manifest\n");
        return 0;
    }

    sound_options_init(&options, input_path, output_path);
    if (!parse_sound_flags(token_count, tokens, 3, &options, 0)) {
        fprintf(stderr, "ana-convert: invalid sound options at line %d\n", line_number);
        free(input_path);
        free(output_path);
        return 0;
    }

    result = run_sound_options(&options);

    free(input_path);
    free(output_path);
    return result == 0;
}

static int run_manifest_music(
    char** tokens,
    int token_count,
    const char* manifest_dir,
    const char* output_dir,
    int line_number)
{
    char* input_path;
    char* output_path;
    int ok;

    if (token_count != 3) {
        fprintf(stderr, "ana-convert: invalid music entry at line %d\n", line_number);
        return 0;
    }

    if (!has_extension(tokens[2], ".mod")) {
        fprintf(
            stderr,
            "ana-convert: unsupported music format at line %d (expected .mod)\n",
            line_number);
        return 0;
    }

    input_path = path_join(manifest_dir, tokens[2]);
    output_path = asset_output_path(output_dir, tokens[1], ".mod");
    if (input_path == NULL || output_path == NULL) {
        free(input_path);
        free(output_path);
        fprintf(stderr, "ana-convert: out of memory while reading manifest\n");
        return 0;
    }

    ok = copy_binary_file(input_path, output_path);
    if (ok) {
        printf("Wrote %s from %s\n", output_path, input_path);
    }

    free(input_path);
    free(output_path);
    return ok;
}

static int run_build_command(int argc, char** argv)
{
    const char* manifest_path;
    const char* output_dir;
    FILE* file;
    char* manifest_dir;
    char line[512];
    char original_line[512];
    char* tokens[ANA_CONVERT_MAX_TOKENS];
    ANA_ManifestPalette palettes[ANA_CONVERT_MAX_MANIFEST_PALETTES];
    int palette_count;
    int line_number;
    int token_count;
    int saw_header;
    int ok;
    int i;

    if (!parse_build_args(argc, argv, &manifest_path, &output_dir)) {
        print_usage();
        return 1;
    }

    if (!ensure_dir(output_dir)) {
        return 1;
    }

    manifest_dir = path_dirname(manifest_path);
    if (manifest_dir == NULL) {
        fprintf(stderr, "ana-convert: out of memory while reading manifest\n");
        return 1;
    }

    for (i = 0; i < ANA_CONVERT_MAX_MANIFEST_PALETTES; i++) {
        palettes[i].name = NULL;
        palettes[i].path = NULL;
    }

    file = fopen(manifest_path, "r");
    if (file == NULL) {
        fprintf(stderr, "ana-convert: could not open manifest '%s'\n", manifest_path);
        free(manifest_dir);
        return 1;
    }

    palette_count = 0;
    line_number = 0;
    saw_header = 0;
    ok = 1;
    while (ok && fgets(line, (int)sizeof(line), file) != NULL) {
        line_number++;
        strcpy(original_line, line);
        token_count = tokenize_line(line, tokens, ANA_CONVERT_MAX_TOKENS);
        if (token_count == 0 || tokens[0][0] == '#') {
            continue;
        }

        if (!saw_header) {
            if (token_count != 2 ||
                    strcmp(tokens[0], "ANA_ASSETS") != 0 ||
                    strcmp(tokens[1], "1") != 0) {
                fprintf(
                    stderr,
                    "ana-convert: invalid manifest header in '%s'\n",
                    manifest_path);
                ok = 0;
            }
            saw_header = 1;
            continue;
        }

        if (strcmp(tokens[0], "palette") == 0) {
            ok = run_manifest_palette(
                tokens,
                token_count,
                manifest_dir,
                output_dir,
                palettes,
                &palette_count,
                line_number);
        } else if (strcmp(tokens[0], "image") == 0) {
            ok = run_manifest_image(
                tokens,
                token_count,
                manifest_dir,
                output_dir,
                palettes,
                palette_count,
                line_number);
        } else if (strcmp(tokens[0], "font") == 0) {
            ok = run_manifest_font(
                tokens,
                token_count,
                manifest_dir,
                output_dir,
                palettes,
                palette_count,
                line_number);
        } else if (strcmp(tokens[0], "sound") == 0) {
            ok = run_manifest_sound(
                tokens,
                token_count,
                manifest_dir,
                output_dir,
                line_number);
        } else if (strcmp(tokens[0], "music") == 0) {
            ok = run_manifest_music(
                tokens,
                token_count,
                manifest_dir,
                output_dir,
                line_number);
        } else {
            fprintf(
                stderr,
                "ana-convert: unknown manifest entry at line %d: %s",
                line_number,
                original_line);
            ok = 0;
        }
    }

    if (fclose(file) != 0) {
        ok = 0;
    }

    if (ok && !saw_header) {
        fprintf(stderr, "ana-convert: manifest '%s' is empty\n", manifest_path);
        ok = 0;
    }

    manifest_free_palettes(palettes, palette_count);
    free(manifest_dir);
    return ok ? 0 : 1;
}

int main(int argc, char** argv)
{
    if (argc == 2 && strcmp(argv[1], "--version") == 0) {
        printf("%s\n", ANA_VERSION_STRING);
        return 0;
    }

    if (argc >= 2 && strcmp(argv[1], "image") == 0) {
        return run_image_command(argc, argv);
    }

    if (argc >= 2 && strcmp(argv[1], "font") == 0) {
        return run_font_command(argc, argv);
    }

    if (argc >= 2 && strcmp(argv[1], "sound") == 0) {
        return run_sound_command(argc, argv);
    }

    if (argc >= 2 && strcmp(argv[1], "palette") == 0) {
        return run_palette_command(argc, argv);
    }

    if (argc >= 2 && strcmp(argv[1], "build") == 0) {
        return run_build_command(argc, argv);
    }

    print_usage();
    return 1;
}
