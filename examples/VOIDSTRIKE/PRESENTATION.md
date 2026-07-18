# VOIDSTRIKE presentation contract

This package is the original, deterministic source art for the Null Foundry
presentation. It is deliberately separate from game rules: gameplay chooses a
named asset and a frame; these assets never imply damage, collision, cadence,
or spawn behavior.

## Runtime contract

Build `assets/assets.ana` into the normal VOIDSTRIKE asset output directory.
The runtime loads the names below with the same host/Amiga root convention as
the maintained ANA examples. `#000000` is transparency and `voidstrike` is the
single 16-colour palette.

| Gameplay role | Asset name | Source dimensions | Frame contract |
| --- | --- | ---: | --- |
| Base craft | `player_base` | 36x28 | one frame |
| Speed module | `player_speed` | 36x28 | one frame |
| Twin Shot module | `player_twin` | 36x28 | one frame |
| Wide Shot module | `player_wide` | 36x28 | one frame |
| Laser module | `player_laser` | 36x28 | one frame |
| Ground turret | `defense_node` | 24x20 | one frame |
| Ground vehicle | `maintenance_crawler` | 28x16 | one frame |
| Air formation member | `security_drone` | 60x20 | three 20x20 frames |
| Boss | `reactor_guardian` | 112x56 | one frame |
| Player / hostile bolts | `player_shot`, `hostile_shot` | 8x12 / 8x8 | one frame |
| Energy core | `energy_core` | 16x16 | one frame |
| Explosion | `explosion` | 96x16 | six 16x16 frames |
| Terrain kit | `null_foundry_tiles` | 192x16 | twelve 16x16 frames |
| Bottom module dock | `module_dock` | 128x16 | four 32x16 frames |
| Title logo | `title_wordmark` | 192x40 | one frame |

The individual player assets show each installed module on a shared base-craft
spine. If game rules allow multiple modules simultaneously, the renderer should
compose the matching overlays or use the most recently installed asset until a
combined-sheet follow-up is added; it must not silently change the collision
shape to match unused transparent padding.

## Source and provenance

`assets/generate_source_art.py` generates every committed PPM source image from
original, small pixel-cluster geometry. It uses no private inspiration input,
external image, extraction, trace, or palette sampling. Run it from its own
directory after editing the generator:

```sh
python3 generate_source_art.py
```

`assets/presentation_board.ppm` is a generated 320x256 native-resolution
readability board. It is intentionally not packed into the runtime manifest;
it provides a durable visual review surface for the title band, calm central
lane, terrain masses, boss core, player growth, hostile shot, energy core, and
module dock before integrated FS-UAE capture exists.

The PPMs and `.anasfx` recipes are licensed under the repository's MIT license,
authored for VOIDSTRIKE by the Gaia team on 2026-07-18. The candidate product
owner MOD is intentionally not referenced or copied. Its SHA-256 is
`baa0604d129006216978b6992047311847fe3df031b9ab48ddd727499753d7ff`; it
identifies itself as `memorydust` / "made by codex of razor 1911", and no
explicit compatible redistribution license has been found. It remains a hard
release gate.

## SFX event names

`fire`, `pickup`, `install`, `explosion`, `player_death`, and `victory` are
short original sound recipes. The game owns when they play. Runtime audio must
retain ANA's music/SFX channel policy and may omit music until the supplied MOD
has a recorded compatible license.
