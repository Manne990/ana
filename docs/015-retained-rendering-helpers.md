# Spec 015: Retained rendering helpers

## Syfte

Minska mangden spelkod som kravs for Amiga-vanlig rendering utan att gomma
Amigans prestandakostnader.

ANA Invaders visar att ett spel snabbt behover egen kod for:

- rektangelhantering
- 8-pixels-alignade dirty rects
- retained objects som sparar tidigare position
- rensning av gamla objektpositioner
- selektiv omritning av overlappande objekt
- HUD-text som bara ritas om nar vardet andras
- tydlig lagerordning

Detta fungerar i Invaders, men ar inte kod varje ANA-anvandare ska behova skriva
fran noll. Spec 015 beskriver vilka delar som bor lyftas in i ramverket.

## Mal

- Erbjuda sma `ANA_Rect`-helpers som ar anvandbara i alla spel.
- Ge ett hog-niva API for retained/dirty 2D-objekt.
- Minska behovet av full-screen clear i vanliga spel.
- Gora lagerordning explicit och forutsagbar.
- Behalla mojligheten att skriva egen optimerad rendering nar spelet behover
  det.
- Halla API:t C89-kompatibelt och billigt nog for stock A1200-klassen.

## Icke-mal

- Bygga ett komplett scene graph.
- Gora en generell fysik- eller kollisionsmotor.
- Ersatta all specialrendering i Invaders i forsta steget.
- Lasa fast namnet `ANA_Sprite` innan hardware sprites har en tydlig plats.
- Krava heap-allokering for varje objekt.

## Del 1: ANA_Rect-helpers

Invaders har idag egen `invaders_make_rect()` och egen dirty-align-logik i
`examples/invaders/invaders_render.c`.

Det bor finnas generella helpers:

```c
ANA_Rect ana_rect_make(int x, int y, int w, int h);
ANA_Rect ana_rect_clip(ANA_Rect rect, ANA_Rect bounds);
ANA_Rect ana_rect_union(ANA_Rect a, ANA_Rect b);
int ana_rect_is_empty(ANA_Rect rect);
int ana_rect_contains(ANA_Rect outer, ANA_Rect inner);
```

For Amiga-rendering behovs ocksa align-hjalpare. Den bor antingen vara intern
eller markeras som advanced eftersom den exponerar bitplane-/bytegranularitet:

```c
ANA_Rect ana_rect_align_x8(ANA_Rect rect, int min_x, int max_x);
```

`ana_rect_align_x8` ska:

- runda ned `x` till multipel av 8
- runda upp `x + w` till multipel av 8
- klippa mot angivna granser
- returnera en tom rect om resultatet saknar yta

## Del 2: Retained/dirty BOB-hjalpare

Invaders lagrar foregaende positioner for player, bullets och explosioner, och
rensar deras gamla rektanglar manuellt. Det ar ett vanligt BOB-monster.

Foreslaget forsta API:

```c
typedef struct ANA_Bob {
    ANA_Image image;
    ANA_Image previous_image;
    int frame;
    int x;
    int y;
    int previous_x;
    int previous_y;
    int previous_frame;
    int previous_w;
    int previous_h;
    int visible;
    int previous_visible;
    unsigned char clear_color;
} ANA_Bob;

void ana_bob_init(ANA_Bob* bob, ANA_Image image);
void ana_bob_set_image(ANA_Bob* bob, ANA_Image image);
void ana_bob_set_position(ANA_Bob* bob, int x, int y);
void ana_bob_set_frame(ANA_Bob* bob, int frame);
void ana_bob_set_visible(ANA_Bob* bob, int visible);
int ana_bob_is_unchanged(const ANA_Bob* bob);
ANA_Rect ana_bob_rect(const ANA_Bob* bob);
ANA_Rect ana_bob_previous_rect(const ANA_Bob* bob);
```

Forsta draw-flodet kan vara explicit:

```c
void ana_bob_clear_previous(const ANA_Bob* bob);
void ana_bob_draw(const ANA_Bob* bob);
void ana_bob_commit(ANA_Bob* bob);
```

Det gor kostnaden synlig:

```c
ana_bob_clear_previous(&player);
ana_bob_draw(&player);
ana_bob_commit(&player);
```

Senare kan en draw-list gora detta automatiskt.

### Overlap repair

Den svarare delen i Invaders ar att ett rorligt objekt kan passera over retained
bakgrund, till exempel fiendeformation eller shields. Nar gammal position
rensats maste overlappande retained grafik repareras.

Ett generellt API bor darfor ha en callback:

```c
typedef void (*ANA_RedrawCallback)(ANA_Rect rect, void* user_data);

typedef struct ANA_RetainedLayer {
    ANA_RedrawCallback redraw;
    void* user_data;
} ANA_RetainedLayer;
```

`ana_bob_clear_previous` har varianter som reparerar overlapp:

```c
void ana_bob_clear_previous_with_layers(
    const ANA_Bob* bob,
    const ANA_RetainedLayer* layers,
    int layer_count);

ANA_Rect ana_bob_clear_previous_x8_with_layers(
    const ANA_Bob* bob,
    int min_x,
    int max_x,
    const ANA_RetainedLayer* layers,
    int layer_count);
```

Invaders skulle da kunna registrera:

- formation layer
- shield layer
- explosion layer

och slippa hardkoda repair-anrop for varje clear.

## Del 3: Dirty redraw groups och layers

Invaders har implicit lagerordning:

1. HUD
2. state messages
3. shields
4. formation
5. explosions
6. bullets
7. player

Den ordningen bor kunna uttryckas explicit.

Foreslaget enkelt begrepp:

```c
typedef struct ANA_DrawLayer {
    ANA_RedrawCallback redraw;
    void* user_data;
    int dirty;
} ANA_DrawLayer;

void ana_layer_mark_dirty(ANA_DrawLayer* layer);
void ana_layer_draw_if_dirty(ANA_DrawLayer* layer);
```

Detta ska inte vara ett komplett scene graph. Det ar en liten hjalpare for
retained grupper som HUD, tilemap, shield grid eller fiendeformation.

## Del 4: HUD/text-cache

Invaders ritar bara om HUD nar score, lives eller status andras. Samma monster
ar vanligt i spel med score, ammo, timer och statusrad.

Forsta API kan vara en enkel label-cache:

```c
typedef struct ANA_Label {
    ANA_Font font;
    int x;
    int y;
    unsigned char color;
    unsigned char clear_color;
    int clear_width;
    char text[32];
    int dirty;
} ANA_Label;

void ana_label_init(
    ANA_Label* label,
    ANA_Font font,
    int x,
    int y,
    int clear_width);
void ana_label_set_text(ANA_Label* label, const char* text);
void ana_label_draw_if_dirty(ANA_Label* label);
```

Det ska ga att anvanda utan heap och utan formatstrangsmagi:

```c
char score_text[16];

ana_format_int(score_text, sizeof(score_text), "SCORE ", score);
ana_label_set_text(&score_label, score_text);
ana_label_draw_if_dirty(&score_label);
```

Format-helpern kan vanta. Det viktiga i 0.2 ar dirty text-cache.

## Foreslagen implementationordning

1. `ANA_Rect`-helpers.
2. `ANA_Bob` som explicit clear/draw/commit-helper utan layer-repair.
3. Enkel `ANA_Label`.
4. Retained layer callback for overlap repair.
5. Refaktorera Invaders stegvis sa den anvander helpers dar de minskar kod utan
   att sanka prestandan.

## Krav pa prestanda

- Helpers far inte allokera heap-minne per frame.
- API:t ska fungera med statiska arrayer.
- `ANA_Bob` ska inte krava virtuell dispatch eller komplex traversal.
- Dirty rects ska fortsatt kunna vara sma och 8-pixels-alignade pa Amiga.
- Ramverket ska inte automatiskt gora full-screen clear som fallback utan att
  spelet uttryckligen ber om det.

## Acceptanskriterier

- Invaders kan ersatta minst sin egen `invaders_make_rect()` med
  `ana_rect_make()`.
- Minst ett rorligt objekt i Invaders kan anvanda `ANA_Bob` utan FPS-regression.
- HUD i Invaders kan anvanda `ANA_Label` eller motsvarande dirty text-helper.
- Existerande `hello` och `invaders` bygger oforandrat beteendemassigt.
- API:t dokumenteras som en hog-niva render-helper, inte som krav for enkla
  spel.

## Implementationsstatus

Implementerat:

- `ANA_Rect`-helpers i `include/ana/ana_helpers.h`.
- `ANA_Bob` med explicit `clear_previous`, `draw`, `commit`, image-byte,
  unchanged-check och previous frame/size-state.
- `ANA_RetainedLayer` for callback-baserad overlap repair.
- `ana_retained_clear_rect` och `ana_retained_clear_rect_x8` for generisk
  retained clear med valfri layer-repair.
- `ANA_DrawLayer` for sma retained grupper.
- `ANA_Label` for dirty HUD/text-cache utan heap per frame.
- Host-tester for rect helpers, retained BOB, layer callback och label-cache.
- Invaders anvander `ana_rect_make`, `ana_rect_align_x8`, `ANA_Bob` for
  player/bullet/explosion-state och `ANA_Label` for HUD.

Medvetet kvar i Invaders:

- Exakt lagerordning for formation, shields, explosioner, bullets och player.
- Spel-specifik overlap repair for formation och shields.
- Bullet/explosion-state, eftersom det fortfarande ar kopplat till Invaders
  regler och prestandaval.

## Oppna fragor

- Ska `ANA_Bob` vara det publika namnet, eller ska det heta `ANA_DrawObject` for
  att kannas mer hog-niva?
- Ska layer-repair leva i gfx-modulen eller i en separat `ana_scene`/
  `ana_render_helpers`-modul?
- Hur mycket state ska ANA spara per draw-slot nar direct-present bara anvander
  en slot men host/dubbelbuffer kan behova fler?
