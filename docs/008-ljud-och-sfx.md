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
void ana_free_sound(ANA_Sound sound);
void ana_play_sound(ANA_Sound sound);
void ana_stop_all_sounds(void);
void ana_set_sound_volume(int volume);
```

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

## Inte i 0.1

- Musiksystem.
- Tracker/mod playback.
- Mixning av manga samtidiga ljud i mjukvara.
- Runtime-WAV-dekodning.
- Avancerade effekter.

## Acceptanskriterier

- Invaders kan spela skottljud.
- Invaders kan spela explosionsljud.
- Enemy step/game over-ljud kan spelas utan att spelkod hanterar kanaler direkt.
- Ljud-API:t allokerar inte i normal playback.
