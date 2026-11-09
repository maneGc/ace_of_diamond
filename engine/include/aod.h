/*
 * aod.h — Ace of Diamond: declaraciones del motor en C.
 *
 * Cada módulo implementa ecuaciones de la lección del modelo (docs):
 *   rng.c        generador aleatorio y sorteos (uniforme, normal, Gamma, Beta)   Ec. 22
 *   data.c       lectura de los CSV de data/inputs/
 *   prior.c      prior de la liga por el método de momentos                     Ec. 8–9
 *   aging.c      curva de edad (recta ajustada a los cambios anuales)           Ec. 14
 *   talent.c     posterior del jugador: pesos, era, shrinkage, edad             Ec. 10–14
 *   montecarlo.c simulación de temporadas con OpenMP                            Ec. 22
 *   stats.c      media, percentiles, p-value y error estándar                   Ec. 23–24
 */
#ifndef AOD_H
#define AOD_H

#include <stdint.h>

/* ---------------------------------------------------------------- datos */
typedef struct { char id[12]; int season; int pa; int hr; char league; } SeasonRow;
typedef struct { int season; char league; long pa; long hr; } LeagueRow;
typedef struct { char id[12]; char first[48]; char last[48]; int by, bm, bd; } Player;

typedef struct {
    SeasonRow *s; int ns;   /* temporadas de bateadores (ordenadas por id, temporada) */
    LeagueRow *l; int nl;   /* totales de cada liga */
    Player    *p; int np;   /* jugadores */
} Dataset;

int  data_load(Dataset *ds, const char *dir);          /* 0 = ok */
void data_free(Dataset *ds);
const Player    *data_find_player(const Dataset *ds, const char *query); /* id o "Nombre Apellido" */
const SeasonRow *data_season(const Dataset *ds, const char *id, int season);
double           data_lambda(const Dataset *ds, int season, char league); /* λ = HR/PA de la liga */
int              age_at(const Player *p, int season);  /* edad al 30 de junio */

/* ---------------------------------------------------------------- utilidades */
double logit(double p);
double expit(double z);

/* ---------------------------------------------------------------- generador aleatorio */
typedef struct { uint64_t s[4]; } Rng;
void   rng_seed(Rng *r, uint64_t seed, uint64_t stream);
double rng_uniform(Rng *r);                 /* U(0,1) */
double rng_normal(Rng *r);                  /* N(0,1) */
double rng_gamma(Rng *r, double shape);     /* Gamma(shape, 1) */

/* ---------------------------------------------------------------- nivel 1: prior de la liga */
typedef struct {
    int    J;          /* bateadores-temporada usados */
    double m;          /* tasa media de HR */
    double s2;         /* varianza observada */
    double noise;      /* ruido binomial m(1-m)·mean(1/PA) */
    double v;          /* varianza del talento real */
    double n0, a0, b0; /* prior Beta(a0, b0), n0 = a0 + b0 */
} Prior;
Prior prior_estimate(const Dataset *ds, int t, int years, int min_pa);

/* ---------------------------------------------------------------- nivel 1: curva de edad */
typedef struct { int npairs; double b0, b1; } Aging;   /* δ(e) = b0 + b1·e */
Aging  aging_fit(const Dataset *ds, int last_season, int min_pa);
double aging_curve(const Aging *ag, int e);             /* a(e), con a(27) = 0 */

/* ---------------------------------------------------------------- nivel 1: talento del jugador */
typedef struct { int use_era; int use_age; } TalentOpts;
typedef struct {
    int    nseasons;
    int    season[3], pa[3], hr[3];
    double w[3], lambda_s[3], hr_star[3];
    double lambda_t;
    double hr_w, n, r;        /* HR* ponderados, PA ponderadas, tasa r */
    double alpha, beta;       /* posterior Beta(α, β) */
    double mean_tilde;        /* E[θ̃] = α/(α+β) */
    double w_player;          /* n/(n+n0): peso del dato del jugador */
    int    age_t;             /* edad en la temporada t */
    double delta;             /* Δ_edad (logit) */
    double theta;             /* expit(logit E[θ̃] + Δ) */
} Talent;
int talent_estimate(const Dataset *ds, const Prior *pr, const Aging *ag, const Player *pl,
                    int t, TalentOpts opt, Talent *out);   /* 0 = ok */

/* ---------------------------------------------------------------- Monte Carlo */
enum { MODEL_V1 = 1, MODEL_V2 = 2 };
typedef struct {
    int      model;        /* v1: θ fijo; v2: θ ~ Beta en cada iteración */
    double   theta_fixed;  /* v1 */
    double   alpha, beta;  /* v2 */
    double   delta;        /* v2: Δ_edad en logit */
    int      n_pa;         /* turnos de la temporada */
    const double *kfac;    /* e^{c_j} por turno (NULL = contexto neutro, etapa 3) */
    long     n_iter;       /* N temporadas simuladas */
    uint64_t seed;
} SimInput;
#define HIST_MAX 200
void mc_run(const SimInput *in, long hist[HIST_MAX + 1]);

/* ---------------------------------------------------------------- estadísticos */
typedef struct {
    long   n;
    double mean, sd;
    int    p10, p50, p90;
    long   exceed;        /* simulaciones con HR >= observados */
    double pvalue, se;    /* exceed/N y su error estándar */
} Summary;
Summary stats_summary(const long hist[HIST_MAX + 1], int observed);

#endif
