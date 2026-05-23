#include "ana.h"
#include "ana_internal.h"

#include <assert.h>
#include <stdio.h>

#define TEST_SOUND_SIZE 24

static unsigned char test_sound_bytes[TEST_SOUND_SIZE] = {
    'A', 'N', 'A', 'S', 'N', 'D', '0', '1',
    0x40, 0x1f,
    0x04, 0x00, 0x00, 0x00,
    48,
    2,
    1,
    0,
    0,
    0,
    0x20, 0xe0, 0x10, 0xf0
};

static void write_test_file(const char* path)
{
    FILE* file;

    file = fopen(path, "wb");
    assert(file != 0);
    assert(fwrite(test_sound_bytes, 1u, sizeof(test_sound_bytes), file) ==
        sizeof(test_sound_bytes));
    fclose(file);
}

static void test_sound_loading(void)
{
    ANA_Sound sound;
    ANA_Sound file_sound;
    unsigned char bad_magic[TEST_SOUND_SIZE];
    unsigned char bad_rate[TEST_SOUND_SIZE];
    const char* path;

    sound = ana_load_sound_data(test_sound_bytes, (long)sizeof(test_sound_bytes));
    assert(sound != 0);

    assert(ana_load_sound_data(0, 0L) == 0);
    assert(ana_load_sound_data(test_sound_bytes, 8L) == 0);

    bad_magic[0] = 'X';
    bad_magic[1] = 'N';
    bad_magic[2] = 'A';
    bad_magic[3] = 'S';
    bad_magic[4] = 'N';
    bad_magic[5] = 'D';
    bad_magic[6] = '0';
    bad_magic[7] = '1';
    bad_magic[8] = 0x40;
    bad_magic[9] = 0x1f;
    bad_magic[10] = 0x04;
    bad_magic[11] = 0x00;
    bad_magic[12] = 0x00;
    bad_magic[13] = 0x00;
    bad_magic[14] = 48;
    bad_magic[15] = 2;
    bad_magic[16] = 1;
    bad_magic[17] = 0;
    bad_magic[18] = 0;
    bad_magic[19] = 0;
    bad_magic[20] = 0x20;
    bad_magic[21] = 0xe0;
    bad_magic[22] = 0x10;
    bad_magic[23] = 0xf0;
    assert(ana_load_sound_data(bad_magic, (long)sizeof(bad_magic)) == 0);

    bad_rate[0] = 'A';
    bad_rate[1] = 'N';
    bad_rate[2] = 'A';
    bad_rate[3] = 'S';
    bad_rate[4] = 'N';
    bad_rate[5] = 'D';
    bad_rate[6] = '0';
    bad_rate[7] = '1';
    bad_rate[8] = 0x64;
    bad_rate[9] = 0x00;
    bad_rate[10] = 0x04;
    bad_rate[11] = 0x00;
    bad_rate[12] = 0x00;
    bad_rate[13] = 0x00;
    bad_rate[14] = 48;
    bad_rate[15] = 2;
    bad_rate[16] = 1;
    bad_rate[17] = 0;
    bad_rate[18] = 0;
    bad_rate[19] = 0;
    bad_rate[20] = 0x20;
    bad_rate[21] = 0xe0;
    bad_rate[22] = 0x10;
    bad_rate[23] = 0xf0;
    assert(ana_load_sound_data(bad_rate, (long)sizeof(bad_rate)) == 0);

    path = "build/tests/sound_test.anasnd";
    write_test_file(path);
    file_sound = ana_load_sound(path);
    assert(file_sound != 0);
    assert(ana_load_sound(0) == 0);
    assert(ana_load_sound("build/tests/missing.anasnd") == 0);

    ana_free_sound(file_sound);
    ana_free_sound(sound);
}

static void test_sound_playback_state(void)
{
    ANA_Sound sound;
    int i;

    assert(ana_sound_open(ana_default_profile()) == ANA_OK);
    assert(ana_sound_active_channel_count() == 0);

    sound = ana_load_sound_data(test_sound_bytes, (long)sizeof(test_sound_bytes));
    assert(sound != 0);

    ana_set_sound_volume(80);
    assert(ana_sound_global_volume() == 64);
    ana_set_sound_volume(-5);
    assert(ana_sound_global_volume() == 0);
    ana_set_sound_volume(32);
    assert(ana_sound_global_volume() == 32);

    for (i = 0; i < 5; i++) {
        ana_play_sound(sound);
    }
    assert(ana_sound_active_channel_count() == 4);

    ana_sound_update();
    assert(ana_sound_active_channel_count() == 0);

    ana_play_sound(sound);
    assert(ana_sound_active_channel_count() == 1);
    ana_stop_all_sounds();
    assert(ana_sound_active_channel_count() == 0);
    ana_play_sound(0);
    assert(ana_sound_active_channel_count() == 0);

    ana_free_sound(sound);
    ana_sound_close();
}

int main(void)
{
    test_sound_loading();
    test_sound_playback_state();

    return 0;
}
