#include "ana/ana_sound.h"

#include "ana_internal.h"
#include "sound/ana_ptplayer.h"

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
#define ANA_MUSIC_MIN_MOD_BYTES 1084L
#define ANA_MUSIC_MAX_MOD_BYTES 1048576L

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
    long sample_memory_size;
    int volume;
    int priority;
    unsigned int flags;
    unsigned char* sample_memory;
    unsigned char* samples;
};

struct ANA_MusicData {
    unsigned char* bytes;
    long size;
    int channels;
};

typedef struct ANA_SoundChannel {
    ANA_Sound sound;
    int active;
    int priority;
    int remaining_ticks;
} ANA_SoundChannel;

#ifdef ANA_TARGET_AMIGA
typedef struct ANA_PtSfxStructure {
    void* sfx_ptr;
    unsigned short sfx_len;
    unsigned short sfx_per;
    unsigned short sfx_vol;
    signed char sfx_cha;
    signed char sfx_pri;
} ANA_PtSfxStructure;
#endif

static ANA_SoundChannel ana_sound_channels[ANA_SOUND_MAX_CHANNELS];
static int ana_sound_volume = ANA_SOUND_MAX_VOLUME;
static int ana_sound_next_channel = 0;
static int ana_sound_fps = ANA_DEFAULT_FPS;

static const ANA_AudioConfig ana_audio_default_config = {
    ANA_AUDIO_ALL_CHANNELS,
    ANA_AUDIO_ALL_CHANNELS,
    1,
    1
};

static ANA_AudioConfig ana_audio_current_config = {
    ANA_AUDIO_ALL_CHANNELS,
    ANA_AUDIO_ALL_CHANNELS,
    1,
    1
};

static ANA_Music ana_music_current = NULL;
static ANA_AudioChannels ana_music_channels = 0u;
static ANA_AudioChannels ana_music_stolen_channels = 0u;
static int ana_music_playing = 0;
static int ana_music_paused = 0;
static int ana_music_loop = ANA_MUSIC_ONCE;
static int ana_music_volume = ANA_SOUND_MAX_VOLUME;

#ifdef ANA_TARGET_AMIGA
static int ana_ptplayer_installed = 0;
static ANA_PtSfxStructure ana_pt_sfx_requests[ANA_SOUND_MAX_CHANNELS];
#endif

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

static int ana_music_read_u16_be(const unsigned char* bytes)
{
    return ((int)bytes[0] << 8) | (int)bytes[1];
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

static ANA_AudioChannels ana_audio_sanitize_channels(ANA_AudioChannels channels)
{
    return (ANA_AudioChannels)(channels & ANA_AUDIO_ALL_CHANNELS);
}

static ANA_AudioChannels ana_audio_channel_bit(int channel)
{
    return (ANA_AudioChannels)(1u << channel);
}

static int ana_audio_channel_is_allowed(
    ANA_AudioChannels channels,
    int channel)
{
    return (channels & ana_audio_channel_bit(channel)) != 0u;
}

static ANA_AudioChannels ana_music_choose_channels(void)
{
    ANA_AudioChannels channels;
    int i;

    channels = ana_audio_sanitize_channels(ana_audio_current_config.music_channels);
    if (ana_audio_current_config.music_can_use_free_sfx_channels) {
        for (i = 0; i < ANA_SOUND_MAX_CHANNELS; i++) {
            if (ana_audio_channel_is_allowed(
                    ana_audio_current_config.sfx_channels,
                    i) &&
                    !ana_sound_channels[i].active) {
                channels = (ANA_AudioChannels)(channels | ana_audio_channel_bit(i));
            }
        }
    }

    return ana_audio_sanitize_channels(channels);
}

static ANA_AudioChannels ana_sound_allowed_sfx_channels(void)
{
    ANA_AudioChannels channels;

    channels = ana_audio_sanitize_channels(ana_audio_current_config.sfx_channels);
    if (ana_audio_current_config.sfx_can_steal_music && ana_music_playing) {
        channels = (ANA_AudioChannels)(channels | ana_music_channels);
    }

    return ana_audio_sanitize_channels(channels);
}

static void ana_music_mark_channel_stolen(int channel)
{
    ANA_AudioChannels channel_bit;

    if (!ana_music_playing) {
        return;
    }

    channel_bit = ana_audio_channel_bit(channel);
    if ((ana_music_channels & channel_bit) != 0u) {
        ana_music_stolen_channels =
            (ANA_AudioChannels)(ana_music_stolen_channels | channel_bit);
    }
}

#ifdef ANA_TARGET_AMIGA
static int ana_music_reserved_channel_mask(void)
{
    if (ana_audio_current_config.sfx_can_steal_music) {
        return 0;
    }

    return (int)ana_music_channels;
}

static void ana_music_apply_ptplayer_state(void)
{
    if (!ana_ptplayer_installed) {
        return;
    }

    ana_pt_musicmask(ana_music_reserved_channel_mask());
    ana_pt_channelmask((int)ana_music_channels);

    if (ana_music_playing && !ana_music_paused &&
            ana_music_current != NULL && ana_music_channels != 0u) {
        ana_pt_mastervol(ana_music_volume);
        ana_pt_enable(1);
    } else {
        ana_pt_mastervol(0);
        ana_pt_enable(0);
    }
}

static void ana_music_silence_ptplayer(void)
{
    if (!ana_ptplayer_installed) {
        return;
    }

    ana_pt_enable(0);
    ana_pt_mastervol(0);
    ana_pt_channelmask(0);
}

static int ana_sound_has_active_channels(void)
{
    int i;

    for (i = 0; i < ANA_SOUND_MAX_CHANNELS; i++) {
        if (ana_sound_channels[i].active) {
            return 1;
        }
    }

    return 0;
}

static int ana_ptplayer_ensure_installed(void)
{
    if (!ana_ptplayer_installed) {
        ana_ptplayer_installed = ana_pt_install() != 0;
    }

    return ana_ptplayer_installed;
}

static void ana_ptplayer_release_if_idle(void)
{
    if (!ana_ptplayer_installed || ana_music_playing ||
            ana_sound_has_active_channels()) {
        return;
    }

    ana_pt_end();
    ana_pt_remove();
    ana_ptplayer_installed = 0;
}
#endif

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

static void* ana_music_alloc_data(long size)
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

static void ana_music_free_data(void* data, long size)
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
    sound->sample_memory_size = alloc_size + 2L;
    sound->volume = volume;
    sound->priority = priority;
    sound->flags = flags;
    sound->sample_memory =
        (unsigned char*)ana_sound_alloc_sample_data(sound->sample_memory_size);
    if (sound->sample_memory == NULL) {
        free(sound);
        return NULL;
    }
    sound->samples = sound->sample_memory + 2;

    memset(sound->sample_memory, 0, (size_t)sound->sample_memory_size);
    memcpy(sound->samples, payload, (size_t)sample_length);

    return sound;
}

static int ana_music_signature_channels(const unsigned char* signature)
{
    if (signature == NULL) {
        return 0;
    }

    if (memcmp(signature, "M.K.", 4u) == 0 ||
            memcmp(signature, "M!K!", 4u) == 0 ||
            memcmp(signature, "FLT4", 4u) == 0 ||
            memcmp(signature, "4CHN", 4u) == 0) {
        return 4;
    }

    if (signature[1] == 'C' &&
            signature[2] == 'H' &&
            signature[3] == 'N' &&
            signature[0] >= '1' &&
            signature[0] <= '9') {
        return (int)(signature[0] - '0');
    }

    return 0;
}

static int ana_music_is_valid_mod(const unsigned char* bytes, long size)
{
    if (bytes == NULL ||
            size < ANA_MUSIC_MIN_MOD_BYTES ||
            size > ANA_MUSIC_MAX_MOD_BYTES) {
        return 0;
    }

    return ana_music_signature_channels(bytes + 1080) > 0;
}

static void ana_music_prepare_mod_samples(unsigned char* bytes, long size)
{
    int i;
    int sample;
    int max_pattern;
    int pattern;
    long sample_offset;
    long sample_length;
    long sample_header;

    if (bytes == NULL || size < ANA_MUSIC_MIN_MOD_BYTES ||
            ana_music_signature_channels(bytes + 1080) != 4) {
        return;
    }

    max_pattern = 0;
    for (i = 0; i < 128; i++) {
        pattern = (int)bytes[952 + i];
        if (pattern > max_pattern) {
            max_pattern = pattern;
        }
    }

    sample_offset = 1084L + ((long)max_pattern + 1L) * 1024L;
    if (sample_offset >= size) {
        return;
    }

    for (sample = 0; sample < 31; sample++) {
        sample_header = 20L + (long)sample * 30L;
        sample_length =
            (long)ana_music_read_u16_be(bytes + sample_header) * 2L;
        if (sample_length <= 0L) {
            continue;
        }

        if (sample_offset + sample_length > size) {
            return;
        }

        if (sample_length >= 2L) {
            bytes[sample_offset] = 0;
            bytes[sample_offset + 1L] = 0;
        }
        sample_offset += sample_length;
    }
}

static ANA_Music ana_music_create_from_payload(
    const unsigned char* bytes,
    long size)
{
    ANA_Music music;

    if (!ana_music_is_valid_mod(bytes, size)) {
        return NULL;
    }

    music = (ANA_Music)malloc(sizeof(struct ANA_MusicData));
    if (music == NULL) {
        return NULL;
    }

    music->bytes = (unsigned char*)ana_music_alloc_data(size);
    if (music->bytes == NULL) {
        free(music);
        return NULL;
    }

    memcpy(music->bytes, bytes, (size_t)size);
    ana_music_prepare_mod_samples(music->bytes, size);
    music->size = size;
    music->channels = ana_music_signature_channels(bytes + 1080);

    return music;
}

static int ana_sound_choose_channel(ANA_Sound sound)
{
    int i;
    int channel;
    ANA_AudioChannels allowed_channels;

    allowed_channels = ana_sound_allowed_sfx_channels();
    if (allowed_channels == 0u) {
        return -1;
    }

    for (i = 0; i < ANA_SOUND_MAX_CHANNELS; i++) {
        channel = (ana_sound_next_channel + i) % ANA_SOUND_MAX_CHANNELS;
        if (ana_audio_channel_is_allowed(allowed_channels, channel) &&
                !ana_sound_channels[channel].active) {
            ana_sound_next_channel = (channel + 1) % ANA_SOUND_MAX_CHANNELS;
            return channel;
        }
    }

    for (i = 0; i < ANA_SOUND_MAX_CHANNELS; i++) {
        channel = (ana_sound_next_channel + i) % ANA_SOUND_MAX_CHANNELS;
        if (ana_audio_channel_is_allowed(allowed_channels, channel) &&
                sound->priority >= ana_sound_channels[channel].priority) {
            ana_sound_next_channel = (channel + 1) % ANA_SOUND_MAX_CHANNELS;
            return channel;
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

static unsigned short ana_amiga_audio_period_for_rate(int sample_rate)
{
    unsigned short period;

    period =
        (unsigned short)(ANA_AMIGA_AUDIO_CLOCK / (unsigned long)sample_rate);
    if (period < 124u) {
        period = 124u;
    }

    return period;
}

static unsigned short ana_amiga_audio_volume_for_sound(ANA_Sound sound)
{
    return (unsigned short)(
        (sound->volume * ana_sound_volume + (ANA_SOUND_MAX_VOLUME / 2)) /
        ANA_SOUND_MAX_VOLUME);
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
    period = ana_amiga_audio_period_for_rate(sound->sample_rate);

    length_words = (unsigned short)(sound->sample_alloc_size / 2L);
    volume = ana_amiga_audio_volume_for_sound(sound);

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

static int ana_amiga_ptplayer_play_channel(int channel, ANA_Sound sound)
{
    ANA_PtSfxStructure* sfx;
    int priority;

    if (!ana_ptplayer_installed || sound == NULL ||
            sound->samples == NULL || channel < 0 ||
            channel >= ANA_SOUND_MAX_CHANNELS) {
        return 0;
    }

    priority = sound->priority;
    if (priority < 1) {
        priority = 1;
    }
    if (priority > 127) {
        priority = 127;
    }

    sfx = &ana_pt_sfx_requests[channel];
    sfx->sfx_ptr = sound->samples;
    sfx->sfx_len = (unsigned short)(sound->sample_alloc_size / 2L);
    sfx->sfx_per = ana_amiga_audio_period_for_rate(sound->sample_rate);
    sfx->sfx_vol = ana_amiga_audio_volume_for_sound(sound);
    sfx->sfx_cha = (signed char)channel;
    sfx->sfx_pri = (signed char)priority;

    return ana_pt_playfx(sfx) != NULL;
}
#endif

static void ana_sound_stop_channel(int channel)
{
    if (channel < 0 || channel >= ANA_SOUND_MAX_CHANNELS) {
        return;
    }

#ifdef ANA_TARGET_AMIGA
    if (ana_ptplayer_installed) {
        ana_pt_stopfx(channel);
    } else {
        ana_amiga_audio_stop_channel(channel);
    }
#endif
    ana_music_stolen_channels =
        (ANA_AudioChannels)(
            ana_music_stolen_channels & ~ana_audio_channel_bit(channel));
#ifdef ANA_TARGET_AMIGA
    ana_music_apply_ptplayer_state();
#endif
    ana_sound_channels[channel].sound = NULL;
    ana_sound_channels[channel].active = 0;
    ana_sound_channels[channel].priority = 0;
    ana_sound_channels[channel].remaining_ticks = 0;
#ifdef ANA_TARGET_AMIGA
    ana_ptplayer_release_if_idle();
#endif
}

ANA_Result ana_sound_open(const ANA_Profile* profile)
{
    if (profile != NULL && profile->fps > 0) {
        ana_sound_fps = profile->fps;
    } else {
        ana_sound_fps = ANA_DEFAULT_FPS;
    }

    ana_stop_all_sounds();
    ana_stop_music();
    ana_sound_next_channel = 0;
#ifdef ANA_TARGET_AMIGA
    ana_ptplayer_release_if_idle();
#endif
    return ANA_OK;
}

void ana_sound_close(void)
{
    ana_stop_music();
    ana_stop_all_sounds();
#ifdef ANA_TARGET_AMIGA
    if (ana_ptplayer_installed) {
        ana_pt_end();
        ana_pt_remove();
        ana_ptplayer_installed = 0;
    }
#endif
}

void ana_configure_audio(const ANA_AudioConfig* config)
{
    if (config == NULL) {
        ana_audio_current_config = ana_audio_default_config;
        return;
    }

    ana_audio_current_config.music_channels =
        ana_audio_sanitize_channels(config->music_channels);
    ana_audio_current_config.sfx_channels =
        ana_audio_sanitize_channels(config->sfx_channels);
    ana_audio_current_config.sfx_can_steal_music =
        config->sfx_can_steal_music != 0;
    ana_audio_current_config.music_can_use_free_sfx_channels =
        config->music_can_use_free_sfx_channels != 0;

    if (ana_music_playing) {
        ana_music_channels = ana_music_choose_channels();
        ana_music_stolen_channels =
            (ANA_AudioChannels)(
                ana_music_stolen_channels & ana_music_channels);
        if (ana_music_channels == 0u) {
            ana_stop_music();
        } else {
#ifdef ANA_TARGET_AMIGA
            ana_music_apply_ptplayer_state();
#endif
        }
    }
}

ANA_AudioConfig ana_audio_config(void)
{
    return ana_audio_current_config;
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

ANA_Music ana_load_music(const char* path)
{
    FILE* file;
    ANA_Music music;
    size_t bytes_read;
    long size;

    if (path == NULL) {
        return NULL;
    }

    file = fopen(path, "rb");
    if (file == NULL) {
        return NULL;
    }

    if (fseek(file, 0L, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }

    size = ftell(file);
    if (size < ANA_MUSIC_MIN_MOD_BYTES ||
            size > ANA_MUSIC_MAX_MOD_BYTES ||
            fseek(file, 0L, SEEK_SET) != 0) {
        fclose(file);
        return NULL;
    }

    music = (ANA_Music)malloc(sizeof(struct ANA_MusicData));
    if (music == NULL) {
        fclose(file);
        return NULL;
    }
    music->bytes = NULL;
    music->size = 0L;
    music->channels = 0;

    music->bytes = (unsigned char*)ana_music_alloc_data(size);
    if (music->bytes == NULL) {
        free(music);
        fclose(file);
        return NULL;
    }

    bytes_read = fread(music->bytes, 1u, (size_t)size, file);
    fclose(file);
    if (bytes_read != (size_t)size) {
        ana_music_free_data(music->bytes, size);
        free(music);
        return NULL;
    }

    if (!ana_music_is_valid_mod(music->bytes, size)) {
        ana_music_free_data(music->bytes, size);
        free(music);
        return NULL;
    }

    ana_music_prepare_mod_samples(music->bytes, size);
    music->size = size;
    music->channels = ana_music_signature_channels(music->bytes + 1080);

    return music;
}

ANA_Music ana_load_music_data(const unsigned char* bytes, long size)
{
    return ana_music_create_from_payload(bytes, size);
}

void ana_free_music(ANA_Music music)
{
    if (music == NULL) {
        return;
    }

    if (ana_music_current == music) {
        ana_stop_music();
    }

    ana_music_free_data(music->bytes, music->size);
    music->bytes = NULL;
    free(music);
}

void ana_play_music(ANA_Music music, int loop)
{
    if (music == NULL || music->bytes == NULL) {
        return;
    }

    ana_music_current = music;
    ana_music_channels = ana_music_choose_channels();
    ana_music_stolen_channels = 0u;
    ana_music_playing = ana_music_channels != 0u;
    ana_music_paused = 0;
    ana_music_loop = loop != 0 ? ANA_MUSIC_LOOP : ANA_MUSIC_ONCE;

#ifdef ANA_TARGET_AMIGA
    if (ana_music_playing && ana_ptplayer_ensure_installed()) {
        ana_pt_init(music->bytes);
        ana_music_apply_ptplayer_state();
    } else if (ana_music_playing) {
        ana_music_channels = 0u;
        ana_music_playing = 0;
    }
#endif
}

void ana_stop_music(void)
{
#ifdef ANA_TARGET_AMIGA
    ana_music_silence_ptplayer();
#endif
    ana_music_current = NULL;
    ana_music_channels = 0u;
    ana_music_stolen_channels = 0u;
    ana_music_playing = 0;
    ana_music_paused = 0;
    ana_music_loop = ANA_MUSIC_ONCE;
#ifdef ANA_TARGET_AMIGA
    ana_ptplayer_release_if_idle();
#endif
}

void ana_pause_music(void)
{
    if (ana_music_playing) {
        ana_music_paused = 1;
#ifdef ANA_TARGET_AMIGA
        ana_music_apply_ptplayer_state();
#endif
    }
}

void ana_resume_music(void)
{
    if (ana_music_current != NULL && ana_music_channels != 0u) {
        ana_music_playing = 1;
        ana_music_paused = 0;
#ifdef ANA_TARGET_AMIGA
        ana_music_apply_ptplayer_state();
#endif
    }
}

void ana_set_music_volume(int volume)
{
    ana_music_volume = ana_sound_clamp_volume(volume);
#ifdef ANA_TARGET_AMIGA
    ana_music_apply_ptplayer_state();
#endif
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

    ana_sound_free_sample_data(sound->sample_memory, sound->sample_memory_size);
    sound->sample_memory = NULL;
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

#ifdef ANA_TARGET_AMIGA
    if (ana_ptplayer_installed) {
        if (!ana_amiga_ptplayer_play_channel(channel, sound)) {
            return;
        }
    } else {
        ana_amiga_audio_play_channel(channel, sound);
    }
#endif

    ana_music_mark_channel_stolen(channel);
    ana_sound_channels[channel].sound = sound;
    ana_sound_channels[channel].active = 1;
    ana_sound_channels[channel].priority = sound->priority;
    ana_sound_channels[channel].remaining_ticks =
        ana_sound_ticks_for_sound(sound);
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
#ifdef ANA_TARGET_AMIGA
    ana_ptplayer_release_if_idle();
#endif
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

int ana_music_is_playing(void)
{
    return ana_music_playing && !ana_music_paused;
}

int ana_music_is_paused(void)
{
    return ana_music_paused;
}

int ana_music_looping(void)
{
    return ana_music_loop == ANA_MUSIC_LOOP;
}

int ana_music_global_volume(void)
{
    return ana_music_volume;
}

ANA_AudioChannels ana_music_active_channel_mask(void)
{
    if (!ana_music_playing) {
        return 0u;
    }

    return (ANA_AudioChannels)(
        ana_music_channels & ~ana_music_stolen_channels);
}
