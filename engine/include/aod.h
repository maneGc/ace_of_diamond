/*
 * aod.h — Ace of Diamond: declaraciones del motor en C.
 *
 * Capa 0: tasa cruda. El talento del jugador sale solo de sus propios datos.
 *   data.c        lectura de los CSV de data/inputs/
 *   talent.c      talento del jugador: θ = Σ HR / Σ PA de sus 3 temporadas anteriores
 *   rng.c         números aleatorios uniformes U(0,1)
 *   simulacion.c  temporadas simuladas: cada turno es un ensayo de Bernoulli(θ)
 */
#ifndef AOD_H
#define AOD_H

#include <stdint.h>

/* ---------------------------------------------------------------- datos */
typedef struct { char id[12]; int season; int pa; int hr; char league; } SeasonRow;
typedef struct { char id[12]; char first[48]; char last[48]; } Player;

typedef struct {
    SeasonRow *s; int ns;   /* temporadas de bateadores (ordenadas por id, temporada) */
    Player    *p; int np;   /* jugadores */
} Dataset;

int  data_load(Dataset *ds, const char *dir);          /* 0 = ok */
void data_free(Dataset *ds);
const Player    *data_find_player(const Dataset *ds, const char *query); /* id o "Nombre Apellido" */
const SeasonRow *data_season(const Dataset *ds, const char *id, int season);

/* ---------------------------------------------------------------- talento del jugador */
typedef struct {
    int    nseasons;                   /* temporadas anteriores con turnos (0 a 3) */
    int    season[3], pa[3], hr[3];    /* t−1, t−2, t−3 (las que existan) */
    int    pa_sum, hr_sum;             /* Σ PA y Σ HR */
    double theta;                      /* θ = Σ HR / Σ PA */
} Talent;
int talent_estimate(const Dataset *ds, const Player *pl, int t, Talent *out);   /* 0 = ok */

/* ---------------------------------------------------------------- números aleatorios */
typedef struct { uint64_t s[4]; } Rng;                 /* estado del generador xoshiro256** */
void   rng_seed(Rng *r, uint64_t seed);
double rng_uniform(Rng *r);                            /* U(0,1) */

/* ---------------------------------------------------------------- simulación */
#define HIST_MAX 200                                   /* HR máximos que se cuentan por temporada */
/* Simula n_sim temporadas de n_pa turnos. En cada turno es HR si u < θ, con u ~ U(0,1).
 * hist[k] = cuántas temporadas simuladas terminaron con k HR. */
void simular_temporadas(double theta, int n_pa, long n_sim, uint64_t semilla, long hist[HIST_MAX + 1]);

#endif
