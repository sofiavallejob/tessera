# mellotron~ - Mellotron style tape keyboard

Part of the Pd synth collection by Sofia Vallejo Budziszewski, IEM KUG Graz Austria. Version 1 (2026). 16 voices, 8 presets.

A keyboard of tapes. Load your own wav files onto the keys and each key plays its tape from the start. The tape stops after a few seconds like the real machine until you release the key. Wow flutter drift hiss and tone loss. A built-in synthetic stand-in plays when no samples are loaded. Sixteen voices.

## Use

Open `mellotron~-help.pd`. It lists every parameter with its message name and has clickable examples of every message. Create the object as `[mellotron~]`, `[mellotron~ name]` or `[mellotron~ name 8]` (name and voice count).

## Parameters

All values are 0 to 1. Switches take a step number. Set one with `set <name> <value>`.

| Name | Meaning | Choices |
|---|---|---|
| `tape` | tape sound | 0..2: flute / strings / choir (stand-in when no wav files are loaded) |
| `length` | tape length | 0..1 |
| `loop` | loop tape | 0..1: off / on |
| `pitch` | pitch control | 0..1 |
| `wow` | wow | 0..1 |
| `flutter` | flutter | 0..1 |
| `drift` | drift | 0..1 |
| `tone` | tone | 0..1 |
| `hiss` | tape hiss | 0..1 |
| `att` | attack | 0..1 |
| `rel` | release | 0..1 |
| `vel` | velocity | 0..1 |
| `vol` | volume | 0..1 |

## Factory presets

0. Flute Tape
1. String Tape
2. Choir Tape
3. Worn Tape
4. Clean Tape
5. Short Tape
6. Looping Pad
7. Seasick Choir

## Messages

`note <n> <vel>`, `bend`, `bendrange`, `mod`, `aftertouch`, `sustain`, `panic`, `set <name> <value>`, `params`, `preset <n>`, `next`, `prev`, `mutate <0..1>`, `A`, `B`, `morph <0..1>`, `store <name>`, `save <file>`, `load <file>`, `data <0..1>`, `datamap <param> <amount>`, `datamap clear`.

`data` is a general modulation inlet. Map it to up to four continuous parameters with `datamap` and feed it any value scaled to 0..1 (a sensor, a column of a csv file, a seismic trace). Parameters are also named `<name>-<param>` so the stash preset library can store scenes (naming rules followed, not yet tested together).

Every synth has A/B morph (`A`, `B`, `morph`) and `mutate` for random variations.

Not compared against hardware: these are models of the architecture with original presets.

## What is simplified

- No samples are shipped. Load your own wav files with `samples <folder>` (name each file with its root midi note like 48.wav) or `sample <note> <file>`.
- One file without a number is treated as middle C and transposed over the keyboard. Every key with no tape uses the nearest tape.
- Without samples a synthetic flute strings or choir stand-in plays. It is not a recording of any instrument.
- Linear interpolation: pitching a tape far up will alias.
- Not compared against hardware or other software.
