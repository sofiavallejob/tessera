# alphajuno~ - Alpha Juno style 6-voice synth

Part of the Pd synth collection by Sofia Vallejo Budziszewski, IEM KUG Graz Austria. Version 1 (2026). 6 voices, 16 presets.

One DCO per voice with saw pulse sub and noise then a switched high-pass and a 24 dB low-pass. One envelope for pitch filter and amplitude and a chorus with adjustable rate. Six voices.

## Use

Open `alphajuno~-help.pd`. It lists every parameter with its message name and has clickable examples of every message. Create the object as `[alphajuno~]`, `[alphajuno~ name]` or `[alphajuno~ name 8]` (name and voice count).

## Parameters

All values are 0 to 1. Switches take a step number. Set one with `set <name> <value>`.

| Name | Meaning | Choices |
|---|---|---|
| `range` | range | 0..2: 16 / 8 / 4 feet |
| `saw` | saw | 0..1 |
| `pulse` | pulse | 0..1 |
| `sub` | sub osc | 0..1 |
| `noise` | noise | 0..1 |
| `pw` | pulse width | 0..1 |
| `pwmode` | pw source | 0..2: manual / lfo / envelope |
| `pwamt` | pw mod | 0..1 |
| `lforate` | lfo rate | 0..1 |
| `lfodelay` | lfo delay | 0..1 |
| `lfowave` | lfo wave | 0..2: triangle / square / random |
| `lfopitch` | lfo to pitch | 0..1 |
| `lfofilt` | lfo to filter | 0..1 |
| `hpf` | high-pass | 0..3: off / low / mid / high |
| `cutoff` | cutoff | 0..1 |
| `res` | resonance | 0..1 |
| `envfilt` | env to filter | 0..1 |
| `envpitch` | env to pitch | 0..1 |
| `kbd` | key follow | 0..1 |
| `att` | attack | 0..1 |
| `dec` | decay | 0..1 |
| `sus` | sustain | 0..1 |
| `rel` | release | 0..1 |
| `vca` | amp source | 0..1: gate / envelope |
| `vel` | velocity | 0..1 |
| `drive` | drive | 0..1 |
| `chorus` | chorus | 0..2: off / I / II |
| `chrate` | chorus rate | 0..1 |
| `vol` | volume | 0..1 |

## Factory presets

0. Warm Brass
1. PWM Strings
2. Glass Pad
3. Bass One
4. Sub Floor
5. Poly Lead
6. Soft Pluck
7. Sweep Pad
8. Brass Stab
9. Vox Pad
10. Clav Funk
11. Air
12. Pulse Keys
13. Dream Chorus
14. Saw Stack
15. Harp

## Messages

`note <n> <vel>`, `bend`, `bendrange`, `mod`, `aftertouch`, `sustain`, `panic`, `set <name> <value>`, `params`, `preset <n>`, `next`, `prev`, `mutate <0..1>`, `A`, `B`, `morph <0..1>`, `store <name>`, `save <file>`, `load <file>`, `data <0..1>`, `datamap <param> <amount>`, `datamap clear`.

`data` is a general modulation inlet. Map it to up to four continuous parameters with `datamap` and feed it any value scaled to 0..1 (a sensor, a column of a csv file, a seismic trace). Parameters are also named `<name>-<param>` so the stash preset library can store scenes (naming rules followed, not yet tested together).

Every synth has A/B morph (`A`, `B`, `morph`) and `mutate` for random variations.

Not compared against hardware: these are models of the architecture with original presets.

## What is simplified

- The hardware has a 4-stage envelope with three time and level pairs. Here it is an ADSR routed to pitch filter and amplitude.
- The low-pass does not self-oscillate (as on the hardware). Chorus rate is adjustable.
- No portamento or chord memory yet.
- Not compared against hardware or other software.
