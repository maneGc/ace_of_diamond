/*
 * demo.c — Demo del talento: calcula, paso a paso, el θ de un jugador con la capa más
 * nueva (CAPA_ACTUAL): sus 3 temporadas anteriores con pesos w = 1, 0.8, 0.6 (capa 1),
 * traducidas al ambiente de jonrones de la liga en t (capa 3) y mezcladas con la liga en
 * una Beta(α, β) (capa 2).
 *
 * Usa data.c, prior.c y talent.c.
 *
 * Compilar, desde la carpeta del repositorio:
 *   gcc -std=c11 -O2 -Iengine/include engine/src/data.c engine/src/prior.c engine/src/talent.c engine/src/demo/demo.c -lm -o demo
 *
 * Ejecutar:
 *   ./demo                              pregunta el jugador y la temporada
 *   ./demo "Aaron Judge" 2022
 *   ./demo judge                        sin temporada: usa su temporada con más HR
 *   ./demo bondb001 2001                también acepta el id de Retrosheet
 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "aod.h"
#ifdef _WIN32
#include <wchar.h>
#include <windows.h>
#endif

#define MAX_CAND 20   /* cuántos jugadores se listan cuando el nombre coincide con varios */

static int quiet_exit = 0;   /* 1 = el usuario salió con "q": no hace falta pausar */

#ifdef _WIN32
/* Con doble clic, Windows abre una consola solo para este programa y la cierra
 * en cuanto termina. Si somos el único proceso en la consola, esperamos Enter
 * para que se pueda leer el resultado. Desde una terminal no hace nada. */
static void pause_if_double_clicked(void)
{
    DWORD procs[2];
    if (!quiet_exit && GetConsoleProcessList(procs, 2) == 1) {
        printf("Presiona Enter para cerrar...");
        fflush(stdout);
        getchar();
    }
}
#endif

static const char *find_data_dir(void)
{
    static const char *cands[] = {"data/inputs", "../data/inputs", "../../data/inputs"};
    for (int i = 0; i < 3; i++) {
        char p[256];
        snprintf(p, sizeof p, "%s/players.csv", cands[i]);
        FILE *f = fopen(p, "r");
        if (f) { fclose(f); return cands[i]; }
    }
    return "data/inputs";
}

static void usage(void)
{
    fprintf(stderr, "Uso: demo [\"Nombre Apellido\" | apellido | id] [temporada]\n"
                    "Sin jugador, el demo te lo pregunta.\n"
                    "Ejemplo: demo \"Aaron Judge\" 2022\n");
}

/* ------------------------------------------------------------ lectura del teclado */

/* Lee una línea del teclado en UTF-8, sin el salto de línea. 0 = fin de la entrada. */
static int read_line(char *buf, int n)
{
#ifdef _WIN32
    /* En la consola de Windows, fgets no lee bien la ñ ni los acentos: se lee en UTF-16 y se convierte. */
    HANDLE h = GetStdHandle(STD_INPUT_HANDLE);
    DWORD mode, got = 0;
    if (GetConsoleMode(h, &mode)) {
        wchar_t w[256];
        if (!ReadConsoleW(h, w, 255, &got, NULL) || got == 0) return 0;
        w[got] = 0;
        if (w[0] == 0x1A) return 0;                    /* Ctrl+Z */
        w[wcscspn(w, L"\r\n")] = 0;
        WideCharToMultiByte(CP_UTF8, 0, w, -1, buf, n, NULL, NULL);
        return 1;
    }
#endif
    if (!fgets(buf, n, stdin)) return 0;
    buf[strcspn(buf, "\r\n")] = 0;
    return 1;
}

static void trim(char *s)
{
    char *a = s;
    while (isspace((unsigned char)*a)) a++;
    memmove(s, a, strlen(a) + 1);
    for (size_t k = strlen(s); k > 0 && isspace((unsigned char)s[k - 1]); k--) s[k - 1] = 0;
}

/* ------------------------------------------------------------ búsqueda de jugadores */

/* Copia s en minúsculas y sin acentos (Acuña -> acuna), para comparar nombres. */
static void fold(const char *s, char *out, size_t n)
{
    /* letras latinas con acento en UTF-8: 0xC3 seguido de 0x80–0xBF (mayúsculas y minúsculas) */
    static const char base[] = "aaaaaaaceeeeiiiidnooooo?ouuuuyty";
    size_t k = 0;
    for (const unsigned char *c = (const unsigned char *)s; *c && k + 1 < n; c++) {
        if (c[0] == 0xC3 && c[1] >= 0x80 && c[1] <= 0xBF) { out[k++] = base[c[1] & 0x1F]; c++; }
        else out[k++] = (char)tolower(*c);
    }
    out[k] = 0;
}

/* Busca por id, por nombre completo o por una parte del nombre, sin importar acentos ni mayúsculas.
 * Guarda hasta `max` jugadores en out y devuelve cuántos coinciden en total. */
static int search_players(const Dataset *ds, const char *query, const Player **out, int max)
{
    char q[128], full[128], ff[128];
    fold(query, q, sizeof q);
    if (!*q) return 0;
    int n = 0;
    for (int pass = 0; pass < 2 && n == 0; pass++) {   /* 1.º coincidencia exacta, 2.º parcial */
        for (int i = 0; i < ds->np; i++) {
            const Player *p = &ds->p[i];
            snprintf(full, sizeof full, "%s %s", p->first, p->last);
            fold(full, ff, sizeof ff);
            const int hit = pass == 0 ? (!strcmp(q, ff) || !strcmp(q, p->id)) : strstr(ff, q) != NULL;
            if (hit) { if (n < max) out[n] = p; n++; }
        }
    }
    return n;
}

/* Primera y última temporada con datos, y la temporada (>= 1998) con más HR. */
static void player_seasons(const Dataset *ds, const Player *pl, int *first, int *last, int *best, int *best_hr)
{
    *first = *last = *best = 0; *best_hr = -1;
    for (int i = 0; i < ds->ns; i++) {
        const SeasonRow *s = &ds->s[i];
        if (strcmp(s->id, pl->id)) continue;
        if (!*first || s->season < *first) *first = s->season;
        if (s->season > *last) *last = s->season;
        if (s->season >= 1998 && s->hr > *best_hr) { *best_hr = s->hr; *best = s->season; }
    }
}

static void list_players(const Dataset *ds, const Player **c, int n)
{
    printf("   Hay %d jugadores que coinciden:\n", n);
    for (int i = 0; i < n && i < MAX_CAND; i++) {
        int first, last, best, best_hr;
        player_seasons(ds, c[i], &first, &last, &best, &best_hr);
        printf("   %2d) %s %s  (id %s", i + 1, c[i]->first, c[i]->last, c[i]->id);
        if (first) printf(", jugó %d–%d", first, last);
        printf(")\n");
    }
    if (n > MAX_CAND) printf("   ... y %d más.\n", n - MAX_CAND);
}

/* ------------------------------------------------------------ modo interactivo */

/* Pregunta el jugador hasta encontrar uno. NULL = el usuario quiere salir. */
static const Player *ask_player(const Dataset *ds)
{
    char q[128];
    for (;;) {
        printf("\nJugador (nombre, apellido o id · Enter = Barry Bonds · q = salir): ");
        fflush(stdout);
        if (!read_line(q, sizeof q)) return NULL;
        trim(q);
        if (!strcmp(q, "q") || !strcmp(q, "Q") || !strcmp(q, "salir")) return NULL;
        if (!*q) strcpy(q, "Barry Bonds");

        const Player *c[MAX_CAND];
        const int n = search_players(ds, q, c, MAX_CAND);
        if (n == 0) { printf("   No encontré a \"%s\". Prueba solo con el apellido.\n", q); continue; }
        if (n == 1) return c[0];
        list_players(ds, c, n);
        if (n > MAX_CAND) { printf("   Escribe el nombre más completo.\n"); continue; }
        printf("   Elige un número (Enter = buscar otro): ");
        fflush(stdout);
        if (!read_line(q, sizeof q)) return NULL;
        const int k = atoi(q);
        if (k >= 1 && k <= n) return c[k - 1];
    }
}

/* Pregunta la temporada. 0 = el usuario quiere salir. */
static int ask_season(const Dataset *ds, const Player *pl)
{
    int first, last, best, best_hr;
    player_seasons(ds, pl, &first, &last, &best, &best_hr);
    printf("   %s %s: temporadas con datos %d–%d", pl->first, pl->last, first, last);
    if (best) printf(" · su máximo desde 1998: %d HR en %d", best_hr, best);
    printf("\n");

    const int def = best ? best : last + 1;
    char q[32];
    for (;;) {
        printf("Temporada (1998–2026 · Enter = %d): ", def);
        fflush(stdout);
        if (!read_line(q, sizeof q)) return 0;
        trim(q);
        if (!strcmp(q, "q") || !strcmp(q, "Q") || !strcmp(q, "salir")) return 0;
        if (!*q) return def;
        const int t = atoi(q);
        if (t >= 1998 && t <= 2026) return t;
        printf("   Escribe un año entre 1998 y 2026.\n");
    }
}

/* ------------------------------------------------------------ el análisis */

/* Última temporada que hay en los datos. */
static int last_data_season(const Dataset *ds)
{
    int last = 0;
    for (int i = 0; i < ds->ns; i++)
        if (ds->s[i].season > last) last = ds->s[i].season;
    return last;
}

static int analyze(const Dataset *ds, const Player *pl, int t)
{
    if (t < 1998 || t > 2026) {
        fprintf(stderr, "Usa una temporada entre 1998 y 2026: el modelo necesita los 3 años anteriores (los datos empiezan en 1995).\n");
        return 1;
    }

    /* ---- el modelo: el talento sale solo de los datos del jugador */
    Talent tl;
    if (talent_estimate(ds, pl, t, CAPA_ACTUAL, &tl)) {
        fprintf(stderr, "%s %s no jugó entre %d y %d: no hay datos para estimar su talento.\n",
                pl->first, pl->last, t - 3, t - 1);
        return 1;
    }
    const SeasonRow *cur = data_season(ds, pl->id, t);
    const int future = t > last_data_season(ds);

    printf("\nAce of Diamond · Capa %d · talento de %s %s para %d\n", CAPA_ACTUAL, pl->first, pl->last, t);

    /* ---- paso 1: sus temporadas anteriores */
    printf("\n1) Sus temporadas anteriores · talent.c\n");
    printf("   año      PA    HR    HR/PA   λ liga     HR*   peso   peso·PA   peso·HR*\n");
    for (int i = 0; i < tl.nseasons; i++)
        printf("   %d   %4d   %3d   %5.2f %%   %5.2f %%   %5.1f   %.1f   %7.1f   %8.1f\n", tl.season[i], tl.pa[i], tl.hr[i],
               100.0 * tl.hr[i] / tl.pa[i], 100 * tl.lambda_s[i], tl.hr_star[i], tl.w[i], tl.w[i] * tl.pa[i], tl.w[i] * tl.hr_star[i]);
    printf("   total                                                 %7.1f   %8.1f\n", tl.pa_w, tl.hr_w);
    const SeasonRow *hay_t = data_season(ds, pl->id, t);
    printf("   λ de su liga en %d = %.2f %%%s\n", t, 100 * tl.lambda_t,
           hay_t ? "" : " (todavía no hay datos de esa temporada: se usa el más reciente)");
    printf("   HR* = PA · expit(logit(HR/PA) − logit λ de ese año + logit λ de %d): sus HR en el ambiente de %d\n", t, t);
    if (tl.nseasons < 3) printf("   (solo jugó %d de las 3 temporadas: se usan las que tiene)\n", tl.nseasons);

    /* ---- paso 2: el talento */
    printf("\n2) Talento · talent.c\n");
    const double tasa = tl.hr_w / tl.pa_w;
    printf("   Tasa con pesos = Σ peso·HR* / Σ peso·PA = %.1f / %.1f = %.2f %%", tl.hr_w, tl.pa_w, 100 * tasa);
    printf("   (sin pesos: %d / %d = %.2f %%)\n", tl.hr_sum, tl.pa_sum, 100.0 * tl.hr_sum / tl.pa_sum);
    if (CAPA_ACTUAL >= 2) {
        const Prior *L = &tl.liga;
        printf("\n3) La liga · prior.c\n");
        printf("   %d temporadas de regulares (>= 300 PA) de %d a %d, cada una traducida al ambiente de %d\n", L->J, t - 3, t - 1, t);
        printf("   promedio m = %.2f %%   varianza del talento v = %.2e   n0 = %.0f turnos\n", 100 * L->m, L->v, L->n0);
        printf("   La liga es como un jugador con %.1f HR en %.0f turnos: Beta(α0 = %.1f, β0 = %.1f)\n", L->a0, L->n0, L->a0, L->b0);
        printf("\n4) Jugador + liga (Bayes) · talent.c\n");
        printf("   α = α0 + Σ peso·HR*        = %.1f + %.1f = %.1f\n", L->a0, tl.hr_w, tl.alpha);
        printf("   β = β0 + Σ peso·(PA − HR*) = %.1f + %.1f = %.1f\n", L->b0, tl.pa_w - tl.hr_w, tl.beta);
        printf("   θ = α / (α + β) = %.2f %%   (pesa %.0f %% el jugador y %.0f %% la liga)\n",
               100 * tl.theta, 100 * tl.peso_jugador, 100 * (1 - tl.peso_jugador));
    }

    /* ---- resultado */
    printf("\nResultado\n");
    if (cur) {
        printf("   En %d tuvo %d turnos  ->  HR esperados ≈ θ · PA = %.1f\n", t, cur->pa, tl.theta * cur->pa);
        printf("   HR reales: %d\n", cur->hr);
    } else if (future) {
        printf("   %d todavía no tiene datos: por cada 600 turnos se esperarían unos %.0f HR.\n", t, 600 * tl.theta);
    } else {
        printf("   %s %s no jugó en %d.\n", pl->first, pl->last, t);
    }
    return 0;
}

/* ------------------------------------------------------------ main */

int main(int argc, char **argv)
{
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);                       /* acentos y letras griegas en la consola */
    atexit(pause_if_double_clicked);                   /* también cuando hay error */
#endif
    const char *name = NULL;
    int t = 0, npos = 0;
    for (int i = 1; i < argc; i++) {
        if      (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) { usage(); return 0; }
        else if (npos == 0) { name = argv[i]; npos++; }
        else if (npos == 1) { t = atoi(argv[i]); npos++; }
        else { usage(); return 1; }
    }

    Dataset ds;
    if (data_load(&ds, find_data_dir())) {
        fprintf(stderr, "No encontré data/inputs/. Ejecuta el demo desde la carpeta del repositorio.\n");
        return 1;
    }
    int rc = 0;

    if (!name) {
        /* ---- modo interactivo: pregunta jugador y temporada hasta que el usuario salga */
        printf("\nAce of Diamond · Capa %d · talento θ de un jugador con sus propios datos\n", CAPA_ACTUAL);
        for (;;) {
            const Player *pl = ask_player(&ds);
            if (!pl) break;
            const int season = ask_season(&ds, pl);
            if (!season) break;
            analyze(&ds, pl, season);
        }
        printf("\n");
        quiet_exit = 1;
    } else {
        /* ---- con argumentos: un solo análisis */
        const Player *c[MAX_CAND];
        const int n = search_players(&ds, name, c, MAX_CAND);
        if (n == 0) {
            fprintf(stderr, "Jugador no encontrado: %s\n", name);
            rc = 1;
        } else if (n > 1) {
            list_players(&ds, c, n);
            fprintf(stderr, "Escribe el nombre completo o el id del jugador.\n");
            rc = 1;
        } else {
            if (!t) {                                  /* sin temporada: la de más HR */
                int first, last, best, best_hr;
                player_seasons(&ds, c[0], &first, &last, &best, &best_hr);
                t = best ? best : last + 1;
            }
            rc = analyze(&ds, c[0], t);
            printf("\n");
        }
    }

    data_free(&ds);
    return rc;
}
