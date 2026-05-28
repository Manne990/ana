# Spec 017: Scroll, camera och tilemap

## Bakgrund

Byte Brothers visar en viktig grans for ANA:s nuvarande renderer. Dirty rects ar
effektivt nar bakgrunden ar statisk, som i Invaders och AMAze. En scrollande
plattformare eller vertikal shooter gor daremot stora delar av viewporten dirty
nar kameran ror sig. Om spelet losser det genom att rensa och rita om hela
spelvyn fran C varje frame blir C2P- och minneskostnaden for hog pa en stock
A1200 utan Fast RAM.

Nuvarande Byte Brothers-scroll ar darfor en tillfallig fallback: den prioriterar
korrekt bild framfor slutlig prestanda. Den ska inte betraktas som ANA:s
langsiktiga scrollmodell.

## Mal

- Ge spelaren ett enkelt hog-niva-API for kamera och scrollande tilemaps.
- Stodja bade spelarstyrd kamera, t.ex. plattformsspel, och kontinuerlig
  scroll, t.ex. vertikala shooters.
- Undvika att spelkod behover rakna tile-intervall, screen/world-offsets och
  exposed strips sjalv.
- Halla HUD och spelviewport separerade sa en statisk HUD inte blir dirty pa
  varje scrollsteg.
- Bygga en modell som kan optimeras for Amiga utan att spel-API:t maste andras.
- Behalla C- och Amiga-escape hatches for spel som behover egen hot path.

## Icke-mal for forsta versionen

- Ingen komplett scene graph.
- Ingen generell fysikmotor.
- Ingen godtycklig zoom/rotation.
- Ingen garanti for pixelperfekt fri scroll pa alla maskinprofiler.
- Ingen A500/68000-baseline for fullskarmscroll i 0.2.

## Grundprincip

ANA ska skilja pa tre renderfall:

1. Statisk viewport med dirty objekt.
   Detta ar dagens starka fall: rorliga objekt rensar sin gamla rektangel,
   reparerar statiska lager och ritas pa nytt.

2. Snappad tile-/byte-scroll.
   Kameran ror sig i steg som passar Amigans minneslayout, normalt 8 pixlar
   horisontellt och tilehojd eller mindre vertikalt. ANA ritar bara nya strips
   som kommer in i viewporten och ritar om rorliga objekt.

3. Hardware-scroll-backend.
   Senare Amiga-backend kan anvanda bitplane-offset/fine scroll och overdraw-
   marginaler, sa skarmen flyttas av hardvaran och ANA bara ritar nya kolumner
   eller rader. Detta ar den langsiktigt ratta modellen for jamn scroll.

Dagens C2P/direct-present-modell ska vara fallback, inte huvudstrategi for
kontinuerlig fullskarmscroll.

## API-skiss

Namnen ar preliminara men visar avsikten.

```c
typedef struct ANA_Camera {
    int x;
    int y;
    int view_x;
    int view_y;
    int view_w;
    int view_h;
    int world_w;
    int world_h;
    int snap_x;
    int snap_y;
} ANA_Camera;

void ana_camera_init(
    ANA_Camera* camera,
    int view_x,
    int view_y,
    int view_w,
    int view_h,
    int world_w,
    int world_h);

void ana_camera_set_snap(ANA_Camera* camera, int snap_x, int snap_y);
void ana_camera_set_position(ANA_Camera* camera, int x, int y);
void ana_camera_scroll_by(ANA_Camera* camera, int dx, int dy);
void ana_camera_follow_rect(
    ANA_Camera* camera,
    ANA_Rect target,
    int margin_x,
    int margin_y);

ANA_Rect ana_camera_world_view(const ANA_Camera* camera);
ANA_Rect ana_camera_world_to_screen_rect(
    const ANA_Camera* camera,
    ANA_Rect world_rect);
```

Tilemap:

```c
typedef unsigned char (*ANA_TileAtCallback)(
    int tile_x,
    int tile_y,
    void* user_data);

typedef void (*ANA_TileDrawCallback)(
    unsigned char tile,
    int screen_x,
    int screen_y,
    void* user_data);

typedef struct ANA_Tilemap {
    int tile_w;
    int tile_h;
    int map_w;
    int map_h;
    ANA_TileAtCallback tile_at;
    ANA_TileDrawCallback draw_tile;
    void* user_data;
} ANA_Tilemap;

void ana_tilemap_draw_view(
    const ANA_Tilemap* tilemap,
    const ANA_Camera* camera);
```

Scroll state:

```c
typedef struct ANA_ScrollLayer {
    ANA_Tilemap tilemap;
    ANA_Camera camera;
    int previous_x;
    int previous_y;
    unsigned char clear_color;
    int full_redraw;
} ANA_ScrollLayer;

void ana_scroll_layer_init(
    ANA_ScrollLayer* layer,
    const ANA_Tilemap* tilemap,
    const ANA_Camera* camera,
    unsigned char clear_color);

void ana_scroll_layer_mark_dirty(ANA_ScrollLayer* layer);
void ana_scroll_layer_draw(ANA_ScrollLayer* layer);
```

Forsta implementationen kan vara konservativ och rita om viewporten nar kameran
andras. Det viktiga ar att spelkod redan anvander ratt API. Sedan kan
implementationen bytas till strip- eller hardware-scroll utan att Byte Brothers
eller framtida spel behover rakna om sin rendering.

## Prestandamodell

For en 320x240 viewport ar full redraw varje frame dyrt:

- manga chunky writes
- stor C2P-yta
- manga tile-draw-callbacks
- risk for ojamt frame pacing nar scroll och sprites hander samtidigt

En stripbaserad modell vid horisontell scroll ska i stallet gora:

- flytta eller hardvaruscrolla befintlig bild
- rita ny kolumn/kolumner som exponeras
- rita om movers och HUD endast nar de andras

For vertikal shooter-scroll ar samma princip:

- scrolla bakgrundslagret kontinuerligt
- rita ny rad/rader i framkant
- hall objekt/HUD separat fran bakgrundslagret

## Backends

### Portable fallback

Alla plattformar kan anvanda viewport redraw. Den ar enkel och korrekt men inte
prestationsmalet for stora scrollande spel pa Amiga.

### Snapped dirty strips

Amiga-forsta optimering for 0.2:

- kamera snappar till 8 pixlar horisontellt
- ANA identifierar exponerade strips
- bara strips och rorliga objekt markeras dirty
- HUD ar ett separat statiskt lager

Detta passar tilebaserade plattformsspel och ar betydligt battre an att rita om
hela viewporten.

### Hardware scroll

Langsiktig backend:

- anvand bitplane-start/fine-scroll dar det ar praktiskt
- ha en overdraw-marginal utanfor synlig viewport
- rita nya tile strips i marginalen
- behall samma `ANA_Camera`/`ANA_ScrollLayer`-API

Detta ar troligen nodvandigt for stabil, mjuk fullskarmscroll pa stock A1200
utan att offra for mycket CPU till C2P.

## Byte Brothers-migrering

Byte Brothers ska stegvis sluta aga egen scrolllogik:

1. Ersatt egen `bb_camera_x`-hantering med `ANA_Camera`.
2. Ersatt egen tile-intervalberakning med `ana_tilemap_draw_view`.
3. Flytta viewport redraw/camera change state till `ANA_ScrollLayer`.
4. Lat spelet endast beskriva tilemap-data, collision och entities.
5. Nar strip/hardware-backend finns ska Byte Brothers fa den utan att
   spelreglerna andras.

## Acceptanskriterier

- Byte Brothers anvander `ANA_Camera` for world/screen-konvertering.
- Byte Brothers anvander `ANA_Tilemap` eller `ANA_ScrollLayer` for tile redraw.
- Spelkod raknar inte langre synliga tile-intervall sjalv.
- Debug stats visar tydligt nar scroll faller tillbaka till full viewport redraw.
- Kontinuerlig vertikal scroll kan demonstreras med ett enkelt test eller sample.
- Ingen regression for Invaders/AMAze, som fortsatt ska kunna anvanda dirty
  retained rendering utan scrollsystemet.

## Oppna fragor

- Hur mycket av hardware scroll kan kombineras med nuvarande chunky/C2P-modell?
- Ska hardware scroll forst stodja endast horisontell scroll, eller bade X/Y?
- Ska tilemaps ritas med callbacks, preconverted tile images, eller bada?
- Ska parallax vara flera `ANA_ScrollLayer`, eller ett separat hogre API?
- Vilken scrollgranularitet ar rimlig for stock A1200: 1, 2, 4, 8 eller
  tilebaserade steg?

