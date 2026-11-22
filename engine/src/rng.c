/*
 * rng.c — Números aleatorios uniformes U(0,1).
 *
 * Generador: xoshiro256** (Blackman y Vigna), rápido y de alta calidad.
 * Semilla:   SplitMix64, que convierte un número cualquiera en los 4 valores
 *            iniciales del generador. Con la misma semilla salen los mismos
 *            números, así la simulación se puede repetir exactamente.
 */
#include "aod.h"

static inline uint64_t rotl(uint64_t x, int k) { return (x << k) | (x >> (64 - k)); }

static uint64_t splitmix(uint64_t *x)
{
    uint64_t z = (*x += 0x9e3779b97f4a7c15ULL);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}

void rng_seed(Rng *r, uint64_t seed)
{
    uint64_t x = seed;
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
