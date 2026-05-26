# Spec 013: Asset pipeline 0.2

## Syfte

Gora ANA:s assetflode mer likt den trevliga delen av XNA Content Pipeline:
lagg kallfiler i projektet, bygg, och ladda ett forkonverterat asset i spelet.

ANA ska inte kopiera XNA:s `.xnb`-system eller forsoka dolja Amigans
begransningar. Malet ar ett litet, begripligt host-flode som gor ratt sak
innan spelet startar, sa runtime slipper dyr avkodning, fargmatchning och
formatlogik.

## Mal for 0.2

- PNG-input for bilder.
- PNG/PPM-input for fasta bitmapfonter.
- Gemensam palettfil for ett spel eller exempel.
- Automatisk asset-build via `make`.
- Ett enkelt manifestformat for bilder, bitmapfonter och pass-through assets.
- Tydliga fel nar en asset inte passar malprofilen.
- Invaders ska kunna byggas fran kallassets istallet for handbyggda byte-arrays.

## Begrepp

- Source asset: originalfil pa utvecklingsdatorn, till exempel `player.png`.
- Built asset: ANA-format som laddas av runtime, till exempel `player.anaimg`.
- Palette asset: gemensam palett som styr hur source assets konverteras.
- Manifest: enkel textfil som listar assets och konverteringsregler.

## Foreslaget arbetsflode

Ett spelprojekt kan ha denna struktur:

```text
examples/invaders/
  assets/
    assets.ana
    palette.png
    hud_font.png
    player.png
    invader.png
    explosion.png
  main.c
```

Utvecklaren kor:

```sh
make examples/invaders-assets
make amiga-examples
make adfs
```

Make bygger da:

```text
build/assets/invaders/game.anapal
build/assets/invaders/player.anaimg
build/assets/invaders/invader.anaimg
build/assets/invaders/explosion.anaimg
build/assets/invaders/hud.anafnt
```

Spelet laddar sedan assets via vanliga runtime-API:er:

```c
player = ana_load_image("assets/player.anaimg");
```

For sma smoke tests ska `ana_load_image_data` fortsatt fungera, men riktiga
exempel ska i forsta hand visa filbaserat assetflode.

## `ana-convert` CLI

Direkt konvertering ska fortsatt fungera:

```sh
ana-convert image player.png --out player.anaimg --palette game.anapal --transparent '#ff00ff'
ana-convert image explosion.png --out explosion.anaimg --palette game.anapal --frame-width 16 --frame-height 16
ana-convert font hud_font.png --out hud.anafnt --colors 2 --char-width 6 --char-height 7 --first-char 48 --chars 43 --transparent '#ff00ff'
ana-convert sound fire.anasfx --out fire.anasnd
```

Palett kan skapas fran en masterbild:

```sh
ana-convert palette palette.png --out game.anapal --colors 16
```

Manifest kan byggas i ett steg:

```sh
ana-convert build assets/assets.ana --out build/assets/invaders
```

## Palettformat

0.2 introducerar ett enkelt textformat for paletter. Det ska vara latt att
skriva, diff:a och felsoka.

```text
ANA_PALETTE 1
0 #000000
1 #ffffff
2 #ffff00
3 #00ff00
4 #ff0000
```

Regler:

- Max 16 farger for basprofilen.
- Index ar explicit 0-15.
- Hexformat `#RRGGBB` ar obligatoriskt i 0.2.
- Saknade index ar fel om en image refererar till dem.
- Verktyget quantizar till Amigans 12-bit RGB nar det skriver runtime-data,
  men felmeddelanden ska visa originalfargen.

## Manifestformat

Manifestet ska vara radbaserat och avsiktligt mindre avancerat an XNA:s
content project. 0.2 fokuserar pa bilder, fasta bitmapfonter och enkla SFX-
recept, men kan aven kopiera `.mod`-musik utan runtime-konvertering.

```text
ANA_ASSETS 1

palette game palette.png --colors 16

image player player.png --palette game --transparent #ff00ff
image invader invader.png --palette game
image explosion explosion.png --palette game --frame-width 16 --frame-height 16 --transparent #ff00ff
font hud hud_font.png --colors 2 --char-width 6 --char-height 7 --first-char 48 --chars 43 --transparent #ff00ff
sound fire fire.anasfx
music theme theme.mod
```

Regler:

- Forsta token ar asset-typ: `palette`, `image`, `font`, `sound` eller
  `music`.
- Andra token ar asset-namn.
- Tredje token ar source-fil relativt manifestet.
- Outputnamn skapas fran asset-namnet: `player.anaimg`, `hud.anafnt`,
  `fire.anasnd`, `game.anapal`, `theme.mod`.
- CLI-flaggor efter source-filen fungerar som for direktkommandot.
- `music` tar inga flaggor i forsta versionen utan kopierar `.mod` rakt av.
  Runtime laddar sedan MOD-filen med `ana_load_music`.
- Manifestet far inte krava JSON, XML eller ett externt byggsystem.

## PNG-stod

PNG-dekodning sker endast i host-verktyget, aldrig i Amiga-runtime.

Krav:

- Lasa indexed, RGB och RGBA PNG.
- Alpha 0 ska kunna bli maskdata.
- `--transparent #RRGGBB` ska fortsatt fungera.
- Om en pixel inte finns i vald palett ska verktyget ge ett tydligt fel.
- Automatisk narmaste-farg-matchning ar inte default i 0.2.

Tillaten implementation:

- En liten vendrad host-only PNG-dekoder, till exempel `stb_image`, ar okej.
- Ingen PNG-kod far lankas in i `libana.a`.

## Makefile-integration

Repo:t ska ha tydliga targets:

```sh
make assets
make examples/invaders-assets
make clean-assets
```

Krav:

- Asset-build ska skapa output under `build/assets/`.
- Amiga-exempel ska bero pa sina built assets nar det ar praktiskt.
- CI ska bygga assets innan exempel och ADF:er.
- Om source-filer saknas ska felet peka pa manifestet och filnamnet.

0.2 behover inte perfekt dependency tracking per enskild PNG. Det ar acceptabelt
att bygga om alla assets i ett manifest nar manifestet eller en source asset ar
nyare.

## Runtime-kontrakt

Runtime ska fortsatt vara enkel:

- `ana_load_image(path)` laddar `.anaimg`.
- `ana_load_font(path)` laddar `.anafnt`.
- `ana_load_image_data(bytes, size)` finns kvar for inbaddade exempel.
- Runtime validerar ANA-formatets header och storlek.
- Runtime forsoker inte ladda PNG, tolka manifest eller matcha palett.

## Relation till XNA

Detta ar inspirerat av XNA:s Content Pipeline pa tre satt:

- Source assets ar separerade fran runtime-format.
- Konvertering sker vid build-tid.
- Spelet laddar assets via enkla hog-niva begrepp.

Detta ar medvetet annorlunda:

- Ingen generisk typbaserad `ContentManager.Load<T>()` i 0.2.
- Inga importer/processor-klasser.
- Inget binart content project-format.
- Inga dolda runtime-dependencies.

## Felhantering

`ana-convert` ska returnera non-zero vid fel och skriva fel som ar begripliga
for en spelutvecklare:

- for manga farger
- pixel utanfor palett
- ogiltig transparent farg
- frame-storlek delar inte image exakt
- source-fil saknas
- output kan inte skrivas

Felmeddelanden bor innehalla filnamn och, nar rimligt, pixelposition.

## Inte i 0.2

- Ljudkonvertering.
- XNA project import.
- Hot reload.
- AI-genererade assets.
- Avancerad fargkvantisering eller dithering.
- Packade asset bundles.

## Acceptanskriterier

- `ana-convert image` kan lasa PNG och skriva `.anaimg`.
- `ana-convert font` kan lasa PNG/PPM och skriva `.anafnt`.
- `ana-convert palette` kan skapa `.anapal` fran PNG.
- `ana-convert build` kan bygga ett enkelt manifest med palett, flera bilder och
  en bitmapfont.
- Invaders assets kan ligga som source-filer och byggas till `build/assets/`.
- CI bygger och testar assetflodet.
- Dokumentationen visar ett komplett exempel fran PNG till `ana_load_image`.

## Implementationsstatus

- `ana-convert image` laser PNG och PPM P3/P6.
- `ana-convert image` kan anvanda `--palette game.anapal`.
- `ana-convert palette` skapar textbaserade `.anapal`-filer fran PNG/PPM.
- `ana-convert build` bygger ett radbaserat `ANA_ASSETS 1`-manifest.
- `ana-convert font` bygger fasta bitmapfonter ovanpa image-formatet.
- `ana-convert sound` bygger `.anasnd` fran enkla `.anasfx`-recept.
- PNG-dekodning ligger vendrad och host-only i `tools/ana-convert/vendor/`.
- `make assets`, `make examples/invaders-assets` och `make clean-assets` finns.
- Invaders bildassets ligger som PNG-source assets under
  `examples/invaders/assets/` och byggs till `.anaimg`.
- Invaders HUD-font ligger som source asset och byggs till `.anafnt`.
- Invaders SFX ligger som `.anasfx` source assets och byggs till `.anasnd`.
- Den gamla kodbaserade `.anaimg`-genereringen for Invaders ar borttagen.
- Testsviten bygger PPM, PNG, `.anapal`, `.anafnt`, `.anasnd` och
  manifestfloden.
