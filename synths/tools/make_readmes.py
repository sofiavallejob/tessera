#!/usr/bin/env python3
"""Writes sh101/README.md alphajuno/README.md b3/README.md from the parameter tables in the headers."""
import os, re, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import make_panel as mp
ROOT = mp.ROOT
FACTS = {
 "sh101": ("sh101~", "SH-101 style monophonic synth", "mono, 16 presets",
   ["Original presets and a model of the architecture: this is not a copy of any ROM or circuit.",
    "The filter is a zero-delay-feedback ladder with tanh drive and self-oscillates at full resonance.",
    "No sequencer or arpeggiator yet."]),
 "alphajuno": ("alphajuno~", "Alpha Juno style 6-voice synth", "6 voices, 16 presets",
   ["The hardware has a 4-stage envelope with three time and level pairs. Here it is an ADSR routed to pitch filter and amplitude.",
    "The low-pass does not self-oscillate (as on the hardware). Chorus rate is adjustable.",
    "No portamento or chord memory yet."]),
 "b3": ("b3~", "B-3 style tonewheel organ", "up to 32 voices, 14 presets",
   ["Additive equal-tempered sines. This is not a tonewheel-by-tonewheel model and has no crosstalk or leakage.",
    "Percussion fires on the first held key only and cuts the 1 foot drawbar as on the real organ.",
    "Rotary speaker is horn and drum with inertia. It is an effect model and not a measured cabinet."]),

 "prophet5": ("prophet5~", "Prophet-5 style 5-voice polysynth", "5 voices, 16 presets",
   ["Original presets and a model of the architecture. Each voice has its own LFO phase and drift instead of one global LFO.",
    "No unison or portamento yet.",
    "Oscillator B is computed first so it can act as sync master and poly-mod source."]),
 "minimoog": ("minimoog~", "Minimoog Model D style monosynth", "mono, 14 presets",
   ["Original presets and a model of the architecture. No hardware-measured filter or oscillator curves.",
    "Oscillator range lo is a fixed low-frequency range used as an LFO.",
    "No external audio input and no keyboard glide time calibration."]),
 "tx81z": ("tx81z~", "TX81Z style 4-operator FM synth", "8 voices, 16 presets",
   ["Eight algorithms (modulator to carrier): 1 is 4>3>2>1, 2 is (4+3)>2>1, 3 is 4>3>1 and 2>1, 4 is 4>3>2>1 split with operator 4 also a carrier, 5 is 4>3>2 plus 1, 6 is 4 into 1 2 and 3, 7 is 4>3 plus 1 and 2 free, 8 is all four as carriers. Numbering follows this table and not a Yamaha chart.",
    "One ADSR per operator instead of the five-stage envelope of the hardware. No operator key scaling.",
    "Frequency ratios are a table of 16 values and not the continuous coarse and fine of the TX81Z."]),
 "ms20": ("ms20~", "MS-20 style monosynth", "mono, 12 presets",
   ["The patch panel is a matrix of amounts and not physical cables. 0.5 means no modulation.",
    "Filters are a two-pole model with a soft-clipped state and not a circuit simulation of the Korg 35 chip.",
    "No external signal input."]),
 "cz101": ("cz101~", "CZ-101 style phase distortion synth", "8 voices, 14 presets",
   ["Phase distortion curves are in the spirit of the CZ and not copied. The hardware has eight-stage envelopes, here they are ADSR.",
    "Two times oversampling. No line modulation beyond ring and noise."]),
 "mellotron": ("mellotron~", "Mellotron style tape keyboard", "16 voices, 8 presets",
   ["No samples are shipped. Load your own wav files with `samples <folder>` (name each file with its root midi note like 48.wav) or `sample <note> <file>`.",
    "One file without a number is treated as middle C and transposed over the keyboard. Every key with no tape uses the nearest tape.",
    "Without samples a synthetic flute strings or choir stand-in plays. It is not a recording of any instrument.",
    "Linear interpolation: pitching a tape far up will alias."]),
}
def presets(syn):
    s = open(os.path.join(ROOT, syn, mp.HEADER[syn][0])).read()
    return re.findall(r'\{"([^"]+)",\s+"', s)
for syn in mp.SYNTHS:
    cls, title, short, limits = FACTS[syn]
    params = mp.read_params(syn)
    L = ["# %s - %s" % (cls, title), "",
         "Part of the Pd synth collection by Sofia Vallejo Budziszewski, IEM KUG Graz Austria. Version 1 (2026). %s." % short.capitalize(), "",
         mp.DESC[syn], "",
         "## Use", "",
         "Open `%s-help.pd`. It lists every parameter with its message name and has clickable examples of every message. Create the object as `[%s]`, `[%s name]` or `[%s name 8]` (name and voice count)." % (cls, cls, cls, cls), "",
         "## Parameters", "", "All values are 0 to 1. Switches take a step number. Set one with `set <name> <value>`.", "",
         "| Name | Meaning | Choices |", "|---|---|---|"]
    for n, d, st in params:
        o = mp.opt_text(syn, n)
        L.append("| `%s` | %s | %s |" % (n, mp.LABELS.get(n, n), ("0..%d: %s" % (st - 1, o.replace("|", "/"))) if st >= 2 else "0..1"))
    L += ["", "## Factory presets", ""] + ["%d. %s" % (i, p) for i, p in enumerate(presets(syn))]
    L += ["", "## Messages", "", "`note <n> <vel>`, `bend`, `bendrange`, `mod`, `aftertouch`, `sustain`, `panic`, `set <name> <value>`, `params`, `preset <n>`, `next`, `prev`, `mutate <0..1>`, `A`, `B`, `morph <0..1>`, `store <name>`, `save <file>`, `load <file>`, `data <0..1>`, `datamap <param> <amount>`, `datamap clear`.", "", "`data` is a general modulation inlet. Map it to up to four continuous parameters with `datamap` and feed it any value scaled to 0..1 (a sensor, a column of a csv file, a seismic trace). Parameters are also named `<name>-<param>` so the stash preset library can store scenes (naming rules followed, not yet tested together).", "", "Every synth has A/B morph (`A`, `B`, `morph`) and `mutate` for random variations.", "", "Not compared against hardware: these are models of the architecture with original presets.", "",
          "## What is simplified", ""] + ["- " + s for s in limits] + ["- Not compared against hardware or other software.", ""]
    open(os.path.join(ROOT, syn, "README.md"), "w").write("\n".join(L))
    print("wrote", syn)
