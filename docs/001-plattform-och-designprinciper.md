# Spec 001: Plattform och designprinciper

## Syfte

Definiera vad ANA 0.1 ar, vilka Amiga-miljoer som prioriteras och vilka tekniska principer som styr ramverket. Den har specen ska fungera som kompassen for resten av 0.1-arbetet.

ANA 0.1 ska ge en XNA/MonoGame-liknande arbetsmodell i C, men vara anpassad for Amiga och inte vara en direkt port av XNA.

## Mal for 0.1

- C API, inte C++.
- AmigaOS-vanlig runtime forst, inte bare metal.
- Primart OCS/ECS.
- Forsta skarmlaget ar PAL 320x256.
- Fast 50 Hz update-loop.
- Prestanda nara handskriven vanilla C.
- Enkel nog att forsta genom exempelspel.

## Designprinciper

- Inga dolda allokeringar i `update` eller `draw`.
- Assets ska vara forkonverterade till Amiga-vanliga format.
- Runtime ska gora sa lite implicit arbete som mojligt.
- API:t ska vara litet, tydligt och C-idiomatiskt.
- Debug-builds far ha extra kontroller; release-builds ska kunna stanga av dem.
- Funktioner som mappar direkt mot Amiga-hardvara ska inte gomma viktiga begransningar.
- ANA ska hjalpa spelkod att bli tydlig, inte forsoka bli en stor generell motor.

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
- Det finns en tydlig grans mellan ANA 0.1 och senare ambitionsnivaer.

