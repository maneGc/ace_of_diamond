/*
 * main.c — Programa de línea de comandos del motor Ace of Diamond.
 *
 *   aod analizar --jugador "Barry Bonds" --temporada 2001 [opciones]
 *   aod casos    [--n N]          (tabla v1 vs v2 de las temporadas famosas)
 *
 * Opciones:
 *   --modelo v1|v2     v1 = talento fijo (Binomial); v2 = talento bayesiano (por defecto)
 *   --n N              temporadas simuladas (por defecto 1 000 000)
 *   --semilla S        semilla del generador (por defecto 2001)
 *   --sin-era          desactiva el ajuste por era
 *   --sin-edad         desactiva el ajuste por edad
 *   --datos DIR        carpeta con los CSV (por defecto data/inputs)
 *   --json             salida en JSON (para la web)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "aod.h"
#ifdef _OPENMP
#include <omp.h>
#endif

typedef struct {
    const char *player, *data_dir;
    int season, model, use_era, use_age, json;
    long n;
    uint64_t seed;
} Args;

typedef struct { Prior prior; Aging aging; Talent talent; Summary sum; int observed, n_pa; double secs; } Result;

static double now(void)
{
#ifdef _OPENMP
    return omp_get_wtime();
#else
    return (double)clock() / CLOCKS_PER_SEC;
#endif
}

/* Ejecuta el modelo completo para un jugador y una temporada. 0 = ok */
static int run(const Dataset *ds, const Player *pl, const Args *a, Result *res)
{
    memset(res, 0, sizeof *res);
    const SeasonRow *cur = data_season(ds, pl->id, a->season);
    if (!cur) { fprintf(stderr, "%s %s no tiene turnos en %d.\n", pl->first, pl->last, a->season); return 1; }
    res->observed = cur->hr;
    res->n_pa = cur->pa;

    res->prior = prior_estimate(ds, a->season, 3, 300);           /* liga: t−3 … t−1, ≥ 300 PA */
    res->aging = aging_fit(ds, a->season - 1, 200);               /* pares hasta t−1, ≥ 200 PA */
    TalentOpts opt = { a->model == MODEL_V2 && a->use_era, a->model == MODEL_V2 && a->use_age };
    talent_estimate(ds, &res->prior, &res->aging, pl, a->season, opt, &res->talent);
    if (res->talent.nseasons == 0) { fprintf(stderr, "Sin temporadas previas para %s %s.\n", pl->first, pl->last); return 1; }

    SimInput in = {0};
    in.model = a->model;
    in.theta_fixed = res->talent.r;                 /* v1: tasa ponderada, sin incertidumbre */
    in.alpha = res->talent.alpha; in.beta = res->talent.beta; in.delta = res->talent.delta;
    in.n_pa = cur->pa; in.kfac = NULL;              /* contexto neutro hasta la etapa 3 */
    in.n_iter = a->n; in.seed = a->seed;

    long hist[HIST_MAX + 1];
    const double t0 = now();
    mc_run(&in, hist);
    res->secs = now() - t0;
    res->sum = stats_summary(hist, res->observed);
    return 0;
}

static void print_report(const Player *pl, const Args *a, const Result *r)
{
    const Talent *t = &r->talent;
    printf("\n== %s %s · temporada %d · modelo %s ==\n", pl->first, pl->last, a->season, a->model == MODEL_V1 ? "v1 (talento fijo)" : "v2 (talento bayesiano)");
    printf("\n[Prior de la liga]  %d bateadores con >= 300 PA en %d-%d\n", r->prior.J, a->season - 3, a->season - 1);
    printf("  m = %.5f   s2 = %.3e   ruido = %.3e   v = %.3e   n0 = %.1f   (a0 = %.2f, b0 = %.2f)\n",
           r->prior.m, r->prior.s2, r->prior.noise, r->prior.v, r->prior.n0, r->prior.a0, r->prior.b0);
    printf("\n[Temporadas previas]  era %s\n", (a->model == MODEL_V2 && a->use_era) ? "activada" : "desactivada");
    printf("  temporada   PA   HR   peso   lambda_s   HR*\n");
    for (int i = 0; i < t->nseasons; i++)
        printf("  %8d  %4d  %3d   %.1f    %.5f   %6.2f\n", t->season[i], t->pa[i], t->hr[i], t->w[i], t->lambda_s[i], t->hr_star[i]);
    printf("  lambda_t = %.5f   HR* ponderados = %.2f   n = %.1f   r = %.5f\n", t->lambda_t, t->hr_w, t->n, t->r);
    if (a->model == MODEL_V2) {
        printf("\n[Posterior]  alpha = %.2f   beta = %.2f   E[theta~] = %.5f   peso del jugador = %.1f %%\n",
               t->alpha, t->beta, t->mean_tilde, 100 * t->w_player);
        printf("[Edad]  %d años · curva con %d pares (pico en %.1f años) · Delta = %+.4f %s\n",
               t->age_t, r->aging.npairs, -r->aging.b0 / r->aging.b1, t->delta, a->use_age ? "" : "(desactivada)");
        printf("[Talento]  theta = %.5f\n", t->theta);
    } else {
        printf("\n[Talento fijo]  theta = r = %.5f\n", t->r);
    }
    const Summary *s = &r->sum;
    printf("\n[Monte Carlo]  N = %ld temporadas de %d PA · semilla %llu · %.2f s\n", s->n, r->n_pa, (unsigned long long)a->seed, r->secs);
    printf("  HR esperados = %.2f   desviación = %.2f   mediana = %d   intervalo 80%% = %d - %d\n", s->mean, s->sd, s->p50, s->p10, s->p90);
    printf("  HR reales = %d   simulaciones con HR >= %d: %ld\n", r->observed, r->observed, s->exceed);
    if (s->exceed > 0) printf("  p-value = %.3e  (+/- %.1e)  ~ 1 en %.0f\n\n", s->pvalue, s->se, 1.0 / s->pvalue);
    else printf("  p-value < 1/N = %.1e\n\n", 1.0 / s->n);
}

static void print_json(const Player *pl, const Args *a, const Result *r)
{
    const Talent *t = &r->talent; const Summary *s = &r->sum;
    printf("{\"player_id\":\"%s\",\"name\":\"%s %s\",\"season\":%d,\"model\":\"v%d\",\"use_era\":%d,\"use_age\":%d,",
           pl->id, pl->first, pl->last, a->season, a->model, a->use_era, a->use_age);
    printf("\"prior\":{\"J\":%d,\"m\":%.6f,\"v\":%.6e,\"n0\":%.3f,\"a0\":%.4f,\"b0\":%.4f},", r->prior.J, r->prior.m, r->prior.v, r->prior.n0, r->prior.a0, r->prior.b0);
    printf("\"talent\":{\"r\":%.6f,\"n\":%.2f,\"alpha\":%.4f,\"beta\":%.4f,\"mean_tilde\":%.6f,\"w_player\":%.4f,\"age\":%d,\"delta\":%.5f,\"theta\":%.6f},",
           t->r, t->n, t->alpha, t->beta, t->mean_tilde, t->w_player, t->age_t, t->delta, t->theta);
    printf("\"pa\":%d,\"observed\":%d,\"n\":%ld,\"seed\":%llu,\"mean\":%.4f,\"sd\":%.4f,\"p10\":%d,\"p50\":%d,\"p90\":%d,\"exceed\":%ld,\"pvalue\":%.6e,\"se\":%.3e,\"secs\":%.3f}\n",
           r->n_pa, r->observed, s->n, (unsigned long long)a->seed, s->mean, s->sd, s->p10, s->p50, s->p90, s->exceed, s->pvalue, s->se, r->secs);
}

static void usage(void)
{
    fprintf(stderr, "Uso:\n  aod analizar --jugador \"Barry Bonds\" --temporada 2001 [--modelo v1|v2] [--n N] [--semilla S] [--sin-era] [--sin-edad] [--json]\n"
                    "  aod casos [--n N]\n");
}

static const char *find_data_dir(const char *given)
{
    if (given) return given;
    static const char *cands[] = {"data/inputs", "../data/inputs"};
    for (int i = 0; i < 2; i++) {
        char p[256]; snprintf(p, sizeof p, "%s/players.csv", cands[i]);
        FILE *f = fopen(p, "r");
        if (f) { fclose(f); return cands[i]; }
    }
    return "data/inputs";
}

int main(int argc, char **argv)
{
    if (argc < 2) { usage(); return 1; }
    const char *cmd = argv[1];
    Args a = {0};
    a.model = MODEL_V2; a.use_era = 1; a.use_age = 1; a.n = 1000000; a.seed = 2001;
    for (int i = 2; i < argc; i++) {
        if      (!strcmp(argv[i], "--jugador")   && i + 1 < argc) a.player = argv[++i];
        else if (!strcmp(argv[i], "--temporada") && i + 1 < argc) a.season = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--modelo")    && i + 1 < argc) a.model = (argv[++i][1] == '1') ? MODEL_V1 : MODEL_V2;
        else if (!strcmp(argv[i], "--n")         && i + 1 < argc) a.n = atol(argv[++i]);
        else if (!strcmp(argv[i], "--semilla")   && i + 1 < argc) a.seed = strtoull(argv[++i], NULL, 10);
        else if (!strcmp(argv[i], "--datos")     && i + 1 < argc) a.data_dir = argv[++i];
        else if (!strcmp(argv[i], "--sin-era"))  a.use_era = 0;
        else if (!strcmp(argv[i], "--sin-edad")) a.use_age = 0;
        else if (!strcmp(argv[i], "--json"))     a.json = 1;
        else { fprintf(stderr, "Opción desconocida: %s\n", argv[i]); usage(); return 1; }
    }

    Dataset ds;
    if (data_load(&ds, find_data_dir(a.data_dir))) {
        fprintf(stderr, "Genera los datos con:  python etl/run_all.py  y  python etl/export_inputs.py\n");
        return 1;
    }
    int rc = 0;

    if (!strcmp(cmd, "analizar")) {
        if (!a.player || !a.season) { usage(); rc = 1; goto end; }
        const Player *pl = data_find_player(&ds, a.player);
        if (!pl) { fprintf(stderr, "Jugador no encontrado: %s\n", a.player); rc = 1; goto end; }
        Result r;
        if ((rc = run(&ds, pl, &a, &r))) goto end;
        if (a.json) print_json(pl, &a, &r); else print_report(pl, &a, &r);
    } else if (!strcmp(cmd, "casos")) {
        static const struct { const char *name; int season; } C[] = {
            {"Barry Bonds", 2001}, {"Mark McGwire", 1998}, {"Sammy Sosa", 1998},
            {"Aaron Judge", 2022}, {"Shohei Ohtani", 2024}};
        printf("\n%-15s %4s %4s | %-6s %7s %9s %11s | %-6s %7s %9s %11s\n", "Jugador", "Año", "HR",
               "v1", "media", "IC 80%", "p-value", "v2", "media", "IC 80%", "p-value");
        for (int i = 0; i < 5; i++) {
            const Player *pl = data_find_player(&ds, C[i].name);
            Result r1, r2; Args a1 = a, a2 = a;
            a1.season = a2.season = C[i].season; a1.model = MODEL_V1; a2.model = MODEL_V2;
            if (!pl || run(&ds, pl, &a1, &r1) || run(&ds, pl, &a2, &r2)) continue;
            char i1[16], i2[16], p1[24], p2[24];
            snprintf(i1, sizeof i1, "%d-%d", r1.sum.p10, r1.sum.p90);
            snprintf(i2, sizeof i2, "%d-%d", r2.sum.p10, r2.sum.p90);
            if (r1.sum.exceed) snprintf(p1, sizeof p1, "%.2e", r1.sum.pvalue); else snprintf(p1, sizeof p1, "<%.0e", 1.0 / r1.sum.n);
            if (r2.sum.exceed) snprintf(p2, sizeof p2, "%.2e", r2.sum.pvalue); else snprintf(p2, sizeof p2, "<%.0e", 1.0 / r2.sum.n);
            printf("%-15s %4d %4d | %-6s %7.1f %9s %11s | %-6s %7.1f %9s %11s\n", C[i].name, C[i].season, r1.observed,
                   "", r1.sum.mean, i1, p1, "", r2.sum.mean, i2, p2);
        }
        printf("\nN = %ld simulaciones por caso · v1 = talento fijo · v2 = talento bayesiano (era + edad)\n\n", a.n);
    } else { usage(); rc = 1; }
end:
    data_free(&ds);
    return rc;
}
