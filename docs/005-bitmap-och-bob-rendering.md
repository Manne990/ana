# Spec 005: Bitmap och BOB-rendering

## Syfte

Gora det enkelt att rita rorliga objekt, tillrackligt snabbt for Invaders-fiender, spelare, skott och explosioner.

## Mal for 0.1

- Ladda forkonverterade planar-bilder.
- Rita maskade BOBs vid x/y.
- Stod for flera frames per image.
- Enkel clipping mot skarm.
- Ingen runtime-dekodning av PNG/IFF i spelet.

## Foreslaget API

```c
typedef struct ANA_Image ANA_Image;

ANA_Image* ana_image_load(const char* path);
void ana_image_free(ANA_Image* image);

void ana_image_draw(ANA_Image* image, int x, int y);
void ana_image_draw_frame(ANA_Image* image, int frame, int x, int y);
int ana_image_width(const ANA_Image* image);
int ana_image_height(const ANA_Image* image);
int ana_image_frame_count(const ANA_Image* image);
```

## Assetkrav

ANA image-formatet for 0.1 ska innehalla:

- bredd
- hojd
- antal frames
- bitplane-info
- maskdata om bilden ska ritas transparent
- planar bilddata i Amiga-vanlig layout

## Renderingregler

- Bilddata ska vara forberedd for den aktuella bitplane-profilen.
- Maskad ritning ska anvanda effektiv Amiga-vanlig strategi.
- Clipping ska vara korrekt nog for objekt som gar delvis utanfor skarmen.
- Rendering ska inte allokera minne.

## Inte i 0.1

- Skalning.
- Rotation.
- Alpha blending.
- Subpixel-rendering.
- Generell sprite batcher om det komplicerar implementationen.

## Acceptanskriterier

- Invaders kan rita en hel fiendeformation med animation.
- Spelare, skott och explosioner kan ritas via samma image-API.
- Delvis offscreen-objekt kraschar inte och ritar inte utanfor buffert.
- Prestandan ar tillracklig for ett stabilt 50 Hz Invaders-exempel pa malprofilen.

