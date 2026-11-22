/*
 * simular.c — Calcula θ del jugador con una capa, simula sus temporadas y guarda un CSV para Python.
 *
 * Compilar:  gcc -std=c11 -O2 -Iengine/include engine/src/data.c engine/src/talent.c engine/src/rng.c engine/src/simulacion.c engine/src/experimentos/simular.c -o simular
 * Ejecutar:  ./simular "Barry Bonds" 2001        (capa más nueva)
 *            ./simular "Barry Bonds" 2001 0      (capa 0)
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
    if (argc < 3) { fprintf(stderr, "Uso: simular \"Nombre Apellido\" temporada [capa]\n"); return 1; }
    const int t = atoi(argv[2]);
    const int capa = argc > 3 ? atoi(argv[3]) : CAPA_ACTUAL;

    Dataset ds;
    if (data_load(&ds, "data/inputs")) return 1;
    const Player *pl = data_find_player(&ds, argv[1]);
    Talent tl;
    if (!pl || talent_estimate(&ds, pl, t, capa, &tl)) {
        fprintf(stderr, "No hay datos de %s entre %d y %d\n", argv[1], t - 3, t - 1);
        return 1;
    }
    const SeasonRow *cur = data_season(&ds, pl->id, t);
    const int pa = cur ? cur->pa : 600;                  /* turnos de esa temporada; si no jugó, 600 */
    const int real = cur ? cur->hr : -1;                 /* HR que de verdad pegó; -1 si no hay datos */

    long hist[HIST_MAX + 1];
    simular_temporadas(tl.theta, pa, N_SIM, 2001, hist);

    char ruta[128];
    snprintf(ruta, sizeof ruta, "experimentos/capa%d/%s_%d.csv", capa, pl->id, t);
    FILE *f = fopen(ruta, "w");
    if (!f) { fprintf(stderr, "No pude escribir %s\n", ruta); return 1; }
    fprintf(f, "capa,jugador,temporada,theta,pa,real,hr,temporadas\n");
    for (int k = 0; k <= HIST_MAX; k++)
        if (hist[k]) fprintf(f, "%d,%s %s,%d,%.6f,%d,%d,%d,%ld\n", capa, pl->first, pl->last, t, tl.theta, pa, real, k, hist[k]);
    fclose(f);

    printf("Capa %d · %s %s %d: θ = %.2f %%, %d turnos, %d temporadas simuladas -> %s\n",
           capa, pl->first, pl->last, t, 100 * tl.theta, pa, N_SIM, ruta);
    data_free(&ds);
    return 0;
}
