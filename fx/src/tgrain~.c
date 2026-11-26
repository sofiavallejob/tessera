/* [tgrain~] - pitch-aware granulator. Copyright (c) 2026 Sofia Vallejo Budziszewski. BSD 3-Clause. */
#define FX_PD 1
#include "grain.h"
static const fx_def *grain_pick(int argc, t_atom *argv) { (void)argc; (void)argv; return &grain_def; }
FX_PD_CLASS("tgrain~", grain_pick, tgrain_tilde_setup)
