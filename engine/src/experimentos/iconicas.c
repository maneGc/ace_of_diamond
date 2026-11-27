/*
 * iconicas.c — ¿Qué tan anómalas fueron las temporadas icónicas de jonrones?
 *
 * Para cada temporada de la lista y cada capa de 0 a CAPA_ACTUAL:
 *   1. θ con sus 3 temporadas anteriores                        (talent.c)
 *   2. N_SIM temporadas simuladas                               (simulacion.c)
 *   3. p-value = proporción de temporadas simuladas con HR >= los que de verdad pegó
 * Escribe dos CSV en experimentos/iconicas/:
 *   resumen.csv      una fila por temporada y capa: θ, promedio, rango del 80 % y p-value
 *   histogramas.csv  cuántas temporadas simuladas terminaron con k HR, por temporada y capa
 *
 * Compilar:  gcc -std=c11 -O2 -Iengine/include engine/src/data.c engine/src/prior.c engine/src/talent.c engine/src/rng.c engine/src/simulacion.c engine/src/experimentos/iconicas.c -lm -o iconicas
 * Ejecutar:  ./iconicas
 */
#include <stdio.h>
#include "aod.h"
#ifdef _WIN32
#include <windows.h>
#endif

#define N_SIM 1000000

static const struct { const char *jugador; int temporada; } ICONICAS[] = {
    {"Barry Bonds", 2001},     {"Mark McGwire", 1998},      {"Sammy Sosa", 1998},     {"Mark McGwire", 1999},
    {"Sammy Sosa", 2001},      {"Sammy Sosa", 1999},        {"Aaron Judge", 2022},    {"Cal Raleigh", 2025},
    {"Giancarlo Stanton", 2017}, {"Ryan Howard", 2006},     {"Aaron Judge", 2024},    {"Luis Gonzalez", 2001},
    {"Alex Rodriguez", 2002},  {"Shohei Ohtani", 2024},     {"bautj002", 2010},          /* hay dos José Bautista: se usa el id */
};
#define N_ICONICAS (int)(sizeof ICONICAS / sizeof ICONICAS[0])

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
    FILE *fr = fopen("experimentos/iconicas/resumen.csv", "w");
    FILE *fh = fopen("experimentos/iconicas/histogramas.csv", "w");
    if (!fr || !fh) { fprintf(stderr, "No pude escribir en experimentos/iconicas/\n"); return 1; }
    fprintf(fr, "capa,jugador,temporada,pa,real,theta,media,p10,p90,pvalue\n");
    fprintf(fh, "capa,jugador,temporada,hr,temporadas\n");

    printf("%-26s %4s   %s\n", "temporada", "HR", "p-value por capa (0 · 1 · 2 · 3 · 4)");
    for (int i = 0; i < N_ICONICAS; i++) {
        const Player *pl = data_find_player(&ds, ICONICAS[i].jugador);
        const SeasonRow *cur = pl ? data_season(&ds, pl->id, ICONICAS[i].temporada) : NULL;
        if (!cur) { fprintf(stderr, "No encontré %s %d\n", ICONICAS[i].jugador, ICONICAS[i].temporada); continue; }
        char nombre[100];
        snprintf(nombre, sizeof nombre, "%s %s", pl->first, pl->last);
        printf("%-21s %d %4d  ", nombre, cur->season, cur->hr);
        for (int capa = 0; capa <= CAPA_ACTUAL; capa++) {
            Talent tl;
            if (talent_estimate(&ds, pl, cur->season, capa, &tl)) continue;
            long hist[HIST_MAX + 1];
            simular_talento(&tl, cur->pa, N_SIM, 2001, hist);
            double suma = 0;
            long mayores = 0;                                   /* temporadas simuladas con HR >= real */
            for (int k = 0; k <= HIST_MAX; k++) {
                suma += (double)k * hist[k];
                if (k >= cur->hr) mayores += hist[k];
                if (hist[k]) fprintf(fh, "%d,%s %s,%d,%d,%ld\n", capa, pl->first, pl->last, cur->season, k, hist[k]);
            }
            const double p = (double)mayores / N_SIM;
            fprintf(fr, "%d,%s %s,%d,%d,%d,%.6f,%.2f,%d,%d,%.3e\n", capa, pl->first, pl->last, cur->season, cur->pa,
                    cur->hr, tl.theta, suma / N_SIM, percentil(hist, 0.10), percentil(hist, 0.90), p);
            printf(" %9.2e", p);
        }
        printf("\n");
    }
    fclose(fr); fclose(fh);
    printf("\n%d temporadas simuladas por capa · escrito en experimentos/iconicas/\n", N_SIM);
    data_free(&ds);
    return 0;
}
