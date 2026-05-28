# Spec 001: Plattform och designprinciper

## Syfte

Definiera vad ANA 0.1 ar, vilka Amiga-miljoer som prioriteras och vilka tekniska principer som styr ramverket. Den har specen ska fungera som kompassen for resten av 0.1-arbetet.

ANA 0.1 ska ge en XNA/MonoGame-liknande arbetsmodell i C, men vara anpassad for Amiga och inte vara en direkt port av XNA.

Den primara malgruppen ar utvecklare som vill skriva spel pa en hogre niva an Amiga assembler eller handskriven vanilla C. ANA ska gora den enkla vagen trevlig, utan att stanga dorren for lag-niva C eller assembler nar ett spel behover mer kontroll.

## Mal for 0.1

- C API, inte C++.
- AmigaOS-vanlig runtime forst, inte bare metal.
- Primart OCS/ECS.
- Forsta skarmlaget ar PAL 320x256.
- Fast 50 Hz update-loop.
- Prestanda nara handskriven vanilla C.
- Enkel nog att forsta genom exempelspel.
- Publika namn ska vara begripliga for ramverksvana utvecklare, inte bara Amiga-specialister.

## Designprinciper

- Inga dolda allokeringar i `update` eller `draw`.
- Assets ska vara forkonverterade till Amiga-vanliga format.
- Runtime ska gora sa lite implicit arbete som mojligt.
- API:t ska vara litet, tydligt och C-idiomatiskt.
- Bas-API:t ska anvanda hog-niva namn som `ANA_Image`, `ANA_Font` och `ANA_Sound`.
- Amiga-specifika begrepp som BOBs, bitplanes, blitter och hardware sprites far finnas i implementationen eller i advanced-API, men ska inte kravs i forsta tutorialen.
- ANA ska valja den snabbaste korrekta Amiga-tekniken bakom ett hog-niva-API. Spelkoden beskriver avsikten, till exempel statisk dirty rendering eller scrollande tilemap, medan backend kan anvanda dirty rects, blitter, bitplane-layout, hardware scroll eller andra Amiga-specifika tekniker utan att spelet byter API.
- ANA ska erbjuda escape hatches for egen C, AmigaOS-anrop eller assembler dar det ar praktiskt.
- Debug-builds far ha extra kontroller; release-builds ska kunna stanga av dem.
- Funktioner som mappar direkt mot Amiga-hardvara ska inte gomma viktiga begransningar.
- ANA ska hjalpa spelkod att bli tydlig, inte forsoka bli en stor generell motor.

## Namngivning

0.1 ska anvanda hog-niva namn i bas-API:t:

- `ANA_Image` for ritbara bildassets.
- `ANA_Font` for bitmapfont-assets.
- `ANA_Sound` for korta ljudassets.
- `ANA_Game` och `ANA_Time` for runtime-modellen.

Amiga-specifika namn ska anvandas forsiktigt:

- `ANA_Bob` bor inte vara ett nyborjarbegrepp i 0.1, aven om implementationen kan anvanda BOB-tekniker.
- `ANA_Sprite` ska undvikas som generellt namn tills vi vet om det betyder logisk spelsprite eller Amiga hardware sprite.
- Om hardware sprites exponeras senare bor de fa ett tydligt advanced-namn, exempelvis `ANA_HWSprite`.

## Plattform

0.1 siktar pa:

- Amiga 500-liknande OCS/ECS-mal.
- PAL 50 Hz.
- 320x256.
- 4 bitplanes / 16 farger som basprofil.
- Joystick och tangentbord.
- Kortare sample-baserade ljudeffekter.

## Inte i 0.1

- AGA-specifika funktioner.
- 3D.
- Musiksystem/tracker playback, om det riskerar att forsena 0.1.
- Dynamisk runtime-dekodning av PNG, WAV eller andra tunga moderna format.
- Generell fysikmotor.
- Full UI- eller scenhanteringsmotor.

## Acceptanskriterier

- Det ar tydligt vilka maskiner och konfigurationer ANA 0.1 riktar sig mot.
- Varje annan 0.1-spec kan harleda sina begransningar fran denna spec.
- Det finns en uttalad regel om att `update` och `draw` ska vara fria fran dolda allokeringar.
- Det finns en uttalad regel om att bas-API:t ska vara nyborjarvanligt och inte Amiga-jargongdrivet.
- Det finns en uttalad escape hatch-princip for lag-niva optimering.
- Det finns en tydlig grans mellan ANA 0.1 och senare ambitionsnivaer.
