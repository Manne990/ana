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
typedef struct ANA_Color {
    unsigned char r;
    unsigned char g;
    unsigned char b;
} ANA_Color;

int ana_gfx_open(void);
void ana_gfx_close(void);
void ana_gfx_set_palette(const ANA_Color* colors, int count);
void ana_gfx_clear(unsigned char color_index);
void ana_gfx_present(void);
```

## Buffertstrategi

0.1 ska valja en praktisk och tydlig strategi:

- dubbelbuffring om minne och implementation tillater
- annars en dokumenterad strategi med kontrollerad ritordning och frame sync

Valet ska dokumenteras eftersom det paverkar vilka renderingstekniker exempelspelen bor anvanda.

## Prestandakrav

- `ana_gfx_clear` ska vara forutsagbar och billig nog for Invaders.
- `ana_gfx_present` ska inte gora ovantat tungt arbete.
- Ingen konvertering av pixeldata far ske per frame.

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

