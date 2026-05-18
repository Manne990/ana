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
- kollision mellan skott och fiender
- kollision mellan fiendeskott och spelare
- explosioner
- score
- lives
- game over
- restart

## Grafik och ljud

Showcaset ska ha hyfsad polish:

- tydlig 16-fargers palett
- lasbar bitmapfont
- minst ett par animation frames for fiender
- enkel explosionanimation
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
