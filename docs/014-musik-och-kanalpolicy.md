# Spec 014: Musik och kanalpolicy

## Syfte

Ge ANA ett musiksystem som passar Amiga, utan att lasa fast spelutvecklaren i
en enda kanalstrategi.

Amigan har fyra Paula-ljudkanaler. Musik, SFX och spelkansla paverkas direkt av
hur de kanalerna anvands. ANA ska darfor ha en enkel default som fungerar for
nyborjare, men ocksa ett tydligt API dar spelet kan bestamma hur kanalerna ska
fordelas.

## Mal

- Stod for bakgrundsmusik i ett Amiga-nara format.
- Hog-niva API for att ladda, spela, stoppa och loopa musik.
- Konfigurerbar kanalpolicy for musik och SFX.
- Default-beteende som later ett enkelt spel fa musik och SFX utan extra setup.
- Ingen dold mjukvarumixning i 0.1/0.2.

## Implementationsstatus

Forsta implementationen finns i ramverket:

- publik kanalpolicy via `ANA_AudioConfig`
- SFX-spelaren respekterar `sfx_channels`
- SFX kan blockeras fran musikkanaler eller tillatas stjala dem enligt policy
- musik-API:t kan ladda, validera och aga ProTracker MOD-data
- `ana_play_music`, pause/resume/stop och volym hanterar runtime-state
- asset-manifest kan kopiera `.mod` med `music name file.mod`
- Invaders-manifestet paketerar en liten ProTracker MOD som `assets/theme.mod`
- Amiga-backenden lankar in Frank Wille/Phx `ptplayer`, som ar public domain
- MOD-data allokeras i Chip RAM pa Amiga innan replay
- ptplayer installeras lazy nar musik startas och tas bort igen nar musik/SFX
  ar idle
- Invaders laddar och startar `assets/theme.mod` under `load`, spelar musik
  pa title/clear/game-over och stoppar den under aktiv gameplay for att skydda
  FPS
- Invaders behandlar musik som optional sa spelet kan starta aven om en stor
  MOD inte far plats i minnet
- Invaders satter en explicit kanalpolicy med tva musikkanaler och tva
  SFX-kanaler
- Invaders genererade SFX har enkel attack/release-envelope sa de inte klickar
  lika hardt nar de spelas via Paula eller ptplayer

Kvar att harda senare:

- mer testning av kanalpolicy med musik och manga samtidiga SFX
- battre rapportering om AmigaOS/audio.device inte kan allokera kanaler
- budget/rekommendationer for MOD-storlek per malmaskin
- host-side musikpreview om vi vill hora MOD aven utan emulator

## Rekommenderat musikformat

Forsta musikformatet bor vara ProTracker MOD:

- passar Amigans fyra Paula-kanaler
- ar historiskt korrekt och latt att hitta verktyg for
- kan spelas utan runtime-konvertering i forsta steget
- kan senare valideras eller packas av asset pipeline

0.1/0.2 bygger inte en egen tracker eller ett eget musikformat. Amiga-backenden
anvander en vendrad public-domain ProTracker-replayer.

## Foreslaget musik-API

```c
typedef struct ANA_MusicData* ANA_Music;

#define ANA_MUSIC_ONCE 0
#define ANA_MUSIC_LOOP 1

ANA_Music ana_load_music(const char* path);
ANA_Music ana_load_music_data(const unsigned char* bytes, long size);
void ana_free_music(ANA_Music music);

void ana_play_music(ANA_Music music, int loop);
void ana_stop_music(void);
void ana_pause_music(void);
void ana_resume_music(void);
void ana_set_music_volume(int volume);
```

Det ska vara mojligt att skriva:

```c
static ANA_Music theme;

void game_load(void)
{
    theme = ana_load_music("assets/theme.mod");
    ana_play_music(theme, ANA_MUSIC_LOOP);
}

void game_shutdown(void)
{
    ana_stop_music();
    ana_free_music(theme);
}
```

## Kanalbegrepp

ANA exponerar Paula-kanalerna som en enkel bitmask. Namnen ska vara tillrackligt
hog-niva for vanliga C-spel, men anda tydliga for den som kan Amiga.

```c
typedef unsigned char ANA_AudioChannels;

#define ANA_AUDIO_CH_0 0x01
#define ANA_AUDIO_CH_1 0x02
#define ANA_AUDIO_CH_2 0x04
#define ANA_AUDIO_CH_3 0x08
#define ANA_AUDIO_ALL_CHANNELS 0x0f
```

## Kanalpolicy

Spelet ska kunna konfigurera hur musik och SFX far anvanda kanalerna:

```c
typedef struct ANA_AudioConfig {
    ANA_AudioChannels music_channels;
    ANA_AudioChannels sfx_channels;
    int sfx_can_steal_music;
    int music_can_use_free_sfx_channels;
} ANA_AudioConfig;

void ana_configure_audio(const ANA_AudioConfig* config);
ANA_AudioConfig ana_audio_config(void);
```

### Default

Default bor vara enkel och spelbar:

```c
music_channels = ANA_AUDIO_ALL_CHANNELS;
sfx_channels = ANA_AUDIO_ALL_CHANNELS;
sfx_can_steal_music = 1;
music_can_use_free_sfx_channels = 1;
```

Det betyder:

- musik kan anvanda alla fyra kanaler
- SFX kan spela utan att utvecklaren gor setup
- viktiga SFX far kort stjala en kanal
- musiken kan tappa en ton kort, vilket ar acceptabelt for forsta default

## Exempelpolicys

### Stabil musik, en SFX-kanal

```c
ANA_AudioConfig audio = {
    ANA_AUDIO_CH_0 | ANA_AUDIO_CH_1 | ANA_AUDIO_CH_2,
    ANA_AUDIO_CH_3,
    0,
    0
};

ana_configure_audio(&audio);
```

Musiken far tre kanaler. SFX far kanal 3. Inga kanaler stjals.

### Full MOD, SFX far stjala

```c
ANA_AudioConfig audio = {
    ANA_AUDIO_ALL_CHANNELS,
    ANA_AUDIO_ALL_CHANNELS,
    1,
    1
};

ana_configure_audio(&audio);
```

Bra default for spel dar SFX ar viktigare an perfekt musik.

### Bara SFX

```c
ANA_AudioConfig audio = {
    0,
    ANA_AUDIO_ALL_CHANNELS,
    0,
    0
};
```

Passar sma arkadspel, menyer eller tidiga prototyper.

### Bara musik

```c
ANA_AudioConfig audio = {
    ANA_AUDIO_ALL_CHANNELS,
    0,
    0,
    0
};
```

Passar demos, title screens eller musiktest.

## SFX-prioritet

Spec 008 har redan enkel prioritet for `ANA_Sound`. Den ska samspela med
kanalpolicyn:

- ledig SFX-kanal anvands forst
- om ingen SFX-kanal ar ledig kan lagre prioritet ersattas
- om `sfx_can_steal_music` ar pa kan SFX ersatta musikkanal
- om `sfx_can_steal_music` ar av far musikkanaler inte roras

Senare kan ett explicit SFX-options-API laggas till:

```c
typedef struct ANA_SoundOptions {
    int volume;
    int priority;
    ANA_AudioChannels channels;
} ANA_SoundOptions;

void ana_play_sound_ex(ANA_Sound sound, const ANA_SoundOptions* options);
```

Det ar inte nodvandigt for forsta musikversionen.

## Musik och SFX samtidigt

En MOD-replayer anvander normalt fyra kanaler. ANA ska inte latsas att det finns
fler Paula-kanaler an vad hardvaran har.

Forsta implementationen ska valja en tydlig strategi:

- MOD-replayer skriver till de kanaler som finns i `music_channels`
- SFX skriver till de kanaler som finns i `sfx_channels`
- om en kanal ar gemensam avgor policy och prioritet vem som far spela
- nar ett SFX slutar ska musik kunna ta tillbaka kanalen vid nasta musik-tick

## Timing

Musik bor uppdateras stabilt, helst via VBlank/CIA eller annan Amiga-nara tick.
Den forsta Amiga-implementationen anvander `ptplayer` i OS-kompatibelt lage,
dar replayn registrerar CIA-B-interrupt via AmigaOS. Host-builden validerar och
haller API-state, men spelar inte upp MOD-ljud.

## Asset pipeline

Forsta steget:

- `.mod` kan laddas direkt som musikasset
- asset pipeline kan kopiera `.mod`
- manifeststöd: `music theme theme.mod`

Senare steg:

- storlekskontroll
- formatvalidering
- eventuell packning

## Inte i forsta versionen

- Egen tracker.
- MOD-redigering.
- Flerkanalig mjukvarumixning.
- MIDI.
- MP3/WAV/OGG-runtime-dekodning.
- Streaming fran disk.
- Avancerade effekter utover vad MOD-replayern stodjer.

## Acceptanskriterier

- Ett exempel kan ladda och spela en loopad MOD.
- Spelet kan konfigurera kanalpolicy innan musik/SFX spelas.
- SFX kan reserveras till en egen kanal.
- SFX kan, om policy tillater det, stjala musikkanal.
- `ana_stop_music` stoppar musik utan att stoppa aktiva SFX.
- Dokumentationen visar minst tre kanalpolicys.
