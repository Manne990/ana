#include "ana.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ANA_CONVERT_MAX_COLORS 16
#define ANA_CONVERT_MAX_DIMENSION 4096
#define ANA_CONVERT_IMAGE_FLAG_MASKED 0x01u

typedef struct ANA_ConvertColor {
    int r;
    int g;
    int b;
} ANA_ConvertColor;

typedef struct ANA_SourceImage {
    int width;
    int height;
    ANA_ConvertColor* pixels;
} ANA_SourceImage;

typedef struct ANA_ImageOptions {
    const char* input_path;
    const char* output_path;
    int colors;
    int frame_width;
    int frame_height;
    int has_transparent;
    ANA_ConvertColor transparent;
} ANA_ImageOptions;

typedef struct ANA_Palette {
    ANA_ConvertColor colors[ANA_CONVERT_MAX_COLORS];
    int count;
} ANA_Palette;

static void print_usage(void)
{
    printf("ana-convert %s\n", ANA_VERSION_STRING);
    printf("Usage:\n");
    printf("  ana-convert --version\n");
    printf("  ana-convert image input.ppm --out output.anaimg [options]\n");
    printf("\n");
    printf("Image options:\n");
    printf("  --colors N              Palette size, 1-16. Default: 16\n");
    printf("  --frame-width N         Frame width for spritesheets\n");
    printf("  --frame-height N        Frame height for spritesheets\n");
    printf("  --transparent R,G,B     Transparent color, for example 255,0,255\n");
    printf("  --transparent #RRGGBB   Transparent color as hex\n");
    printf("\n");
    printf("The initial image input format is PPM P3/P6.\n");
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

        return parse_hex_byte(text + 1, &color->r) &&
            parse_hex_byte(text + 3, &color->g) &&
            parse_hex_byte(text + 5, &color->b);
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

static int palette_find_color(const ANA_Palette* palette, const ANA_ConvertColor* color)
{
    int i;

    for (i = 0; i < palette->count; i++) {
        if (colors_equal(&palette->colors[i], color)) {
            return i;
        }
    }

    return -1;
}

static int palette_add_color(ANA_Palette* palette, const ANA_ConvertColor* color, int limit)
{
    int existing;

    existing = palette_find_color(palette, color);
    if (existing >= 0) {
        return existing;
    }

    if (palette->count >= limit) {
        return -1;
    }

    palette->colors[palette->count] = *color;
    palette->count++;

    return palette->count - 1;
}

static int build_palette(
    const ANA_SourceImage* image,
    const ANA_ImageOptions* options,
    ANA_Palette* palette)
{
    int pixel_count;
    int i;
    const ANA_ConvertColor* color;

    palette->count = 0;
    pixel_count = image->width * image->height;

    for (i = 0; i < pixel_count; i++) {
        color = &image->pixels[i];
        if (options->has_transparent &&
                colors_equal(color, &options->transparent)) {
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

            if (has_mask && colors_equal(color, &options->transparent)) {
                continue;
            }

            if (has_mask) {
                set_plan_bit(mask, row_bytes, x, y);
            }

            color_index = palette_find_color(palette, color);
            if (color_index < 0) {
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

static int write_ana_image(
    const char* path,
    const ANA_SourceImage* image,
    const ANA_ImageOptions* options,
    const ANA_Palette* palette)
{
    FILE* file;
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

    bitplanes = bitplanes_for_color_count(options->colors);
    row_bytes = (frame_width + 7) / 8;
    has_mask = options->has_transparent;

    file = fopen(path, "wb");
    if (file == NULL) {
        fprintf(stderr, "ana-convert: could not open output '%s'\n", path);
        return 0;
    }

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

    if (fclose(file) != 0) {
        ok = 0;
    }

    if (!ok) {
        fprintf(stderr, "ana-convert: failed while writing '%s'\n", path);
        return 0;
    }

    return 1;
}

static int parse_image_args(int argc, char** argv, ANA_ImageOptions* options)
{
    int i;

    if (argc < 5) {
        return 0;
    }

    options->input_path = argv[2];
    options->output_path = NULL;
    options->colors = ANA_DEFAULT_COLORS;
    options->frame_width = 0;
    options->frame_height = 0;
    options->has_transparent = 0;
    options->transparent.r = 0;
    options->transparent.g = 0;
    options->transparent.b = 0;

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

    ok = read_ppm(options.input_path, &image) &&
        build_palette(&image, &options, &palette) &&
        write_ana_image(options.output_path, &image, &options, &palette);

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

int main(int argc, char** argv)
{
    if (argc == 2 && strcmp(argv[1], "--version") == 0) {
        printf("%s\n", ANA_VERSION_STRING);
        return 0;
    }

    if (argc >= 2 && strcmp(argv[1], "image") == 0) {
        return run_image_command(argc, argv);
    }

    print_usage();
    return 1;
}
