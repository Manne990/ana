#include "ana/ana_sound.h"

#include "ana_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef ANA_TARGET_AMIGA
#include <exec/memory.h>
#include <proto/exec.h>
#endif

#define ANA_SOUND_HEADER_SIZE 20
#define ANA_SOUND_MAX_CHANNELS 4
#define ANA_SOUND_MAX_VOLUME 64
#define ANA_SOUND_MIN_SAMPLE_RATE 1000
#define ANA_SOUND_MAX_SAMPLE_RATE 30000
#define ANA_SOUND_MAX_SAMPLE_BYTES 131070L
#define ANA_SOUND_FLAG_SIGNED_8BIT 0x01u

#ifdef ANA_TARGET_AMIGA
#define ANA_AMIGA_AUDIO_CLOCK 3546895UL
#define ANA_AMIGA_CUSTOM_BASE ((volatile unsigned char*)0xdff000UL)
#define ANA_AMIGA_DMACON \
    (*(volatile unsigned short*)(ANA_AMIGA_CUSTOM_BASE + 0x096))
#define ANA_AMIGA_DMAF_SETCLR 0x8000u
#define ANA_AMIGA_DMAF_MASTER 0x0200u
#define ANA_AMIGA_DMAF_AUD0 0x0001u
#endif

struct ANA_SoundData {
    int sample_rate;
    long sample_length;
    long sample_alloc_size;
    int volume;
    int priority;
    unsigned int flags;
    unsigned char* samples;
};

typedef struct ANA_SoundChannel {
    ANA_Sound sound;
    int active;
    int priority;
    int remaining_ticks;
} ANA_SoundChannel;

static ANA_SoundChannel ana_sound_channels[ANA_SOUND_MAX_CHANNELS];
static int ana_sound_volume = ANA_SOUND_MAX_VOLUME;
static int ana_sound_next_channel = 0;
static int ana_sound_fps = ANA_DEFAULT_FPS;

static int ana_sound_magic_is_valid(const unsigned char* header)
{
    return header != NULL &&
        header[0] == 'A' &&
        header[1] == 'N' &&
        header[2] == 'A' &&
        header[3] == 'S' &&
        header[4] == 'N' &&
        header[5] == 'D' &&
        header[6] == '0' &&
        header[7] == '1';
}

static int ana_sound_read_u16_le(const unsigned char* bytes)
{
    return (int)bytes[0] | ((int)bytes[1] << 8);
}

static long ana_sound_read_u32_le(const unsigned char* bytes)
{
    unsigned long value;

    value = (unsigned long)bytes[0] |
        ((unsigned long)bytes[1] << 8) |
        ((unsigned long)bytes[2] << 16) |
        ((unsigned long)bytes[3] << 24);

    if (value > (unsigned long)ANA_SOUND_MAX_SAMPLE_BYTES) {
        return ANA_SOUND_MAX_SAMPLE_BYTES + 1L;
    }

    return (long)value;
}

static int ana_sound_clamp_volume(int volume)
{
    if (volume < 0) {
        return 0;
    }

    if (volume > ANA_SOUND_MAX_VOLUME) {
        return ANA_SOUND_MAX_VOLUME;
    }

    return volume;
}

static int ana_sound_validate_header(
    const unsigned char* header,
    int* sample_rate,
    long* sample_length,
    int* volume,
    int* priority,
    unsigned int* flags)
{
    if (!ana_sound_magic_is_valid(header)) {
        return 0;
    }

    *sample_rate = ana_sound_read_u16_le(header + 8);
    *sample_length = ana_sound_read_u32_le(header + 10);
    *volume = ana_sound_clamp_volume((int)header[14]);
    *priority = (int)header[15];
    *flags = (unsigned int)header[16];

    if (*sample_rate < ANA_SOUND_MIN_SAMPLE_RATE ||
            *sample_rate > ANA_SOUND_MAX_SAMPLE_RATE ||
            *sample_length <= 0L ||
            *sample_length > ANA_SOUND_MAX_SAMPLE_BYTES ||
            ((*flags & ANA_SOUND_FLAG_SIGNED_8BIT) == 0u)) {
        return 0;
    }

    return 1;
}

static void* ana_sound_alloc_sample_data(long size)
{
    if (size <= 0L) {
        return NULL;
    }

#ifdef ANA_TARGET_AMIGA
    return AllocMem((ULONG)size, MEMF_CHIP | MEMF_CLEAR);
#else
    return malloc((size_t)size);
#endif
}

static void ana_sound_free_sample_data(void* data, long size)
{
    if (data == NULL) {
        return;
    }

#ifdef ANA_TARGET_AMIGA
    FreeMem(data, (ULONG)size);
#else
    (void)size;
    free(data);
#endif
}

static ANA_Sound ana_sound_create_from_payload(
    int sample_rate,
    long sample_length,
    int volume,
    int priority,
    unsigned int flags,
    const unsigned char* payload)
{
    ANA_Sound sound;
    long alloc_size;

    alloc_size = sample_length;
    if ((alloc_size & 1L) != 0L) {
        alloc_size++;
    }

    sound = (ANA_Sound)malloc(sizeof(struct ANA_SoundData));
    if (sound == NULL) {
        return NULL;
    }

    sound->sample_rate = sample_rate;
    sound->sample_length = sample_length;
    sound->sample_alloc_size = alloc_size;
    sound->volume = volume;
    sound->priority = priority;
    sound->flags = flags;
    sound->samples = (unsigned char*)ana_sound_alloc_sample_data(alloc_size);
    if (sound->samples == NULL) {
        free(sound);
        return NULL;
    }

    memset(sound->samples, 0, (size_t)alloc_size);
    memcpy(sound->samples, payload, (size_t)sample_length);

    return sound;
}

static int ana_sound_choose_channel(ANA_Sound sound)
{
    int i;

    for (i = 0; i < ANA_SOUND_MAX_CHANNELS; i++) {
        if (!ana_sound_channels[i].active) {
            ana_sound_next_channel = (i + 1) % ANA_SOUND_MAX_CHANNELS;
            return i;
        }
    }

    for (i = 0; i < ANA_SOUND_MAX_CHANNELS; i++) {
        if (sound->priority >= ana_sound_channels[i].priority) {
            ana_sound_next_channel = (i + 1) % ANA_SOUND_MAX_CHANNELS;
            return i;
        }
    }

    return -1;
}

static int ana_sound_ticks_for_sound(ANA_Sound sound)
{
    long ticks;

    if (sound == NULL || sound->sample_rate <= 0 || ana_sound_fps <= 0) {
        return 1;
    }

    ticks =
        ((sound->sample_length * (long)ana_sound_fps) +
            (long)sound->sample_rate - 1L) /
        (long)sound->sample_rate;
    if (ticks < 1L) {
        ticks = 1L;
    }

    if (ticks > 32767L) {
        ticks = 32767L;
    }

    return (int)ticks;
}

#ifdef ANA_TARGET_AMIGA
static volatile unsigned short* ana_amiga_audio_reg(int channel, int offset)
{
    return (volatile unsigned short*)(
        ANA_AMIGA_CUSTOM_BASE + 0x0a0 + (channel * 0x10) + offset);
}

static void ana_amiga_audio_stop_channel(int channel)
{
    ANA_AMIGA_DMACON = (unsigned short)(ANA_AMIGA_DMAF_AUD0 << channel);
    *ana_amiga_audio_reg(channel, 0x08) = 0u;
}

static void ana_amiga_audio_play_channel(int channel, ANA_Sound sound)
{
    unsigned long address;
    unsigned short dma_bit;
    unsigned short period;
    unsigned short length_words;
    unsigned short volume;

    if (sound == NULL || sound->samples == NULL) {
        return;
    }

    address = (unsigned long)sound->samples;
    dma_bit = (unsigned short)(ANA_AMIGA_DMAF_AUD0 << channel);
    period =
        (unsigned short)(ANA_AMIGA_AUDIO_CLOCK / (unsigned long)sound->sample_rate);
    if (period < 124u) {
        period = 124u;
    }

    length_words = (unsigned short)(sound->sample_alloc_size / 2L);
    volume = (unsigned short)(
        (sound->volume * ana_sound_volume + (ANA_SOUND_MAX_VOLUME / 2)) /
        ANA_SOUND_MAX_VOLUME);

    ana_amiga_audio_stop_channel(channel);
    *ana_amiga_audio_reg(channel, 0x00) =
        (unsigned short)((address >> 16) & 0xffffu);
    *ana_amiga_audio_reg(channel, 0x02) =
        (unsigned short)(address & 0xffffu);
    *ana_amiga_audio_reg(channel, 0x04) = length_words;
    *ana_amiga_audio_reg(channel, 0x06) = period;
    *ana_amiga_audio_reg(channel, 0x08) = volume;
    ANA_AMIGA_DMACON =
        (unsigned short)(ANA_AMIGA_DMAF_SETCLR | ANA_AMIGA_DMAF_MASTER | dma_bit);
}
#endif

static void ana_sound_stop_channel(int channel)
{
    if (channel < 0 || channel >= ANA_SOUND_MAX_CHANNELS) {
        return;
    }

#ifdef ANA_TARGET_AMIGA
    ana_amiga_audio_stop_channel(channel);
#endif
    ana_sound_channels[channel].sound = NULL;
    ana_sound_channels[channel].active = 0;
    ana_sound_channels[channel].priority = 0;
    ana_sound_channels[channel].remaining_ticks = 0;
}

ANA_Result ana_sound_open(const ANA_Profile* profile)
{
    if (profile != NULL && profile->fps > 0) {
        ana_sound_fps = profile->fps;
    } else {
        ana_sound_fps = ANA_DEFAULT_FPS;
    }

    ana_stop_all_sounds();
    ana_sound_next_channel = 0;
    return ANA_OK;
}

void ana_sound_close(void)
{
    ana_stop_all_sounds();
}

ANA_Sound ana_load_sound(const char* path)
{
    FILE* file;
    unsigned char header[ANA_SOUND_HEADER_SIZE];
    unsigned char* payload;
    ANA_Sound sound;
    size_t bytes_read;
    int sample_rate;
    int volume;
    int priority;
    unsigned int flags;
    long sample_length;

    if (path == NULL) {
        return NULL;
    }

    file = fopen(path, "rb");
    if (file == NULL) {
        return NULL;
    }

    bytes_read = fread(header, 1u, sizeof(header), file);
    if (bytes_read != sizeof(header) ||
            !ana_sound_validate_header(
                header,
                &sample_rate,
                &sample_length,
                &volume,
                &priority,
                &flags)) {
        fclose(file);
        return NULL;
    }

    payload = (unsigned char*)malloc((size_t)sample_length);
    if (payload == NULL) {
        fclose(file);
        return NULL;
    }

    bytes_read = fread(payload, 1u, (size_t)sample_length, file);
    fclose(file);
    if (bytes_read != (size_t)sample_length) {
        free(payload);
        return NULL;
    }

    sound = ana_sound_create_from_payload(
        sample_rate,
        sample_length,
        volume,
        priority,
        flags,
        payload);
    free(payload);

    return sound;
}

ANA_Sound ana_load_sound_data(const unsigned char* bytes, long size)
{
    int sample_rate;
    int volume;
    int priority;
    unsigned int flags;
    long sample_length;

    if (bytes == NULL || size < ANA_SOUND_HEADER_SIZE) {
        return NULL;
    }

    if (!ana_sound_validate_header(
            bytes,
            &sample_rate,
            &sample_length,
            &volume,
            &priority,
            &flags)) {
        return NULL;
    }

    if (size < ANA_SOUND_HEADER_SIZE + sample_length) {
        return NULL;
    }

    return ana_sound_create_from_payload(
        sample_rate,
        sample_length,
        volume,
        priority,
        flags,
        bytes + ANA_SOUND_HEADER_SIZE);
}

void ana_free_sound(ANA_Sound sound)
{
    int i;

    if (sound == NULL) {
        return;
    }

    for (i = 0; i < ANA_SOUND_MAX_CHANNELS; i++) {
        if (ana_sound_channels[i].sound == sound) {
            ana_sound_stop_channel(i);
        }
    }

    ana_sound_free_sample_data(sound->samples, sound->sample_alloc_size);
    sound->samples = NULL;
    free(sound);
}

void ana_play_sound(ANA_Sound sound)
{
    int channel;

    if (sound == NULL || sound->samples == NULL) {
        return;
    }

    channel = ana_sound_choose_channel(sound);
    if (channel < 0) {
        return;
    }

    ana_sound_channels[channel].sound = sound;
    ana_sound_channels[channel].active = 1;
    ana_sound_channels[channel].priority = sound->priority;
    ana_sound_channels[channel].remaining_ticks =
        ana_sound_ticks_for_sound(sound);

#ifdef ANA_TARGET_AMIGA
    ana_amiga_audio_play_channel(channel, sound);
#endif
}

void ana_sound_update(void)
{
    int i;

    for (i = 0; i < ANA_SOUND_MAX_CHANNELS; i++) {
        if (!ana_sound_channels[i].active) {
            continue;
        }

        ana_sound_channels[i].remaining_ticks--;
        if (ana_sound_channels[i].remaining_ticks <= 0) {
            ana_sound_stop_channel(i);
        }
    }
}

void ana_stop_all_sounds(void)
{
    int i;

    for (i = 0; i < ANA_SOUND_MAX_CHANNELS; i++) {
        ana_sound_stop_channel(i);
    }
}

void ana_set_sound_volume(int volume)
{
    ana_sound_volume = ana_sound_clamp_volume(volume);
}

int ana_sound_active_channel_count(void)
{
    int i;
    int count;

    count = 0;
    for (i = 0; i < ANA_SOUND_MAX_CHANNELS; i++) {
        if (ana_sound_channels[i].active) {
            count++;
        }
    }

    return count;
}

int ana_sound_global_volume(void)
{
    return ana_sound_volume;
}
