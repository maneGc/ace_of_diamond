/*
 * data.c — Lectura de los CSV de data/inputs/ (generados por etl/export_inputs.py).
 * Formato simple: separado por comas, sin comillas, primera línea = encabezado.
 */
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "aod.h"

double logit(double p) { return log(p / (1.0 - p)); }
double expit(double z) { return 1.0 / (1.0 + exp(-z)); }

/* Divide una línea en campos (modifica la línea). Devuelve el número de campos. */
static int split(char *line, char **f, int max)
{
    int n = 0;
    line[strcspn(line, "\r\n")] = 0;
    f[n++] = line;
    for (char *c = line; *c && n < max; c++)
        if (*c == ',') { *c = 0; f[n++] = c + 1; }
    return n;
}

static FILE *open_csv(const char *dir, const char *name)
{
    char path[1024];
    snprintf(path, sizeof path, "%s/%s", dir, name);
    FILE *fh = fopen(path, "r");
    if (!fh) fprintf(stderr, "No se pudo abrir %s\n", path);
    return fh;
}

int data_load(Dataset *ds, const char *dir)
{
    char line[512], *f[8];
    memset(ds, 0, sizeof *ds);
    int cap;

    FILE *fh = open_csv(dir, "player_seasons.csv");
    if (!fh) return 1;
    cap = 32768; ds->s = malloc(cap * sizeof *ds->s);
    if (!fgets(line, sizeof line, fh)) { fclose(fh); return 1; }   /* encabezado */
    while (fgets(line, sizeof line, fh)) {
        if (split(line, f, 8) < 5) continue;
        if (ds->ns == cap) ds->s = realloc(ds->s, (cap *= 2) * sizeof *ds->s);
        SeasonRow *r = &ds->s[ds->ns++];
        snprintf(r->id, sizeof r->id, "%s", f[0]);
        r->season = atoi(f[1]); r->pa = atoi(f[2]); r->hr = atoi(f[3]); r->league = f[4][0];
    }
    fclose(fh);

    if (!(fh = open_csv(dir, "league_seasons.csv"))) return 1;
    cap = 128; ds->l = malloc(cap * sizeof *ds->l);
    if (!fgets(line, sizeof line, fh)) { fclose(fh); return 1; }
    while (fgets(line, sizeof line, fh)) {
        if (split(line, f, 8) < 4) continue;
        if (ds->nl == cap) ds->l = realloc(ds->l, (cap *= 2) * sizeof *ds->l);
        LeagueRow *r = &ds->l[ds->nl++];
        r->season = atoi(f[0]); r->league = f[1][0]; r->pa = atol(f[2]); r->hr = atol(f[3]);
    }
    fclose(fh);

    if (!(fh = open_csv(dir, "players.csv"))) return 1;
    cap = 8192; ds->p = malloc(cap * sizeof *ds->p);
    if (!fgets(line, sizeof line, fh)) { fclose(fh); return 1; }
    while (fgets(line, sizeof line, fh)) {
        if (split(line, f, 8) < 4) continue;
        if (ds->np == cap) ds->p = realloc(ds->p, (cap *= 2) * sizeof *ds->p);
        Player *p = &ds->p[ds->np++];
        snprintf(p->id, sizeof p->id, "%s", f[0]);
        snprintf(p->first, sizeof p->first, "%s", f[1]);
        snprintf(p->last, sizeof p->last, "%s", f[2]);
        p->by = p->bm = p->bd = 0;
        if (strlen(f[3]) >= 10) sscanf(f[3], "%d-%d-%d", &p->by, &p->bm, &p->bd);
    }
    fclose(fh);
    return 0;
}

void data_free(Dataset *ds) { free(ds->s); free(ds->l); free(ds->p); memset(ds, 0, sizeof *ds); }

static int same_ci(const char *a, const char *b)
{
    for (; *a && *b; a++, b++) if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return 0;
    return *a == *b;
}

const Player *data_find_player(const Dataset *ds, const char *q)
{
    char full[100];
    for (int i = 0; i < ds->np; i++) {
        const Player *p = &ds->p[i];
        snprintf(full, sizeof full, "%s %s", p->first, p->last);
        if (same_ci(q, p->id) || same_ci(q, full)) return p;
    }
    return NULL;
}

const SeasonRow *data_season(const Dataset *ds, const char *id, int season)
{
    for (int i = 0; i < ds->ns; i++)
        if (ds->s[i].season == season && strcmp(ds->s[i].id, id) == 0) return &ds->s[i];
    return NULL;
}

double data_lambda(const Dataset *ds, int season, char league)
{
    for (int i = 0; i < ds->nl; i++)
        if (ds->l[i].season == season && ds->l[i].league == league)
            return (double)ds->l[i].hr / (double)ds->l[i].pa;
    return NAN;
}

int age_at(const Player *p, int season)
{
    if (p->by == 0) return -1;
    return season - p->by - ((p->bm > 6 || (p->bm == 6 && p->bd > 30)) ? 1 : 0);
}
