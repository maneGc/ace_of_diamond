/*
 * talent.c — Talento del jugador, por capas.
 *
 *   capa 0 (tasa cruda):  θ = (HR_{t−1} + HR_{t−2} + HR_{t−3}) / (PA_{t−1} + PA_{t−2} + PA_{t−3})
 *   capa 1 (pesos):       θ = (1·HR_{t−1} + 0.8·HR_{t−2} + 0.6·HR_{t−3}) / (1·PA_{t−1} + 0.8·PA_{t−2} + 0.6·PA_{t−3})
 *   capa 2 (Beta):        θ ~ Beta(α, β),  α = α0 + Σ w·HR,  β = β0 + Σ w·(PA − HR)
 *   capa 3 (época):       lo mismo, pero cada temporada pasada se traduce antes al ambiente de t:
 *                         HR*_s = PA_s · expit( logit(HR_s/PA_s) − logit λ_s + logit λ_t )
 *   capa 4 (estadio):     y también a los estadios donde bateó en t:
 *                         HR*_s = PA_s · expit( … − parque_s + parque_t )
 *
 * En la capa 1 la temporada más reciente pesa más (pesos de Marcel, Tom Tango: 5/4/3).
 * En la capa 2 se parte de la liga, Beta(α0, β0) de prior.c, y se le suman los jonrones y
 * los turnos sin jonrón del jugador; θ deja de ser un número fijo.
 * En la capa 3, λ_s es el promedio de jonrones de su liga en la temporada s y λ_t el de la
 * temporada que se quiere predecir (ver ambiente en data.c). Si en su época se pegaban más
 * jonrones que en t, sus HR se bajan; si se pegaban menos, se suben (Schell, 2005; log5).
 * En la capa 4, parque_s es el promedio (sobre sus turnos) del logaritmo del factor de los
 * estadios donde bateó en s, y parque_t el de t. Es el contexto c_j del modelo, solo con el estadio.
 * Si no jugó alguna de esas temporadas, se usan las que sí jugó.
 */
#include <math.h>
#include <string.h>
#include "aod.h"

static const double PESOS[3] = {1.0, 0.8, 0.6};       /* t−1, t−2, t−3 */

int talent_estimate(const Dataset *ds, const Player *pl, int t, int capa, Talent *o)
{
    memset(o, 0, sizeof *o);
    const SeasonRow *cur = data_season(ds, pl->id, t);
    char liga_t = cur ? cur->league : 0;               /* su liga en t (si no jugó, la más reciente) */
    o->parque_t = cur ? cur->parque : 0.0;             /* sus estadios en t (sin datos: neutros) */

    for (int k = 1; k <= 3; k++) {
        const SeasonRow *s = data_season(ds, pl->id, t - k);
        if (!s || s->pa == 0) continue;
        const int i = o->nseasons++;
        o->season[i] = s->season; o->pa[i] = s->pa; o->hr[i] = s->hr;
        o->w[i] = capa >= 1 ? PESOS[k - 1] : 1.0;          /* capa 0: todas pesan igual */
        o->lambda_s[i] = data_lambda(ds, s->season, s->league);
        o->parque_s[i] = s->parque;
        o->hr_star[i] = s->hr;
        o->pa_sum += s->pa;
        o->hr_sum += s->hr;
        if (!liga_t) liga_t = s->league;
    }
    if (o->pa_sum == 0) return 1;                      /* no jugó en esas 3 temporadas */

    if (capa >= 3) {                                   /* capas 3 y 4: traducir al ambiente de t */
        o->lambda_t = ambiente(ds, t, liga_t);
        for (int i = 0; i < o->nseasons; i++)
            if (o->hr[i] > 0 && !isnan(o->lambda_s[i]) && !isnan(o->lambda_t)) {
                double z = logit((double)o->hr[i] / o->pa[i]) - logit(o->lambda_s[i]) + logit(o->lambda_t);
                if (capa >= 4) z += o->parque_t - o->parque_s[i];    /* capa 4: a sus estadios de t */
                o->hr_star[i] = o->pa[i] * expit(z);
            }
    }
    for (int i = 0; i < o->nseasons; i++) {
        o->pa_w += o->w[i] * o->pa[i];
        o->hr_w += o->w[i] * o->hr_star[i];
    }
    o->theta = o->hr_w / o->pa_w;

    if (capa >= 2) {                                   /* capa 2: mezclar con la liga */
        o->liga = prior_estimate(ds, t, 3, 300, capa, o->parque_t);   /* regulares de t−3 a t−1 */
        if (o->liga.J < 2) return 1;                   /* sin datos de la liga */
        o->alpha = o->liga.a0 + o->hr_w;
        o->beta  = o->liga.b0 + (o->pa_w - o->hr_w);
        o->theta = o->alpha / (o->alpha + o->beta);
        o->peso_jugador = o->pa_w / (o->pa_w + o->liga.n0);
    }
    return 0;
}
