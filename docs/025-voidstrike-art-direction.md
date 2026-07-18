# Spec 025: VOIDSTRIKE Art Direction

## Purpose

Define an original, production-ready visual direction for VOIDSTRIKE. This
document translates private inspiration material into abstract design qualities
and then turns those qualities into a distinct palette, shape language, HUD,
terrain system, player silhouette, enemy families, effects, asset workflow, and
originality boundary.

The inspiration images are reference material only. They are not source assets,
must not be traced, sampled, edited into new assets, or included in public
builds, captures, repositories, or the Build Week submission unless their rights
are independently cleared.

This document complements:

- [Spec 023: Build Week Vertical Shooter Sample](023-build-week-vertical-shooter-sample.md)
- [Spec 024: VOIDSTRIKE Ways of Working](024-voidstrike-ways-of-working.md)

## Abstract art-direction brief

VOIDSTRIKE should feel like a fast military flight through the interior of an
enormous abandoned orbital machine. The world is constructed, functional, and
hostile rather than organic or decorative. Cold metal and deep space dominate
the image; compact fields of electric color communicate danger, energy,
collectibles, and player power.

The visual identity should combine:

- top-down industrial science fiction
- dense but readable mechanical terrain
- strong separation between playfield, actors, projectiles, and HUD
- a small base craft that visibly grows through attached modules
- restrained animation with decisive flashes and impact frames
- a limited 16-color palette used consistently across every asset
- crisp pixel clusters designed for PAL lores rather than downscaled artwork
- a functional arcade-machine presentation with no ornamental interface chrome

The emotional tone is:

```text
cold / mechanical / lonely / precise / dangerous / energized
```

The game should not look nostalgic because it imitates a particular commercial
title. It should feel authentically Amiga because its composition, palette,
animation, and rendering decisions are made for the actual hardware target.

## Creative premise

The level takes place above and within a dormant orbital foundry called the
**Null Foundry**. VOIDSTRIKE enters through an exposed service trench and moves
toward the machine's central control reactor.

This premise supports the required game content without requiring story scenes:

- ground turrets are foundry defense nodes
- ground vehicles are maintenance crawlers repurposed as interceptors
- air formations are compact security drones
- energy cores are recovered machine fragments
- installed player modules are visible salvaged components
- the boss is the foundry's central defense intelligence embodied as a large
  reactor guardian

The names are working vocabulary for visual production. Gameplay and Build Week
materials may simplify them if another original setting is chosen, but the final
asset set must retain one coherent premise.

## Visual design pillars

### 1. Readable at native size

Every asset is designed at its intended pixel dimensions and reviewed at 1x.
Nearest-neighbor enlargement is useful for inspection but must not become the
primary design view. A projectile, pickup, collision threat, or selected module
must remain identifiable on a 320x256 display in motion.

### 2. Function before decoration

Terrain communicates structure, not noise. Actors communicate behavior through
silhouette, facing, and color. HUD elements communicate state with minimal
chrome. Decorative detail must not compete with enemy bullets or energy cores.

### 3. Modular growth is the hero image

VOIDSTRIKE begins as a narrow, agile central craft. Installed modules attach to
clearly visible hardpoints and change its silhouette. A screenshot should reveal
roughly how upgraded the craft is even when the HUD is cropped out.

### 4. Cold world, hot gameplay

The environment uses the dark and cool half of the palette. Player shots,
pickups, damage, hazards, and selection feedback use the brighter accent colors.
Bright color is a gameplay resource and should not be spent on arbitrary
background decoration.

### 5. Original construction

Sprites and tiles are built from VOIDSTRIKE's own geometry and constraints. No
asset begins as a traced commercial screenshot, extracted sprite, recolor, or
pixel-for-pixel layout study.

## Technical visual constraints

- Target display: PAL lores, 320x256.
- Target palette: 16 indexed colors, four bitplanes.
- Pixel work: integer coordinates and hard pixel edges only.
- Scaling in tools or documentation: nearest-neighbor only.
- Transparent sprite index: palette index 0 unless ANA's final manifest requires
  a different explicit index.
- Primary terrain grid: 16x16 tiles.
- Small effects may use 8x8 cells or tightly cropped frames.
- Avoid gradients that require dithering over large moving regions.
- Avoid single-pixel high-frequency texture across the whole playfield; it makes
  scrolling shimmer and increases visual noise.
- Keep masked actor bounds tight to reduce redraw and collision ambiguity.
- Reuse palette entries rather than introducing per-asset private palettes.

## Provisional 16-color palette

The palette uses Amiga-friendly 12-bit color steps expanded to 8-bit hex. Exact
values may be tuned once representative assets are viewed in FS-UAE, but index
roles should remain stable after the first playable art pass.

| Index | Hex | Role |
| ---: | --- | --- |
| 0 | `#000000` | Transparency, void, deepest separation |
| 1 | `#111122` | Deep navy-black background |
| 2 | `#222233` | Dark armor and recesses |
| 3 | `#334455` | Shadowed steel |
| 4 | `#556677` | Mid steel and terrain body |
| 5 | `#778899` | Light steel and secondary edges |
| 6 | `#AABBCC` | Bright metal and readable highlights |
| 7 | `#DDEEFF` | White-hot highlight, text, player core |
| 8 | `#003366` | Deep technical blue |
| 9 | `#0055AA` | Cobalt panels and player shadow color |
| 10 | `#00AADD` | Electric cyan, player energy and selection |
| 11 | `#116633` | Dark energy conduit and safe inactive state |
| 12 | `#44DD77` | Energy core, positive pickup, ready state |
| 13 | `#FFAA22` | Amber machinery and projectile warning |
| 14 | `#FFDD44` | Hazard highlight and high-priority telegraph |
| 15 | `#DD3344` | Damage, hostile fire, critical warning |

### Palette policy

- Terrain primarily uses indices 1-6 and 8-9.
- The player primarily uses 5-7, 9-10, and small amounts of 12.
- Normal enemies use 2-6 plus one behavior accent.
- Hostile projectiles primarily use 13-15.
- Pickups primarily use 10, 12, and 14.
- HUD text uses 6 or 7 against 1 or 2.
- White, yellow, green, and red must remain scarce enough to preserve their
  information value.
- Damage flashes may temporarily remap an actor toward 7 and 15, but the whole
  screen should not flash repeatedly.

The first palette review must include a representative gameplay screen, not
isolated swatches.

## Shape language

VOIDSTRIKE uses three related but distinct geometric vocabularies.

### Human/player technology

- narrow central spine
- forward-pointing chevrons
- paired but not perfectly mirrored module hardpoints
- compact swept surfaces
- exposed cyan energy seams
- light central cockpit or reactor core
- readable negative space between attached modules

Player geometry should suggest an adaptable interceptor assembled for speed,
not a broad flying wing.

### Foundry terrain and ground systems

- rectangular armor plates
- rails, conduits, access channels, vents, and recessed machinery
- repeated bolts or seams used sparingly and at meaningful joins
- chamfered corners rather than rounded consumer-product forms
- dark cavities surrounded by brighter structural edges
- amber/yellow hazard markings only at dangerous or interactive boundaries

### Autonomous enemies

- heavier blocks and closed silhouettes
- circular sensor or weapon apertures embedded in angular bodies
- red or amber behavior accents
- less exposed negative space than the player
- movement-specific silhouettes: anchored turret, low crawler, compact drone

The boss combines the foundry architecture with enemy sensor geometry. It should
look like part of the level that has awakened, not simply a larger player ship.

## Player-craft silhouette

### Base craft

The base VOIDSTRIKE craft should occupy approximately 16-20 pixels in width and
20-24 pixels in height, subject to gameplay testing. It consists of:

- a bright central core/cockpit
- a narrow forward nose
- two small stabilizer surfaces
- two visible lateral module hardpoints
- one rear or central drive region

The nose, core, and exhaust direction must make orientation unambiguous without
animation.

### Installed modules

Each module changes the craft in a distinct direction:

- **Speed:** emphasizes the rear drive with brighter or longer energy exhaust;
  it should not add much width.
- **Twin Shot:** adds two compact forward emitters on the lateral hardpoints.
- **Wide Shot:** adds outward-angled emitter housings and visibly increases
  width.
- **Laser:** adds a centered forward focusing rail or prong and a brighter cyan
  charge state.

Installed modules must use shared attachment geometry so the final craft feels
constructed rather than replaced by unrelated upgrade sprites.

### Collision readability

The collision region should grow when external modules grow the craft, matching
the core game tradeoff. Collidable pixels must be visibly solid. Use a small,
consistent inset if gameplay requires forgiveness, but do not hide a tiny hitbox
inside a much larger ship or include invisible padding outside the sprite.

The debug build may visualize collision bounds. Those overlays are diagnostic
only and must not appear in the normal build or public captures.

## Enemy-family direction

### Defense node: ground turret

- embedded in a terrain socket rather than floating above the floor
- squat octagonal or chamfered base
- one obvious rotating or directional weapon aperture
- amber charge cue followed by red hostile fire
- two or three frames at most for aiming/fire feedback

### Maintenance crawler: ground vehicle

- low rectangular chassis with visible direction of travel
- paired track, rail, or magnetic-contact shapes
- brighter service panel that doubles as a weak-point cue
- movement conveyed through alternating contact pixels or a small light cycle
- no resemblance to a conventional real-world tank

### Security drone: air formation

- compact diamond, dart, or split-prong silhouette
- dark center with one bright hostile sensor
- strong contrast against every terrain family
- one bank frame in each horizontal direction if budget allows
- formation readability takes priority over individual surface detail

### Reactor guardian: boss

- asymmetrical central machine anchored to the width of the playfield
- one bright vulnerable reactor visible from the first phase but initially
  protected by moving armor or emitters
- attacks originate from visible mechanisms
- second phase exposes more of the reactor and changes the silhouette
- damage removes, darkens, or destabilizes components instead of only changing
  a health value
- defeat transitions from controlled machinery to broken energy release without
  copying a recognizable commercial boss design

## Terrain direction

The five-minute level moves through four visually connected foundry zones:

1. **Outer service deck:** broad armor plates, sparse rails, clear onboarding.
2. **Conduit field:** blue energy channels and denser machine intersections.
3. **Fabrication trench:** moving or implied machinery, hazard boundaries, and
   tighter navigation contrast.
4. **Reactor approach:** darker structure, stronger red/amber warnings, and
   geometry that visually converges toward the boss.

These are visual phases within one continuous level, not separate worlds.

### Tile families

Build a small reusable set before adding decorative variants:

- base armor plate
- edge and corner pieces
- recessed channel
- horizontal and vertical conduit
- conduit intersection
- vent or grille
- rail or service track
- hazard edge
- turret socket
- crawler lane
- damaged/broken plate
- reactor approach trim

Variation should come from composition and a few controlled overlay details.
Avoid building every screen as a unique illustration.

### Scroll readability

- Prefer larger stable masses over uniform checkerboard detail.
- Align important seams with the tile grid.
- Use blue conduits to guide the eye vertically without resembling a literal
  road.
- Keep the player's immediate maneuvering area darker and calmer than the outer
  margins during dense encounters.
- Do not place red, yellow, or green decorative pixels where they can be confused
  with threats, warnings, or pickups.

## HUD principles

The HUD must be compact, original, and subordinate to the playfield.

### Layout

- Use a thin top status band for score and remaining lives.
- Use a shallow bottom **module dock** with four compact module cells.
- Keep the main playfield full width; do not use a large permanent right-side
  instrument panel.
- Separate HUD and playfield with one structural line or dark gap, not a thick
  decorative frame.

Suggested information placement:

```text
TOP LEFT       SCORE
TOP RIGHT      LIVES / CRAFT ICONS
BOTTOM CENTER  FOUR MODULE CELLS
BOTTOM EDGE    CURRENT SELECTION / INSTALLED STATE
```

### Module dock

- Four square or chamfered cells represent Speed, Twin Shot, Wide Shot, and
  Laser.
- Use original abstract icons rather than miniature copied weapon art.
- The selected cell uses cyan outline or pulse animation.
- An installed module uses a stable green or white status mark.
- An unavailable/inactive cell remains dark steel, not invisible.
- Text labels may appear in the title/control explanation; gameplay should rely
  primarily on icon plus consistent cell position.
- Do not reproduce a commercial game's exact upgrade order, typography, bar
  proportions, or highlight behavior merely because the mechanic is related.

### Typography

- Create or use a confirmed open-source ANA bitmap font.
- Prefer compact uppercase glyphs around 6x8 or another proven ANA size.
- Keep numerals especially distinct at native resolution.
- Avoid faux-futuristic cuts that damage readability.
- Do not copy a commercial game's logo lettering or HUD font.

## Effects and animation

### Player fire

- Basic shot: bright compact white/cyan core.
- Twin Shot: two clearly separated emitters, not simply a thicker projectile.
- Wide Shot: smaller angled bolts with readable trajectories.
- Laser: narrow cyan/white energy line with restrained charge feedback.

### Hostile fire

- Use amber for telegraph or slow projectiles.
- Use red for immediate collision threat.
- Keep enemy bullets geometrically distinct from player fire and pickups.

### Energy cores

- Small rotating or pulsing diamond/core shape.
- Green/cyan/yellow cycle using only a few frames.
- Must remain visible over every terrain zone.
- Avoid a star silhouette or other item shape strongly associated with a
  specific inspiration title.

### Explosions

- Four to six compact frames are sufficient.
- Expand from white/yellow core through amber/red fragments into dark smoke or
  absence.
- Avoid large full-screen flashes during normal enemy deaths.
- Boss defeat may use staged component explosions built from the same effect
  vocabulary.

### Animation budget

Use animation where it communicates state:

- player drive: 2 frames
- module charge or selection: 2-3 frames
- normal enemy movement/bank: 2-3 frames
- turret fire: 2-3 frames
- pickup pulse: 3-4 frames
- explosion: 4-6 frames
- boss mechanisms: a few phase-specific components rather than a monolithic
  large frame sequence

Additional frames require evidence that they improve readability or feel enough
to justify memory, asset, conversion, and redraw cost.

## Audio relationship

The human product owner supplies the original MOD music. The Gaia team creates
the basic original SFX. Visual and audio design should reinforce each other:

- cyan/white actions use clean, high-frequency player feedback
- green pickups use a short positive ascending cue
- amber telegraphs use a restrained warning onset
- red damage uses a harder low or noisy transient
- module installation should sound more substantial than ordinary collection
- boss phase change should have a distinct machine-power transition

The owning citizen must integrate and test the MOD and SFX through ANA's channel
policy. The music must not mask firing, pickup, installation, damage, or boss
telegraphs.

## Forbidden direct similarities

VOIDSTRIKE must not include:

- the names, logos, fictional factions, vehicles, characters, or story terms of
  any inspiration game
- traced, extracted, recolored, cropped, or edited commercial screenshots or
  sprites
- a player-craft silhouette substantially copied from a reference craft
- recognizable enemy, turret, boss, or terrain silhouettes from the references
- copied level layouts, encounter sequences, secret locations, or formations
- a large branded right-side cockpit panel derived from a reference layout
- an upgrade HUD with the same labels, order, typography, proportions, icons,
  and selection behavior as a particular commercial game
- copied score layouts, life icons, logo treatments, fonts, or decorative frames
- sampled or recreated commercial music, melodies, instruments, or sound effects
- commercial trademarks in public repository files, screenshots, video, or
  submission materials
- inspiration images packaged as documentation assets in the public repository

Genre conventions remain available: vertical scrolling, shooting, collecting
energy, installing upgrades, ground and air targets, score, lives, and a boss.
Originality comes from VOIDSTRIKE's concrete expression and combination of those
conventions.

## Reference handling and provenance

- Inspiration images remain private, local, untracked references unless their
  rights are separately cleared.
- Do not load them into the ANA asset pipeline.
- Do not use automated image conversion, palette extraction, tracing, or sprite
  segmentation on them.
- Public art discussions should cite abstract qualities from this document
  rather than embedding the images.
- All final source art, converted assets, fonts, SFX, and music must have an
  author and open-source license recorded in `ASSET_LICENSES.md` or equivalent.
- The original MOD supplied by the product owner must record its author and
  license before it is committed as a release asset.
- AI-assisted source art must still be reviewed for recognizable third-party
  elements and licensed consistently with the repository.

## Art-production sequence

Art production should follow the game's feedback loops rather than attempt a
complete asset pack before anything runs.

### Pass 1: Palette and readability board

Create one original mock gameplay frame containing:

- representative terrain
- base player craft
- one example of each enemy family
- player and hostile projectiles
- an energy core
- top HUD and module dock

Review at 1x and in the ANA host build. Tune the shared palette before producing
large asset sets.

### Pass 2: Player and modules

Create the base craft and all module permutations required by implementation.
Verify:

- orientation
- attachment consistency
- collision readability
- distinction among module states
- maximum upgraded width
- animation and mask bounds

Test over every planned terrain zone in motion.

### Pass 3: Terrain kit

Build the minimum 16x16 tile family and a 30-60 second representative scroll
strip. Verify tile seams, color noise, exposed-strip rendering, and stock-A1200
cost before expanding the level.

### Pass 4: Enemy families and combat effects

Create one complete readable variant of each enemy, then add only the frames and
variations required by gameplay. Verify mixed waves with real projectiles and
pickups rather than isolated sprite sheets.

### Pass 5: HUD and state presentation

Complete title, controls, HUD, game over, and victory presentation. Verify that
Space is always understood as module install and that the one-button joystick
mapping remains clear.

### Pass 6: Boss and final polish

Build the boss from established terrain and enemy geometry. Add final effects,
damage states, and phase feedback only after the full-run harness can reach and
defeat it.

## Visual feedback loops

Every significant art change should run the smallest useful loop:

1. render the asset in the ANA host build at native resolution
2. capture consecutive frontbuffer frames
3. inspect object bounds, masks, palette indices, animation, and HUD separation
4. run the relevant deterministic gameplay scenario
5. run the matching FS-UAE scenario on the A1200 profile
6. inspect representative frames or a contact sheet
7. compare performance and redraw metrics with the previous accepted result
8. record the decision, remaining problem, or accepted result

The visual oracle should detect missing, partial, stale, or duplicated actor
components. Agent and human review should assess readability, hierarchy, pacing,
and coherence. Neither automated pixel checks nor aesthetic judgment alone is
sufficient.

The human product owner will play continuously and provide taste and pacing
feedback. Important visual observations should become explicit decisions or
reproducible issues rather than remaining in private conversation.

## Asset deliverables

The final asset set should include, at minimum:

- one indexed shared palette asset
- original bitmap font or documented compatible ANA font
- title/logo treatment
- base player craft and required module combinations
- drive and module-state frames
- three normal enemy families
- boss components and damage states
- 16x16 terrain tiles for all four visual phases
- player and hostile projectiles
- energy-core pickup
- explosion and damage effects
- HUD/module-dock elements
- title, game-over, and victory presentation assets
- basic original SFX
- product-owner-supplied original MOD music
- source files, converted outputs, manifest entries, and license provenance

## Acceptance criteria

- The game has a coherent original identity at native 320x256 resolution.
- All final graphics use one intentional 16-color indexed palette or a documented
  compatible variation required by ANA.
- The player, hostile fire, pickups, enemy families, boss weak point, and module
  selection remain readable over every terrain phase.
- Each installed module visibly changes the craft through consistent attachment
  geometry.
- Collision growth is visually understandable and contains no invisible padding
  or misleading non-collidable mass.
- The HUD uses a thin top band and compact bottom module dock rather than a
  copied side-panel layout.
- Terrain supports smooth vertical motion without excessive shimmer or gameplay
  color confusion.
- No public asset is traced, extracted, recolored, or otherwise derived from a
  commercial screenshot or sprite.
- Inspiration images are absent from the public build and repository unless
  rights are independently cleared.
- Every shipped asset has recorded authorship and an open-source-compatible
  license.
- Host and FS-UAE visual feedback loops pass, and representative frames have
  been inspected by a citizen, the coach, and the human product owner.
- The art remains within the performance thresholds defined by Specs 023 and
  024 on the stock-A1200 baseline.
