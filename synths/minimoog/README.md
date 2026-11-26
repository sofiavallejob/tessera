# minimoog~ - Minimoog Model D style monosynth

Part of the Pd synth collection by Sofia Vallejo Budziszewski, IEM KUG Graz Austria. Version 1 (2026). Mono, 14 presets.

Three oscillators with six waveforms and six ranges each plus hard sync and a noise source go into a 24 dB ladder filter with emphasis and contour. Filter and loudness envelopes with a decay switch glide legato LFO and overload drive. Mono.

## Use

Open `minimoog~-help.pd`. It lists every parameter with its message name and has clickable examples of every message. Create the object as `[minimoog~]`, `[minimoog~ name]` or `[minimoog~ name 8]` (name and voice count).

## Parameters

All values are 0 to 1. Switches take a step number. Set one with `set <name> <value>`.

| Name | Meaning | Choices |
|---|---|---|
| `range1` | osc 1 range | 0..5: lo / 32 / 16 / 8 / 4 / 2 feet |
| `wave1` | op 1 wave | 0..5: triangle / shark tooth / saw / reverse saw / wide pulse / narrow pulse |
| `range2` | osc 2 range | 0..5: lo / 32 / 16 / 8 / 4 / 2 feet |
| `wave2` | op 2 wave | 0..5: triangle / shark tooth / saw / reverse saw / wide pulse / narrow pulse |
| `tune2` | osc 2 tune | 0..1 |
| `range3` | osc 3 range | 0..5: lo / 32 / 16 / 8 / 4 / 2 feet |
| `wave3` | op 3 wave | 0..5: triangle / shark tooth / saw / reverse saw / wide pulse / narrow pulse |
| `tune3` | osc 3 tune | 0..1 |
| `kbd3` | osc 3 keyboard | 0..1: fixed / keyboard |
| `sync` | sync | 0..1: off / on |
| `mix1` | osc 1 level | 0..1 |
| `mix2` | level line 2 | 0..1 |
| `mix3` | osc 3 level | 0..1 |
| `noise` | noise | 0..1 |
| `noisetype` | noise colour | 0..1: white / pink |
| `cutoff` | cutoff | 0..1 |
| `res` | resonance | 0..1 |
| `envamt` | env amount | 0..1 |
| `kbd` | key follow | 0..2: off / half / full |
| `fatt` | filter attack | 0..1 |
| `fdec` | filter decay | 0..1 |
| `fsus` | filter sustain | 0..1 |
| `aatt` | amp attack | 0..1 |
| `adec` | amp decay | 0..1 |
| `asus` | amp sustain | 0..1 |
| `decay` | decay switch | 0..1: off / on |
| `glide` | portamento | 0..1 |
| `legato` | legato | 0..1: retrigger / legato |
| `lforate` | lfo rate | 0..1 |
| `lfowave` | lfo wave | 0..1: triangle / square |
| `lfopitch` | lfo to pitch | 0..1 |
| `lfofilt` | lfo to filter | 0..1 |
| `vel` | velocity | 0..1 |
| `drive` | drive | 0..1 |
| `vol` | volume | 0..1 |

## Factory presets

0. Fat Bass
1. Classic Lead
2. Sync Screamer
3. Pulse Bass
4. Hollow Flute
5. Brass Section
6. Squelch
7. Zap Tom
8. Wobble
9. Vibrato Lead
10. Wind
11. Octave Bass
12. Soft Triangle
13. Pluck Lead

## Messages

`note <n> <vel>`, `bend`, `bendrange`, `mod`, `aftertouch`, `sustain`, `panic`, `set <name> <value>`, `params`, `preset <n>`, `next`, `prev`, `mutate <0..1>`, `A`, `B`, `morph <0..1>`, `store <name>`, `save <file>`, `load <file>`, `data <0..1>`, `datamap <param> <amount>`, `datamap clear`.

`data` is a general modulation inlet. Map it to up to four continuous parameters with `datamap` and feed it any value scaled to 0..1 (a sensor, a column of a csv file, a seismic trace). Parameters are also named `<name>-<param>` so the stash preset library can store scenes (naming rules followed, not yet tested together).

Every synth has A/B morph (`A`, `B`, `morph`) and `mutate` for random variations.

Not compared against hardware: these are models of the architecture with original presets.

## What is simplified

- Original presets and a model of the architecture. No hardware-measured filter or oscillator curves.
- Oscillator range lo is a fixed low-frequency range used as an LFO.
- No external audio input and no keyboard glide time calibration.
- Not compared against hardware or other software.
