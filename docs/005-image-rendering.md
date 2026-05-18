# Spec 005: Image-rendering

## Syfte

Gora det enkelt att rita bilder och rorliga objekt, tillrackligt snabbt for Invaders-fiender, spelare, skott och explosioner.

## Mal for 0.1

- Ladda forkonverterade planar-bilder.
- Rita maskade images vid x/y.
- Stod for flera frames per image.
- Enkel clipping mot skarm.
- Ingen runtime-dekodning av PNG/IFF i spelet.
- Halla BOB/blitter-detaljer i implementationen eller advanced-dokumentation.

## Foreslaget API

```c
typedef struct ANA_ImageData* ANA_Image;

ANA_Image ana_load_image(const char* path);
void ana_free_image(ANA_Image image);

void ana_draw_image(ANA_Image image, int x, int y);
void ana_draw_image_frame(ANA_Image image, int frame, int x, int y);
int ana_image_width(ANA_Image image);
int ana_image_height(ANA_Image image);
int ana_image_frame_count(ANA_Image image);
```

`ANA_Image` ar ett publikt handle-begrepp. Implementationens datarepresentation kan vara pekare, id-tabeller, BOB-data eller annan Amiga-anpassad layout.

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
- Implementation far anvanda blitter/BOB-tekniker utan att exponera dem i bas-API:t.
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
