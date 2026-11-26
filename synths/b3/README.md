# b3~ - B-3 style tonewheel organ

Part of the Pd synth collection by Sofia Vallejo Budziszewski, IEM KUG Graz Austria. Version 1 (2026). Up to 32 voices, 14 presets.

Nine drawbars with single-trigger percussion key click scanner vibrato and chorus overdrive and a rotary speaker with horn and drum. Up to 32 voices.

## Use

Open `b3~-help.pd`. It lists every parameter with its message name and has clickable examples of every message. Create the object as `[b3~]`, `[b3~ name]` or `[b3~ name 8]` (name and voice count).

## Parameters

All values are 0 to 1. Switches take a step number. Set one with `set <name> <value>`.

| Name | Meaning | Choices |
|---|---|---|
| `d16` | 16 | 0..1 |
| `d513` | 5 1/3 | 0..1 |
| `d8` | 8 | 0..1 |
| `d4` | 4 | 0..1 |
| `d223` | 2 2/3 | 0..1 |
| `d2` | 2 | 0..1 |
| `d135` | 1 3/5 | 0..1 |
| `d113` | 1 1/3 | 0..1 |
| `d1` | 1 | 0..1 |
| `perc` | percussion | 0..1: off / on |
| `pharm` | harmonic | 0..1: 2nd / 3rd |
| `pdec` | decay | 0..1: fast / slow |
| `pvol` | level | 0..1: soft / normal |
| `click` | key click | 0..1 |
| `vib` | vibrato | 0..6: off / V1 V2 V3 / C1 C2 C3 |
| `leslie` | rotary | 0..2: off / slow / fast |
| `drive` | drive | 0..1 |
| `vol` | volume | 0..1 |

## Factory presets

0. Jazz 888
1. Full Organ
2. Gospel
3. Rock
4. Mellow Flute
5. Church
6. Pad Organ
7. Percussive
8. Ballad
9. Reed Buzz
10. Soft Pad 4
11. Third Perc
12. Dry Organ
13. Thin Lead

## Messages

`note <n> <vel>`, `bend`, `bendrange`, `mod`, `aftertouch`, `sustain`, `panic`, `set <name> <value>`, `params`, `preset <n>`, `next`, `prev`, `mutate <0..1>`, `A`, `B`, `morph <0..1>`, `store <name>`, `save <file>`, `load <file>`, `data <0..1>`, `datamap <param> <amount>`, `datamap clear`.

`data` is a general modulation inlet. Map it to up to four continuous parameters with `datamap` and feed it any value scaled to 0..1 (a sensor, a column of a csv file, a seismic trace). Parameters are also named `<name>-<param>` so the stash preset library can store scenes (naming rules followed, not yet tested together).

Every synth has A/B morph (`A`, `B`, `morph`) and `mutate` for random variations.

Not compared against hardware: these are models of the architecture with original presets.

## What is simplified

- Additive equal-tempered sines. This is not a tonewheel-by-tonewheel model and has no crosstalk or leakage.
- Percussion fires on the first held key only and cuts the 1 foot drawbar as on the real organ.
- Rotary speaker is horn and drum with inertia. It is an effect model and not a measured cabinet.
- Not compared against hardware or other software.
