/* [tmodal~] - modal resonator bank. Copyright (c) 2026 Sofia Vallejo Budziszewski. BSD 3-Clause. */
#define FX_PD 1
#include "modal.h"
static const fx_def *modal_pick(int argc, t_atom *argv) { (void)argc; (void)argv; return &modal_def; }
FX_PD_CLASS("tmodal~", modal_pick, tmodal_tilde_setup)
