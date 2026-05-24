# Spec 012: Dokumentation och releasepaket

## Syfte

Gora ANA 0.1 begripligt, byggbart och testbart for andra utvecklare.

## Mal for 0.1

- README med tydlig introduktion.
- "Your first ANA game"-guide.
- API-oversikt.
- Asset pipeline-guide.
- Build-instruktioner.
- Kort performance guide.
- Lista over kanda begransningar.
- Licens och release-notes.

## Dokumentationsinnehall

Minsta dokumentation for publik 0.1:

- vad ANA ar
- vilka Amiga-mal 0.1 stoder
- hur man bygger `libana`
- hur man bygger och kor `examples/hello`
- hur man bygger och kor `examples/invaders`
- hur man konverterar assets
- hur game loop-modellen fungerar
- vilka delar av API:t som ar stabila nog for 0.1

## Releasepaket

Ett 0.1-paket bor innehalla:

- kallkod
- headers
- byggfiler
- `ana-convert`
- exempelspel
- exempelassets eller assetkallor
- dokumentation
- licens

Om binarer inkluderas ska det vara tydligt vilken toolchain och malmiljo de ar byggda for.

## Performance guide

Kort guide med praktiska regler:

- allokera inte i `update`/`draw`
- forkonvertera assets
- hall antal ritade images realistiskt for OCS/ECS
- anvand palett och bitplanes medvetet
- optimera hot paths med lag-niva C eller assembler nar ramverkets standardvag inte racker
- mata verklig prestanda i emulator och pa hardvara nar mojligt

## Inte i 0.1

- Komplett bok/manual.
- API-stabilitetslofte for alla framtida versioner.
- Automatisk dokumentationssajt.
- Full tutorialserie.

## Acceptanskriterier

- En ny utvecklare kan bygga och kora `hello`.
- En ny utvecklare kan bygga och kora `ana-invaders`.
- En ny utvecklare kan konvertera minst en bildasset och ladda den i ett exempel.
- Det framgar tydligt vad ANA 0.1 klarar och vad som ligger utanfor scope.

## Implementationsstatus

- README ar nu en kort ingangsida med lankar till praktiska docs.
- `docs/getting-started.md` beskriver host-build, Amiga-build, ADF-build och ett
  forsta minimalt ANA-spel.
- `docs/api-overview.md` sammanfattar den publika 0.1-API-ytan.
- `docs/asset-pipeline-guide.md` beskriver aktuell `.anaimg`-pipeline,
  spritesheets, transparens och Invaders-assets.
- `docs/build-and-release.md` beskriver build-output, CI-output och
  releasepaketet.
- `docs/performance-guide.md` beskriver praktiska prestandaregler och
  nuvarande A1200-baseline.
- `docs/known-limitations.md` listar avgransningar for 0.1.
- `docs/release-notes-0.1.md` ar ett draft-underlag for senare publik release.
- `docs/development-routine.md` satter en rutin for att granska README och
  berorda docs efter varje forandring.
- `make release-package` skapar ett kallkodspaket under `build/release/`.
