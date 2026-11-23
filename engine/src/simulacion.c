/*
 * simulacion.c — Temporadas simuladas con ensayos de Bernoulli.
 *
 *   Y_j ~ Bernoulli(θ)  para cada turno j = 1 … PA      (1 = jonrón, 0 = no)
 *   HR  = Y_1 + Y_2 + … + Y_PA
 *
 * Un ensayo de Bernoulli se hace con un número aleatorio u ~ U(0,1):
 * es jonrón si u < θ, que pasa exactamente con probabilidad θ.
 * Se repite la temporada n_sim veces y se cuenta cuántas terminaron con 0, 1, 2, … HR.
 *
 * Capa 2: el talento no se conoce exacto, θ ~ Beta(α, β). En cada temporada simulada se
 * sortea primero un θ con X ~ Gamma(α), Y ~ Gamma(β), θ = X / (X + Y), y luego se juegan
 * los turnos con ese θ. Así cada temporada lleva dos azares: el del talento y el de los turnos.
 */
#include <string.h>
#include "aod.h"

void simular_temporadas(double theta, int n_pa, long n_sim, uint64_t semilla, long hist[HIST_MAX + 1])
{
    memset(hist, 0, (HIST_MAX + 1) * sizeof(long));
    Rng r;
    rng_seed(&r, semilla);
    for (long i = 0; i < n_sim; i++) {                 /* una temporada */
        int hr = 0;
        for (int j = 0; j < n_pa; j++)                 /* un turno al bate */
            if (rng_uniform(&r) < theta) hr++;
        hist[hr < HIST_MAX ? hr : HIST_MAX]++;
    }
}

void simular_temporadas_beta(double alpha, double beta, int n_pa, long n_sim, uint64_t semilla, long hist[HIST_MAX + 1])
{
    memset(hist, 0, (HIST_MAX + 1) * sizeof(long));
    Rng r;
    rng_seed(&r, semilla);
    for (long i = 0; i < n_sim; i++) {                 /* una temporada */
        const double x = rng_gamma(&r, alpha), y = rng_gamma(&r, beta);
        const double theta = x / (x + y);              /* el talento de esta temporada */
        int hr = 0;
        for (int j = 0; j < n_pa; j++)                 /* un turno al bate */
            if (rng_uniform(&r) < theta) hr++;
        hist[hr < HIST_MAX ? hr : HIST_MAX]++;
    }
}

void simular_talento(const Talent *tl, int n_pa, long n_sim, uint64_t semilla, long hist[HIST_MAX + 1])
{
    if (tl->alpha > 0) simular_temporadas_beta(tl->alpha, tl->beta, n_pa, n_sim, semilla, hist);
    else               simular_temporadas(tl->theta, n_pa, n_sim, semilla, hist);
}
