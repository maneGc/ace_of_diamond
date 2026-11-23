/*
 * aod.h — Ace of Diamond: declaraciones del motor en C.
 *
 * El talento del jugador sale solo de sus propios datos, por capas:
 *   capa 0 (tasa cruda): θ = Σ HR / Σ PA de sus 3 temporadas anteriores
 *   capa 1 (pesos):      θ = Σ w·HR / Σ w·PA, con w = 1, 0.8, 0.6 para t−1, t−2, t−3
 *   capa 2 (Beta):       θ ~ Beta(α, β), con α = α0 + Σ w·HR y β = β0 + Σ w·(PA − HR):
 *                        el jugador se mezcla con la liga y su talento ya no es un número fijo
 *   capa 3 (época):      antes de sumar, cada temporada pasada se traduce al ambiente de
 *                        jonrones de la liga en t: HR* = PA·expit(logit(HR/PA) − logit λ_s + logit λ_t)
 *
 *   data.c        lectura de los CSV de data/inputs/ y el ambiente λ de cada liga
 *   prior.c       la liga: Beta(α0, β0) por el método de momentos (capa 2)
 *   talent.c      talento del jugador (capa 0, 1 o 2)
 *   rng.c         números aleatorios: uniforme, normal y Gamma
 *   simulacion.c  temporadas simuladas: cada turno es un ensayo de Bernoulli(θ)
 */
#ifndef AOD_H
#define AOD_H

#include <stdint.h>

/* ---------------------------------------------------------------- datos */
typedef struct { char id[12]; int season; int pa; int hr; char league; } SeasonRow;
typedef struct { char id[12]; char first[48]; char last[48]; } Player;
typedef struct { int season; char league; long pa; long hr; } LeagueRow;

typedef struct {
    SeasonRow *s; int ns;   /* temporadas de bateadores (ordenadas por id, temporada) */
    Player    *p; int np;   /* jugadores */
    LeagueRow *l; int nl;   /* totales de cada liga por temporada (capa 3) */
} Dataset;

int  data_load(Dataset *ds, const char *dir);          /* 0 = ok */
void data_free(Dataset *ds);
const Player    *data_find_player(const Dataset *ds, const char *query); /* id o "Nombre Apellido" */
const SeasonRow *data_season(const Dataset *ds, const char *id, int season);
double           data_lambda(const Dataset *ds, int season, char league);  /* λ = HR/PA de la liga; NAN si no hay */
/* El ambiente λ de la liga en la temporada t que se quiere predecir (capa 3):
 *   modo análisis (por defecto): el λ real de t, si t ya tiene datos
 *   modo predicción: el λ de la temporada anterior, como si t todavía no empezara
 * Si t no tiene datos (una temporada futura), en los dos modos se usa el más reciente. */
double ambiente(const Dataset *ds, int t, char league);
void   ambiente_modo_prediccion(int activo);           /* 1 = predicción, 0 = análisis */

/* ---------------------------------------------------------------- utilidades */
double logit(double p);                                /* ln(p / (1 − p)) */
double expit(double z);                                /* 1 / (1 + e^(−z)) */

/* ---------------------------------------------------------------- la liga (capa 2) */
typedef struct {
    int    J;                          /* temporadas de regulares usadas */
    double m, s2, noise, v;            /* promedio, varianza, parte de suerte, parte de talento */
    double n0, a0, b0;                 /* Beta(α0, β0) de la liga, n0 = α0 + β0 */
} Prior;
/* epoca = 1: cada tasa se traduce al ambiente de t antes de promediar (capa 3) */
Prior prior_estimate(const Dataset *ds, int t, int years, int min_pa, int epoca);

/* ---------------------------------------------------------------- talento del jugador */
#define CAPA_ACTUAL 3                  /* la capa más nueva de esta rama */
typedef struct {
    int    nseasons;                   /* temporadas anteriores con turnos (0 a 3) */
    int    season[3], pa[3], hr[3];    /* t−1, t−2, t−3 (las que existan) */
    double w[3];                       /* peso de cada temporada */
    int    pa_sum, hr_sum;             /* Σ PA y Σ HR (sin pesos) */
    double lambda_s[3], hr_star[3];    /* capa 3: λ de su liga en cada temporada y HR traducidos a t */
    double lambda_t;                   /* capa 3: λ de su liga en t (ver ambiente) */
    double pa_w, hr_w;                 /* Σ w·PA y Σ w·HR (en la capa 3, Σ w·HR*) */
    Prior  liga;                       /* capa 2: la liga en t−3 … t−1 */
    double alpha, beta;                /* capa 2: θ ~ Beta(α, β); en capas 0 y 1 valen 0 */
    double peso_jugador;               /* capa 2: Σ w·PA / (Σ w·PA + n0) */
    double theta;                      /* capas 0 y 1: Σ w·HR / Σ w·PA · capa 2: α / (α + β) */
} Talent;
/* capa = 0: todas las temporadas pesan 1 · capa = 1: pesos 1, 0.8, 0.6 · capa = 2: pesos + Beta
 * capa = 3: pesos + Beta + época */
int talent_estimate(const Dataset *ds, const Player *pl, int t, int capa, Talent *out);   /* 0 = ok */

/* ---------------------------------------------------------------- números aleatorios */
typedef struct { uint64_t s[4]; } Rng;                 /* estado del generador xoshiro256** */
void   rng_seed(Rng *r, uint64_t seed);
double rng_uniform(Rng *r);                            /* U(0,1) */
double rng_normal(Rng *r);                             /* N(0,1) */
double rng_gamma(Rng *r, double k);                    /* Gamma(k, 1) */

/* ---------------------------------------------------------------- simulación */
#define HIST_MAX 200                                   /* HR máximos que se cuentan por temporada */
/* Simula n_sim temporadas de n_pa turnos. En cada turno es HR si u < θ, con u ~ U(0,1).
 * hist[k] = cuántas temporadas simuladas terminaron con k HR. */
void simular_temporadas(double theta, int n_pa, long n_sim, uint64_t semilla, long hist[HIST_MAX + 1]);
/* Igual, pero en cada temporada se sortea primero un talento θ ~ Beta(α, β) (capa 2). */
void simular_temporadas_beta(double alpha, double beta, int n_pa, long n_sim, uint64_t semilla, long hist[HIST_MAX + 1]);
/* Elige según la capa del talento: Beta si tl->alpha > 0, θ fijo si no. */
void simular_talento(const Talent *tl, int n_pa, long n_sim, uint64_t semilla, long hist[HIST_MAX + 1]);

#endif
