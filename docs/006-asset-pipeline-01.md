# Spec 006: Asset pipeline 0.1

## Syfte

Flytta tung assetbearbetning till modern dator sa att Amiga-runtime bara laddar data som redan ar formaterad for malhardvaran.

## Mal for 0.1

- Ett host-verktyg: `ana-convert`.
- Konvertera PNG till ANA image-format.
- Hantera 16-fargers palett for basprofilen.
- Skapa maskdata fran alpha eller transparent farg.
- Generera metadata for frames/spritesheets.
- Dokumentera outputformatet tillrackligt for runtime och framtida verktyg.

## Foreslaget CLI

```bash
ana-convert image invader.png --colors 16 --out invader.anaimg
ana-convert image player.png --palette game.pal --out player.anaimg
ana-convert image explosion.png --frame-width 16 --frame-height 16 --out explosion.anaimg
```

## Initial implementation

For att fa ett komplett flode utan externa dependencies borjar `ana-convert`
med PPM som inputformat:

```bash
ana-convert image invader.ppm --out invader.anaimg --colors 16
ana-convert image explosion_sheet.ppm --out explosion.anaimg --colors 16 --frame-width 16 --frame-height 16
ana-convert image player.ppm --out player.anaimg --transparent 255,0,255
ana-convert image player.ppm --out player.anaimg --transparent '#ff00ff'
```

Stodet ar avsiktligt smalt:

- PPM P3 och P6 kan lasas.
- Unika RGB-farger samlas i encounter order upp till `--colors`, max 16.
- `--transparent` skapar maskdata i `.anaimg`.
- `--frame-width` och `--frame-height` delar en spritesheet i frames radvis.
- Output skrivs i `.anaimg`-formatet som dokumenteras i Spec 005.

PNG ar fortfarande malformatet for ett bekvamare 0.1-flode, men PPM-flodet
gor att runtime, image-rendering och CI kan validera pipeline-beteendet redan
nu.

## Bildkonvertering

Verktyget ska kunna:

- lasa PNG pa host-datorn
- validera eller skapa palett
- skriva planar bilddata
- skriva maskdata
- dela spritesheet i frames
- ge tydliga felmeddelanden nar bilden inte passar malprofilen

## Palettflode

0.1 bor stodja minst ett av dessa floden:

- gemensam palettfil for hela spelet
- palett extraherad fran en masterbild
- explicit palett via CLI-argument

Invaders-exemplet ska anvanda ett dokumenterat palettflode.

## Ljudkonvertering

Om ljud hinner ingar antingen:

- samma `ana-convert` med `sound`-kommando
- separat senare verktyg

Ljudkonvertering far inte blockera grafikpipeline for 0.1.

## Inte i 0.1

- Runtime-PNG-laddning.
- Automatisk avancerad fargkvantisering om enkel palettvalidering racker.
- Komplett content project-format som XNA Content Pipeline.
- Hot reload.

## Acceptanskriterier

- Alla grafikassets till `ana-invaders` kan genereras fran kallbilder.
- Runtime kan ladda outputfiler utan extra konvertering.
- Fel i assetformat eller palett upptacks i verktyget dar det ar rimligt.
- Outputformatet ar dokumenterat i repo:t.
