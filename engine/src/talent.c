/*
 * talent.c — Talento del jugador, solo con sus propios datos.
 *
 *   capa 0 (tasa cruda):  θ = (HR_{t−1} + HR_{t−2} + HR_{t−3}) / (PA_{t−1} + PA_{t−2} + PA_{t−3})
 *   capa 1 (pesos):       θ = (1·HR_{t−1} + 0.8·HR_{t−2} + 0.6·HR_{t−3}) / (1·PA_{t−1} + 0.8·PA_{t−2} + 0.6·PA_{t−3})
 *
 * En la capa 1 la temporada más reciente pesa más, porque dice más de cómo está
 * el jugador hoy. Los pesos 1 · 0.8 · 0.6 son los del sistema Marcel (Tom Tango: 5/4/3).
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
    return 0;
}
