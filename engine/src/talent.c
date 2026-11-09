/*
 * talent.c — Talento del jugador: posterior bayesiana (lección, Ec. 10–14).
 *
 *   HR*_s = PA_s · expit( logit(HR_s/PA_s) − logit λ_s + logit λ_t )     ajuste por era (Ec. 11)
 *   α = α0 + Σ w_k HR*_{t−k},  β = β0 + Σ w_k (PA_{t−k} − HR*_{t−k})     posterior (Ec. 12)
 *   E[θ̃] = α/(α+β) = n/(n+n0)·r + n0/(n+n0)·m                           shrinkage (Ec. 13)
 *   θ = expit( logit E[θ̃] + Δ_edad ),  Δ_edad = a(edad_t) − media_w a(edad_k)   (Ec. 14)
 * Pesos w = (1, 0.8, 0.6) para las temporadas t−1, t−2, t−3.
 */
#include <math.h>
#include <string.h>
#include "aod.h"

static const double W[3] = {1.0, 0.8, 0.6};

/* λ de la liga en una temporada; si no existe (temporada futura), la más reciente anterior */
static double lambda_at(const Dataset *ds, int season, char league)
{
    for (int s = season; s > season - 10; s--) {
        double l = data_lambda(ds, s, league);
        if (!isnan(l)) return l;
    }
    return NAN;
}

int talent_estimate(const Dataset *ds, const Prior *pr, const Aging *ag, const Player *pl,
                    int t, TalentOpts opt, Talent *o)
{
    memset(o, 0, sizeof *o);
    const SeasonRow *cur = data_season(ds, pl->id, t);
    char league_t = cur ? cur->league : 0;

    for (int k = 1; k <= 3; k++) {
        const SeasonRow *s = data_season(ds, pl->id, t - k);
        if (!s || s->pa == 0) continue;
        const int i = o->nseasons++;
        o->season[i] = s->season; o->pa[i] = s->pa; o->hr[i] = s->hr; o->w[i] = W[k - 1];
        o->lambda_s[i] = lambda_at(ds, s->season, s->league);
        if (!league_t) league_t = s->league;              /* sin datos en t: última liga conocida */
    }
    o->lambda_t = league_t ? lambda_at(ds, t, league_t) : NAN;

    double sum_w = 0, sum_wa = 0;
    for (int i = 0; i < o->nseasons; i++) {
        double hs = o->hr[i];
        if (opt.use_era && o->hr[i] > 0 && !isnan(o->lambda_t))
            hs = o->pa[i] * expit(logit((double)o->hr[i] / o->pa[i]) - logit(o->lambda_s[i]) + logit(o->lambda_t));
        o->hr_star[i] = hs;
        o->hr_w += o->w[i] * hs;
        o->n    += o->w[i] * o->pa[i];
        const int e = age_at(pl, o->season[i]);
        if (e > 0) { sum_w += o->w[i]; sum_wa += o->w[i] * aging_curve(ag, e); }
    }
    o->r          = o->n > 0 ? o->hr_w / o->n : NAN;
    o->alpha      = pr->a0 + o->hr_w;
    o->beta       = pr->b0 + (o->n - o->hr_w);
    o->mean_tilde = o->alpha / (o->alpha + o->beta);
    o->w_player   = o->n / (o->n + pr->n0);

    o->age_t = age_at(pl, t);
    o->delta = 0.0;
    if (opt.use_age && o->age_t > 0 && sum_w > 0 && ag->npairs > 0)
        o->delta = aging_curve(ag, o->age_t) - sum_wa / sum_w;
    o->theta = expit(logit(o->mean_tilde) + o->delta);
    return 0;
}
