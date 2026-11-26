# Pd synths

Classic synth architectures as Pure Data externals, with original presets. By Sofia Vallejo Budziszewski, IEM KUG Graz Austria. Version 1 (2026).

| Folder | External | What it is |
|---|---|---|
| `dx7/` | `dx7~` | DX7-style 6-operator FM synth with SysEx support and a patch library browser |
| `sh101/` | `sh101~` | SH-101 style monophonic synth |
| `alphajuno/` | `alphajuno~` | Alpha Juno style 6-voice synth with chorus |
| `b3/` | `b3~` | B-3 style tonewheel organ with rotary speaker |
| `prophet5/` | `prophet5~` | Prophet-5 style 5-voice polysynth with poly-mod |
| `minimoog/` | `minimoog~` | Minimoog Model D style monosynth |
| `tx81z/` | `tx81z~` | TX81Z style 4-operator FM synth with 8 waveforms and 8 algorithms |
| `ms20/` | `ms20~` | MS-20 style monosynth with screaming filters and a patch matrix |
| `cz101/` | `cz101~` | CZ-101 style phase distortion synth |
| `mellotron/` | `mellotron~` | Mellotron style tape keyboard for your own wav files |
| `common/` | | shared DSP kit, voice engine and Pd wrapper used by every synth except dx7. Every synth also has A/B morph, mutate, a data inlet (`data`, `datamap`) and stash-ready parameter names |
| `tools/` | | generators for the help patches and READMEs |
| `test/` | | offline render tests and a Pd API stand-in test |

Each folder has its own README and help patch. The help patch is documentation first: open its pages for presets, parameters, messages, examples and the engine. The model names describe an architecture. They are not copies of any ROM or circuit, and nothing here has been compared against hardware or other software.

## Build (macOS)

```
cmake -S . -B build -DPD_INCLUDE_DIR=/Applications/Pd-0.55-1.app/Contents/Resources/src
cmake --build build
cmake --install build --prefix /path/to/your/externals
```

The install puts every external and its help patches side by side in that folder, which is what Pd expects. Restart Pd after installing. Add `-DBUILD_TESTS=ON` for the offline render tests.

## Add a synth

Make a folder `<name>/` with `<name>.h` (parameters, preset strings and voice callbacks, see `sh101/sh101.h`) and a three-line `<name>~.c`.
Add the name to `ALL_SYNTHS` in `CMakeLists.txt` and to the tables in `tools/make_panel.py`, then run `python3 tools/make_panel.py` and `python3 tools/make_readmes.py`.
