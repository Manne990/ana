# Spec 016: Byte Brothers platform sample

## Syfte

Bygga ett tredje ANA-sample som stressar ramverket med en scrollande
plattformare. Spelet ska inte kopiera Giana Sisters visuellt, men ska testa
samma typ av ramverksbehov: tilemap, kamera, plattformsfysik, spriteanimation,
collectibles, fiender, hemliga block, musik, ljudeffekter och A1200-prestanda.

Forsta spelbara slice ska fokusera pa en karaktar, Byte, och en liten men
representativ bana. Senare kan Bit, fler abilities och fler varldar laggas till.

## Presentation

Samplet ska kannas som ett riktigt Amiga-spel snarare an ett tekniskt test.
Forsta versionen ska darfor ha:

- Fullscreen PAL lores via ANA:s screen setup, utan Workbench-fonster eller
  terminaltext under gameplay.
- `ANA_RENDER_TILE_SCROLL` i `ANA_Game`, sa samplet deklarerar att det behover
  scroll-/tilemap-orienterad rendering snarare an statisk dirty rendering.
- Bakgrundsmusik som loopar under spelet.
- Ljudeffekter for minst hopp, collectible, power-up, fiendekollision och
  levelmal.
- En integrerad och diskret HUD, ritad som en del av spelbilden snarare an som
  separat debugtext. HUD:en ska visa poang/kodfragment, liv eller energi och
  eventuell disk-/gate-status.
- Debug stats far fortfarande finnas i debug-builds, men ska inte vara den
  primara presentationen av spelet.

## Symboler

Kartorna nedan ar designskisser i samma anda som AMAze-kartan. De ska kunna bli
basen for ett enkelt textbaserat level-format senare.

```text
.  tom luft
S  startposition
E  mal/portal till nasta level
#  solid tile/mark/vagg
-  plattform man kan sta pa
?  block med power-up om spelaren slar underifran
H  dolt block som aktiveras underifran
*  kodfragment/poangcollectible
o  stor bonus eller RAM-upgrade
B  sonderbar block/vagg
v  studsande fiende
T  Taxman cameo-fiende
^  glitch/hazard
D  virtuell disknyckel
G  diskport eller annan gate
~  farlig strom/magnetisk yta
```

Regler for forsta implementation:

- Karta lagras i C som en array av strangar, precis som AMAze.
- Varje symbol motsvarar en tile eller entity-spawn.
- Runtime kan packa om kartan till separata lager: collision tiles,
  collectibles, entity spawns och triggers.
- Kartformatet ska senare kunna flyttas till asset pipeline utan att spelkoden
  behover andras mycket.

## Level 1: Motherboard Meadows

Introduktionsbana for kamera, hopp, collectibles, power-up-block och en enkel
fiende. Temat ar kretskort, kopparbanor och blinkande LEDs.

```text
................................................................
................................................................
.......................................................*........
......................*......?.......................-----....E.
.................----------.............*....................###
....................................--------....................
..........*..................H..........................v.......
......---------...........########..............----............
................................................................
....................v....................*......................
....S...........----------............----------................
########.......................?...........................#####
###########.............---------------..............###########
###############..................................###############
####################.............D...........###################
################################################################
```

## Level 2: Floppy Forest

Mer vertikal bana med disketter som plattformar, magnetisk fara och en virtuell
disknyckel som oppnar en gate. Ska testa backtracking och kamera over storre
hojdskillnader.

```text
................................................................
.......................................................G....E...
......................................................#######...
..............*.........................D.......................
..........---------..............---------------................
................................................................
....................~~~~~...................*...................
....S.....-----.................v.......----------..............
########........................................................
###########............?.........................H..............
.................-------------..............#########...........
................................................................
...........*..................v.........................*.......
.......---------...........########.................---------...
................................................................
################################################################
```

## Level 3: Memory Caverns

Tatare layout med RAM-block, sonderbara vaggar och alternativa routes. Ska testa
tile-kollision, breakable tiles och dolda block.

```text
................................................................
............................................................E...
.......................................................#########
.............*.................B.B.B.....................*......
.........---------..........#########...............---------...
.................................?..............................
....S...........B.B...........--------.........v................
########.....########...............................########....
................................................................
........H....................*.............B.B.B................
.....########.............----------.....#########..............
........................v.......................................
...............*......................*.........................
..........------------............------------..................
......................................................o.........
################################################################
```

## Level 4: Virus Core

Forsta mer aggressiva banan. Glitch-hazards, fler fiender och Taxman-cameo.
Ska testa palett-/glitch-effekter senare, men forsta slice kan rita det som
vanliga tiles.

```text
................................................................
.......................................................E........
.....................................................#####......
..........*.............^...........*...........................
......----------......#####.....----------..............T.......
....................................................########....
..................v......................~~~~~..................
....S.....----..........?...........---------------.............
########........................................................
###########............H....................*...................
.................-------------..........----------..............
..............................v.................................
...........*.............^^^^^^......................*..........
.......---------........########.................---------......
.....................................o..........................
################################################################
```

## Ramverksbehov

Det har samplet bor driva fram foljande ANA-omraden:

- `ANA_Tilemap` eller motsvarande helper for tilebaserad bana.
- `ANA_Camera` for varldskoordinater till skarmkoordinater.
- Tile-kollision: solid, one-way, hazard, breakable och bump-block.
- Spriteanimation med enkla frame-index och timing.
- Level assets fran text/manifest senare.
- Musik- och SFX-anvandning enligt ANA:s kanalpolicy, inklusive en enkel
  strategi for vilka kanaler spelet reserverar eller later SFX stjala.
- HUD/text-hjalpare som passar spelbilden utan tung redraw-kostnad.
- Debug stats for scrollande spel, framfor allt dirty pixels och c2p-kostnad.

## Forsta leverans

Forsta implementationen ska endast krava:

- Level 1 som spelbar bana.
- Byte med vanster/hoger, hopp och enkel dash.
- Kamera som foljer spelaren.
- `#`, `-`, `?`, `H`, `*`, `v` och `E`.
- Fullscreen-presentation med diskret HUD.
- Loopande bakgrundsmusik och grundlaggande ljudeffekter.
- A1200 normal- och debug-ADF.

Ovriga levels far ligga kvar som design- och formatprov tills grunden ar stabil.

## Implementationsstatus

Forsta spelbara slice finns i `examples/byte_brothers/`:

- `main.c` registrerar ANA-callbacks och kor PAL lores 320x256 vid 50 fps.
- `byte_brothers_game.c` ager fyra textkartor, plattformsfysik, kamera,
  collectibles, hazards, power-/hidden-/breakable-blocks, enkla patrullerande
  fiender och levelovergang.
- `byte_brothers_render.c` ritar fullscreen-HUD, tiles, spelare och fiender med
  dirty redraw. Spelet anvander `ANA_Camera` for world/screen-konvertering och
  1-pixelkamera for mjukare fallback-scroll. Den gor fortfarande explicit
  viewport-redraw vid scroll, eftersom ANA annu saknar tilemap- eller
  hardware-scroll-hjalpare for kontinuerlig plattformsscroll.
- `byte_brothers_assets.c` laddar SFX och loopad MOD enligt en enkel
  musik/SFX-kanalpolicy.

Byggmal:

```sh
make byte-brothers-a1200-adf
make byte-brothers-a1200-debug-adf
```

Kvar till senare: andra spelbara karaktaren Bit, riktiga sprite-assets och
animationer, textbaserade level-assets via asset pipeline, parallax/glitch-
effekter och mer avancerade fiendebeteenden.
