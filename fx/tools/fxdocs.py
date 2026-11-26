"""Text for the tessera effect help patches and readmes. Plain words, no commas or semicolons in comments (Pd special characters)."""

ORDER = ["freeze~", "smear~", "gate~", "cross~", "grain~", "modal~", "datamod~"]

# which C tables belong to which effect: (source file, param table, preset table, engine object, instance name)
SRC = {
    "freeze~":  ("src/tspec~.c", "tf_params", "tf_presets", "tessera/tspec~ freeze 2048", "f1"),
    "smear~":   ("src/tspec~.c", "ts_params_smear", "ts_presets_smear", "tessera/tspec~ smear 2048", "s1"),
    "gate~":    ("src/tspec~.c", "tg_params", "tg_presets", "tessera/tspec~ gate 2048", "t1"),
    "cross~":   ("src/tspec~.c", "tc_params", "tc_presets", "tessera/tspec~ cross 2048", "c1"),
    "grain~":   ("src/grain.h", "grain_params", "grain_presets", "tessera/grain~", "g1"),
    "modal~":   ("src/modal.h", "modal_params", "modal_presets", "tessera/modal~", "m1"),
    "datamod~": ("src/datamod.h", "datamod_params", "datamod_presets", "tessera/datamod~", "d1"),
}
NIN = {"freeze~": 1, "smear~": 1, "gate~": 1, "cross~": 2, "grain~": 1, "modal~": 1, "datamod~": 0}
NOUT = {"freeze~": 1, "smear~": 1, "gate~": 1, "cross~": 1, "grain~": 2, "modal~": 2, "datamod~": 4}
NCTL = {"freeze~": 0, "smear~": 0, "gate~": 0, "cross~": 0, "grain~": 1, "modal~": 1, "datamod~": 2}

TITLE = {
    "freeze~": "spectral freeze", "smear~": "spectral smear", "gate~": "spectral gate", "cross~": "cross synthesis between two sounds",
    "grain~": "pitch-aware granulator", "modal~": "modal resonator bank", "datamod~": "data modulators",
}
DESC = {
    "freeze~": "holds the spectrum of the incoming sound and keeps it sounding after the input stops. jitter and shimmer keep the held sound alive",
    "smear~": "blurs the spectrum over time and over frequency so short sounds turn into long washes",
    "gate~": "keeps only the strongest frequency bins of each moment and removes the rest. invert keeps the quiet bins instead",
    "cross~": "cross synthesis. the left inlet is sound a (the one you hear) and the right inlet is sound b. a takes the spectral shape or phase of b",
    "grain~": "a granulator that listens to the pitch of its input. grains can be cut to whole periods of the sound and retuned to the nearest note of a scale. grains can play backwards with a probability and the read position can be scrubbed by data",
    "modal~": "a bank of up to 32 tuned resonators. the mode ratios come from a material (glass wood bell plate bar string tube bowl) or are learned from a sound. modes can be snapped to a scale",
    "datamod~": "turns data into four control signals. any data series (a csv column or a list) plays as an lfo. there are also a lorenz attractor and a henon map and a cellular automaton with all 256 rules",
}
HOW = {
    "freeze~": "a short time fourier transform (2048 points and 75 percent overlap and a hann window) splits the sound into frames. while frozen one frame is repeated with random phase and level changes so the held sound does not sound static",
    "smear~": "the same stft engine. the magnitude of every bin is averaged over time (or the peaks are held) and blurred over neighbouring bins. the phase is kept or randomised",
    "gate~": "the same stft engine. every frame is compared with its own loudest bin and bins below the threshold are turned down smoothly with attack and release times",
    "cross~": "the same stft engine with two inputs. four methods combine the magnitude and phase of the two frames: a spectral envelope vocoder and phase borrowing and magnitude borrowing and a morph",
    "grain~": "a four second ring buffer is read by up to 64 overlapping grains. the pitch of the input is tracked with a yin style difference function every 256 samples. grains use cubic interpolation and a window that goes from hann to almost rectangular",
    "modal~": "every mode is a two pole resonator tuned to the root note times a ratio. the gain is normalised so a sine at the mode frequency passes at the mode weight. learn records one second of input and picks the strongest peaks of its averaged spectrum as the new modes",
    "datamod~": "the lorenz system is integrated per sample with runge kutta. the henon map and the automaton step at the chosen rate. a series is read with hold or linear or cubic interpolation. all lanes are smoothed with a one pole filter",
}
HOWTO = {
    "freeze~": ["open the engine page and switch the sound on", "set freeze to on and then switch the sound off. the held sound keeps playing", "move jitter and shimmer to change how static the held sound is"],
    "smear~": ["open the engine page and switch the sound on", "increase time to make the tail longer", "shape toward 1 holds the loudest moments and gives the longest tails"],
    "gate~": ["open the engine page and switch the sound on (the source has a tone and some noise)", "raise thresh until only the tone is left", "turn invert on to keep what was removed"],
    "cross~": ["open the engine page and switch the tones on", "choose a method. envelope gives the classic vocoder sound", "lower amount to blend sound a back in"],
    "grain~": ["open the engine page and switch the sound on", "load a preset and listen. size and density set the texture", "turn tune to 1 with a scale to pull every grain to the nearest scale note of the detected pitch", "move scrub or map data to it to read the past of the sound"],
    "modal~": ["open the engine page and switch the sound on or press strike", "choose a material and a root note", "set scale to pentatonic and snap to 1 to pull every mode onto the scale", "send learn and play a sound for one second to take its modes"],
    "datamod~": ["open the engine page. the four lanes drive an oscillator and show the data", "choose a source. lorenz and henon need no data", "send read file.csv 2 to play the second column of a csv file as an lfo", "connect the data outlet to the left inlet of any tessera effect and map it with datamap"],
}
TIPS = {
    "freeze~": ["latency is the fft size (2048 samples). send size 4096 for a smoother hold", "freeze can be mapped to nothing but mix and jitter can be mapped to data"],
    "smear~": ["short time values give reverb-like blur. long values give pads", "width blurs across frequency and makes the sound more noisy"],
    "gate~": ["thresh is relative to the loudest bin of every moment so it works at any level", "floor at -90 removes the closed bins completely"],
    "cross~": ["use a sustained sound as a and speech or drums as b for the best vocoder effect", "morph moves levels and phase from a to b together"],
    "grain~": ["sync on makes grains start on period boundaries so tones stay clean", "the pitch outlet sends pitch hz confidence about twenty times per second", "reverse 0.3 with spray gives a cloud that sometimes runs backwards"],
    "modal~": ["a sine at the root excites the first mode strongly. broadband sounds excite all modes", "drive is in db. raise it for quiet inputs", "stretch moves the modes away from or toward harmonic positions"],
    "datamod~": ["rate means different things: traversals per second for a series and time units per second for lorenz and steps per second for henon and ca", "set clock to external and send step to drive the automaton from your own metro", "the cells outlet message gives the first 16 cells of the automaton row as a pattern"],
}
PROBLEMS = {
    "freeze~": ["no sound: the effect has a delay of one fft size and needs dsp on", "clicks when freeze is switched: the release is short on purpose. use mix to blend"],
    "smear~": ["no sound: check mix and that dsp is on", "very quiet: shape near 0 averages the sound and lowers the level. raise shape"],
    "gate~": ["silence: thresh is too high. lower it", "ringing: raise release or lower floor"],
    "cross~": ["nothing happens: sound b needs to be connected to the right inlet", "weak effect: raise amount and mix"],
    "grain~": ["no sound: the buffer must fill first. scrub 0 reads the present and needs some input", "tune has no effect: the input must have a clear pitch. watch the pitch outlet", "grains sound rough: turn sync on or raise the grain size"],
    "modal~": ["very quiet output: raise drive or mix and strike the bank", "learn found nothing: the input was silent or too quiet. send learn again while a sound plays", "material learned sounds like nothing: learn first"],
    "datamod~": ["read failed: the path is relative to the patch folder. use an absolute path or put the file next to the patch", "series is flat: the column has no variation or you chose a text column", "no output: sources step by the clock. check rate and that dsp is on"],
}

# per parameter: description
PDESC = {
 "freeze": "on holds the current spectrum", "jitter": "random phase of every frame. 0 is a pure drone and higher is airier", "shimmer": "random level of every bin. a slow glittering movement",
 "mix": "0 is the dry sound and 1 is only the effect", "time": "how long a sound keeps ringing in ms", "shape": "0 averages over time and 1 holds the loudest moments",
 "width": "blur across neighbouring bins", "diffuse": "random phase that turns held tones into noise",
 "thresh": "bins quieter than this (db below the loudest bin) are closed", "floor": "level of closed bins in db", "attack": "how fast bins open in ms",
 "release": "how fast bins close in ms", "invert": "on keeps the quiet bins and removes the strong ones",
 "cmode": "envelope phase magnitude or morph", "amount": "0 is a alone and 1 is the full effect",
 "size": "grain length in ms (with sync on it is rounded to whole periods)", "density": "grains per second", "scrub": "read position in the past. 0 is now and 1 is about four seconds ago",
 "spray": "random extra delay of the read position in ms", "pitch": "transposition of every grain in semitones", "tune": "how strongly grains are pulled to the nearest scale note of the detected pitch",
 "key": "root of the scale", "scale": "scale used by tune (and by snap in modal~)", "scatter": "random pitch per grain in semitones", "reverse": "probability that a grain plays backwards",
 "sync": "on cuts grains to whole periods and starts them on period boundaries", "spread": "stereo spread of the grains (or of the modes)", "gain": "output gain in db", "freeze_g": "",
 "root": "midi note of the lowest mode", "material": "mode ratios: glass wood bell plate bar string tube bowl or learned", "modes": "number of resonators", "snap": "how far modes are pulled onto the scale",
 "decay": "decay time of the lowest mode in seconds", "damp": "higher modes decay faster as this grows", "stretch": "moves modes away from (plus) or toward (minus) harmonic positions",
 "bright": "weight of the higher modes", "drive": "input gain in db", "level": "output level in db",
 "source": "series or lorenz or henon or ca", "rate": "speed. see the tips", "smooth": "smoothing time of the lanes in ms", "depth": "scales the output around the middle",
 "offset": "shifts the output", "bipolar": "on gives -1 to 1 instead of 0 to 1", "interp": "series interpolation: hold or linear or cubic", "loop": "series end behaviour: once or loop or pingpong",
 "chaos": "lorenz rho and henon a. higher is wilder", "rule": "cellular automaton rule 0 to 255", "density_ca": "", "clock": "internal or external (send step)", "warp": "bends the shape of the output",
}
PDESC_OVERRIDE = {
    ("grain~", "mix"): "0 is the dry sound and 1 is only grains",
    ("datamod~", "density"): "fill of the first automaton row after a reseed",
    ("datamod~", "width"): "number of automaton cells (8 to 64)",
    ("smear~", "mix"): "0 is the dry sound and 1 is only the effect",
    ("grain~", "spread"): "stereo spread of the grains",
    ("modal~", "spread"): "stereo spread of the modes",
    ("grain~", "freeze"): "on stops writing to the buffer so the grains keep reading the same sound",
    ("freeze~", "width"): "",
    ("cross~", "width"): "how coarse the spectral envelope is in the envelope method",
    ("smear~", "width"): "blur across neighbouring bins",
    ("grain~", "key"): "root of the scale for tune", ("modal~", "key"): "root of the scale for snap",
    ("grain~", "scale"): "scale for tune", ("modal~", "scale"): "scale for snap",
}

MSGS = {
 "freeze~": [("size 4096", "fft size 256 to 16384 (latency equals the size)"), ("reset", "clears the buffers"), ("latency", "prints the delay in the console")],
 "smear~": [("size 4096", "fft size 256 to 16384 (latency equals the size)"), ("reset", "clears the buffers"), ("latency", "prints the delay in the console")],
 "gate~": [("size 4096", "fft size 256 to 16384 (latency equals the size)"), ("reset", "clears the buffers"), ("latency", "prints the delay in the console")],
 "cross~": [("size 4096", "fft size 256 to 16384 (latency equals the size)"), ("reset", "clears the buffers"), ("latency", "prints the delay in the console"), ("cmode phase", "method by name or number")],
 "grain~": [("pitch", "the pitch outlet sends pitch hz confidence about twenty times per second while dsp is on")],
 "modal~": [("strike 1", "sets all modes ringing at their own weights. the number is the strength"), ("learn", "records one second of the input and takes its strongest peaks as modes. then choose material learned. the outlet reports how many modes were found")],
 "datamod~": [("read data.csv 2", "plays column 2 of a csv or text file (header lines are skipped). the path is relative to the patch"), ("series 1 5 2 8 3", "plays a list as the series"), ("array mytable", "plays a pd array"),
              ("seed 7", "restarts the chaos sources from a seed"), ("restart", "series back to the start"), ("step", "advances henon or ca by one step (use with clock external)"), ("reseed", "fills the first automaton row at random with the density param"), ("cells 32", "automaton with one single cell in the middle")],
}
