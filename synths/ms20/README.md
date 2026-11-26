# ms20~ - MS-20 style monosynth

Part of the Pd synth collection by Sofia Vallejo Budziszewski, IEM KUG Graz Austria. Version 1 (2026). Mono, 12 presets.

Two oscillators with ring modulation and noise feed a high-pass and a low-pass filter that scream when you raise the peak. Two envelope generators and a modulation generator are wired by a matrix of amounts: every source can go to pitch pulse width and both cutoffs. Mono.

## Use

Open `ms20~-help.pd`. It lists every parameter with its message name and has clickable examples of every message. Create the object as `[ms20~]`, `[ms20~ name]` or `[ms20~ name 8]` (name and voice count).

## Parameters

All values are 0 to 1. Switches take a step number. Set one with `set <name> <value>`.

| Name | Meaning | Choices |
|---|---|---|
| `range1` | osc 1 range | 0..3: 16 / 8 / 4 / 2 feet |
| `wave1` | op 1 wave | 0..3: triangle / saw / pulse / noise |
| `pw1` | pulse width | 0..1 |
| `range2` | osc 2 range | 0..3: 16 / 8 / 4 / 2 feet |
| `wave2` | op 2 wave | 0..3: saw / square / triangle / noise |
| `tune2` | osc 2 tune | 0..1 |
| `lvl1` | level osc 1 | 0..1 |
| `lvl2` | level osc 2 | 0..1 |
| `ring` | ring mod | 0..1 |
| `noise` | noise | 0..1 |
| `hpcut` | high-pass cutoff | 0..1 |
| `hppeak` | high-pass peak | 0..1 |
| `lpcut` | low-pass cutoff | 0..1 |
| `lppeak` | low-pass peak | 0..1 |
| `drive` | drive | 0..1 |
| `serial` | filter routing | 0..1: parallel / series |
| `mgrate` | mg rate | 0..1 |
| `mgwave` | mg wave | 0..3: triangle / saw / square / random |
| `mg2pitch` | mg to pitch | 0..1 |
| `mg2lp` | mg to low-pass | 0..1 |
| `mg2hp` | mg to high-pass | 0..1 |
| `mg2pw` | mg to pw | 0..1 |
| `att` | attack | 0..1 |
| `dec` | decay | 0..1 |
| `sus` | sustain | 0..1 |
| `rel` | release | 0..1 |
| `eatt` | eg2 attack | 0..1 |
| `edec` | eg2 decay | 0..1 |
| `esus` | eg2 sustain | 0..1 |
| `erel` | eg2 release | 0..1 |
| `eg2pitch` | eg2 to pitch | 0..1 |
| `eg2lp` | eg2 to low-pass | 0..1 |
| `eg2hp` | eg2 to high-pass | 0..1 |
| `kbd` | key follow | 0..1 |
| `glide` | portamento | 0..1 |
| `legato` | legato | 0..1: retrigger / legato |
| `vel` | velocity | 0..1 |
| `vol` | volume | 0..1 |

## Factory presets

0. Screamer
1. Ring Bell
2. Wow Bass
3. Sync Alarm
4. Filter Wah
5. Pulse Lead
6. Wind Noise
7. Zap Pitch
8. Dual Saw
9. Sub Growl
10. Self Sine
11. Tri Flute

## Messages

`note <n> <vel>`, `bend`, `bendrange`, `mod`, `aftertouch`, `sustain`, `panic`, `set <name> <value>`, `params`, `preset <n>`, `next`, `prev`, `mutate <0..1>`, `A`, `B`, `morph <0..1>`, `store <name>`, `save <file>`, `load <file>`, `data <0..1>`, `datamap <param> <amount>`, `datamap clear`.

`data` is a general modulation inlet. Map it to up to four continuous parameters with `datamap` and feed it any value scaled to 0..1 (a sensor, a column of a csv file, a seismic trace). Parameters are also named `<name>-<param>` so the stash preset library can store scenes (naming rules followed, not yet tested together).

Every synth has A/B morph (`A`, `B`, `morph`) and `mutate` for random variations.

Not compared against hardware: these are models of the architecture with original presets.

## What is simplified

- The patch panel is a matrix of amounts and not physical cables. 0.5 means no modulation.
- Filters are a two-pole model with a soft-clipped state and not a circuit simulation of the Korg 35 chip.
- No external signal input.
- Not compared against hardware or other software.
