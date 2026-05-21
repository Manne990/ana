# Spec 004: Grafik, skarm, palett och buffertar

## Syfte

Gora det enkelt for spelkod att oppna en Amiga-skarm, satta palett, rensa ritbuffert och presentera en frame utan att varje spel maste skriva lag-niva display-initiering.

## Mal for 0.1

- 320x256 PAL.
- 4 bitplanes / 16 farger som basprofil.
- Palett kan sattas fran kod eller laddad asset.
- Rensning av aktiv ritbuffert.
- Stabil frame sync.
- Strategi for att undvika synligt flicker.

## Foreslaget API

```c
typedef enum ANA_ScreenMode {
    ANA_SCREEN_PAL_LORES
} ANA_ScreenMode;

typedef struct ANA_Color {
    unsigned char r;
    unsigned char g;
    unsigned char b;
} ANA_Color;

void ana_set_palette(const ANA_Color* colors, int count);
void ana_clear(unsigned char color_index);
void ana_present(void);
ANA_RenderStats ana_render_stats(void);
```

Skarmoppning och stangning sker normalt via `ana_run` och `ANA_Game`. Lagre `ana_gfx_*`-funktioner kan finnas internt eller som advanced-API senare, men basexempel ska inte behova anropa dem.

## Buffertstrategi

0.1 ska valja en praktisk och tydlig strategi:

- dubbelbuffring om minne och implementation tillater
- annars en dokumenterad strategi med kontrollerad ritordning och frame sync

Valet ska dokumenteras eftersom det paverkar vilka renderingstekniker exempelspelen bor anvanda.

## Implementerad Amiga-backend

Amiga-bygget anvander en Intuition custom screen i PAL lores:

- 320x256
- 4 bitplanes / 16 farger
- en borderless window ovanpa screenen for raw-key input
- `WaitTOF()` i `ana_present`
- defaultpalett sa `ana_clear` syns aven utan explicit palett
- software-present fran ANA:s draw-buffer till Amiga bitplanes
- dubbelbuffrad screen-bitmap sa kopieringen sker mot dold frame
- dirty-rect-konvertering for ritade images och text, med flera sma rects per
  frame sa HUD-text och spelarsprite inte tvingar en stor gemensam konvertering
- Amiga-target byggs optimerat eftersom ooptimerad 68K-kod blir for langsam
  for per-frame rendering

Den publika spelkoden ritar fortfarande med `ana_clear` och `ana_draw_image`.
Den forsta backend-implementationen prioriterar korrekt synlig output och enkel
debuggning. Direktritning till planar buffers eller blitteroptimerade paths kan
laggas under samma API nar vi borjar pressa Invaders-prestanda.

## Prestandakrav

- `ana_clear` ska vara forutsagbar och billig nog for Invaders.
- `ana_present` ska inte gora ovantat tungt arbete.
- Per-frame konvertering ska begransas till de omraden som faktiskt ritats om.
- `ana_render_stats()` ska kunna anvandas efter en korning for att logga
  renderkostnad, till exempel dirty rects, konverterad yta och clear-yta.

## Inte i 0.1

- Godtyckliga upplosningar.
- AGA-paletter eller HAM-lagen.
- Runtime-fargkvantisering.
- Windowed desktop-lage som huvudmal.

## Acceptanskriterier

- ANA kan oppna en stabil 320x256 PAL-display.
- Paletten kan sattas och syns korrekt.
- Skarmen kan rensas varje frame.
- `hello` kan visa en enkel rorlig form eller bild utan flicker som stor huvudflodet.
