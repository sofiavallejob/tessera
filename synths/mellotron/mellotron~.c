#define _GNU_SOURCE 1
/* mellotron~ - Mellotron style tape keyboard (your own WAV files). See mellotron.h and synthwrap.h. */
#include "m_pd.h"
#include "mellotron.h"
#include "synthwrap.h"
SK_PD_CLASS(mellotron, "mellotron~", mel_def)
