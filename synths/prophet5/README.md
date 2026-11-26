# prophet5~ - Prophet-5 style 5-voice polysynth

Part of the Pd synth collection by Sofia Vallejo Budziszewski, IEM KUG Graz Austria. Version 1 (2026). 5 voices, 16 presets.

Five voices with two oscillators each. Oscillator A has saw and pulse with pulse width and oscillator B has saw triangle and pulse with sync and a low-frequency mode. Poly-mod sends the filter envelope and oscillator B to pitch pulse width and filter. A 24 dB low-pass two ADSR envelopes an LFO and analogue-style drift.

## Use

Open `prophet5~-help.pd`. It lists every parameter with its message name and has clickable examples of every message. Create the object as `[prophet5~]`, `[prophet5~ name]` or `[prophet5~ name 8]` (name and voice count).

## Parameters

All values are 0 to 1. Switches take a step number. Set one with `set <name> <value>`.

| Name | Meaning | Choices |
|---|---|---|
| `asaw` | a saw | 0..1: off / on |
| `apulse` | a pulse | 0..1: off / on |
| `pw` | pulse width | 0..1 |
| `bsaw` | b saw | 0..1: off / on |
| `btri` | b triangle | 0..1: off / on |
| `bpulse` | b pulse | 0..1: off / on |
| `bfreq` | b frequency | 0..1 |
| `bfine` | b fine tune | 0..1 |
| `blow` | b low freq | 0..1: off / on |
| `bkbd` | b key follow | 0..1: off / on |
| `sync` | sync | 0..1: off / on |
| `pmfenv` | poly mod filter env | 0..1 |
| `pmoscb` | poly mod osc b | 0..1 |
| `pmfreq` | pm to freq a | 0..1: off / on |
| `pmpw` | pm to pw a | 0..1: off / on |
| `pmfilt` | pm to filter | 0..1: off / on |
| `mixa` | level a | 0..1 |
| `mixb` | level b | 0..1 |
| `noise` | noise | 0..1 |
| `cutoff` | cutoff | 0..1 |
| `res` | resonance | 0..1 |
| `envamt` | env amount | 0..1 |
| `kbd` | key follow | 0..2: off / half / full |
| `fatt` | filter attack | 0..1 |
| `fdec` | filter decay | 0..1 |
| `fsus` | filter sustain | 0..1 |
| `frel` | filter release | 0..1 |
| `aatt` | amp attack | 0..1 |
| `adec` | amp decay | 0..1 |
| `asus` | amp sustain | 0..1 |
| `arel` | amp release | 0..1 |
| `lforate` | lfo rate | 0..1 |
| `lfoshape` | lfo shape | 0..2: triangle / saw / square |
| `lfofreq` | lfo to freq | 0..1 |
| `lfopw` | lfo to pw | 0..1 |
| `lfofilt` | lfo to filter | 0..1 |
| `drift` | drift | 0..1 |
| `vel` | velocity | 0..1 |
| `vol` | volume | 0..1 |

## Factory presets

0. Warm Brass
1. Poly Strings
2. Soft Pad
3. Sync Lead
4. Poly Bass
5. Fm Bell
6. Pwm Keys
7. Pluck
8. Choir
9. Hollow Reed
10. Noise Wash
11. Detuned Saws
12. Zap
13. Soft Bell Pad

## Messages

`note <n> <vel>`, `bend`, `bendrange`, `mod`, `aftertouch`, `sustain`, `panic`, `set <name> <value>`, `params`, `preset <n>`, `next`, `prev`, `mutate <0..1>`, `A`, `B`, `morph <0..1>`, `store <name>`, `save <file>`, `load <file>`, `data <0..1>`, `datamap <param> <amount>`, `datamap clear`.

`data` is a general modulation inlet. Map it to up to four continuous parameters with `datamap` and feed it any value scaled to 0..1 (a sensor, a column of a csv file, a seismic trace). Parameters are also named `<name>-<param>` so the stash preset library can store scenes (naming rules followed, not yet tested together).

Every synth has A/B morph (`A`, `B`, `morph`) and `mutate` for random variations.

Not compared against hardware: these are models of the architecture with original presets.

## What is simplified

- Original presets and a model of the architecture. Each voice has its own LFO phase and drift instead of one global LFO.
- No unison or portamento yet.
- Oscillator B is computed first so it can act as sync master and poly-mod source.
- Not compared against hardware or other software.
