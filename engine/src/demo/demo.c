/*
 * demo.c — Demo del Avance 1: calcula, paso a paso, el talento θ de un jugador.
 *
 * Usa solo data.c, prior.c, aging.c y talent.c (todavía sin contexto ni Monte Carlo).
 *
 * Compilar, desde la carpeta del repositorio:
 *   gcc -std=c11 -O2 -Iengine/include engine/src/data.c engine/src/prior.c engine/src/aging.c engine/src/talent.c engine/src/demo/demo.c -lm -o demo
 *
 * Ejecutar:
 *   ./demo                              pregunta el jugador y la temporada
 *   ./demo "Aaron Judge" 2022
 *   ./demo judge                        sin temporada: usa su temporada con más HR
 *   ./demo "Barry Bonds" 2001 --sin-edad --sin-era
 *   ./demo bondb001 2001                también acepta el id de Retrosheet
 */
#include <ctype.h>
#include <math.h>
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
    fprintf(stderr, "Uso: demo [\"Nombre Apellido\" | apellido | id] [temporada] [--sin-era] [--sin-edad]\n"
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
        if (c[i]->by) printf(", nació en %d", c[i]->by);
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

static int analyze(const Dataset *ds, const Player *pl, int t, TalentOpts opt)
{
    if (t < 1998 || t > 2026) {
        fprintf(stderr, "Usa una temporada entre 1998 y 2026: el modelo necesita los 3 años anteriores (los datos empiezan en 1995).\n");
        return 1;
    }

    /* ---- el modelo: prior de la liga, curva de edad y talento del jugador */
    const Prior pr = prior_estimate(ds, t, 3, 300);    /* liga: t−3 … t−1, ≥ 300 PA */
    const Aging ag = aging_fit(ds, t - 1, 200);        /* pares de temporadas hasta t−1 */
    Talent tl;
    talent_estimate(ds, &pr, &ag, pl, t, opt, &tl);
    if (tl.nseasons == 0) {
        fprintf(stderr, "%s %s no jugó entre %d y %d: no hay datos para estimar su talento.\n",
                pl->first, pl->last, t - 3, t - 1);
        return 1;
    }
    const SeasonRow *cur = data_season(ds, pl->id, t);
    const int future = isnan(data_lambda(ds, t, 'A')) && isnan(data_lambda(ds, t, 'N'));

    printf("\nAce of Diamond · Avance 1 · talento de %s %s para %d", pl->first, pl->last, t);
    if (tl.age_t > 0) printf(" (%d años)", tl.age_t);
    printf("\n");

    /* ---- paso 1: prior.c */
    printf("\n1) Prior de la liga · prior.c\n");
    printf("   %d temporadas regulares (>= 300 PA) de %d a %d\n", pr.J, t - 3, t - 1);
    printf("   m     = %.3f %%      promedio de HR/PA\n", 100 * pr.m);
    printf("   s²    = %.3e    cuánto varían los jugadores entre sí\n", pr.s2);
    printf("   ruido = %.3e    la parte que es solo suerte\n", pr.noise);
    printf("   v     = %.3e    varianza del talento real (%.0f %% de s²)\n", pr.v, 100 * pr.v / pr.s2);
    printf("   n0    = %.1f turnos imaginarios  ->  Beta(α0 = %.2f, β0 = %.2f)\n", pr.n0, pr.a0, pr.b0);

    /* ---- paso 2: talent.c, ajuste por época y pesos */
    printf("\n2) Temporadas anteriores · talent.c\n");
    if (!opt.use_era) printf("   (ajuste por época desactivado: HR* = HR)\n");
    printf("   año      PA    HR   λ liga     HR*    peso\n");
    for (int i = 0; i < tl.nseasons; i++)
        printf("   %d   %4d   %3d   %.2f %%   %6.2f    %.1f\n",
               tl.season[i], tl.pa[i], tl.hr[i], 100 * tl.lambda_s[i], tl.hr_star[i], tl.w[i]);
    printf("   λ de la liga en %d = %.2f %%%s\n", t, 100 * tl.lambda_t, future ? " (último año con datos)" : "");
    printf("   Ponderado: %.1f HR en %.1f PA  ->  r = %.2f %%\n", tl.hr_w, tl.n, 100 * tl.r);

    /* ---- paso 3: Bayes */
    printf("\n3) Bayes: liga + jugador · talent.c\n");
    printf("   α = α0 + HR*        = %.2f + %.2f = %.2f\n", pr.a0, tl.hr_w, tl.alpha);
    printf("   β = β0 + (PA − HR*) = %.2f + %.2f = %.2f\n", pr.b0, tl.n - tl.hr_w, tl.beta);
    printf("   Centro de la Beta = α / (α + β) = %.2f %%\n", 100 * tl.mean_tilde);
    printf("   Peso del jugador  = n / (n + n0) = %.1f %%   (la liga pesa %.1f %%)\n",
           100 * tl.w_player, 100 * (1 - tl.w_player));

    /* ---- paso 4: aging.c */
    printf("\n4) Edad · aging.c\n");
    if (!opt.use_age) {
        printf("   (ajuste por edad desactivado: Δ = 0)\n");
    } else if (tl.age_t <= 0 || ag.npairs == 0) {
        printf("   Sin fecha de nacimiento o sin curva: Δ = 0\n");
    } else {
        int emin = 200, emax = 0;
        for (int i = 0; i < tl.nseasons; i++) {
            const int e = age_at(pl, tl.season[i]);
            if (e > 0 && e < emin) emin = e;
            if (e > emax) emax = e;
        }
        printf("   Recta δ(e) = %.4f %+.5f·e, ajustada con %d pares de temporadas", ag.b0, ag.b1, ag.npairs);
        if (ag.b1 < 0) printf(" · pico ≈ %.1f años", -ag.b0 / ag.b1);
        printf("\n   Edad %d frente a sus temporadas a los %d–%d  ->  Δ = %+.3f (escala logit)\n",
               tl.age_t, emin, emax, tl.delta);
    }

    /* ---- resultado */
    printf("\nResultado\n");
    printf("   θ = expit(logit(%.2f %%) %+.3f) = %.2f %% por turno\n", 100 * tl.mean_tilde, tl.delta, 100 * tl.theta);
    if (cur) {
        printf("   En %d tuvo %d turnos  ->  HR esperados ≈ θ · PA = %.1f\n", t, cur->pa, tl.theta * cur->pa);
        printf("   HR reales: %d\n", cur->hr);
    } else if (future) {
        printf("   %d todavía no tiene datos: por cada 600 turnos se esperarían unos %.0f HR.\n", t, 600 * tl.theta);
    } else {
        printf("   %s %s no jugó en %d.\n", pl->first, pl->last, t);
    }
    printf("\n   Siguiente etapa: contexto cⱼ y simulación Monte Carlo para medir qué tan probable es ese resultado.\n");
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
    TalentOpts opt = {1, 1};
    for (int i = 1; i < argc; i++) {
        if      (!strcmp(argv[i], "--sin-era"))  opt.use_era = 0;
        else if (!strcmp(argv[i], "--sin-edad")) opt.use_age = 0;
        else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) { usage(); return 0; }
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
        printf("\nAce of Diamond · Avance 1 · talento θ de un jugador\n");
        for (;;) {
            const Player *pl = ask_player(&ds);
            if (!pl) break;
            const int season = ask_season(&ds, pl);
            if (!season) break;
            analyze(&ds, pl, season, opt);
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
            rc = analyze(&ds, c[0], t, opt);
            printf("\n");
        }
    }

    data_free(&ds);
    return rc;
}
