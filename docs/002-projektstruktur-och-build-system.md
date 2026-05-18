# Spec 002: Projektstruktur och build system

## Syfte

Skapa en ren repo- och build-struktur dar ANA kan byggas som ett bibliotek och anvandas av exempelspel utan att runtime-kod kopieras runt.

## Mal for 0.1

- `libana` byggs som ett statiskt C-bibliotek.
- Exempelspel lankar mot `libana`.
- Verktyg som `ana-convert` ligger separerade fran Amiga-runtime.
- En ny utvecklare ska kunna bygga allt med ett dokumenterat kommando.

## Foreslagen struktur

```text
include/
  ana/
    ana.h
    ana_core.h
    ana_gfx.h
    ana_input.h
    ana_audio.h
src/
  core/
  gfx/
  input/
  audio/
tools/
  ana-convert/
examples/
  hello/
  invaders/
docs/
```

## Build-flode

0.1 bor borja med ett enkelt build-flode, exempelvis Makefile-baserat. Det viktigaste ar att det ar transparent och fungerar bra for cross-build.

Builden ska kunna gora minst:

- bygga `libana.a`
- bygga `examples/hello`
- bygga `examples/invaders`
- bygga `tools/ana-convert`
- rensa build-output

## Toolchain

Dokumentationen ska ange:

- rekommenderad C-kompilator/toolchain
- hur include paths satts
- hur `libana` lankas
- hur output flyttas eller paketeras for emulator/test

## Inte i 0.1

- Avancerat dependency management.
- Flera samtidiga buildsystem om det inte behovs.
- Automatisk publicering/releasepipeline.
- Stod for alla Amiga-kompilatorer fran start.

## Acceptanskriterier

- `libana` och exempelspelen kan byggas med ett kommando.
- Exempelspelen anvander publika ANA-headers och lankar mot biblioteket.
- `ana-convert` byggs som ett host-verktyg, inte som Amiga-runtime.
- Repo-strukturen ar tillrackligt tydlig for att nya specs kan placera filer utan gissningar.

