# sh101~ - SH-101 style monophonic synth

Part of the Pd synth collection by Sofia Vallejo Budziszewski, IEM KUG Graz Austria. Version 1 (2026). Mono, 16 presets.

One oscillator with saw pulse sub and noise into a self-oscillating 24 dB ladder filter with one envelope and one LFO. Mono with portamento and legato.

## Use

Open `sh101~-help.pd`. It lists every parameter with its message name and has clickable examples of every message. Create the object as `[sh101~]`, `[sh101~ name]` or `[sh101~ name 8]` (name and voice count).

## Parameters

All values are 0 to 1. Switches take a step number. Set one with `set <name> <value>`.

| Name | Meaning | Choices |
|---|---|---|
| `saw` | saw | 0..1 |
| `pulse` | pulse | 0..1 |
| `sub` | sub osc | 0..1 |
| `submode` | sub mode | 0..2: 1 down / 2 down / 2 pulse |
| `noise` | noise | 0..1 |
| `pw` | pulse width | 0..1 |
| `pwmode` | pw source | 0..2: manual / lfo / envelope |
| `pwamt` | pw mod | 0..1 |
| `lforate` | lfo rate | 0..1 |
| `lfowave` | lfo wave | 0..2: triangle / square / random |
| `lfopitch` | lfo to pitch | 0..1 |
| `lfofilt` | lfo to filter | 0..1 |
| `cutoff` | cutoff | 0..1 |
| `res` | resonance | 0..1 |
| `envamt` | env amount | 0..1 |
| `kbd` | key follow | 0..1 |
| `att` | attack | 0..1 |
| `dec` | decay | 0..1 |
| `sus` | sustain | 0..1 |
| `rel` | release | 0..1 |
| `vca` | amp source | 0..1: gate / envelope |
| `glide` | portamento | 0..1 |
| `legato` | legato | 0..1: retrigger / legato |
| `range` | range | 0..3: 16 / 8 / 4 / 2 feet |
| `vel` | velocity | 0..1 |
| `drive` | drive | 0..1 |
| `vol` | volume | 0..1 |

## Factory presets

0. Classic Bass
1. Square Sub
2. Squelch
3. PWM Lead
4. Fat Saw Lead
5. Zap
6. Pluck
7. Resonant Sweep
8. Laser Siren
9. Noise Hit
10. Wind
11. Hollow Reed
12. Wobble
13. Blip
14. Rez Pad
15. Self Osc Sine

## Messages

`note <n> <vel>`, `bend`, `bendrange`, `mod`, `aftertouch`, `sustain`, `panic`, `set <name> <value>`, `params`, `preset <n>`, `next`, `prev`, `mutate <0..1>`, `A`, `B`, `morph <0..1>`, `store <name>`, `save <file>`, `load <file>`, `data <0..1>`, `datamap <param> <amount>`, `datamap clear`.

`data` is a general modulation inlet. Map it to up to four continuous parameters with `datamap` and feed it any value scaled to 0..1 (a sensor, a column of a csv file, a seismic trace). Parameters are also named `<name>-<param>` so the stash preset library can store scenes (naming rules followed, not yet tested together).

Every synth has A/B morph (`A`, `B`, `morph`) and `mutate` for random variations.

Not compared against hardware: these are models of the architecture with original presets.

## What is simplified

- Original presets and a model of the architecture: this is not a copy of any ROM or circuit.
- The filter is a zero-delay-feedback ladder with tanh drive and self-oscillates at full resonance.
- No sequencer or arpeggiator yet.
- Not compared against hardware or other software.
