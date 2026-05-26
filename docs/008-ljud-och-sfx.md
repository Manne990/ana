# Spec 008: Ljud och SFX

## Syfte

Ge ANA 0.1 tillrackligt ljudstod for korta ljudeffekter i Invaders: skott, explosion, enemy step och game over.

## Mal for 0.1

- Ladda forkonverterade ljudsamples.
- Spela kort SFX med ett enkelt API.
- Hantera Amiga-ljudkanaler utan att spelkod maste gora det manuellt.
- Ha enkel prioritet eller kanalstrategi.
- Global volym eller sample-volym om implementationen tillater.

## Foreslaget API

```c
typedef struct ANA_SoundData* ANA_Sound;

ANA_Sound ana_load_sound(const char* path);
ANA_Sound ana_load_sound_data(const unsigned char* bytes, long size);
void ana_free_sound(ANA_Sound sound);
void ana_play_sound(ANA_Sound sound);
void ana_stop_all_sounds(void);
void ana_set_sound_volume(int volume);
```

`ana_load_sound_data` laser samma format fran en inbaddad byte-array. Det ar
praktiskt for sma ADF-exempel som just nu paketeras som en ensam korbar fil.

## Kanalstrategi

0.1 ska dokumentera hur SFX anvander Amigans ljudkanaler. En enkel start kan vara:

- fire-and-forget playback
- nyare ljud far ersatta aldre ljud pa en kanal
- vissa ljud kan ha hogre prioritet, exempelvis explosion over enemy step

Det exakta valet ska vara enkelt att forsta i exempelspel.

## Assetformat

Ljudassets ska vara forkonverterade till ett runtime-vanligt format:

- sampledata i format som Amiga kan spela effektivt
- sample rate metadata
- langd
- eventuell default-volym

0.1-formatet ar `.anasnd`:

```text
offset  size  description
0       8     magic: ANASND01
8       2     sample rate, little endian
10      4     sample length in bytes, little endian
14      1     default volume, 0-64
15      1     priority, higher can replace lower priority sounds
16      1     flags, bit 0 = signed 8-bit PCM
17      3     reserved, must be 0
20      n     signed 8-bit PCM sample bytes
```

Sampledata paddas internt till jamnt antal bytes nar det behovs, eftersom
Paula spelar ljud-DMA i ord. Playback allokerar inte minne; allokering sker vid
load.

## Implementation 0.1

- Host-backenden validerar format, laddar samples och haller kanalstatus sa
  tester kan verifiera API-beteende.
- Amiga-backenden allokerar sampledata i Chip RAM. Nar musikbackenden ar
  installerad spelas SFX via samma `ptplayer`-kanalhantering som MOD-musiken;
  annars finns den enklare direkta Paula-vagen kvar som fallback.
- `ana_play_sound` ar fire-and-forget. Den valjer en ledig kanal nar det gar,
  annars ersatts en kanal enligt enkel prioritet/round-robin.
- Spec 014 lagger till `ANA_AudioConfig`, sa SFX kan begransas till valda
  Paula-kanaler eller tillatas stjala musikkanaler enligt spelets policy.
- Runtime stoppar kanalen nar samplets beraknade langd har passerat, eftersom
  Paula annars loopar ljud-DMA kontinuerligt.
- `ana_set_sound_volume` satter global volym 0-64.

## Inte i 0.1

- Mixning av manga samtidiga ljud i mjukvara.
- Runtime-WAV-dekodning.
- Avancerade effekter.

## Acceptanskriterier

- Invaders kan spela skottljud.
- Invaders kan spela explosionsljud.
- Enemy step/game over-ljud kan spelas utan att spelkod hanterar kanaler direkt.
- Ljud-API:t allokerar inte i normal playback.
- Asset pipelinen kan bygga enkla `.anasnd`-filer fran `.anasfx`-recept.
