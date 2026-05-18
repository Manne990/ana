# Spec 009: Bitmapfont och text

## Syfte

Ge ANA ett enkelt textsystem for score, high score, lives, title screen och game over.

## Mal for 0.1

- Ladda bitmapfont som asset.
- Rita text vid x/y.
- Fast teckenbredd racker for 0.1.
- Stod for siffror, versaler och grundlaggande symboler.
- Rendering via samma grafiska profil som ovriga bilder.

## Foreslaget API

```c
typedef struct ANA_FontData* ANA_Font;

ANA_Font ana_load_font(const char* path);
void ana_free_font(ANA_Font font);
void ana_draw_text(ANA_Font font, int x, int y, const char* text);
void ana_draw_int(ANA_Font font, int x, int y, int value);
int ana_text_width(ANA_Font font, const char* text);
```

## Fontformat

0.1 kan bygga font ovanpa ANA image-formatet:

- en spritesheet med tecken
- metadata for teckenbredd/teckenhojd
- teckentabell eller fast ASCII-intervall

For Invaders racker ett begransat tecken-set.

## Renderingbeteende

- Text ritas utan dynamisk layout.
- Ingen radbrytning behovs i 0.1.
- Okanda tecken kan hoppas over eller ritas som placeholder.
- Text ska inte allokera minne vid ritning.

## Inte i 0.1

- Proportionella fonter om fast bredd ar enklare.
- Kerning.
- Unicode-text.
- Automatisk radbrytning.
- Textinput.

## Acceptanskriterier

- Invaders kan visa score och high score.
- Invaders kan visa lives, title och game over.
- Numeriska varden kan ritas utan att spelkod maste bygga komplex textlogik.
- Text-rendering fungerar inom samma palett och buffertstrategi som ovrig grafik.
