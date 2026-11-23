/*
 * talent.c — Talento del jugador, por capas.
 *
 *   capa 0 (tasa cruda):  θ = (HR_{t−1} + HR_{t−2} + HR_{t−3}) / (PA_{t−1} + PA_{t−2} + PA_{t−3})
 *   capa 1 (pesos):       θ = (1·HR_{t−1} + 0.8·HR_{t−2} + 0.6·HR_{t−3}) / (1·PA_{t−1} + 0.8·PA_{t−2} + 0.6·PA_{t−3})
 *   capa 2 (Beta):        θ ~ Beta(α, β),  α = α0 + Σ w·HR,  β = β0 + Σ w·(PA − HR)
 *
 * En la capa 1 la temporada más reciente pesa más (pesos de Marcel, Tom Tango: 5/4/3).
 * En la capa 2 se parte de la liga, Beta(α0, β0) de prior.c, y se le suman los jonrones y
 * los turnos sin jonrón del jugador. Su promedio,
 *   α / (α + β) = n/(n + n0) · (tasa del jugador) + n0/(n + n0) · (promedio de la liga),  n = Σ w·PA,
 * acerca al jugador al promedio de la liga, más cuanto menos turnos tenga. Y θ deja de ser
 * un número fijo: la simulación sortea un θ distinto en cada temporada.
 * Si no jugó alguna de esas temporadas, se usan las que sí jugó.
 */
#include <string.h>
#include "aod.h"

static const double PESOS[3] = {1.0, 0.8, 0.6};       /* t−1, t−2, t−3 */

int talent_estimate(const Dataset *ds, const Player *pl, int t, int capa, Talent *o)
{
    memset(o, 0, sizeof *o);
    for (int k = 1; k <= 3; k++) {
        const SeasonRow *s = data_season(ds, pl->id, t - k);
        if (!s || s->pa == 0) continue;
        const int i = o->nseasons++;
        o->season[i] = s->season; o->pa[i] = s->pa; o->hr[i] = s->hr;
        o->w[i] = capa >= 1 ? PESOS[k - 1] : 1.0;          /* capa 0: todas pesan igual */
        o->pa_sum += s->pa;
        o->hr_sum += s->hr;
        o->pa_w += o->w[i] * s->pa;
        o->hr_w += o->w[i] * s->hr;
    }
    if (o->pa_sum == 0) return 1;                      /* no jugó en esas 3 temporadas */
    o->theta = o->hr_w / o->pa_w;

    if (capa >= 2) {                                   /* capa 2: mezclar con la liga */
        o->liga = prior_estimate(ds, t, 3, 300);       /* regulares de t−3 a t−1 */
        if (o->liga.J < 2) return 1;                   /* sin datos de la liga */
        o->alpha = o->liga.a0 + o->hr_w;
        o->beta  = o->liga.b0 + (o->pa_w - o->hr_w);
        o->theta = o->alpha / (o->alpha + o->beta);
        o->peso_jugador = o->pa_w / (o->pa_w + o->liga.n0);
    }
    return 0;
}
