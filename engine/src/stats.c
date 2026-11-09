/*
 * stats.c — Lo que se calcula con las N temporadas simuladas (lección, Ec. 23–24).
 *
 *   P̂(HR ≥ x) = (1/N) Σ 1[HR(i) ≥ x]     p-value = P̂(HR ≥ HR_obs)
 *   SE(P̂) = sqrt( P̂(1 − P̂) / N )
 *   Percentil q: el primer k donde el histograma acumulado alcanza q·N.
 * Todo sale del histograma: no hace falta guardar cada temporada.
 */
#include <math.h>
#include "aod.h"

static int percentile(const long hist[HIST_MAX + 1], long n, double q)
{
    long acc = 0;
    for (int k = 0; k <= HIST_MAX; k++) {
        acc += hist[k];
        if (acc >= q * n) return k;
    }
    return HIST_MAX;
}

Summary stats_summary(const long hist[HIST_MAX + 1], int observed)
{
    Summary s = {0};
    double sum = 0, sum2 = 0;
    for (int k = 0; k <= HIST_MAX; k++) {
        s.n += hist[k];
        sum += (double)k * hist[k];
        sum2 += (double)k * k * hist[k];
        if (observed >= 0 && k >= observed) s.exceed += hist[k];
    }
    if (s.n == 0) return s;
    s.mean = sum / s.n;
    s.sd = sqrt(sum2 / s.n - s.mean * s.mean);
    s.p10 = percentile(hist, s.n, 0.10);
    s.p50 = percentile(hist, s.n, 0.50);
    s.p90 = percentile(hist, s.n, 0.90);
    s.pvalue = (double)s.exceed / s.n;
    s.se = sqrt(s.pvalue * (1.0 - s.pvalue) / s.n);
    return s;
}
