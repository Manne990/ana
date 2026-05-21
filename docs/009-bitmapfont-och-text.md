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
ANA_Font ana_load_font_data(const unsigned char* bytes, long size);
void ana_free_font(ANA_Font font);
void ana_set_font_color(ANA_Font font, unsigned char color_index);
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

## Initialt `.anafnt`-format

0.1-runtime kan lasa ett litet binart wrapper-format ovanpa `.anaimg`:

- 8 bytes magic: `ANAFNT01`
- 16-bit little-endian teckenbredd
- 16-bit little-endian teckenhojd
- 8-bit forsta ASCII-tecken
- 8-bit antal tecken
- 2 reserverade bytes, maste vara 0
- en komplett `.anaimg` payload direkt efter headern

Varje frame i `.anaimg` motsvarar ett tecken i ASCII-intervallet. Okanda
tecken ritas inte men flyttar pennan en teckenbredd, vilket gor att space kan
anvandas aven i sma fonter som bara innehaller siffror och versaler.

`ana_load_font_data` laser samma format fran inbaddade byte-arrays. Det gor
det mojligt for sma exempel att ha HUD-fonten i executable utan extra filer pa
ADF:en.

## Renderingbeteende

- Text ritas utan dynamisk layout.
- Ingen radbrytning behovs i 0.1.
- Okanda tecken kan hoppas over eller ritas som placeholder.
- `ana_set_font_color` tintar fontens solida pixlar till en palettfarg.
  Standardfargen ar palettindex 1.
- Text ska inte allokera minne vid ritning.
- `ana_draw_int` ska bygga sin text i stack-minne och inte allokera.

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
