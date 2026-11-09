/*
 * montecarlo.c — Simulación de temporadas (lección, Ec. 22).
 *
 * Cada iteración es una temporada posible:
 *   v1: odds_θ = θ/(1−θ) fijo.
 *   v2: X ~ Gamma(α), Y ~ Gamma(β)  ⇒  θ̃ = X/(X+Y) ~ Beta(α, β), y sus odds son X/Y;
 *       odds_θ = (X/Y) · e^{Δ_edad}.
 *   Para cada turno j:  q = odds_θ · e^{c_j}  y es HR si  u·(1+q) < q   (⇔ u < q/(1+q) = p_j).
 *   Se suma 1 en hist[HR].
 *
 * Reproducibilidad: las N iteraciones se dividen en bloques fijos de BLOCK;
 * el bloque b usa el stream (semilla, b). El resultado no depende del número de hilos.
 */
#include <math.h>
#include <string.h>
#include "aod.h"
#ifdef _OPENMP
#include <omp.h>
#endif

#define BLOCK 4096

void mc_run(const SimInput *in, long hist[HIST_MAX + 1])
{
    memset(hist, 0, (HIST_MAX + 1) * sizeof(long));
    const long nblocks = (in->n_iter + BLOCK - 1) / BLOCK;
    const double odds_fixed = in->theta_fixed / (1.0 - in->theta_fixed);
    const double e_delta = exp(in->delta);

    #pragma omp parallel
    {
        long h[HIST_MAX + 1];
        memset(h, 0, sizeof h);

        #pragma omp for schedule(dynamic, 4)
        for (long b = 0; b < nblocks; b++) {
            Rng r;
            rng_seed(&r, in->seed, (uint64_t)b);
            const long first = b * BLOCK;
            const long last = first + BLOCK < in->n_iter ? first + BLOCK : in->n_iter;
            for (long i = first; i < last; i++) {
                double odds = odds_fixed;
                if (in->model == MODEL_V2) {
                    const double x = rng_gamma(&r, in->alpha), y = rng_gamma(&r, in->beta);
                    odds = (x / y) * e_delta;
                }
                int hr = 0;
                for (int j = 0; j < in->n_pa; j++) {
                    const double q = in->kfac ? odds * in->kfac[j] : odds;
                    if (rng_uniform(&r) * (1.0 + q) < q) hr++;
                }
                h[hr < HIST_MAX ? hr : HIST_MAX]++;
            }
        }
        #pragma omp critical
        for (int k = 0; k <= HIST_MAX; k++) hist[k] += h[k];
    }
}
