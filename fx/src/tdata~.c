/* [tdata~] - data modulators. Copyright (c) 2026 Sofia Vallejo Budziszewski. BSD 3-Clause. */
#define FX_PD 1
#include "datamod.h"
#include "g_canvas.h"

static const fx_def *datamod_pick(int argc, t_atom *argv) { (void)argc; (void)argv; return &datamod_def; }

/* [array <name>( copies a Pd array (table) as the series */
static void datamod_array(t_fxpd *x, t_symbol *name)
{
    t_garray *g = (t_garray *)pd_findbyclass(name, garray_class);
    int n; t_word *w; float *v;
    if (!g || !garray_getfloatwords(g, &n, &w) || n < 2) { pd_error(x, "datamod~: no array '%s' with at least 2 points", name->s_name); return; }
    v = (float *)malloc((size_t)n * sizeof(float));
    if (!v) return;
    for (int i = 0; i < n; i++) v[i] = w[i].w_float;
    dm_set_series((dm_state *)x->e->u, v, n);
    free(v);
    post("datamod~: series from array %s (%d points)", name->s_name, n);
}
void tdata_tilde_setup(void)
{
    fxpd_setup("tdata~", datamod_pick);
    class_addmethod(fxpd_class, (t_method)datamod_array, gensym("array"), A_SYMBOL, 0);
}
