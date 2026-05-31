# Spec 019: Layered rendering roadmap

## Bakgrund

ANA ska inte forsoka optimera ett helt spel med en enda renderer. Amiga-
hardvaran passar battre for lager: playfields, sprites, blitter-Bobs, HUD och
enkla overlay-effekter har olika kostnadsmodell.

Nasta rendersteg bor darfor vara layer-baserat. Spelet beskriver vad varje
lager ar till for, medan ANA valjer snabbaste korrekta backend for aktuell
Amiga-profil.

## Princip

Ett lager deklarerar sin natur:

```c
ANA_LAYER_TILE_SCROLL
ANA_LAYER_SPRITES
ANA_LAYER_STATIC
ANA_LAYER_EFFECT
```

ANA mappar detta till en backend:

```c
ANA_BACKEND_PLANAR_SCROLL
ANA_BACKEND_BLITTER_BOBS
ANA_BACKEND_HARDWARE_SPRITES
ANA_BACKEND_DIRTY_RECTS
ANA_BACKEND_CHUNKY_C2P
```

Det viktiga kontraktet ar att spelkoden beskriver avsikten, inte Amiga-
detaljerna. Byte Brothers ska exempelvis kunna saga "scrollande tile-
bakgrund", "aktorer" och "HUD" utan att sjalv hantera bitplane pointers,
BPLCON1, blitterstrips eller dirty repair.

## Forsta malbild

Byte Brothers bor pa sikt besta av tre separata renderlager:

1. Side-scrollande tile/playfield-lager.
   - Planar Amiga-backend.
   - Bredare bitmap/playfield an skarmen.
   - Scroll via viewport/bitplane-offset och fine scroll.
   - Bara nya tile-kolumner/rader ritas in.

2. Sprite-/actor-lager.
   - Optimerat for rorliga objekt.
   - Hardware sprites dar de passar.
   - Blitter-Bobs/masked blits for fler eller storre objekt.
   - Ska inte tvinga omritning av hela bakgrunden.

3. Statisk overlay/HUD/text.
   - Dirty rects eller annan billig partial update.
   - Uppdateras bara nar innehall andras.
   - Ska vara separat fran scrollande playfield.

## C forst, assembler senare

Forsta implementationen bor goras i C och anvanda AmigaOS/graphics.library och
blitter dar det ar rimligt. Assembler ska inte vara startpunkten.

Assembler blir bara motiverat om matning visar en tydlig hot path, till exempel
tile-dekodning, masked Bob-rutin, egen copper/display setup eller cycle-tajt
innerloop for A500-lage.

## Nasta implementeringsordning

1. Stada nuvarande WIP och behall bara API-delar som hjalper layer-modellen.
2. Gor `ANA_SCROLL_BACKEND_HARDWARE` till ett sant planar-scroll-kontrakt.
   Forsta Amiga direct-present-steget finns: en bred planar side-scroll-bitmap
   med `RasInfo->RxOffset`.
3. Implementera minsta fungerande planar side-scroll-playfield.
   Forsta versionen ritar om hela world-playfieldet vid invalidation och
   anvander Byte Brothers som konsument.
4. Hall sprites/HUD utanfor scroll-playfieldet.
   Forsta versionen ritar actor-rectfills direkt i den offsetade planar-
   bitmapen och cachear HUD-bandet som planar data, sa kamerarorelse kan
   blitta HUD-cachen i stallet for att kora HUD-callbacken varje frame.
5. Byt fran full world-bitmap till overdraw/ringbuffer med blitterritade
   inkommande tile-strips.
6. Mat artefakter och FPS innan vidare optimering.

Om steg 3 inte snabbt visar verklig planar/hardware-scroll ska arbetet stoppas
och designen omvarderas innan mer tid laggs pa optimering.
