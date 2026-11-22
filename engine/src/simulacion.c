/*
 * simulacion.c — Temporadas simuladas con ensayos de Bernoulli.
 *
 *   Y_j ~ Bernoulli(θ)  para cada turno j = 1 … PA      (1 = jonrón, 0 = no)
 *   HR  = Y_1 + Y_2 + … + Y_PA
 *
 * Un ensayo de Bernoulli se hace con un número aleatorio u ~ U(0,1):
 * es jonrón si u < θ, que pasa exactamente con probabilidad θ.
 * Se repite la temporada n_sim veces y se cuenta cuántas terminaron con 0, 1, 2, … HR.
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
