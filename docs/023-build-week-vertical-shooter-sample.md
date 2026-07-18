# Spec 023: Build Week Vertical Shooter Sample

## Game name

VOIDSTRIKE

## Purpose

Build a small, complete, original vertically scrolling shooter with ANA. The
game should serve both as a polished ANA example and as a potential OpenAI Build
Week submission created with Codex and GPT-5.6.

Team execution and visual production are defined separately in
[Spec 024](024-voidstrike-ways-of-working.md) and
[Spec 025](025-voidstrike-art-direction.md).

The first release is a five-minute arcade-style vertical slice rather than the
start of a large game. A successful run ends after one complete level and a
boss. An unsuccessful run may end earlier when the player loses all lives.

VOIDSTRIKE must have its own visual identity, setting, characters, level design,
music, sound effects, and source assets.

The Build Week positioning is:

> An original Amiga game built by an autonomous AI product organization.

## Product premise

The player pilots a small craft over a continuously scrolling world. Destroyed
enemies release energy cores. Each collected core moves a cursor along a module
selection rail. The player decides when to install the currently selected
module.

Installed modules visibly change the craft. Offensive modules make it more
powerful but also increase its physical silhouette and collision risk. This
creates the central decision:

> Build a powerful ship one module at a time, but every upgrade makes you a
> bigger target.

The implementation may adjust exact module sizes and hitboxes during balancing,
but the tradeoff between power and exposure is a core product requirement.

## Inspiration and originality

The game is inspired by the broad tradition of classic vertically scrolling
shooters, including the strategic weapon selection of *Slap Fight* and the
distinct ground/air interactions of *Xenon*. These references describe genre
inspiration only.

The project must not reproduce either game. In particular, it must not reuse or
closely imitate their:

- names, logos, story, characters, ships, enemies, or bosses
- graphics, sprite silhouettes, palettes, level layouts, or user interface
- music, sound effects, text, or other assets
- exact encounter sequences or distinctive presentation

All shipped material must be original to the project or used under a compatible
license that is recorded in the repository. The Build Week demo must not use
unlicensed music, trademarks, or copyrighted footage.

Reference links:

- <https://en.wikipedia.org/wiki/Slap_Fight>
- <https://en.wikipedia.org/wiki/Xenon_(video_game)>
- <https://openai.devpost.com/rules>

## Design pillars

1. **Immediately playable.** Movement and shooting must make sense without a
   tutorial beyond a short control card on the title screen.
2. **One meaningful decision.** The module rail must create a recurring choice
   between taking an available upgrade and waiting for another one.
3. **Short and complete.** A full successful run should take approximately five
   minutes and end with a clear victory state.
4. **Readable action.** Enemy shots, pickups, terrain, the player hitbox, and the
   selected module must remain legible within the Amiga color and resolution
   constraints.
5. **Authentic ANA example.** The code should demonstrate ANA's public APIs and
   remain understandable to developers learning from the sample.
6. **Polish over breadth.** One balanced level is more important than additional
   levels, weapons, enemies, or framework features.

## Target platforms

The required targets are:

- the normal ANA host build, for easy judging and development
- a normal A1200 ADF
- an instrumented A1200 debug ADF

The Amiga baseline is a stock A1200 profile. The game should use PAL lores and
target stable 50 Hz simulation. Visual scope must be reduced before accepting
unreliable frame pacing on the baseline target.

The normal A1200 build has these performance thresholds:

- target: stable 50 fps
- acceptance floor: at least 45 fps average across the complete gameplay run
- sustained floor: no five-second active-gameplay window below 40 fps

Loading, boot, and intentional non-gameplay transitions are excluded from the
sustained floor but must not hide gameplay stalls. The instrumented debug build
has a separate guidance floor of 35 fps average. Debug performance is diagnostic
evidence and never replaces normal-build release measurement.

## Run structure

A successful run should last about five minutes:

| Time | Purpose |
| --- | --- |
| 0:00-0:45 | Introduce movement, firing, the first enemy, and the first energy core. |
| 0:45-2:00 | Introduce the remaining normal enemy types one at a time. |
| 2:00-3:30 | Combine enemies, terrain, and module choices into denser encounters. |
| 3:30-4:15 | Deliver a short final escalation and a last chance to build the craft. |
| 4:15-5:00 | Fight the boss and reach the victory state. |

The timing is a pacing target, not a frame-exact script. It should remain close
enough that the complete level can be demonstrated within a short video.

## Core loop

1. Move through the continuously scrolling level.
2. Shoot ground and airborne enemies.
3. Avoid enemies, projectiles, and hazardous terrain.
4. Collect energy cores released by selected enemies.
5. Observe the highlighted entry on the module rail.
6. Press the install action to attach the selected module, or wait and collect
   another core to advance the selection.
7. Reach and defeat the boss before losing all lives.

## Controls

The game must support keyboard and joystick movement and firing.

| Function | Keyboard | Joystick/gamepad |
| --- | --- | --- |
| Move | Arrow keys | Directional stick or D-pad |
| Fire | Left Ctrl | Primary fire button |
| Install selected module | Space | Space on the Amiga keyboard |
| Pause | P | Optional |
| Quit | Escape | Platform-standard quit mapping |

Space is always the install control. The target controller is a classic
one-button Amiga joystick: the stick moves, its single button fires, and Space
on the Amiga keyboard installs the highlighted module. VOIDSTRIKE does not
require or assign a second joystick button.

The title screen must show a compact control card equivalent to:

```text
ARROWS / JOYSTICK  MOVE
CTRL / FIRE        SHOOT
SPACE              INSTALL MODULE
```

Input must go through ANA's public direction/action mapping. Game code must not
read platform joystick or keyboard hardware directly.

## Player and life model

- The player begins each run with three lives.
- A direct hit destroys the current craft and consumes one life.
- Respawn should be quick and provide a short, clearly visible grace period.
- A respawn returns the craft to its base configuration unless playtesting shows
  that losing every module makes the remaining run unreasonably punitive.
- Losing the final life enters the game-over state.
- The player can restart without relaunching the program.

The exact respawn penalty may be tuned, but it must be deterministic and clearly
communicated by the presentation.

## Module selection rail

The first playable version should contain four module entries:

1. **Speed** - improves movement speed without adding a large external module.
2. **Twin Shot** - adds a second forward firing point and widens the craft.
3. **Wide Shot** - covers a broader angle at lower focused damage.
4. **Laser** - provides a narrow, high-damage or piercing attack with an
   appropriate fire-rate tradeoff.

Required behavior:

- Collecting an energy core advances the highlighted entry by one position.
- The selection wraps after the final entry.
- Pressing Space or the mapped install action applies the highlighted module.
- Installing a module clears or resets the active selection state.
- Installed external modules must be visible on the craft.
- The HUD must make the current selection and installed state readable.
- A module that is already installed must have a defined result, such as an
  upgrade tier, score award, or skipped selection; it must not produce an
  ambiguous no-op.

Exact damage values, fire rates, and module geometry are balancing data and do
not need to be fixed in the first implementation commit.

## Enemies

The level should use three reusable normal enemy families:

### Ground turret

- Stationary relative to the world.
- Fires aimed or fixed-pattern shots at a readable cadence.
- Teaches the player to move laterally while attacking ground targets.

### Ground vehicle

- Moves along a simple path or lane.
- Uses the same ground collision and projectile systems as the turret where
  possible.
- Creates timing pressure without requiring complex pathfinding.

### Air formation

- Enters as a small scripted formation.
- Uses one or two simple movement patterns.
- Can cross the player's movement space and makes Wide Shot useful.

Variations should come from placement, timing, speed, formation, and projectile
cadence rather than additional enemy classes.

## Boss

The level ends with one boss lasting roughly 30-45 seconds for a reasonably
upgraded player.

The boss should:

- reuse normal movement, projectile, collision, and damage primitives
- have a clearly readable vulnerable region
- contain no more than two attack phases
- visibly react to damage
- test movement and module choices already taught by the level
- enter a deterministic defeat sequence followed by the victory state

The boss must not require a one-off framework subsystem.

## Scoring and pickups

The required scoring model is intentionally small:

- points for destroying enemies
- a larger award for defeating the boss
- optional bonus points for unused lives or installed modules at completion
- current score visible in the HUD

Only energy cores are required as pickups. Additional pickup types should be
added only if the complete run is already stable and polished.

## Game states

The minimum state set is:

- title
- playing
- paused, if pause is implemented
- player death/respawn
- game over
- boss defeated/victory

Both game over and victory must offer a direct restart path. Debug information
must remain restricted to debug builds and must not replace the normal HUD or
presentation.

## Presentation

The game should feel like a small finished Amiga title, not a framework test.
The first release requires:

- an original title and title screen
- a coherent original 16-color visual identity
- a readable player craft with visible installed modules
- distinct ground enemies, air enemies, projectiles, pickups, and boss
- a static HUD separated from the scrolling playfield
- simple explosions and damage feedback
- basic original firing, pickup, install, explosion, player-death, and victory
  sound effects created by the Gaia team
- original music supplied by the human product owner and integrated through
  ANA's normal music and channel-policy path

No placeholder copied from an existing commercial game may appear in the public
repository, final build, screenshots, or demo video.

The visual direction must be explicitly set before presentation is treated as
complete. It must define at least the setting, shape language, palette mood,
player and enemy silhouettes, terrain vocabulary, and how installed modules
remain readable. The asset pipeline may be prepared before that decision, but
final graphics must not emerge from unrelated placeholder styles.

All VOIDSTRIKE code and shipped assets in the ANA repository must be open source
under the repository license or a documented compatible license. Third-party
material, if any, still requires source and license provenance even though the
repository itself is open source.

## ANA implementation direction

The sample should live under a new `examples/<game-name>/` directory once the
name is chosen. It should follow the normal ANA application structure with an
`ANA_Game` entry point and separate game-state and rendering modules when that
improves readability.

The intended framework usage is:

- `ANA_RENDER_VERTICAL_SCROLL` for the primary playfield
- `ANA_Camera` and `ANA_TileLayer` or the current public vertical-scroll path
- separate logical playfield, actor, and HUD layers
- ANA image and bitmap-font APIs for presentation
- ANA input directions/actions for keyboard and joystick parity
- ANA sound and music APIs using the documented channel policy
- ANA asset manifests and conversion tools for all shipped assets

The implementation should prefer ANA public APIs. A direct C, Amiga, blitter,
or other hardware-specific escape hatch is acceptable only when profiling
shows that it is necessary, it remains isolated, and the reason is documented.

The game must not silently broaden into a rewrite of ANA's scrolling renderer.
If the current vertical-scroll path cannot support the intended presentation on
the stock A1200, first reduce visual scope or scrolling cost. A narrowly scoped
framework fix may be made when it is required by the sample and independently
verified.

## Verification

The project should have deterministic host-side tests for game rules where
practical. At minimum, verification should cover:

- keyboard and joystick direction/action mappings
- player movement bounds and firing cadence
- energy-core collection and rail advancement
- module installation and repeat-install behavior
- collisions among player, enemies, and projectiles
- life loss, respawn grace, game over, and restart
- scripted enemy spawn progression
- boss phase transition, defeat, and victory
- a full-run smoke test that reaches the boss and victory

The normal host build, normal A1200 ADF, and debug A1200 ADF must build
reproducibly. The ADF must be booted and played in the supported emulator
workflow. Performance should be measured on the stock-A1200 profile, with debug
instrumentation results kept distinct from normal-build game feel.

## Build Week evidence and submission readiness

If the game is submitted to OpenAI Build Week, the repository must preserve a
clear boundary between ANA work that predates the submission period and the new
game created during it.

Required evidence and materials include:

- dated Git history for the new game and any required ANA extensions
- the Codex session ID for the project thread where most core functionality was
  built
- an English README with setup, build, run, controls, and testing instructions
- a clear account of how Codex and GPT-5.6 contributed and where the human made
  key product, engineering, and design decisions
- a testable host build or demo plus the ADF artifact
- a public YouTube demonstration shorter than three minutes, with audio
- English submission text and testing instructions

The video should show the title and controls, normal play, module selection,
visible ship growth, mixed enemy encounters, the boss, and a successful end
state. It may use edited cuts rather than showing the entire five-minute run.

## Non-goals for the first release

- Multiple levels or worlds.
- Multiplayer or two-player support.
- A faithful remake, port, or clone of an existing shooter.
- More than three normal enemy families.
- More than four module entries.
- Branching narrative, cutscenes, or extensive story text.
- Online features, accounts, telemetry, or leaderboards.
- Persistent high-score entry if it risks the core game or submission.
- A general bullet-hell, physics, scene-graph, or entity-component framework.
- New A500 or AGA platform support.
- A requirement for a multi-button controller.

## Acceptance criteria

- A new player can understand the controls from the title screen.
- Keyboard and joystick movement/firing both work through ANA input mappings.
- Space installs the highlighted module in both keyboard and one-button
  joystick play.
- A successful run lasts approximately five minutes.
- The run contains three normal enemy families, four module choices, and one
  two-phase-or-simpler boss.
- Installed combat modules visibly change the craft and create a readable
  power-versus-size tradeoff.
- The game can progress from title to victory or game over and restart without
  relaunching.
- The host build and A1200 ADF are playable and match the documented behavior.
- The normal stock-A1200 full run averages at least 45 fps, no five-second
  active-gameplay window falls below 40 fps, and the debug result is reported
  separately.
- All public assets and presentation are original or compatibly licensed.
- The repository and English documentation make the new Build Week work easy
  to identify and test.
