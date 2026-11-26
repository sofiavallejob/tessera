# cz101~ - CZ-101 style phase distortion synth

Part of the Pd synth collection by Sofia Vallejo Budziszewski, IEM KUG Graz Austria. Version 1 (2026). 8 voices, 14 presets.

Phase distortion: each of the two lines per voice is a sine whose phase is bent into saw square pulse double sine and resonant waveforms. An envelope for the distortion amount and one for amplitude plus pitch envelope and ring or noise modulation. Eight voices.

## Use

Open `cz101~-help.pd`. It lists every parameter with its message name and has clickable examples of every message. Create the object as `[cz101~]`, `[cz101~ name]` or `[cz101~ name 8]` (name and voice count).

## Parameters

All values are 0 to 1. Switches take a step number. Set one with `set <name> <value>`.

| Name | Meaning | Choices |
|---|---|---|
| `wave1` | op 1 wave | 0..7: saw / square / pulse / double sine / saw pulse / resonance I / resonance II / resonance III |
| `wave2` | op 2 wave | 0..7: saw / square / pulse / double sine / saw pulse / resonance I / resonance II / resonance III |
| `dcw1` | distortion 1 | 0..1 |
| `dcw2` | distortion 2 | 0..1 |
| `detune` | detune line 2 | 0..1 |
| `oct2` | line 2 octave | 0..2: one octave down / same / one octave up |
| `mod` | line modulation | 0..2: off / ring / noise |
| `mix2` | level line 2 | 0..1 |
| `dcwkbd` | dcw key follow | 0..1 |
| `dcwatt` | dcw attack | 0..1 |
| `dcwdec` | dcw decay | 0..1 |
| `dcwsus` | dcw sustain | 0..1 |
| `dcwrel` | dcw release | 0..1 |
| `att` | attack | 0..1 |
| `dec` | decay | 0..1 |
| `sus` | sustain | 0..1 |
| `rel` | release | 0..1 |
| `pitchamt` | pitch env amount | 0..1 |
| `pitchdec` | pitch env decay | 0..1 |
| `vibrate` | vibrato rate | 0..1 |
| `vibdepth` | vibrato depth | 0..1 |
| `vibdelay` | vibrato delay | 0..1 |
| `vel` | velocity | 0..1 |
| `vol` | volume | 0..1 |

## Factory presets

0. Saw Brass
1. Pink Pluck
2. Cosmic Bass
3. Resonance Lead
4. Square Keys
5. Double Bell
6. Glass Reso
7. String Pad
8. Hollow Pipe
9. Zapper
10. Wood Block
11. Ring Mod Metal
12. Noise Edge
13. Soft Sine

## Messages

`note <n> <vel>`, `bend`, `bendrange`, `mod`, `aftertouch`, `sustain`, `panic`, `set <name> <value>`, `params`, `preset <n>`, `next`, `prev`, `mutate <0..1>`, `A`, `B`, `morph <0..1>`, `store <name>`, `save <file>`, `load <file>`, `data <0..1>`, `datamap <param> <amount>`, `datamap clear`.

`data` is a general modulation inlet. Map it to up to four continuous parameters with `datamap` and feed it any value scaled to 0..1 (a sensor, a column of a csv file, a seismic trace). Parameters are also named `<name>-<param>` so the stash preset library can store scenes (naming rules followed, not yet tested together).

Every synth has A/B morph (`A`, `B`, `morph`) and `mutate` for random variations.

Not compared against hardware: these are models of the architecture with original presets.

## What is simplified

- Phase distortion curves are in the spirit of the CZ and not copied. The hardware has eight-stage envelopes, here they are ADSR.
- Two times oversampling. No line modulation beyond ring and noise.
- Not compared against hardware or other software.
