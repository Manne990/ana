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
- hall antal BOBs realistiskt for OCS/ECS
- anvand palett och bitplanes medvetet
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
