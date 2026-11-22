/*
 * simular.c — Capa 0: calcula θ del jugador, simula sus temporadas y guarda un CSV para Python.
 *
 * Compilar:  gcc -std=c11 -O2 -Iengine/include engine/src/data.c engine/src/talent.c engine/src/rng.c engine/src/simulacion.c engine/src/capa0/simular.c -o simular
 * Ejecutar:  ./simular "Barry Bonds" 2001
 */
#include <stdio.h>
#include <stdlib.h>
#include "aod.h"
#ifdef _WIN32
#include <windows.h>
#endif

#define N_SIM 1000000

int main(int argc, char **argv)
{
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);                         /* para que θ se vea bien en la consola de Windows */
#endif
    if (argc < 3) { fprintf(stderr, "Uso: simular \"Nombre Apellido\" temporada\n"); return 1; }
    const int t = atoi(argv[2]);

    Dataset ds;
    if (data_load(&ds, "data/inputs")) return 1;
    const Player *pl = data_find_player(&ds, argv[1]);
    Talent tl;
    if (!pl || talent_estimate(&ds, pl, t, &tl)) {
        fprintf(stderr, "No hay datos de %s entre %d y %d\n", argv[1], t - 3, t - 1);
        return 1;
    }
    const SeasonRow *cur = data_season(&ds, pl->id, t);
    const int pa = cur ? cur->pa : 600;                  /* turnos de esa temporada; si no jugó, 600 */

    long hist[HIST_MAX + 1];
    simular_temporadas(tl.theta, pa, N_SIM, 2001, hist);

    char ruta[128];
    snprintf(ruta, sizeof ruta, "experimentos/capa0/%s_%d.csv", pl->id, t);
    FILE *f = fopen(ruta, "w");
    if (!f) { fprintf(stderr, "No pude escribir %s\n", ruta); return 1; }
    fprintf(f, "jugador,temporada,theta,pa,hr,temporadas\n");
    for (int k = 0; k <= HIST_MAX; k++)
        if (hist[k]) fprintf(f, "%s %s,%d,%.6f,%d,%d,%ld\n", pl->first, pl->last, t, tl.theta, pa, k, hist[k]);
    fclose(f);

    printf("%s %s %d: θ = %.2f %%, %d turnos, %d temporadas simuladas -> %s\n",
           pl->first, pl->last, t, 100 * tl.theta, pa, N_SIM, ruta);
    data_free(&ds);
    return 0;
}
