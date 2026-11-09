/*
 * rng.c — Números aleatorios y sorteos de distribuciones.
 *
 * Generador: xoshiro256** (Blackman y Vigna), rápido y de alta calidad.
 * Semilla:   SplitMix64. Cada bloque de simulaciones usa su propio "stream"
 *            (semilla + número de bloque), así el resultado es idéntico
 *            con 1 o con 16 hilos.
 */
#include <math.h>
#include "aod.h"

static inline uint64_t rotl(uint64_t x, int k) { return (x << k) | (x >> (64 - k)); }

static uint64_t splitmix(uint64_t *x)
{
    uint64_t z = (*x += 0x9e3779b97f4a7c15ULL);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}

void rng_seed(Rng *r, uint64_t seed, uint64_t stream)
{
    uint64_t x = seed ^ (stream * 0xd1342543de82ef95ULL);
    for (int i = 0; i < 4; i++) r->s[i] = splitmix(&x);
}

static inline uint64_t next(Rng *r)
{
    uint64_t *s = r->s;
    const uint64_t res = rotl(s[1] * 5, 7) * 9, t = s[1] << 17;
    s[2] ^= s[0]; s[3] ^= s[1]; s[1] ^= s[2]; s[0] ^= s[3];
    s[2] ^= t;    s[3] = rotl(s[3], 45);
    return res;
}

/* U(0,1): los 53 bits superiores convertidos a double */
double rng_uniform(Rng *r) { return (double)(next(r) >> 11) * 0x1.0p-53; }

/* N(0,1) por Box-Muller */
double rng_normal(Rng *r)
{
    double u = rng_uniform(r), v = rng_uniform(r);
    if (u < 1e-300) u = 1e-300;
    return sqrt(-2.0 * log(u)) * cos(6.283185307179586 * v);
}

/*
 * Gamma(k, 1) por el método de Marsaglia y Tsang (2000).
 * Se usa para sortear la Beta: si X ~ Gamma(α) e Y ~ Gamma(β),
 * entonces X/(X+Y) ~ Beta(α, β) y X/Y son sus odds (lección, Ec. 22).
 */
double rng_gamma(Rng *r, double k)
{
    if (k < 1.0) return rng_gamma(r, k + 1.0) * pow(rng_uniform(r), 1.0 / k);
    const double d = k - 1.0 / 3.0, c = 1.0 / sqrt(9.0 * d);
    for (;;) {
        double x, v;
        do { x = rng_normal(r); v = 1.0 + c * x; } while (v <= 0.0);
        v = v * v * v;
        const double u = rng_uniform(r);
        if (u < 1.0 - 0.0331 * x * x * x * x) return d * v;
        if (log(u) < 0.5 * x * x + d * (1.0 - v + log(v))) return d * v;
    }
}
