# Spec 024: VOIDSTRIKE Ways of Working

## Purpose

This document defines how the Gaia team, its operator-coach, and any temporary
workers should build, verify, and deliver VOIDSTRIKE with ANA.

It complements [Spec 023](023-build-week-vertical-shooter-sample.md), which
defines the product. This document defines the team, model choices, ownership
boundaries, repository workflow, feedback loops, autonomous FS-UAE playtesting,
performance analysis, evidence, and stopping conditions.

Visual production, originality boundaries, and asset feedback loops are defined
in [Spec 025](025-voidstrike-art-direction.md).

The central operating principle is:

> The team does not infer quality from code, a green host test, or an agent
> self-report. It repeatedly builds the real ADF, plays the complete game in
> FS-UAE, observes structured and visual evidence, measures performance, and
> feeds the result into the next bounded change.

## Delivery context

VOIDSTRIKE is a short Build Week delivery with a fixed product boundary:

- one original vertically scrolling shooter
- one approximately five-minute level
- three normal enemy families
- four module choices
- one boss
- host and stock-A1200 targets
- a complete title-to-victory or title-to-game-over experience

The team should optimize for a complete, polished, testable vertical slice.
Additional features have no value until the complete run works and can be
verified without human intervention.

The external Build Week positioning is:

> An original Amiga game built by an autonomous AI product organization.

The judge's preferred execution path remains an open product decision. The
team must preserve a working host build and ADF while the product owner decides
how the final judging package should be presented. That decision must be closed
before submission packaging, but it does not block core implementation.

## Team topology

The recommended active team is:

| Role | Count | Default model | Reasoning |
| --- | ---: | --- | --- |
| Operator-coach | 1 | `gpt-5.6-sol` | `medium` |
| Permanent citizens | 3 | `gpt-5.6-terra` | `medium` |
| Temporary workers | 0-2 concurrently | `gpt-5.6-luna` | `medium` |
| Specialist escalation worker | At most one when justified | `gpt-5.6-sol` | `high` |
| Human product owner | 1 | Not applicable | Not applicable |

Three citizens provide enough capacity for a core game stream, a presentation
stream, and a verification/delivery stream without turning the repository into
a high-conflict write surface. Adding more citizens requires a concrete reason
and evidence that the existing team cannot make useful progress.

Citizens are permanent, equal identities. There is no lead citizen or
supervisor citizen. Work ownership emerges from the normal issue claim process,
not from coach assignment.

## Why these models

### Coach: GPT-5.6 Sol, medium

The coach needs strong whole-system judgment for:

- ownership and worktree consistency
- claim, PR, CI, merge, and completion evidence
- conflicting external facts
- Build Week provenance and submission readiness
- detecting when a reported blocker is real
- maintaining non-interference while the deadline approaches

`medium` reasoning should be the default because most coach actions are
evidence validation, not open-ended product analysis. The coach must not turn
its stronger model into permission to improve a citizen's implementation or
choose product architecture.

### Citizens: GPT-5.6 Terra, medium

The product spec is concrete, ANA already has established implementation
patterns, and iteration speed matters. Terra is the pragmatic default for
coding, tool use, debugging, test execution, and Git delivery. Medium reasoning
is the starting point; success is measured by working software and feedback,
not by how long the model reasons before acting.

### Workers: GPT-5.6 Luna, medium

Luna is appropriate only for clear, repeatable, bounded work with an objective
result. A Luna worker brief must state:

- the exact goal
- allowed files or read scope
- required inputs
- required output or patch boundary
- tests or evidence that define success
- what the worker must not change

Good Luna worker tasks include:

- inventorying relevant ANA APIs with file references
- checking an asset manifest against a fixed checklist
- analyzing build, test, or performance logs
- producing a structured test matrix
- reviewing a diff against explicit invariants
- implementing a small isolated helper with existing tests
- checking English README or submission material for missing required fields

Luna should not receive open-ended requests such as “make the game fun,” “fix
scrolling performance,” “design the architecture,” or “integrate the release.”

### Sol/high specialist escalation

A citizen may request a Sol/high worker only after it has identified, analyzed,
attempted, and verified a difficult problem. Appropriate examples include:

- unexplained Amiga memory corruption
- a renderer decision with broad ANA compatibility risk
- a performance result that contradicts the expected cost model
- a difficult integration or correctness review before release

The requesting citizen remains the owner and must validate and integrate the
specialist result. Sol is an escalation resource, not the default implementation
lane.

### Runtime-model preflight

Before the product run starts, the coach must verify that every configured model
can actually start in the intended runtime. In particular, the worker broker
must start a bounded Luna preflight and record both the configured model and the
actual runtime model. A documented model name is not sufficient evidence of
account availability.

Current OpenAI model guidance:

- <https://developers.openai.com/api/docs/guides/latest-model>
- <https://learn.chatgpt.com/docs/models>

## Authority boundaries

### Human product owner

The human product owner may define or change:

- product goal and acceptance criteria
- creative direction and final name
- mandatory versus optional scope
- deadline and feature-freeze point
- whether a feature should be cut when a verified gate is missed
- final submission decision

The product owner should express changes as product decisions or revised
acceptance criteria, not as hidden direct assignments to a citizen.

### Operator-coach

The coach may:

- start or restore suitable permanent citizen identities
- provide the initial goal, specs, constraints, and current external facts
- ensure isolated worktrees, trace storage, and write-ahead state
- observe liveness and ask restrictive status questions
- validate branch, PR, CI, merge, issue, artifact, and emulator reality
- broker an exact citizen-requested worker brief
- return raw validation or worker facts to the owning citizen
- attest completion after every required gate is externally true
- preserve session IDs, test artifacts, hashes, and Build Week evidence

The coach must not:

- assign issues to citizens
- choose a citizen's implementation
- become the game's architect or creative director
- edit product code in a citizen's place
- fix a failing test for a citizen
- reinterpret a worker result before returning it to the owner
- call local success complete before the PR is merged and the required runtime
  evidence exists
- allow deadline pressure to erase ownership, trace, or completion boundaries

### Citizens

Each citizen:

- autonomously selects and claims one available issue
- owns planning, implementation, worker decisions, integration, and verification
  for that issue
- uses its own branch and isolated worktree
- continues until a valid stop condition exists
- preserves tests and feedback tooling that will benefit later iterations
- proposes completion but does not self-attest external completion
- resolves conflicts on its own open issue or PR after upstream merges

### Workers

Workers add execution capacity but gain no governance or ownership rights. They
must not claim issues, supervise citizens, merge PRs, or make completion
decisions. Their output is advisory until the owning citizen verifies and
integrates it.

Worker use is value-based, not a quota. Choosing not to use a worker is valid
when delegation would cost more context, review, or merge work than direct
execution.

## Initial work queue

The product owner should provide three outcome-oriented issues without assigning
them to identities:

1. **Build the complete playable five-minute vertical slice.**
   This is the main product issue and should contain the majority of core game
   functionality. Its owning Codex session is the intended `/feedback` session
   for the Build Week submission.
2. **Create the original visual and audio presentation.**
   This includes source assets, manifests, title presentation, HUD, palette,
   sprites, and sound effects. The Gaia team creates the basic original SFX. The
   human product owner supplies the original music, which the owning citizen
   integrates through ANA's normal music path. The work must use an explicit
   interface to the game rather than silently changing game rules.
3. **Deliver autonomous verification and Build Week readiness.**
   This includes deterministic game tests, FS-UAE control and observation,
   full-run automation, visual and performance oracles, README instructions,
   and submission evidence.

The core issue should not be fragmented into many parallel implementation
issues. One session must remain clearly responsible for most core functionality,
and the game needs one coherent owner for its main loop.

The visual direction is a required early product decision. Before final graphics
are accepted, the product owner and team must establish the setting, shape
language, palette mood, player and enemy silhouettes, terrain vocabulary, and
visual treatment of installed modules. Asset-pipeline scaffolding may proceed
before this decision, but disconnected placeholder styles must not harden into
the final game.

## Repository and worktree discipline

- Never run multiple citizens in the canonical ANA checkout.
- Start every citizen from the same verified base commit in a dedicated
  worktree and branch.
- Preserve any dirty checkout, staged change, open PR, active branch, or
  unreconciled citizen state discovered during preflight.
- Use one issue, one branch, one PR, and one owning citizen.
- Do not create stacked or rolling PRs unless Gaia explicitly permits them.
- Merge the smallest coherent dependency first and let owners of conflicting
  open PRs reconcile against the new base.
- Stage only the files belonging to the owned issue.
- Run `git diff --check` before every commit.
- Do not mix temporary diagnostics into an otherwise releasable commit unless
  the diagnostic is becoming a maintained feedback tool.

The coach must verify the exact base commit and worktree status before starting
the run. It must not clean, reset, overwrite, or prune state merely to simplify
setup.

## Feedback-loop philosophy

ANA development should proceed through evidence-bearing loops:

```text
observe -> reproduce -> measure -> change -> rebuild -> replay -> compare -> retain learning
```

Each loop should be small enough that the team can connect the observed result
to the change that caused it.

### Required loop behavior

1. **Observe the real symptom.** Record game phase, build identity, target,
   input path, and visible or structured evidence.
2. **Reproduce deterministically.** Prefer a maintained scenario or full-run
   harness over a one-off manual sequence.
3. **Capture a baseline.** Preserve behavior, frame, timing, or failure data
   before changing the code.
4. **Form one bounded hypothesis.** State what should change in the evidence if
   the hypothesis is correct.
5. **Make the smallest useful change.** Avoid opportunistic cleanup while the
   causal question is still open.
6. **Rerun the narrow loop.** Compare the same scenario and metrics against the
   baseline.
7. **Broaden verification.** Run the full game and adjacent scenarios after the
   narrow test passes.
8. **Inspect both behavior and presentation.** A structured pass cannot prove
   that sprites, HUD, scrolling, or transitions look correct.
9. **Retain durable feedback.** Keep generally useful scenarios, oracles, and
   regression checks in the repository.
10. **Remove temporary instrumentation.** Traces, colored hitboxes, ad hoc
    mounts, and debug-only overlays must be removed after the diagnosis unless
    they are deliberately converted into maintained debug tooling.
11. **Record the learning.** State the failure mode, evidence, fix, verification,
    and any remaining uncertainty in the issue, PR, trace, or relevant guide.

### Evidence hierarchy

Use the following order when facts conflict:

1. the exact release or debug artifact built from the recorded commit
2. structured results produced by that artifact in FS-UAE
3. captured frames or frontbuffer output from that same run
4. host deterministic results
5. source-code reasoning
6. agent narrative or expectation

Agent self-report is never sufficient completion evidence.

## Existing feedback capability audit

ANA already contains substantial host, FS-UAE, input, visual, collision, and
performance feedback tooling created for Byte Brothers. The verification stream
must evaluate that capability before designing replacement tooling.

The initial audit should classify each existing component as:

- reusable without modification
- reusable after generalization or a VOIDSTRIKE scenario adapter
- insufficient for the new vertical-shooter behavior
- obsolete and safe to replace only with evidence

At minimum, evaluate the existing FS-UAE runner, generated configuration,
synthetic input probe, macOS input sender, host frontbuffer dump, frame analyzer,
visual capture, result-file validation, phase markers, sprite/raster telemetry,
and Makefile feedback targets.

Prefer extending a proven common runner or oracle over copying Byte
Brothers-specific code into a parallel implementation. Any new tool must name
the exact gap in the existing loop that it closes. Preserve the old Byte
Brothers feedback targets while generalizing shared behavior.

This audit is an early delivery activity, not a documentation exercise deferred
until the game is complete. A minimal VOIDSTRIKE short-run adapter should be
working as soon as the first playable foundation exists, and the full-run loop
should grow alongside the game.

## Autonomous FS-UAE play requirement

Citizens and the coach must be able to launch, control, observe, complete, and
analyze VOIDSTRIKE in FS-UAE without asking a human to click, type, play, capture
a screenshot, copy an artifact, or interpret the result.

This requires two complementary play modes.

### Mode 1: Deterministic full-run harness

The primary regression loop must boot a real A1200 build in FS-UAE and exercise
the real game from title screen to a terminal state. It must not jump directly
to the boss or call game-state internals to manufacture success.

The harness should provide at least these scenarios:

- `victory`: start, move, fire, collect and install modules, survive the level,
  defeat the boss, and reach the victory state
- `game-over`: start, lose all lives through real collision paths, reach game
  over, and restart
- `input-keyboard`: complete a representative segment using ANA keyboard
  mappings, including Ctrl and Space
- `input-joystick`: complete a representative segment through the joystick
  direction/fire path and use Space on the Amiga keyboard for module install
- `module-progression`: collect cores, wrap the rail, install every module, and
  verify repeated-install behavior
- `boss`: reach the boss through the normal timeline and verify both attack
  phases, damage, defeat, and transition

The deterministic controller may be compiled into a harness build or injected
through a controlled ANA input boundary. It must drive the same update,
collision, rendering, audio, state-transition, and timing code used by the
normal game. Any harness-only shortcut must be explicit and must not invalidate
the behavior being tested.

The full-run result must contain machine-readable fields for at least:

- source commit and build identifier
- ADF SHA-256
- requested and actual scenario
- machine profile and Fast RAM configuration
- total frames, simulated time, and terminal state
- score, remaining lives, and installed modules
- enemies spawned and destroyed
- boss phase and defeat state
- collision and world-bound invariant failures
- input events observed through keyboard and joystick action paths
- minimum, average, and slow-frame timing or FPS data
- render, present, and other available stage timing
- pass/fail plus explicit failure reasons

The process must exit non-zero when the final result is missing, stale, partial,
from the wrong scenario, from the wrong build, or violates an invariant.

### Mode 2: Agent-driven visual play

Deterministic input proves reproducible behavior but cannot judge game feel by
itself. Citizens and the coach also need an agent-facing loop that can:

1. launch the normal or debug ADF in an isolated FS-UAE configuration
2. capture the current visible game frame
3. send bounded keyboard or joystick actions
4. capture later frames and structured telemetry
5. repeat until victory, game over, timeout, or a recorded blocker
6. store the complete action and observation trace

This mode allows an agent to assess:

- whether controls feel responsive
- whether enemy shots and pickups are readable
- whether scrolling is smooth enough to play
- whether the module rail is understandable in motion
- whether ship growth obscures the hitbox
- whether difficulty and pacing match the five-minute goal
- whether boss telegraphs and state transitions are visible

Agent-driven visual play must still be reproducible enough to audit. Record
timestamps or frame numbers, sent actions, captures, build identity, and the
agent's observations. Qualitative judgments must link to concrete frames or
run phases rather than free-form impressions alone.

### Required command contracts

The verification issue should expose stable top-level commands equivalent to:

```sh
make emulator-voidstrike-all
make emulator-voidstrike-full-run
make emulator-voidstrike-visual
make emulator-voidstrike-performance
make emulator-voidstrike-input
```

The exact implementation may differ, but each command must be documented,
non-interactive, fail reliably, and leave its artifacts under a predictable
ignored build directory.

`emulator-voidstrike-all` should be the release feedback gate. It should run the
required deterministic scenarios, full-run victory path, input coverage,
visual oracle, and stock-A1200 performance assertions.

## FS-UAE isolation and artifact freshness

Every automated run must generate its own FS-UAE configuration and isolated
result, log, save-state, and writable-media directories. Automation must launch
FS-UAE directly rather than relying on a saved FS-UAE Launcher entry.

The runner must:

- resolve the intended Kickstart and machine profile before launch
- record the exact executable or ADF path
- hash the ADF before launch
- prevent writable cached floppy copies from silently replacing the new build
- compare any copied or mounted artifact hash with the source artifact
- terminate stale FS-UAE processes when explicitly requested by the test mode
- reject result files whose scenario, frame count, commit, or build ID does not
  match the request
- print recent FS-UAE logs and phase markers on timeout or missing results

A stale FS-UAE cache is a build-identity failure, not a gameplay failure. The
team must verify freshness before debugging source code.

## Independent oracles

No single oracle is sufficient. VOIDSTRIKE should preserve the successful ANA
pattern of combining independent evidence sources.

### Game-rule oracle

Deterministic host tests verify state transitions, spawn timing, collisions,
module selection, life loss, boss behavior, and restart behavior without
emulator latency.

### Amiga behavior oracle

The FS-UAE harness verifies the real m68k binary, Amiga input backend, memory
behavior, audio/channel behavior, render backend, game invariants, and terminal
state.

### Visual oracle

Frame sequences from ANA's indexed frontbuffer should be analyzed for missing,
partial, stale, or duplicated objects, HUD corruption, invalid palettes, and
broken state transitions. Generate contact sheets or selected frames so agents
can inspect the full run efficiently.

When macOS capture APIs mask the emulated chipset surface, a black window capture
must be reported as unavailable evidence, not accepted as a visual pass. Use the
deterministic frontbuffer oracle for pixels and FS-UAE telemetry for the Amiga
path, as ANA already does for Byte Brothers.

### Input oracle

Verify keyboard and joystick paths separately. A host keyboard mapped by FS-UAE
to a joystick port does not prove the Amiga keyboard backend, and a synthetic
ANA action does not prove a real joystick/fire path. The result must state which
path produced each observed action.

### Performance oracle

Measure the stock A1200 without Fast RAM as the release baseline. Collect at
least:

- average and minimum effective FPS
- slowest frame or slow-frame count
- update, draw, present, and available render-stage timing
- visible enemy, projectile, effect, and module counts at the sampled phase
- normal-build game feel separately from debug-build instrumentation

Do not compare an instrumented debug result directly with a normal release
result without labeling the difference. A feature is not accepted merely
because it works with Fast RAM.

The normal-build target is stable 50 fps. The release gate requires:

- at least 45 fps average across the complete gameplay run
- no five-second active-gameplay window below 40 fps
- no hidden exclusion of dense waves, heavily upgraded play, or the boss

Boot, loading, and intentional non-gameplay transitions are excluded from the
five-second sustained floor but must be labeled. The instrumented debug build
has a separate guidance floor of 35 fps average. A debug result below that floor
requires investigation, but only the normal build decides release performance.
The result schema should report both thresholds and the exact windows that were
included.

### Human product feedback

The human product owner will play VOIDSTRIKE continuously during development.
Those sessions are the primary source of taste, pacing, difficulty, readability,
and emotional feedback. Important observations should be recorded as product
decisions, acceptance changes, or reproducible issues so citizens can act on
them without hidden steering.

Human play remains additive evidence. It is not a dependency of the build,
test, diagnosis, or release loop, and it must not be the only way to reproduce a
functional or performance problem.

## Citizen implementation loop

For each owned issue, the citizen should:

1. restore identity, state, branch, and worktree
2. re-read Spec 023, this document, the owned issue, and relevant ANA guidance
3. verify the current base and existing external facts
4. record a worker decision before mutating the workbench
5. establish or run the smallest relevant feedback loop
6. implement one bounded step
7. run the narrow test and inspect its artifacts
8. run adjacent tests when the narrow result passes
9. run the relevant FS-UAE path for any behavior, rendering, input, asset,
   audio, or performance change
10. record evidence and remaining uncertainty
11. remove temporary diagnostics or convert them into maintained tooling
12. commit, push, open the PR, monitor CI, and resolve owned conflicts
13. propose completion only when the issue's runtime evidence exists

Code inspection alone is not an acceptable substitute for steps 5-9.

## Coach feedback loop

The coach runs a separate validation loop and must not merely repeat the
citizen's summary:

1. observe citizen state and liveness without changing product code
2. compare the claimed branch, commit, PR, CI, issue, and worktree state with
   external reality
3. verify that required result artifacts name the expected commit, scenario,
   ADF hash, and machine profile
4. rerun the smallest critical command when independent reproduction is needed
5. inspect structured failures and representative visual artifacts
6. return raw contradictory facts to the citizen without prescribing a fix
7. attest completion only after merge, issue closure, CI, and required runtime
   evidence align

The coach must be technically capable of completing the same autonomous FS-UAE
full run as the citizens. This ensures that runtime evidence is independently
reproducible and not accessible only from a citizen's private environment.

## Quality gates

The product owner should define these gates before launch; the coach validates
them without choosing the implementation:

### Gate 1: Playable foundation

- host build starts from the title screen
- keyboard and joystick movement/fire paths exist
- vertical scrolling, player movement, and firing work
- a deterministic short scenario passes

### Gate 2: Representative play

- at least 90 seconds of scripted gameplay runs in FS-UAE
- all three enemy families appear
- energy cores and the module rail work
- captured frames and Amiga telemetry are available

### Gate 3: Complete ADF

- normal and debug A1200 ADFs build reproducibly
- the full victory scenario reaches the boss and terminal victory state
- game-over and restart paths pass
- the ADF hash and build identity are recorded

### Gate 4: Performance and presentation

- the normal stock-A1200 full run averages at least 45 fps
- no five-second active-gameplay window falls below 40 fps
- debug-build performance is reported separately, with 35 fps as its guidance
  floor
- normal-build gameplay has been played autonomously and inspected
- HUD, scrolling, sprites, projectiles, modules, boss, and transitions are
  visually readable
- no temporary tracing or debug overlays remain in the normal build
- the product owner has played the current build and any resulting decisions or
  issues are recorded

### Gate 5: Submission readiness

- all required tests pass from the intended release commit
- English README and testing instructions are complete
- the core Codex session ID and model/runtime provenance are preserved
- new Build Week work is distinguishable from pre-existing ANA work
- demo capture can show title, controls, module selection, ship growth, mixed
  encounters, boss, and victory
- the tested artifact matches the artifact intended for submission

Reserve the final twelve hours before submission for fixes, balancing, evidence,
README, demo, and submission material. New systems or broad framework changes
should not enter after feature freeze.

## Performance response policy

When performance is below the accepted floor:

1. reproduce on the recorded stock-A1200 profile
2. identify the expensive phase from telemetry
3. reduce game-side cost before broadening ANA scope
4. compare the same deterministic full-run segment before and after the change
5. verify visual correctness independently
6. make a narrow ANA improvement only when the game-side scope cannot meet the
   target and the framework cause is demonstrated

Preferred game-side reductions include fewer simultaneous objects, smaller
dirty regions, less per-frame text work, cheaper effects, simpler masks, and
less full-view redraw. Do not accept an opaque visual downgrade; every scope cut
must preserve readability and the core module decision.

## Asset ownership and licensing

- The Gaia team creates the basic original SFX required by Spec 023.
- The human product owner supplies the original music.
- The owning citizen integrates, verifies, and performance-tests all audio.
- The visual direction must be set explicitly before final graphic production.
- All code and shipped assets committed to ANA must be open source under ANA's
  license or a documented compatible license.
- Maintain source and license provenance for any third-party input even when the
  resulting repository is open source.
- Do not use commercial-game placeholders in public commits, captures, or
  builds.

## Failure and escalation policy

A citizen does not escalate merely because a task is difficult. An escalation
must include:

- exact observed behavior
- exact commit and artifact identity
- reproduction command and scenario
- expected versus actual result
- attempted explanation and change
- resulting evidence
- the smallest question or worker brief that could unblock progress

The coach may answer only within the existing Gaia help boundary. A Sol/high
specialist worker may be brokered when the task is sufficiently bounded and the
owning citizen has an integration and verification plan.

External blockers such as missing Kickstart access, unavailable FS-UAE,
unavailable model entitlement, denied macOS Accessibility/Screen Recording
permission, or a stopped Docker runtime must be recorded explicitly. The team
should prefer test paths that do not depend on UI focus or synthetic macOS input,
but the provisioned Build Week environment must still support every required
autonomous release gate.

## Evidence retained for Build Week

The repository or Gaia trace should preserve:

- initial product spec and ways-of-working document
- issue claims and ownership history
- configured and actual citizen/worker models
- the primary core Codex session ID
- dated commits distinguishing new game work from pre-existing ANA
- worker briefs, raw outputs, integration decisions, and value assessments
- deterministic host and FS-UAE result files
- ADF hashes and generated FS-UAE configurations
- performance measurements and machine profiles
- representative frames, contact sheets, or visual-analysis results
- failures that caused product, framework, or process changes
- final release command output and artifact identity

Evidence should explain both where Codex accelerated the work and where the
human product owner made the key creative and scope decisions.

## Definition of done

VOIDSTRIKE is not done when the code compiles or when a citizen says it is
playable. It is done only when:

- the product acceptance criteria in Spec 023 pass
- the intended release commit is merged and CI is green
- host and A1200 artifacts build reproducibly
- a citizen can autonomously play the complete game in FS-UAE
- the coach can independently repeat the complete FS-UAE run
- victory, game-over, restart, keyboard, joystick, module, and boss paths have
  machine-readable passing evidence
- representative full-run frames have been inspected
- stock-A1200 performance is measured and acceptable
- the normal full run averages at least 45 fps and has no five-second active
  gameplay window below 40 fps
- normal and debug performance evidence is correctly distinguished
- no stale cached ADF or mismatched result can be mistaken for the release
- temporary diagnostics are removed from the normal game
- README, testing instructions, model provenance, and Build Week materials are
  complete
- the submitted or demonstrated artifact has the same recorded identity as the
  verified artifact
