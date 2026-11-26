# dx7~

Polyphonic six-operator FM synthesizer for Pure Data, modelled on the Yamaha DX7. It loads real DX7
SysEx banks and follows the original engine: 32 algorithms, operator feedback (including the
multi-operator loops of algorithms 4 and 6), DX7 envelopes with the original level curve, rate and
level scaling, velocity sensitivity, pitch EG, LFO, fixed-frequency operators.

Like Arturia's DX7 V it keeps the original voice model and SysEx compatibility and adds a few
things on top: 8 waveforms per operator and a vintage / modern output stage. It does not have
their filters, effects, modulation matrix, second LFO or unison.

## Build (macOS)

    cmake -S . -B build
    cmake --build build
    cmake --install build --prefix /Users/sofiavallejo/Documents/PdProjects/externals

That produces a universal (arm64 + x86_64) `dx7~.pd_darwin` and copies it and `dx7~-help.pd` into
the prefix. If CMake can't find Pd's headers, add
`-DPD_INCLUDE_DIR=/Applications/Pd-0.55-1.app/Contents/Resources/src`.
Engine self-test (no Pd needed): `-DDX7_BUILD_TESTS=ON`, then `./build/dx7_selftest`.

## Use

    [dx7~ 16]      creation argument = number of voices (1..32, default 16)

Outlets, left to right: mono signal, info messages (see below), the patch name as a symbol, and the
name of the library bank file as a symbol.
Open `dx7~-help.pd` for a working example and the full message list, and `dx7~-browser.pd` for the
patch browser (library, favourites, saving, morph).

Info outlet messages: `program <n>`, `algo <n>`, `fb <n>`, `ops <l1..l6>` (operator output levels) after
every voice change; `lib <position> <count>`, `libbank <n> <of>`, `slot <n>`, `fav <0/1>` while browsing;
`scan <voices> <banks> <duplicates> <blank>` after a scan; `bank <n>` after loading a bank.

| message | |
|---|---|
| `note <pitch> <vel>` or a list | velocity 0 = note off |
| `noteoff <pitch>`, `sustain <0/1>`, `panic` | |
| `bend <-1..1>`, `bendrange <semitones>`, `modwheel <0..1>`, `modassign <pitch> <amp>` | |
| `load <file> [program]` | `.syx` 32-voice bank or single voice; relative to the patch or a full path |
| `program <0..31>`, `sysex <bytes...>`, `init`, `print`, `name <text>` | |
| `set <name> <value>` | algorithm (1-32), feedback, oscsync, lfospeed, lfodelay, lfopmd, lfoamd, lfosync, lfowave, lfopms, transpose (-24..24), pegr1-4, pegl1-4 |
| `op <1-6> <name> <value>` | r1-r4, l1-l4, bp, ld, rd, lc, rc, rs, ams, kvs, level, mode, coarse, fine, detune |
| `opswitch <1-6> <0/1>`, `wave <1-6> <0-7>`, `dac <vintage/modern>` | wave 0 = sine, 1-7 = extra waves |
| `tune <cents>`, `gain <x>`, `param <index> <value>` | `param` is raw VCED access |

## Library, favourites, saving

Point `dx7~` at a folder of `.syx` banks and browse thousands of voices. Duplicates and blank voices
are dropped while scanning (a voice is a duplicate if every parameter matches, the name is ignored).
Files with several SysEx messages are read completely. Relative paths are looked up next to the patch.

| message | |
|---|---|
| `lib scan <folder or file>` | reads all `.syx` files, sub-folders included, and selects the first voice |
| `lib next`, `lib prev`, `lib first`, `lib last`, `lib random`, `lib goto <n>` | move inside the current selection (wraps around) |
| `lib bank next`, `lib bank prev`, `lib bank all`, `lib bank <n>` | step through the banks, or show everything again |
| `lib search <text>` / `lib search clear` | filter by voice name or bank file name (not case sensitive) |
| `lib favs <0/1>`, `lib algo <1-32, 0 = any>`, `lib reset` | favourites only, algorithm filter, clear all filters |
| `lib fav [0/1]` | mark or unmark the current voice as a favourite (toggles without an argument) |
| `lib store`, `name <text>` | keep the voice you are playing, edits included, in a USER bank as a favourite |
| `savefavs <file>` | writes the favourites as 32-voice banks (`file.syx`, or `file_1.syx`, `file_2.syx` ...) |
| `favload <file>` | marks the voices of a saved favourites bank as favourites again |
| `savevoice <file>`, `savebank <file>` | the current voice as a single-voice message, the loaded 32-voice bank |
| `lib A`, `lib B`, `morph <0..1>` | store two voices and blend between them |
| `mutate <0..1>` | random variation of the current voice (keeps algorithm and frequency ratios) |
| `lib clear` | forget the library, favourites included |

Morph interpolates the continuous parameters (envelopes, levels, feedback, fine tune, pitch EG, LFO speed
and depth). Switches such as algorithm, frequency ratios, curves and waves jump at the halfway point.
The favourites live in memory: use `savefavs` to keep them and `favload` after the next scan.

Operators are numbered as on the DX7 (OP1 is the carrier at the bottom of algorithm 1).
Curve values: 0 = -LIN, 1 = -EXP, 2 = +EXP, 3 = +LIN.

## What was checked, and what wasn't

- Rendered all 32 voices of the ROM1A bank at three pitches against Google's open-source msfa
  emulator: matching levels and waveforms, with LFO and pitch EG compared separately (pitch EG tracks
  within a few cents; waveform correlation drops on LFO-modulated and chaotic-feedback patches
  because of tick-size and rounding differences, with identical RMS).
- Library tests (`test/libtest.c`): pack and unpack round trip on every voice of about 22,000 voices from public
  banks, SysEx writing with checksums, duplicate removal, filters, favourites, morph and mutate.
- Engine self-tests: pitch, ratio/fixed/detune, level scale (0.75 dB per step), LFO depth and
  delay, tremolo, release, sustain, voice stealing, SysEx parsing, 300 random patches (no NaN).
- The Pd wrapper was compiled against Pd's real `m_pd.h` and run under AddressSanitizer with a
  stand-in for Pd's API (messages, bank loading, errors, DSP). It has not been run inside a live
  Pd, and not compared with real hardware or with Arturia's plugin.

## Limits and approximations

- Parameter edits apply to notes played afterwards (the LFO changes immediately).
- One global LFO, as on the DX7. Fixed-frequency operators ignore LFO and pitch EG but follow bend.
- With `oscsync` off the operator phases start at random.
- Amplitude modulation curve and the envelope stage-hold time are fitted to measurements made by
  others (see the provenance note in `dx7core.h`). The pitch EG rate mapping is the msfa one.
- The vintage output stage is a simple 12-bit quantiser with hard clipping, not a measured model of
  the DX7's converter. Output scale is fixed at 0.1 (a full-level carrier peaks at 0.2); use `gain`.
- No portamento, no mono mode, mono output.
