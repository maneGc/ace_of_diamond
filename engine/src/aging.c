/*
 * aging.c — Curva de edad (lección, Ec. 14).
 *
 * Para cada jugador con dos temporadas consecutivas (ambas con ≥ min_pa turnos):
 *   cambio = logit(tasa en e+1) − logit(tasa en e),   peso = media armónica de sus PA
 * Se ajusta por mínimos cuadrados ponderados una recta  δ(e) = b0 + b1·e
 * y la curva es la suma de esos cambios: a(e+1) − a(e) = δ(e), con a(27) = 0.
 * Solo usa pares que terminan en last_season o antes: sin fuga de datos.
 */
#include <stdlib.h>
#include <string.h>
#include "aod.h"

static int cmp_player(const void *key, const void *elem)
{
    return strcmp((const char *)key, ((const Player *)elem)->id);
}

Aging aging_fit(const Dataset *ds, int last_season, int min_pa)
{
    Aging ag = {0};
    double sw = 0, sx = 0, sy = 0, sxx = 0, sxy = 0;

    for (int i = 0; i + 1 < ds->ns; i++) {
        const SeasonRow *a = &ds->s[i], *b = &ds->s[i + 1];
        if (strcmp(a->id, b->id) != 0 || b->season != a->season + 1) continue;
        if (b->season > last_season || a->pa < min_pa || b->pa < min_pa) continue;
        const Player *p = bsearch(a->id, ds->p, ds->np, sizeof *ds->p, cmp_player);
        if (!p) continue;
        const int e = age_at(p, a->season);
        if (e < 18) continue;
        /* +0.5 y +1 evitan logit(0) cuando un jugador no pega HR */
        const double y = logit((b->hr + 0.5) / (b->pa + 1.0)) - logit((a->hr + 0.5) / (a->pa + 1.0));
        const double w = 2.0 / (1.0 / a->pa + 1.0 / b->pa);
        sw += w; sx += w * e; sy += w * y; sxx += w * e * e; sxy += w * e * y;
        ag.npairs++;
    }
    if (ag.npairs < 10) return ag;
    const double mx = sx / sw, my = sy / sw;
    ag.b1 = (sxy / sw - mx * my) / (sxx / sw - mx * mx);
    ag.b0 = my - ag.b1 * mx;
    return ag;
}

double aging_curve(const Aging *ag, int e)
{
    double a = 0.0;
    if (e >= 27) for (int k = 27; k < e; k++)  a += ag->b0 + ag->b1 * k;
    else         for (int k = e; k < 27; k++)  a -= ag->b0 + ag->b1 * k;
    return a;
}
