/*
 * prior.c — Lo que se sabe de la liga antes de ver al jugador (Bayes empírico).
 *
 * Con los bateadores regulares (al menos min_pa turnos) de las temporadas t−years … t−1:
 *   m  = promedio de sus tasas HR/PA                      (el talento típico)
 *   s² = varianza de sus tasas                            (cuánto varían entre sí)
 *   s² ≈ v + m(1−m)·promedio(1/PA)                        (talento real + suerte)
 *   v  = s² − m(1−m)·promedio(1/PA)                       (varianza del talento real)
 *   n0 = m(1−m)/v − 1,  α0 = m·n0,  β0 = (1−m)·n0          (método de momentos)
 *
 * La Beta(α0, β0) tiene promedio m y varianza v: es "un jugador típico de la liga".
 * Solo usa temporadas anteriores a t: no ve el futuro.
 *
 * Capa 3 (epoca = 1): antes de promediar, cada tasa se traduce al ambiente de la liga en t,
 *   r* = expit(logit r − logit λ_s + logit λ_t),
 * para que la liga del prior y el jugador estén en el mismo ambiente de jonrones.
 */
#include <math.h>
#include "aod.h"

Prior prior_estimate(const Dataset *ds, int t, int years, int min_pa, int epoca)
{
    Prior pr = {0};
    double sum_r = 0, sum_r2 = 0, sum_inv = 0;

    for (int i = 0; i < ds->ns; i++) {
        const SeasonRow *s = &ds->s[i];
        if (s->season < t - years || s->season > t - 1 || s->pa < min_pa) continue;
        double r = (double)s->hr / s->pa;
        if (epoca && s->hr > 0) {                           /* capa 3: al ambiente de t */
            const double lam_s = data_lambda(ds, s->season, s->league), lam_t = ambiente(ds, t, s->league);
            if (!isnan(lam_s) && !isnan(lam_t)) r = expit(logit(r) - logit(lam_s) + logit(lam_t));
        }
        sum_r += r; sum_r2 += r * r; sum_inv += 1.0 / s->pa;
        pr.J++;
    }
    if (pr.J < 2) return pr;                                /* sin datos de la liga */

    pr.m     = sum_r / pr.J;
    pr.s2    = sum_r2 / pr.J - pr.m * pr.m;                 /* varianza de las tasas */
    pr.noise = pr.m * (1.0 - pr.m) * (sum_inv / pr.J);      /* la parte que es suerte */
    pr.v     = pr.s2 - pr.noise;                            /* la parte que es talento */
    if (pr.v <= 0) pr.v = 1e-9;                             /* si todo fuera suerte: prior muy fuerte */
    pr.n0 = pr.m * (1.0 - pr.m) / pr.v - 1.0;
    pr.a0 = pr.m * pr.n0;
    pr.b0 = (1.0 - pr.m) * pr.n0;
    return pr;
}
