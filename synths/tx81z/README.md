# tx81z~ - TX81Z style 4-operator FM synth

Part of the Pd synth collection by Sofia Vallejo Budziszewski, IEM KUG Graz Austria. Version 1 (2026). 8 voices, 16 presets.

Four sine-based operators with eight waveforms each and eight algorithms. Every operator has a frequency ratio a fine tune and its own ADSR. Operator 4 has feedback. LFO to pitch and amplitude. Eight voices.

## Use

Open `tx81z~-help.pd`. It lists every parameter with its message name and has clickable examples of every message. Create the object as `[tx81z~]`, `[tx81z~ name]` or `[tx81z~ name 8]` (name and voice count).

## Parameters

All values are 0 to 1. Switches take a step number. Set one with `set <name> <value>`.

| Name | Meaning | Choices |
|---|---|---|
| `ratio1` | op 1 ratio | 0..15: 0.5 / 0.71 / 0.78 / 0.87 / 1 / 1.41 / 1.57 / 1.73 / 2 / 2.82 / 3 / 3.14 / 3.46 / 4 / 5 / 7 |
| `fine1` | op 1 fine | 0..1 |
| `wave1` | op 1 wave | 0..7: sine / half / abs / quarter / double half / double abs / square / decay square |
| `level1` | op 1 level | 0..1 |
| `att1` | op 1 attack | 0..1 |
| `dec1` | op 1 decay | 0..1 |
| `sus1` | op 1 sustain | 0..1 |
| `rel1` | op 1 release | 0..1 |
| `ratio2` | op 2 ratio | 0..15: 0.5 / 0.71 / 0.78 / 0.87 / 1 / 1.41 / 1.57 / 1.73 / 2 / 2.82 / 3 / 3.14 / 3.46 / 4 / 5 / 7 |
| `fine2` | op 2 fine | 0..1 |
| `wave2` | op 2 wave | 0..7: sine / half / abs / quarter / double half / double abs / square / decay square |
| `level2` | op 2 level | 0..1 |
| `att2` | op 2 attack | 0..1 |
| `dec2` | op 2 decay | 0..1 |
| `sus2` | op 2 sustain | 0..1 |
| `rel2` | op 2 release | 0..1 |
| `ratio3` | op 3 ratio | 0..15: 0.5 / 0.71 / 0.78 / 0.87 / 1 / 1.41 / 1.57 / 1.73 / 2 / 2.82 / 3 / 3.14 / 3.46 / 4 / 5 / 7 |
| `fine3` | op 3 fine | 0..1 |
| `wave3` | op 3 wave | 0..7: sine / half / abs / quarter / double half / double abs / square / decay square |
| `level3` | op 3 level | 0..1 |
| `att3` | op 3 attack | 0..1 |
| `dec3` | op 3 decay | 0..1 |
| `sus3` | op 3 sustain | 0..1 |
| `rel3` | op 3 release | 0..1 |
| `ratio4` | op 4 ratio | 0..15: 0.5 / 0.71 / 0.78 / 0.87 / 1 / 1.41 / 1.57 / 1.73 / 2 / 2.82 / 3 / 3.14 / 3.46 / 4 / 5 / 7 |
| `fine4` | op 4 fine | 0..1 |
| `wave4` | op 4 wave | 0..7: sine / half / abs / quarter / double half / double abs / square / decay square |
| `level4` | op 4 level | 0..1 |
| `att4` | op 4 attack | 0..1 |
| `dec4` | op 4 decay | 0..1 |
| `sus4` | op 4 sustain | 0..1 |
| `rel4` | op 4 release | 0..1 |
| `algo` | algorithm | 0..7: 1 / 2 / 3 / 4 / 5 / 6 / 7 / 8 (see the readme for the layouts) |
| `feedback` | feedback op 4 | 0..1 |
| `lforate` | lfo rate | 0..1 |
| `lfopm` | lfo to pitch | 0..1 |
| `lfoam` | lfo to amp | 0..1 |
| `vel` | velocity | 0..1 |
| `vol` | volume | 0..1 |

## Factory presets

0. Soft Keys
1. Tine Piano
2. Glass Bell
3. Marimba
4. Brass Stab
5. Slap Bass
6. Digi Strings
7. Organ Mix
8. Clav
9. Vibes
10. Breath Flute
11. Pluck Harp
12. Square Lead
13. Metal Hit
14. Warm Pad
15. Half Sine Bell

## Messages

`note <n> <vel>`, `bend`, `bendrange`, `mod`, `aftertouch`, `sustain`, `panic`, `set <name> <value>`, `params`, `preset <n>`, `next`, `prev`, `mutate <0..1>`, `A`, `B`, `morph <0..1>`, `store <name>`, `save <file>`, `load <file>`, `data <0..1>`, `datamap <param> <amount>`, `datamap clear`.

`data` is a general modulation inlet. Map it to up to four continuous parameters with `datamap` and feed it any value scaled to 0..1 (a sensor, a column of a csv file, a seismic trace). Parameters are also named `<name>-<param>` so the stash preset library can store scenes (naming rules followed, not yet tested together).

Every synth has A/B morph (`A`, `B`, `morph`) and `mutate` for random variations.

Not compared against hardware: these are models of the architecture with original presets.

## What is simplified

- Eight algorithms (modulator to carrier): 1 is 4>3>2>1, 2 is (4+3)>2>1, 3 is 4>3>1 and 2>1, 4 is 4>3>2>1 split with operator 4 also a carrier, 5 is 4>3>2 plus 1, 6 is 4 into 1 2 and 3, 7 is 4>3 plus 1 and 2 free, 8 is all four as carriers. Numbering follows this table and not a Yamaha chart.
- One ADSR per operator instead of the five-stage envelope of the hardware. No operator key scaling.
- Frequency ratios are a table of 16 values and not the continuous coarse and fine of the TX81Z.
- Not compared against hardware or other software.
