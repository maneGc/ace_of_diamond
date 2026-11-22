/*
 * comparar.c — Capa 0 contra la realidad: predice cada temporada de cada jugador regular
 * usando solo sus 3 temporadas anteriores y la compara con los HR que de verdad pegó.
 *
 * Para cada temporada (1998–2025, sin 2020, con al menos 300 PA):
 *   1. θ = Σ HR / Σ PA de sus 3 temporadas anteriores      (talent.c)
 *   2. N_SIM temporadas simuladas con Bernoulli(θ)          (simulacion.c)
 *   3. promedio y rango del 80 % (percentiles 10 y 90) → una fila del CSV
 *
 * Compilar:  gcc -std=c11 -O2 -Iengine/include engine/src/data.c engine/src/talent.c engine/src/rng.c engine/src/simulacion.c engine/src/capa0/comparar.c -o comparar
 * Ejecutar:  ./comparar
 */
#include <stdio.h>
#include <string.h>
#include "aod.h"
#ifdef _WIN32
#include <windows.h>
#endif

#define N_SIM  2000
#define DESDE  1998
#define HASTA  2025
#define MIN_PA 300

/* Primer k donde las temporadas acumuladas alcanzan la proporción q. */
static int percentil(const long hist[HIST_MAX + 1], double q)
{
    long acc = 0;
    for (int k = 0; k <= HIST_MAX; k++) {
        acc += hist[k];
        if (acc >= q * N_SIM) return k;
    }
    return HIST_MAX;
}

int main(void)
{
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif
    Dataset ds;
    if (data_load(&ds, "data/inputs")) return 1;
    const char *ruta = "experimentos/capa0/comparacion.csv";
    FILE *f = fopen(ruta, "w");
    if (!f) { fprintf(stderr, "No pude escribir %s\n", ruta); return 1; }
    fprintf(f, "jugador,temporada,pa,real,theta,media,p10,p90\n");

    int n = 0;
    for (int i = 0; i < ds.ns; i++) {
        const SeasonRow *s = &ds.s[i];
        if (s->season < DESDE || s->season > HASTA || s->season == 2020 || s->pa < MIN_PA) continue;
        const Player *pl = data_find_player(&ds, s->id);
        Talent tl;
        if (!pl || talent_estimate(&ds, pl, s->season, &tl)) continue;      /* sin temporadas anteriores */

        long hist[HIST_MAX + 1];
        simular_temporadas(tl.theta, s->pa, N_SIM, 2001 + i, hist);        /* otra semilla por temporada */
        double suma = 0;
        for (int k = 0; k <= HIST_MAX; k++) suma += (double)k * hist[k];

        fprintf(f, "%s %s,%d,%d,%d,%.6f,%.2f,%d,%d\n", pl->first, pl->last, s->season, s->pa, s->hr,
                tl.theta, suma / N_SIM, percentil(hist, 0.10), percentil(hist, 0.90));
        if (++n % 500 == 0) fprintf(stderr, "\r%d temporadas...", n);
    }
    fclose(f);
    printf("\r%d temporadas comparadas (%d–%d, sin 2020, >= %d PA, %d simulaciones cada una) -> %s\n",
           n, DESDE, HASTA, MIN_PA, N_SIM, ruta);
    data_free(&ds);
    return 0;
}
