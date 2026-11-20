/*
 * talent.c — Talento del jugador, capa 0: tasa cruda.
 *
 *   θ = (HR_{t−1} + HR_{t−2} + HR_{t−3}) / (PA_{t−1} + PA_{t−2} + PA_{t−3})
 *
 * Solo con los datos del propio jugador: sus HR entre sus turnos al bate (PA)
 * de las 3 temporadas anteriores a la que se quiere predecir. Si no jugó alguna
 * de esas temporadas, se usan las que sí jugó.
 */
#include <string.h>
#include "aod.h"

int talent_estimate(const Dataset *ds, const Player *pl, int t, Talent *o)
{
    memset(o, 0, sizeof *o);
    for (int k = 1; k <= 3; k++) {
        const SeasonRow *s = data_season(ds, pl->id, t - k);
        if (!s || s->pa == 0) continue;
        const int i = o->nseasons++;
        o->season[i] = s->season; o->pa[i] = s->pa; o->hr[i] = s->hr;
        o->pa_sum += s->pa;
        o->hr_sum += s->hr;
    }
    if (o->pa_sum == 0) return 1;                      /* no jugó en esas 3 temporadas */
    o->theta = (double)o->hr_sum / o->pa_sum;
    return 0;
}
