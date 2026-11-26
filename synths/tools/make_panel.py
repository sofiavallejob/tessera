#!/usr/bin/env python3
"""Generates <synth>/<synth>~-help.pd for the synth collection (run from the collection root).
The main page is documentation only (what it is, how it was made, how to use, common problems).
Inside it are subpatches: presets, parameters, messages, examples, engine. Controls are wire-free
(bound by name). Every page is checked for overlapping boxes and the generator refuses to write if any."""
import re, sys, os
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CH, LH, GAP = 8, 20, 10     # conservative character width, line height, minimum gap
SYNTHS = ("sh101", "alphajuno", "b3", "prophet5", "minimoog", "tx81z", "ms20", "cz101", "mellotron")
MONO = ("sh101", "minimoog", "ms20")
BANNER = "=^..^=   %s~   version 1 (2026)   sofia vallejo budziszewski   iem kug graz austria"

TITLE = {"sh101": "sh-101 style monophonic synth", "alphajuno": "alpha juno style 6-voice synth", "b3": "b-3 style tonewheel organ",
         "prophet5": "prophet-5 style 5-voice polysynth", "minimoog": "minimoog model d style monosynth",
         "tx81z": "tx81z style 4-operator fm synth", "ms20": "ms-20 style monosynth with patch matrix", "cz101": "cz-101 style phase distortion synth", "mellotron": "mellotron style tape keyboard for your own wav files"}
DESC = {
 "sh101": "One oscillator with saw pulse sub and noise into a self-oscillating 24 dB ladder filter with one envelope and one LFO. Mono with portamento and legato.",
 "alphajuno": "One DCO per voice with saw pulse sub and noise then a switched high-pass and a 24 dB low-pass. One envelope for pitch filter and amplitude and a chorus with adjustable rate. Six voices.",
 "b3": "Nine drawbars with single-trigger percussion key click scanner vibrato and chorus overdrive and a rotary speaker with horn and drum. Up to 32 voices.",
 "prophet5": "Five voices with two oscillators each. Oscillator A has saw and pulse with pulse width and oscillator B has saw triangle and pulse with sync and a low-frequency mode. Poly-mod sends the filter envelope and oscillator B to pitch pulse width and filter. A 24 dB low-pass two ADSR envelopes an LFO and analogue-style drift.",
 "tx81z": "Four sine-based operators with eight waveforms each and eight algorithms. Every operator has a frequency ratio a fine tune and its own ADSR. Operator 4 has feedback. LFO to pitch and amplitude. Eight voices.",
 "ms20": "Two oscillators with ring modulation and noise feed a high-pass and a low-pass filter that scream when you raise the peak. Two envelope generators and a modulation generator are wired by a matrix of amounts: every source can go to pitch pulse width and both cutoffs. Mono.",
 "cz101": "Phase distortion: each of the two lines per voice is a sine whose phase is bent into saw square pulse double sine and resonant waveforms. An envelope for the distortion amount and one for amplitude plus pitch envelope and ring or noise modulation. Eight voices.",
 "mellotron": "A keyboard of tapes. Load your own wav files onto the keys and each key plays its tape from the start. The tape stops after a few seconds like the real machine until you release the key. Wow flutter drift hiss and tone loss. A built-in synthetic stand-in plays when no samples are loaded. Sixteen voices.",
 "minimoog": "Three oscillators with six waveforms and six ranges each plus hard sync and a noise source go into a 24 dB ladder filter with emphasis and contour. Filter and loudness envelopes with a decay switch glide legato LFO and overload drive. Mono.",
}
HOW = {
 "sh101": ["The oscillator is a band-limited saw and pulse (PolyBLEP) with a sub oscillator that follows the pitch.",
           "The filter is a zero-delay-feedback ladder with tanh saturation in the loop. At full resonance it sings by itself.",
           "One ADSR drives the filter and the amplifier. Mono voice with a note stack so releasing a key goes back to the key still held."],
 "alphajuno": ["Six identical voices each with a band-limited DCO sub oscillator and noise into a switched high-pass and a ladder low-pass.",
               "The real machine has a multi-stage envelope. Here one ADSR is routed to pitch filter and amplitude with separate amounts.",
               "The stereo chorus has two modes and a programmable rate."],
 "b3": ["Each voice is nine sine partials (the drawbar footages) that add up. Percussion is a second or third harmonic with its own decay.",
        "Key click is a short noise burst at note on. Vibrato and chorus is a modulated delay line and the rotary is a horn and a drum with their own speeds and inertia.",
        "It is an additive model and not a tonewheel simulation so there is no crosstalk or leakage."],
 "prophet5": ["Each voice is two oscillators (band-limited) through a ladder low-pass with a filter envelope and an amplifier envelope.",
              "Oscillator B is computed first. It is the sync master and with the filter envelope it is the poly-mod source.",
              "Every voice has its own slow random drift and its own LFO so chords are never perfectly still."],
 "tx81z": ["Each operator is a phase-modulated oscillator. The output of a modulator is added to the phase of the next operator so the algorithm decides who modulates whom.",
           "The eight waveforms are sine and seven shapes cut from it (half rectified full rectified quarter pulses double speed and square types).",
           "Operator 4 feeds back into itself for a rougher spectrum. Envelopes are ADSR per operator."],
 "ms20": ["The two filters are two-pole state variable filters with a soft-clipped band state. At the top of the peak control the damping goes slightly negative so they oscillate by themselves.",
          "There are no cables. The patch panel is a matrix of amounts (0.5 is off) so every source can reach every destination.",
          "Oscillator 1 and 2 can be multiplied by the ring control. The high-pass and low-pass run in series or in parallel."],
 "cz101": ["Each line reads a cosine through a bent phase (phase distortion). The distortion amount moves the bend point so the sine turns into a saw or a square.",
           "The resonant waveforms multiply a cosine of a rising harmonic by a window. Two times oversampling keeps the aliasing low.",
           "The distortion envelope is what makes the characteristic plucks and sweeps. Higher keys get less distortion if dcwkbd is up."],
 "mellotron": ["Each voice reads a tape (a wav file) with linear interpolation at a rate that follows the key. Wow flutter and drift are slow modulations of that rate.",
               "The tape clock runs while the key is down. When it reaches the tape length the sound stops (or loops if loop is on).",
               "Without samples a voice synthesizes a flute strings or choir stand-in. No samples are shipped with this object."],
 "minimoog": ["Three band-limited oscillators with six waveforms. Oscillator 3 can run free as an LFO range or follow the keyboard.",
              "The ladder filter has tanh saturation and self-oscillates. The mixer can overdrive it for the classic thick bass.",
              "It is monophonic with a note stack: release a key and the synth goes back to the key still held."],
}
TIPS = {
 "sh101": ["Load squelch then turn legato on: overlapping notes glide without retriggering the envelope",
           "Raise res to the top and set vca to gate: the filter becomes a sine oscillator that follows the keyboard",
           "Set pwmode to lfo and move pwamt for slow pulse width movement"],
 "alphajuno": ["Load pwm strings and move chrate: the chorus speed is programmable",
               "Move envpitch away from the middle for a pitch sweep on every note",
               "Set lfodelay up and lfopitch a little: vibrato fades in after each key press"],
 "b3": ["Pull the drawbars with the vertical sliders: d8 is the fundamental and d16 is one octave below",
        "Turn perc on: the percussion fires only on the first key of a phrase as on the real organ",
        "Switch leslie between slow and fast: the rotor speeds up and slows down with inertia"],
 "prophet5": ["Load sync lead and move bfreq: oscillator B sweeps against the synced oscillator A",
              "Raise pmoscb and switch pmfreq on: oscillator B frequency-modulates oscillator A for bells",
              "Raise drift and play a chord: every voice wanders a little differently"],
 "tx81z": ["Switch wave2 to half sine and raise level2: the modulator gets brighter without a new operator",
           "Raise feedback on algorithm 1: operator 4 turns from sine to saw-like and the whole chain brightens",
           "Pick algorithm 8 and set all ratios to harmonics: you have an additive organ"],
 "ms20": ["Load screamer and raise lppeak to the top: the low-pass oscillates and plays pitch with the keys",
          "Move mg2lp and mg2hp in opposite directions: the two filters sweep against each other",
          "Raise ring with two different tunes for bells and metallic hits"],
 "cz101": ["Load string pad and move dcw1 while holding a chord: the sine turns into a saw",
           "Try the resonance waves: the dcw knob sweeps the resonant peak up and down",
           "Set mod to ring and detune the second line: metallic and inharmonic"],
 "mellotron": ["Send samples tapes with a folder of wav files named 48.wav 50.wav 52.wav and so on (the number is the midi note)",
               "Raise wow and drift for a seasick tape or turn them down for a clean one",
               "Turn loop on and set length short for a tape that cycles forever"],
 "minimoog": ["Load fat bass: osc 2 sits a little off pitch and osc 3 is an octave below",
              "Set sync on and move tune2: the classic sync sweep",
              "Raise drive and res with a low cutoff for the thick overloaded sound"],
}
PROBLEMS = {
 "all": [
  "no sound: turn dsp on in the engine page and check the volume slider. The synth needs a note: click chord on or play a midi key",
  "nothing changes after a rebuild: pd keeps the old external in memory so close every patch and restart pd",
  "a note is stuck: send panic. A missing note off from a midi device is the usual cause",
  "unknown parameter in the console: names are exact and lowercase so send params to print them all",
  "a bank file is not found: save and load names are relative to the folder of this patch",
  "the sliders do not move when you change a preset: the object must be created with the same name as in the boxes. Default name is the object name",
  "clicks or dropouts: raise the audio buffer in the pd audio settings or lower the voice count in the creation argument",
 ],
 "sh101": ["no pitch glide: raise glide and set legato on and then play overlapping notes"],
 "alphajuno": ["the filter never self-oscillates: that is on purpose because the real low-pass does not either"],
 "b3": ["no percussion: it needs perc on and it only fires on the first key of a phrase so release all keys between tests",
        "the sound is too quiet: raise d8 first because drawbars add up and many open bars are loud"],
 "prophet5": ["the sound is thin: switch bsaw on and raise mixb because a single oscillator is thin by design",
              "sync does nothing: sync needs oscillator B running (mixb up is not required) and bfreq away from the middle"],
 "tx81z": ["no sound from an operator: operators 2 to 4 are modulators in most algorithms so listen for them in algorithm 8",
           "harsh and noisy: lower level on the modulators or lower feedback"],
 "ms20": ["a filter screams too loud: lower drive or the peak of that filter",
          "no sound at all: check lvl1 lvl2 ring and noise since any of them can be zero"],
 "cz101": ["no change when dcw moves: the distortion envelope scales dcw so raise dcwsus or dcwdec",
           "line 2 is silent: raise mix2"],
 "mellotron": ["samples says failed: the folder must contain .wav files (8 16 24 or 32-bit) and the path is relative to this patch",
               "the sound stops after eight seconds: that is the tape. Raise length or turn loop on",
               "wrong pitch: name each file with its root midi note like 48.wav. A single file without a number is treated as middle C"],
 "minimoog": ["a chord plays one note: the minimoog is monophonic so the last key wins",
              "range lo makes no pitch: lo is a low-frequency range used as an LFO"],
}
GROUPS = {
 "sh101": [("oscillator", ["range", "saw", "pulse", "sub", "submode", "noise", "pw", "pwmode", "pwamt"]),
           ("lfo", ["lforate", "lfowave", "lfopitch", "lfofilt"]),
           ("filter", ["cutoff", "res", "envamt", "kbd", "drive"]),
           ("envelope", ["att", "dec", "sus", "rel"]),
           ("output", ["vca", "glide", "legato", "vel", "vol"])],
 "alphajuno": [("dco", ["range", "saw", "pulse", "sub", "noise", "pw", "pwmode", "pwamt"]),
               ("lfo", ["lforate", "lfodelay", "lfowave", "lfopitch", "lfofilt"]),
               ("filter", ["hpf", "cutoff", "res", "envfilt", "envpitch", "kbd", "drive"]),
               ("envelope", ["att", "dec", "sus", "rel", "vca"]),
               ("output", ["vel", "chorus", "chrate", "vol"])],
 "b3": [("percussion", ["perc", "pharm", "pdec", "pvol"]),
        ("vibrato and rotary", ["vib", "leslie"]),
        ("tone", ["click", "drive", "vol"])],
 "prophet5": [("oscillator a", ["asaw", "apulse", "pw"]),
              ("oscillator b", ["bsaw", "btri", "bpulse", "bfreq", "bfine", "blow", "bkbd", "sync"]),
              ("poly mod", ["pmfenv", "pmoscb", "pmfreq", "pmpw", "pmfilt"]),
              ("mixer", ["mixa", "mixb", "noise"]),
              ("filter", ["cutoff", "res", "envamt", "kbd"]),
              ("filter envelope", ["fatt", "fdec", "fsus", "frel"]),
              ("amp envelope", ["aatt", "adec", "asus", "arel"]),
              ("lfo", ["lforate", "lfoshape", "lfofreq", "lfopw", "lfofilt"]),
              ("output", ["drift", "vel", "vol"])],
 "tx81z": [("algorithm", ["algo", "feedback"])] + [("operator %d" % n, ["%s%d" % (k, n) for k in ("ratio", "fine", "wave", "level", "att", "dec", "sus", "rel")]) for n in (1, 2, 3, 4)] + [("lfo and output", ["lforate", "lfopm", "lfoam", "vel", "vol"])],
 "ms20": [("oscillator 1", ["range1", "wave1", "pw1"]), ("oscillator 2", ["range2", "wave2", "tune2"]), ("mixer", ["lvl1", "lvl2", "ring", "noise"]),
          ("filters", ["hpcut", "hppeak", "lpcut", "lppeak", "drive", "serial"]), ("modulation generator", ["mgrate", "mgwave"]),
          ("patch matrix", ["mg2pitch", "mg2lp", "mg2hp", "mg2pw", "eg2pitch", "eg2lp", "eg2hp"]), ("envelope 1 amplifier", ["att", "dec", "sus", "rel"]),
          ("envelope 2 modulation", ["eatt", "edec", "esus", "erel"]), ("output", ["kbd", "glide", "legato", "vel", "vol"])],
 "cz101": [("line 1", ["wave1", "dcw1"]), ("line 2", ["wave2", "dcw2", "detune", "oct2", "mix2"]), ("modulation", ["mod", "dcwkbd"]),
           ("distortion envelope", ["dcwatt", "dcwdec", "dcwsus", "dcwrel"]), ("amplitude envelope", ["att", "dec", "sus", "rel"]),
           ("pitch and vibrato", ["pitchamt", "pitchdec", "vibrate", "vibdepth", "vibdelay"]), ("output", ["vel", "vol"])],
 "mellotron": [("tape", ["tape", "length", "loop", "pitch"]), ("tape character", ["wow", "flutter", "drift", "tone", "hiss"]), ("envelope", ["att", "rel"]), ("output", ["vel", "vol"])],
 "minimoog": [("oscillator 1", ["range1", "wave1", "mix1"]),
              ("oscillator 2", ["range2", "wave2", "tune2", "mix2"]),
              ("oscillator 3", ["range3", "wave3", "tune3", "kbd3", "mix3"]),
              ("mixer", ["sync", "noise", "noisetype"]),
              ("filter", ["cutoff", "res", "envamt", "kbd"]),
              ("filter envelope", ["fatt", "fdec", "fsus"]),
              ("loudness envelope", ["aatt", "adec", "asus", "decay"]),
              ("performance", ["glide", "legato", "lforate", "lfowave", "lfopitch", "lfofilt"]),
              ("output", ["vel", "drive", "vol"])],
}
LABELS = {"range": "range", "saw": "saw", "pulse": "pulse", "sub": "sub osc", "submode": "sub mode", "noise": "noise", "pw": "pulse width",
 "pwmode": "pw source", "pwamt": "pw mod", "lforate": "lfo rate", "lfowave": "lfo wave", "lfopitch": "lfo to pitch", "lfofilt": "lfo to filter",
 "lfodelay": "lfo delay", "cutoff": "cutoff", "res": "resonance", "envamt": "env amount", "envfilt": "env to filter", "envpitch": "env to pitch",
 "kbd": "key follow", "drive": "drive", "att": "attack", "dec": "decay", "sus": "sustain", "rel": "release", "vca": "amp source", "glide": "portamento",
 "legato": "legato", "vel": "velocity", "vol": "volume", "hpf": "high-pass", "chorus": "chorus", "chrate": "chorus rate",
 "d16": "16", "d513": "5 1/3", "d8": "8", "d4": "4", "d223": "2 2/3", "d2": "2", "d135": "1 3/5", "d113": "1 1/3", "d1": "1",
 "perc": "percussion", "pharm": "harmonic", "pdec": "decay", "pvol": "level", "click": "key click", "vib": "vibrato", "leslie": "rotary",
 "asaw": "a saw", "apulse": "a pulse", "bsaw": "b saw", "btri": "b triangle", "bpulse": "b pulse", "bfreq": "b frequency", "bfine": "b fine tune",
 "blow": "b low freq", "bkbd": "b key follow", "sync": "sync", "pmfenv": "poly mod filter env", "pmoscb": "poly mod osc b", "pmfreq": "pm to freq a",
 "pmpw": "pm to pw a", "pmfilt": "pm to filter", "mixa": "level a", "mixb": "level b", "fatt": "filter attack", "fdec": "filter decay",
 "fsus": "filter sustain", "frel": "filter release", "aatt": "amp attack", "adec": "amp decay", "asus": "amp sustain", "arel": "amp release",
 "lfoshape": "lfo shape", "lfofreq": "lfo to freq", "lfopw": "lfo to pw", "drift": "drift",
 "range1": "osc 1 range", "wave1": "osc 1 wave", "mix1": "osc 1 level", "range2": "osc 2 range", "wave2": "osc 2 wave", "tune2": "osc 2 tune",
 "mix2": "osc 2 level", "range3": "osc 3 range", "wave3": "osc 3 wave", "tune3": "osc 3 tune", "kbd3": "osc 3 keyboard", "mix3": "osc 3 level",
 "noisetype": "noise colour", "decay": "decay switch"}
LABELS.update({"algo": "algorithm", "feedback": "feedback op 4", "lfopm": "lfo to pitch", "lfoam": "lfo to amp",
 "pw1": "pulse width", "lvl1": "level osc 1", "lvl2": "level osc 2", "ring": "ring mod", "hpcut": "high-pass cutoff", "hppeak": "high-pass peak", "lpcut": "low-pass cutoff",
 "lppeak": "low-pass peak", "serial": "filter routing", "mgrate": "mg rate", "mgwave": "mg wave", "mg2pitch": "mg to pitch", "mg2lp": "mg to low-pass", "mg2hp": "mg to high-pass",
 "mg2pw": "mg to pw", "eg2pitch": "eg2 to pitch", "eg2lp": "eg2 to low-pass", "eg2hp": "eg2 to high-pass", "eatt": "eg2 attack", "edec": "eg2 decay", "esus": "eg2 sustain", "erel": "eg2 release",
 "dcw1": "distortion 1", "dcw2": "distortion 2", "detune": "detune line 2", "oct2": "line 2 octave", "mod": "line modulation", "mix2": "level line 2", "dcwkbd": "dcw key follow",
 "dcwatt": "dcw attack", "dcwdec": "dcw decay", "dcwsus": "dcw sustain", "dcwrel": "dcw release", "pitchamt": "pitch env amount", "pitchdec": "pitch env decay",
 "vibrate": "vibrato rate", "vibdepth": "vibrato depth", "vibdelay": "vibrato delay", "tape": "tape sound", "length": "tape length", "loop": "loop tape", "pitch": "pitch control",
 "wow": "wow", "flutter": "flutter", "tone": "tone", "hiss": "tape hiss"})
for _n in (1, 2, 3, 4):
    LABELS.update({"ratio%d" % _n: "op %d ratio" % _n, "fine%d" % _n: "op %d fine" % _n, "wave%d" % _n: "op %d wave" % _n, "level%d" % _n: "op %d level" % _n,
                   "att%d" % _n: "op %d attack" % _n, "dec%d" % _n: "op %d decay" % _n, "sus%d" % _n: "op %d sustain" % _n, "rel%d" % _n: "op %d release" % _n})
CZW = "saw | square | pulse | double sine | saw pulse | resonance I | resonance II | resonance III"
TXW = "sine | half | abs | quarter | double half | double abs | square | decay square"
TXR = "0.5 | 0.71 | 0.78 | 0.87 | 1 | 1.41 | 1.57 | 1.73 | 2 | 2.82 | 3 | 3.14 | 3.46 | 4 | 5 | 7"
RANGES = "lo | 32 | 16 | 8 | 4 | 2 feet"
WAVES = "triangle | shark tooth | saw | reverse saw | wide pulse | narrow pulse"
OPTS = {
 "range": {"sh101": "16 | 8 | 4 | 2 feet", "alphajuno": "16 | 8 | 4 feet"},
 "submode": "1 down | 2 down | 2 pulse", "pwmode": "manual | lfo | envelope",
 "lfowave": {"sh101": "triangle | square | random", "alphajuno": "triangle | square | random", "minimoog": "triangle | square"},
 "lfoshape": "triangle | saw | square", "kbd": "off | half | full",
 "vca": "gate | envelope", "legato": "retrigger | legato", "hpf": "off | low | mid | high", "chorus": "off | I | II",
 "perc": "off | on", "pharm": "2nd | 3rd", "pdec": "fast | slow", "pvol": "soft | normal", "vib": "off | V1 V2 V3 | C1 C2 C3", "leslie": "off | slow | fast",
 "range1": {"minimoog": RANGES, "ms20": "16 | 8 | 4 | 2 feet"}, "range2": {"minimoog": RANGES, "ms20": "16 | 8 | 4 | 2 feet"}, "range3": RANGES,
 "wave1": {"minimoog": WAVES, "ms20": "triangle | saw | pulse | noise", "cz101": CZW}, "wave2": {"minimoog": WAVES, "ms20": "saw | square | triangle | noise", "cz101": CZW}, "wave3": WAVES,
 "mgwave": "triangle | saw | square | random", "serial": "parallel | series", "mod": "off | ring | noise", "oct2": "one octave down | same | one octave up",
 "tape": "flute | strings | choir (stand-in when no wav files are loaded)", "algo": "1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 (see the readme for the layouts)",
 "kbd3": "fixed | keyboard", "noisetype": "white | pink", "decay": "off | on"}
OPTS.update({"wave%d" % n: TXW for n in ()})
HEADER = {"sh101": ("sh101.h", "sh101_params"), "alphajuno": ("alphajuno.h", "aj_params"), "b3": ("b3.h", "b3_params"),
          "prophet5": ("prophet5.h", "p5_params"), "minimoog": ("minimoog.h", "mm_params"),
          "tx81z": ("tx81z.h", "tx_params"), "ms20": ("ms20.h", "ms_params"), "cz101": ("cz101.h", "cz_params"), "mellotron": ("mellotron.h", "mel_params")}
SWEEP = {"sh101": "cutoff", "alphajuno": "cutoff", "b3": "drive", "prophet5": "cutoff", "minimoog": "cutoff", "tx81z": "level2", "ms20": "lpcut", "cz101": "dcw1", "mellotron": "tone"}

def read_params(syn):
    fn, arr = HEADER[syn]
    s = open(os.path.join(ROOT, syn, fn)).read()
    body = s[s.index(arr): s.index("};", s.index(arr))]
    return [(m.group(1), float(m.group(2)), int(m.group(3))) for m in re.finditer(r'\{"(\w+)",\s*([-\d.]+)f?,\s*(\d+)\}', body)]

def read_presets(syn):
    s = open(os.path.join(ROOT, syn, HEADER[syn][0])).read()
    return re.findall(r'\{"([^"]+)",\s+"', s)

def opt_text(syn, n):
    if syn == "tx81z" and n[:-1] == "wave" and n[-1] in "1234": return TXW
    if syn == "tx81z" and n[:-1] == "ratio" and n[-1] in "1234": return TXR
    o = OPTS.get(n)
    if isinstance(o, dict): o = o.get(syn)
    return o or "off | on"

class Page:
    """One canvas: items (each is one Pd object line or a whole subpatch) plus boxes for the overlap check."""
    def __init__(self, syn, name, w, h=None):
        self.syn, self.name, self.w, self.h = syn, name, w, h
        self.items, self.boxes, self.conns = [], [], []
        self.ymax = 0
    def _add(self, line, name, x, y, w, h):
        self.items.append(line); self.boxes.append((name, x, y, w, h)); self.ymax = max(self.ymax, y + h)
        return len(self.items) - 1
    def text(self, x, y, s, wrap=None):
        assert not re.search(r'[,;$\\]', s), "comment has a Pd-special character: " + s
        s = s.lower()
        self._add("#X text %d %d %s%s;" % (x, y, s, (", f %d" % wrap) if wrap else ""), "text " + s[:24], x, y,
                  (len(s) if not wrap else min(wrap, len(s))) * CH, (1 if not wrap else -(-len(s) // max(1, wrap - 6))) * LH)
        return y + ((1 if not wrap else -(-len(s) // max(1, wrap - 6))) * LH)
    def para(self, x, y, s, wrap=100, gap=10):
        return self.text(x, y, s, wrap) + gap
    def msg(self, x, y, shown, raw=None):
        return self._add("#X msg %d %d %s;" % (x, y, raw if raw else "\\; " + shown), "msg " + shown[:24], x, y, (max(len(q.strip()) for q in raw.split("\\;")) if raw else len(shown)) * CH + 22, 19 * ((raw.count("\\;") + 1) if raw else 2) + 6)
    def obj(self, x, y, s):
        return self._add("#X obj %d %d %s;" % (x, y, s), "obj " + s[:24], x, y, (len(s) + 1) * CH + 6, 22)
    def sat(self, x, y, w, recv): return self._add("#X symbolatom %d %d %d 0 0 0 - %s - 0;" % (x, y, w, recv), "symbolatom", x, y, w * CH + 12, 22)
    def fat(self, x, y, w, recv): return self._add("#X floatatom %d %d %d 0 0 0 - %s - 0;" % (x, y, w, recv), "floatatom", x, y, w * CH + 12, 22)
    def hsl(self, x, y, w, n):
        s = self.syn
        return self._add("#X obj %d %d hsl %d 18 0 1 0 0 %s-%s %s-%s-r empty -2 -8 0 12 -262144 -1 -1 0 1;" % (x, y, w, s, n, s, n), "hsl " + n, x, y, w + 8, 20)
    def vsl(self, x, y, h, n):
        s = self.syn
        return self._add("#X obj %d %d vsl 22 %d 0 1 0 0 %s-%s %s-%s-r empty 0 -9 0 10 -262144 -1 -1 0 1;" % (x, y, h, s, n, s, n), "vsl " + n, x, y, 26, h + 4)
    def hradio(self, x, y, n, name):
        s = self.syn
        c = 18 if n <= 10 else 14
        return self._add("#X obj %d %d hradio %d 1 0 %d %s-%s %s-%s-r empty 0 -8 0 10 -262144 -1 -1 0;" % (x, y, c, n, s, name, s, name), "radio " + name, x, y, c * n + 3, 20)
    def tgl(self, x, y):
        return self._add("#X obj %d %d tgl 18 0 empty empty empty 17 7 0 10 -262144 -1 -1 0 1;" % (x, y), "tgl", x, y, 20, 20)
    def sub(self, x, y, page):
        body = ["#N canvas 0 50 %d %d %s 0;" % (page.w, page.height(), page.name)] + page.items + ["#X connect %d %d %d %d;" % c for c in page.conns]
        body.append("#X restore %d %d pd %s;" % (x, y, page.name))
        return self._add("\n".join(body), "pd " + page.name, x, y, (len(page.name) + 4) * CH + 6, 22)
    def connect(self, a, ao, b, bi): self.conns.append((a, ao, b, bi))
    def height(self): return max(self.h or 0, self.ymax + 60)
    def lines(self):
        return self.items + ["#X connect %d %d %d %d;" % c for c in self.conns]
    def check(self):
        bad = 0
        for i in range(len(self.boxes)):
            for j in range(i + 1, len(self.boxes)):
                a, b = self.boxes[i], self.boxes[j]
                if a[1] < b[1] + b[3] + GAP and b[1] < a[1] + a[3] + GAP and a[2] < b[2] + b[4] + GAP and b[2] < a[2] + a[4] + GAP:
                    print("OVERLAP on page", self.name, a, b); bad += 1
        wide = max(b[1] + b[3] for b in self.boxes)
        if wide > self.w - 10: print("WIDE page", self.name, wide, "of", self.w); bad += 1
        return bad

def heading(pg, y, s):
    return pg.text(30, y, "--- " + s + " ---") + 24

def page_presets(syn):
    pg = Page(syn, "presets", 900)
    names = read_presets(syn)
    y = 16
    y = pg.para(30, y, "presets: factory sounds are stored as plain text and every control above is a message. Click a box to load that sound", 100)
    y = heading(pg, y, "factory presets")
    half = -(-len(names) // 2); top = y
    for i, n in enumerate(names):
        cx = 30 + (i // half) * 430; cy = top + (i % half) * 54
        pg.msg(cx, cy, "%s preset %d" % (syn, i)); pg.text(cx + 220, cy + 2, "%d  %s" % (i, n))
    y = top + half * 54 + 20
    y = heading(pg, y, "now playing")
    pg.sat(30, y, 20, syn + "-name"); pg.fat(30 + 20 * CH + 40, y, 4, syn + "-index")
    pg.text(30 + 20 * CH + 150, y + 2, "name and number (minus one means edited)"); y += 44
    x = 30
    for shown in ["next", "prev", "mutate 0.3"]:
        s = "%s %s" % (syn, shown); pg.msg(x, y, s); x += (len(s) + 2) * CH + 40
    y += 60
    y = pg.para(30, y, "next and prev step through the presets and wrap around. mutate varies the current sound by a random amount from 0 to 1", 100)
    y = heading(pg, y, "morph between two sounds")
    x = 30
    for shown in ["A", "B"]:
        s = "%s %s" % (syn, shown); pg.msg(x, y, s); x += (len(s) + 2) * CH + 40
    pg.text(x, y + 2, "A and B remember the current sound"); y += 60
    pg.text(30, y + 2, "morph"); pg.hsl(110, y, 300, "morph"); y += 44
    y = pg.para(30, y, "load one preset and press A then load another and press B then move the morph slider. Every parameter blends smoothly between the two", 100)
    y = heading(pg, y, "your own user bank")
    x = 30
    for shown in ["store mysound", "save my.bank", "load my.bank"]:
        s = "%s %s" % (syn, shown); pg.msg(x, y, s); x += (len(s) + 2) * CH + 40
    y += 60
    y = pg.para(30, y, "store keeps the current sound after the factory presets. save writes the user bank to a text file next to this patch and load adds the sounds from a text file", 100)
    y = heading(pg, y, "presets with stash")
    y = pg.para(30, y, "every slider in the parameters page sends to %s-name and listens on %s-name-r so the stash library can store and morph whole scenes: create [stash/bank %s] and send it a scan message for a saved copy of the parameters page. This follows the stash naming rules but has not been tested together yet" % (syn, syn, syn), 100)
    return pg

def page_parameters(syn):
    params = read_params(syn); pinfo = {n: (d, st) for n, d, st in params}
    pg = Page(syn, "parameters", 1500)
    y = 16
    y = pg.para(30, y, "every control here is also a message: set name value with values from 0 to 1. Switches take the step number shown under each one", 120)
    if syn == "b3":
        y = heading(pg, y, "drawbars")
        for i, n in enumerate(["d16", "d513", "d8", "d4", "d223", "d2", "d135", "d113", "d1"]):
            x = 40 + i * 80
            pg.vsl(x, y, 120, n); pg.text(x - 6, y + 134, LABELS[n]); pg.text(x - 6, y + 164, n)
        pg.text(40 + 9 * 80 + 10, y + 40, "drawbar levels 0 to 1: set d8 0.8", 28)
        y += 220
    CW, WR = 300, 34
    LIM = 1440
    mx = 1
    for _t, _ns in GROUPS[syn]:
        for _n in _ns:
            if _n in pinfo and pinfo[_n][1] >= 2:
                mx = max(mx, -(-len("0..%d : " % (pinfo[_n][1] - 1) + opt_text(syn, _n)) // (WR - 6)))
    RH = 64 + mx * LH + 14
    x, rowy, rowmax = 30, y, 0
    for title, names in GROUPS[syn]:
        names = [n for n in names if n in pinfo]
        ncols = -(-len(names) // 4); nrows = -(-len(names) // ncols); bw = ncols * CW
        if x + bw > LIM and x > 30:
            x = 30; rowy += rowmax + 30; rowmax = 0
        pg.text(x, rowy, "--- " + title + " ---")
        for k, n in enumerate(names):
            cx = x + (k // nrows) * CW; cy = rowy + 40 + (k % nrows) * RH
            d, st = pinfo[n]
            pg.text(cx, cy, "%s  -  set %s" % (LABELS.get(n, n), n))
            if st >= 2:
                pg.hradio(cx, cy + 32, st, n)
                pg.text(cx, cy + 64, ("0..%d : " % (st - 1)) + opt_text(syn, n), WR)
            else:
                pg.hsl(cx, cy + 32, 190, n)
        rowmax = max(rowmax, 40 + nrows * RH)
        x += bw + 20
    pg.ymax = max(pg.ymax, rowy + rowmax)
    return pg

def page_messages(syn):
    P = syn
    pg = Page(syn, "messages", 1300)
    y = 16
    y = pg.para(30, y, "click any box to send it to the synth. The synth receives everything through its name so you can use [s %s] from anywhere in your patch" % P, 110)
    rows = [("note 60 100", "play midi note 60 at velocity 100 (velocity 0 is note off)"),
            ("note 60 0", "release note 60"),
            ("bend 0.5", "pitch bend from -1 to 1 (size set by bendrange)"),
            ("bendrange 12", "bend range in semitones (default 2)"),
            ("mod 0.5", "mod wheel from 0 to 1 (more lfo)"),
            ("aftertouch 0.5", "channel pressure from 0 to 1 (opens the filter)"),
            ("sustain 1", "sustain pedal (1 down and 0 up)"),
            ("panic", "silence everything immediately"),
            ("set %s 0.3" % SWEEP[syn], "set one parameter by name (see the parameters page)"),
            ("params", "print every parameter name to the pd window"),
            ("preset 3", "load preset number 3 (counting from 0)"),
            ("next", "next preset (wraps around)"),
            ("prev", "previous preset"),
            ("mutate 0.3", "random variation of the current sound from 0 to 1"),
            ("A", "remember the current sound as A"),
            ("B", "remember the current sound as B"),
            ("morph 0.5", "blend between A and B"),
            ("store mysound", "keep the current sound in the user bank after the factory presets"),
            ("save my.bank", "write the user bank to a text file next to this patch"),
            ("load my.bank", "add the presets from a text file to the user bank"),
            ("data 0.5", "data inlet: any number from 0 to 1 (a sensor or a csv column) that drives mapped parameters"),
            ("datamap %s 0.3" % SWEEP[syn], "map data to a parameter: it moves up to 0.3 either side (a negative amount inverts)"),
            ("datamap clear", "remove all data mappings (up to four at once)")]
    if syn == "mellotron":
        rows += [("samples tapes", "load every wav in the folder tapes next to this patch (name each file with its midi note like 48.wav)"),
                 ("sample 60 c4.wav", "put one wav on the key for midi note 60"),
                 ("samples clear", "unload all tapes and go back to the synthetic stand-in")]
    for shown, desc in rows:
        pg.msg(30, y, "%s %s" % (P, shown)); pg.text(30 + 38 * CH, y + 2, desc); y += 54
    y += 14
    y = heading(pg, y, "outlets of the object")
    y = pg.para(30, y, "outlet 1 audio left   outlet 2 audio right   outlet 3 preset name as a symbol   outlet 4 list with preset index and preset count", 110)
    y = pg.para(30, y, "create it as [%s~]  or  [%s~ name]  or  [%s~ name 8]   where name is the prefix used by every box and 8 is the voice count" % (P, P, P), 110)
    return pg

def page_examples(syn):
    P = syn; sw = SWEEP[syn]
    pg = Page(syn, "examples", 1300, 1300)
    y = 16
    y = pg.para(30, y, "copy these into your own patch. They all talk to [%s~] through [s %s] so the engine page must be running with dsp on" % (P, P), 130)
    # example 1: random melody
    x0, y0 = 30, y + 10
    pg.text(x0, y0, "1  random notes: toggle on")
    t = pg.tgl(x0, y0 + 34); m = pg.obj(x0, y0 + 74, "metro 250"); r = pg.obj(x0, y0 + 114, "random 24"); a = pg.obj(x0, y0 + 154, "+ 48")
    mk = pg.obj(x0, y0 + 194, "makenote 100 200"); pk = pg.obj(x0, y0 + 234, "pack f f"); lp = pg.obj(x0, y0 + 274, "list prepend note")
    lt = pg.obj(x0, y0 + 314, "list trim"); sn = pg.obj(x0, y0 + 354, "s " + P)
    for a_, b_ in [(t, m), (m, r), (r, a), (a, mk)]: pg.connect(a_, 0, b_, 0)
    pg.connect(mk, 0, pk, 0); pg.connect(mk, 1, pk, 1)
    for a_, b_ in [(pk, lp), (lp, lt), (lt, sn)]: pg.connect(a_, 0, b_, 0)
    # example 2: automatic preset browser
    x1 = 330
    pg.text(x1, y0, "2  walk through the presets: toggle on")
    t2 = pg.tgl(x1, y0 + 34); m2 = pg.obj(x1, y0 + 74, "metro 2000"); n2 = pg.msg(x1, y0 + 114, "next", "next"); s2 = pg.obj(x1, y0 + 154, "s " + P)
    for a_, b_ in [(t2, m2), (m2, n2), (n2, s2)]: pg.connect(a_, 0, b_, 0)
    pg.text(x1, y0 + 200, "hold a chord first with chord on in the engine page", 26)
    # example 3: slow morph
    x2 = 650
    pg.text(x2, y0, "3  slow morph between A and B")
    t3 = pg.tgl(x2, y0 + 34); m3 = pg.obj(x2, y0 + 74, "metro 6000")
    g3 = pg.msg(x2, y0 + 114, "0 1 3000 0 3000 3000", "0 \\, 1 3000 \\, 0 3000 3000")
    l3 = pg.obj(x2, y0 + 154, "line 0 30"); p3 = pg.obj(x2, y0 + 194, "list prepend morph"); u3 = pg.obj(x2, y0 + 234, "list trim"); s3 = pg.obj(x2, y0 + 274, "s " + P)
    for a_, b_ in [(t3, m3), (m3, g3), (g3, l3), (l3, p3), (p3, u3), (u3, s3)]: pg.connect(a_, 0, b_, 0)
    pg.text(x2, y0 + 320, "store sounds with A and B first (presets page)", 26)
    # example 4: parameter sweep
    x3 = 980
    pg.text(x3, y0, "4  sweep %s with a ramp" % sw)
    t4 = pg.tgl(x3, y0 + 34); m4 = pg.obj(x3, y0 + 74, "metro 4000")
    g4 = pg.msg(x3, y0 + 114, "0.1 0.9 2000 0.1 2000 2000", "0.1 \\, 0.9 2000 \\, 0.1 2000 2000")
    l4 = pg.obj(x3, y0 + 154, "line 0.1 30"); p4 = pg.obj(x3, y0 + 194, "list prepend set %s" % sw); u4 = pg.obj(x3, y0 + 234, "list trim"); s4 = pg.obj(x3, y0 + 274, "s " + P)
    for a_, b_ in [(t4, m4), (m4, g4), (g4, l4), (l4, p4), (p4, u4), (u4, s4)]: pg.connect(a_, 0, b_, 0)
    yy = y0 + 430
    pg.text(30, yy, "5  data in: a ramp stands in for a sensor or csv column. Click the map box once first")
    mm = pg.msg(30, yy + 34, "%s datamap %s 0.4" % (P, SWEEP[syn]))
    t5 = pg.tgl(30, yy + 94); m5 = pg.obj(30, yy + 134, "metro 4000")
    g5 = pg.msg(30, yy + 174, "0 1 2000 0 2000 2000", "0 \\, 1 2000 \\, 0 2000 2000")
    l5 = pg.obj(30, yy + 214, "line 0 30"); p5 = pg.obj(30, yy + 254, "list prepend data"); u5 = pg.obj(30, yy + 294, "list trim"); s5 = pg.obj(30, yy + 334, "s " + P)
    for a_, b_ in [(t5, m5), (m5, g5), (g5, l5), (l5, p5), (p5, u5), (u5, s5)]: pg.connect(a_, 0, b_, 0)
    pg.text(330, yy + 94, "in your own patch send any value from 0 to 1 to [s %s] as: data 0.7. For a csv series scale it to 0..1 first" % P, 60)
    pg.ymax = max(pg.ymax, yy + 380)
    return pg

def page_midi(syn):
    pg = Page(syn, "midi", 780, 640)
    o = {}
    def add(k, x, y, s): o[k] = pg.obj(x, y, s)
    add("notein", 20, 20, "notein"); add("pack", 20, 70, "pack f f"); add("pn", 20, 120, "list prepend note"); add("tn", 20, 170, "list trim")
    add("bendin", 260, 20, "bendin"); add("b1", 260, 70, "- 8192"); add("b2", 260, 120, "/ 8192"); add("pb", 260, 170, "list prepend bend"); add("tb", 260, 220, "list trim")
    add("ctl1", 500, 20, "ctlin 1"); add("m1", 500, 70, "/ 127"); add("pm", 500, 120, "list prepend mod"); add("tm", 500, 170, "list trim")
    add("ctl64", 20, 320, "ctlin 64"); add("s1", 20, 370, ">= 64"); add("ps", 20, 420, "list prepend sustain"); add("ts", 20, 470, "list trim")
    add("touch", 260, 320, "touchin"); add("a1", 260, 370, "/ 127"); add("pa", 260, 420, "list prepend aftertouch"); add("ta", 260, 470, "list trim")
    add("send", 260, 560, "s " + pg.syn)
    c = pg.connect
    c(o["notein"], 0, o["pack"], 0); c(o["notein"], 1, o["pack"], 1); c(o["pack"], 0, o["pn"], 0); c(o["pn"], 0, o["tn"], 0); c(o["tn"], 0, o["send"], 0)
    for a, b, d, e, f in [("bendin", "b1", "b2", "pb", "tb"), ("ctl1", "m1", None, "pm", "tm"), ("ctl64", "s1", None, "ps", "ts"), ("touch", "a1", None, "pa", "ta")]:
        c(o[a], 0, o[b], 0)
        if d: c(o[b], 0, o[d], 0); c(o[d], 0, o[e], 0)
        else: c(o[b], 0, o[e], 0)
        c(o[e], 0, o[f], 0); c(o[f], 0, o["send"], 0)
    return pg

def page_engine(syn):
    P = syn
    pg = Page(syn, "engine", 1000, 520)
    y = 16
    y = pg.para(30, y, "this page holds the synth itself. Turn dsp on then play your midi keyboard or click chord on", 110)
    sy = pg.obj(30, y + 20, "%s~ %s" % (P, P)); dc = pg.obj(30, y + 100, "dac~")
    pg.connect(sy, 0, dc, 0); pg.connect(sy, 1, dc, 1)
    pg.msg(300, y + 20, "pd dsp 1"); pg.msg(440, y + 20, "pd dsp 0")
    notes = (48, 55, 64, 72)
    on = " ".join("\\; %s note %d 100" % (P, n) for n in notes); off = " ".join("\\; %s note %d 0" % (P, n) for n in notes)
    pg.msg(300, y + 100, "chord on", on); pg.msg(520, y + 100, "chord off", off); pg.msg(740, y + 100, "%s panic" % P)
    mp = page_midi(syn)
    pg.sub(300, y + 230, mp)
    pg.text(430, y + 232, "midi in: notes bend mod wheel sustain and aftertouch (open to see how it is built)", 52)
    y += 300
    note = "mono: a chord plays only the last note" if P in MONO else "polyphonic: a chord plays all four notes"
    pg.text(30, y, "chord on plays c2 g2 e3 c4. " + note, 100)
    pg.text(30, y + 40, "the console prints the version line when the object is created. If the object box shows a dashed border the external was not found: check the externals folder and restart pd", 100)
    pg.ymax = max(pg.ymax, y + 90)
    return pg, mp

def build(syn):
    bad = 0
    pr, pm, ms, ex = page_presets(syn), page_parameters(syn), page_messages(syn), page_examples(syn)
    en, mp = page_engine(syn)
    main = Page(syn, "main", 1000)
    y = 16
    main.text(30, y, BANNER % syn); y += 30
    main.text(30, y, TITLE[syn]); y += 40
    y = main.para(30, y, DESC[syn], 110, 20)
    y = heading(main, y, "how it was made")
    for s in HOW[syn]: y = main.para(30, y, s, 110, 10)
    y += 10
    y = heading(main, y, "how to use")
    for s in ["1  open the engine page below and turn dsp on",
              "2  play a midi keyboard or click chord on",
              "3  open the parameters page and move any control or open presets and step through the sounds",
              "4  everything is a message: see the messages page for the full list and the examples page for patches to copy"]:
        y = main.para(30, y, s, 110, 10)
    y += 10
    for s in TIPS[syn]: y = main.para(30, y, "try:  " + s, 110, 10)
    y += 14
    y = heading(main, y, "common problems")
    for s in PROBLEMS["all"] + PROBLEMS[syn]: y = main.para(30, y, s, 110, 10)
    y += 14
    y = heading(main, y, "click a box to open that page")
    labels = [("presets", pr, "all sounds and morph"), ("parameters", pm, "every control"), ("messages", ms, "clickable list"),
              ("examples", ex, "patches to copy"), ("engine", en, "synth dsp and midi")]
    for i, (n, page, d) in enumerate(labels):
        x = 30 + i * 190
        main.sub(x, y, page); main.text(x, y + 34, d, 22)
    y += 100
    y = main.para(30, y, "this is a model of the architecture with original presets and it is not a copy of any hardware or rom. It has not been compared against the real instrument", 110, 0)
    for p in (pr, pm, ms, ex, en, mp, main): bad += p.check()
    out = ["#N canvas 0 50 1040 %d 12;" % main.height()] + main.lines()
    path = os.path.join(ROOT, syn, "%s~-help.pd" % syn)
    with open(path, "w") as f: f.write("\n".join(out) + "\n")
    print("%s: %d overlaps -> %s" % (syn, bad, path))
    return bad

if __name__ == "__main__":
    sys.exit(1 if sum(build(s) for s in SYNTHS) else 0)
