/*
 * prior.c — Prior de la liga por el método de momentos (Bayes empírico).
 *
 * Lección, Ec. 8 y 9:
 *   s² ≈ v + m(1−m)·mean(1/PA)       (ley de la varianza total: talento + suerte)
 *   v  = s² − m(1−m)·mean(1/PA)      (varianza del talento real)
 *   n0 = m(1−m)/v − 1,  α0 = m·n0,  β0 = (1−m)·n0
 *
 * Solo usa temporadas ANTERIORES a t (de t−years a t−1): sin fuga de datos.
 */
#include <math.h>
#include "aod.h"

Prior prior_estimate(const Dataset *ds, int t, int years, int min_pa)
{
    Prior pr = {0};
    double sum_r = 0, sum_r2 = 0, sum_inv = 0;

    for (int i = 0; i < ds->ns; i++) {
        const SeasonRow *s = &ds->s[i];
        if (s->season < t - years || s->season > t - 1 || s->pa < min_pa) continue;
        const double r = (double)s->hr / s->pa;
        sum_r += r; sum_r2 += r * r; sum_inv += 1.0 / s->pa;
        pr.J++;
    }
    if (pr.J < 2) return pr;

    pr.m     = sum_r / pr.J;
    pr.s2    = sum_r2 / pr.J - pr.m * pr.m;                 /* varianza poblacional */
    pr.noise = pr.m * (1.0 - pr.m) * (sum_inv / pr.J);
    pr.v     = pr.s2 - pr.noise;
    if (pr.v <= 0) pr.v = 1e-9;                             /* si todo fuera ruido: prior muy fuerte */
    pr.n0 = pr.m * (1.0 - pr.m) / pr.v - 1.0;
    pr.a0 = pr.m * pr.n0;
    pr.b0 = (1.0 - pr.m) * pr.n0;
    return pr;
}
