# Spec 011: ANA Invaders showcase

## Syfte

Bygga ett litet, komplett Space Invaders-liknande spel som visar att ANA 0.1 gar att anvanda for ett riktigt Amiga-spel, inte bara for teknikdemos.

## Mal for 0.1

`ana-invaders` ska vara huvudshowcase for ANA 0.1 och driva prioriteringen av ramverkets API.

Spelet ska demonstrera:

- game loop
- input
- image-rendering
- animation
- text/score
- ljudeffekter
- enkel kollisionslogik
- game states
- asset pipeline

## Spelinnehall

Miniminiva for 0.1:

- title screen
- starta spel
- spelare som ror sig vanster/hoger
- spelarskott
- fiendeformation
- fiender som ror sig horisontellt och nedat
- fiender som skjuter
- skyddsblock som stoppar och bryts ner av skott
- kollision mellan skott och fiender
- kollision mellan fiendeskott och spelare
- explosioner
- score
- lives
- game over
- restart

Nuvarande exempel har title screen, styrbar spelarsprite, flera samtidiga
spelarskott pa `ANA_ACTION_1`/Space/fire, en liten bitmapfont-HUD for score,
lives och status, en fiendeformation med tva animation frames, fiendeskott,
skyddsblock, skottkollision, poang per traff, enkla explosioner, game over,
clear och restart.

Renderingen ar optimerad for Amiga genom att HUD och fiendeformation ligger kvar
i respektive draw-buffer tills de andras. Spelet rensar sedan bara gamla
player-, bullet- och explosionsrektanglar med `ana_fill_rect`, och ritar om
fiender lokalt nar ett rorligt objekt passerat over formationen. Vid traff
rensas bara den borttagna fiendens rektangel i varje buffer, och explosioner
lamnas kvar tills deras animation frame faktiskt byts.

## Grafik och ljud

Showcaset ska ha hyfsad polish:

- tydlig 16-fargers palett
- lasbar bitmapfont
- minst ett par animation frames for fiender
- enkel explosionanimation
- klassiska skyddsblock
- skottljud
- explosionsljud
- enemy step-ljud eller motsvarande rytm
- game over-ljud om det ryms

## Kodstil

Spelets kod ska vara ett exempel pa hur ANA ar tankt att anvandas:

- anvand ANA:s publika API for runtime, grafik, input, text och ljud
- hall speciallogik i spelet, inte i ramverket
- anvand de vanliga hog-niva API:erna i forsta hand, exempelvis `ana_draw_image` och `ana_play_sound`
- direkt hardvarukod, egen C eller assembler far anvandas nar det ar en medveten optimering och dokumenteras som en escape hatch
- hall state och regler tillrackligt tydliga for lasare

Exemplet ar uppdelat sa att `examples/invaders/main.c` visar den normala
ANA-applikationsformen med `ANA_Game` och callbacks. Spelreglerna ligger i
`invaders_game.c`, medan den optimerade dirty-rect-renderingen ligger i
`invaders_render.c`. Renderern ager draw-slot state, retained BOB-state och
dirty flags; spelreglerna anropar bara render helpers nar formation, shields
eller hela skarmen maste markeras om.

## Inte i 0.1

- Exakt arkadklon.
- Alla originalets fiendetyper eller regler.
- High score-persistens om det tar fokus fran ramverket.
- Menyer bortom title/restart.
- Flera banor om en vag balanserad niva racker.

## Acceptanskriterier

- Spelet ar spelbart fran title screen till game over.
- Spelet anvander ANA for de centrala systemen.
- Spelet kanns tillrackligt komplett for en publik 0.1-demo.
- Kodbasen fungerar som larande exempel for nya ANA-anvandare.

## Implementationsstatus

- Title, playing, clear och game over finns som separata states.
- Lives raknas ner vid spelartraff; vid sista traff visas game over.
- Aliens som nar spelaren ger game over.
- Space/fire startar spelet fran title och restartar fran clear/game over.
- Skyddsblock stoppar bade spelar- och fiendeskott, bryts ner cell for cell,
  och forstors dar aliens ror sig igenom dem.
- `main.c` ar ett kort entrypoint-exempel; spelregler och rendering ar
  uppdelade i separata moduler.
- Normal-ADF, debug-ADF och sync-ADF bygger samma showcase-assets.
