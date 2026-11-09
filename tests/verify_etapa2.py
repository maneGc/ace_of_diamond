"""
verify_etapa2.py — Verificación independiente del motor en C (etapa 2).

Recalcula el modelo con código distinto al del motor y de forma EXACTA
(integrando sobre la distribución del talento, sin simulación), y comprueba:
  1. Prior, posterior y edad del motor = cálculo independiente.
  2. La simulación Monte Carlo coincide con la distribución exacta
     (media, desviación y p-value dentro de su error estadístico).
  3. v1 (talento fijo) coincide con la fórmula de la Binomial.
  4. Reproducibilidad: misma semilla → mismo resultado, con 1 o con varios hilos.
Uso:  python tests/verify_etapa2.py      (requiere engine/build/aod compilado)
"""
import csv
import json
import math
import os
import subprocess
import sys
from math import exp, lgamma, log
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
AOD = ROOT / "engine" / "build" / ("aod.exe" if os.name == "nt" else "aod")
INP = ROOT / "data" / "inputs"
CASES = [("Barry Bonds", 2001), ("Mark McGwire", 1998), ("Sammy Sosa", 1998),
         ("Aaron Judge", 2022), ("Shohei Ohtani", 2024)]
N = 1_000_000
lg = lambda p: math.log(p / (1 - p))
ex = lambda z: 1 / (1 + math.exp(-z))

# ------------------------------------------------------------- datos
seasons, league, players, birth = {}, {}, {}, {}
for r in csv.DictReader(open(INP / "player_seasons.csv")):
    seasons[(r["player_id"], int(r["season"]))] = (int(r["pa"]), int(r["hr"]), r["league"])
for r in csv.DictReader(open(INP / "league_seasons.csv")):
    league[(int(r["season"]), r["league"])] = int(r["hr"]) / int(r["pa"])
for r in csv.DictReader(open(INP / "players.csv")):
    players[f'{r["name_first"]} {r["name_last"]}'] = (r["player_id"], r["birth_date"])
    birth[r["player_id"]] = r["birth_date"]          # por ID: hay jugadores con el mismo nombre


def age(bd, season):
    y, m, d = map(int, bd.split("-"))
    return season - y - (1 if (m, d) > (6, 30) else 0)


def model(pid, bd, t):
    """Modelo v2 calculado de forma independiente."""
    rows = [(p, h) for (i, s), (p, h, l) in seasons.items() if t - 3 <= s <= t - 1 and p >= 300]
    rates = [h / p for p, h in rows]
    m = sum(rates) / len(rates)
    s2 = sum(x * x for x in rates) / len(rates) - m * m
    v = s2 - m * (1 - m) * sum(1 / p for p, h in rows) / len(rows)
    n0 = m * (1 - m) / v - 1
    # curva de edad: recta ponderada a los cambios de logit entre años consecutivos
    xs, ys, ws = [], [], []
    for (i, s), (p1, h1, _) in seasons.items():
        nxt = seasons.get((i, s + 1))
        if not nxt or s + 1 > t - 1 or p1 < 200 or nxt[0] < 200 or not birth.get(i):
            continue
        xs.append(age(birth[i], s))
        ys.append(lg((nxt[1] + .5) / (nxt[0] + 1)) - lg((h1 + .5) / (p1 + 1)))
        ws.append(2 / (1 / p1 + 1 / nxt[0]))
    sw = sum(ws); mx = sum(w * x for w, x in zip(ws, xs)) / sw; my = sum(w * y for w, y in zip(ws, ys)) / sw
    b1 = sum(w * (x - mx) * (y - my) for w, x, y in zip(ws, xs, ys)) / sum(w * (x - mx) ** 2 for w, x in zip(ws, xs))
    b0 = my - b1 * mx
    A = lambda e: sum(b0 + b1 * k for k in range(27, e)) if e >= 27 else -sum(b0 + b1 * k for k in range(e, 27))
    lt = league[(t, seasons[(pid, t)][2])]
    hw = n = swa = sww = 0
    for k, w in ((1, 1.0), (2, .8), (3, .6)):
        if (pid, t - k) not in seasons:
            continue
        p, h, l = seasons[(pid, t - k)]
        hs = p * ex(lg(h / p) - lg(league[(t - k, l)]) + lg(lt)) if h > 0 else 0
        hw += w * hs; n += w * p; swa += w * A(age(bd, t - k)); sww += w
    a, b = m * n0 + hw, (1 - m) * n0 + n - hw
    delta = A(age(bd, t)) - swa / sww
    # v1: tasa ponderada sin era ni edad
    hr1 = n1 = 0
    for k, w in ((1, 1.0), (2, .8), (3, .6)):
        if (pid, t - k) in seasons:
            p, h, _ = seasons[(pid, t - k)]; hr1 += w * h; n1 += w * p
    return dict(m=m, n0=n0, alpha=a, beta=b, delta=delta, theta=ex(lg(a / (a + b)) + delta), r1=hr1 / n1)


def exact(npa, obs, a=None, b=None, delta=0.0, theta=None, K=4000):
    """Distribución exacta de HR: Binomial (θ fijo) o integrada sobre θ̃ ~ Beta(a, b)."""
    lf = [lgamma(k + 1) for k in range(npa + 2)]
    pmf = [0.0] * (npa + 1)
    grid = [(theta, 1.0)] if theta is not None else []
    if theta is None:
        lb = lgamma(a) + lgamma(b) - lgamma(a + b)
        for i in range(K):
            t = (i + .5) / K * .25
            w = exp((a - 1) * log(t) + (b - 1) * log(1 - t) - lb) * .25 / K
            if w > 1e-15:
                grid.append((ex(lg(t) + delta), w))
    for p, w in grid:
        lp, lq = log(p), log(1 - p)
        for k in range(npa + 1):
            pmf[k] += w * exp(lf[npa] - lf[k] - lf[npa - k] + k * lp + (npa - k) * lq)
    tot = sum(pmf); pmf = [x / tot for x in pmf]
    mean = sum(k * x for k, x in enumerate(pmf)); sd = math.sqrt(sum(k * k * x for k, x in enumerate(pmf)) - mean ** 2)
    return mean, sd, sum(pmf[obs:])


def run(name, t, model_v, threads=None, seed=2001):
    env = dict(os.environ)
    if threads:
        env["OMP_NUM_THREADS"] = str(threads)
    out = subprocess.run([str(AOD), "analizar", "--jugador", name, "--temporada", str(t), "--modelo", model_v,
                          "--n", str(N), "--semilla", str(seed), "--json"], cwd=ROOT, env=env,
                         capture_output=True, text=True, check=True).stdout
    d = json.loads(out); d.pop("secs"); return d


fails = 0
def check(label, ok, detail):
    global fails
    fails += not ok
    print(f"  {'✓' if ok else '✗'} {label:<46} {detail}")


for name, t in CASES:
    pid, bd = players[name]
    npa, obs = seasons[(pid, t)][0], seasons[(pid, t)][1]
    ind = model(pid, bd, t)
    c2, c1 = run(name, t, "v2"), run(name, t, "v1")
    print(f"\n{name} {t}  ({npa} PA, {obs} HR)")
    check("prior n0 y m", abs(c2["prior"]["n0"] - ind["n0"]) < 1e-2 and abs(c2["prior"]["m"] - ind["m"]) < 1e-6,
          f'C: n0={c2["prior"]["n0"]:.3f}  Python: {ind["n0"]:.3f}')
    check("posterior α, β", abs(c2["talent"]["alpha"] - ind["alpha"]) < 1e-2 and abs(c2["talent"]["beta"] - ind["beta"]) < 1e-2,
          f'C: {c2["talent"]["alpha"]:.2f}/{c2["talent"]["beta"]:.2f}  Python: {ind["alpha"]:.2f}/{ind["beta"]:.2f}')
    check("Δ edad y θ", abs(c2["talent"]["delta"] - ind["delta"]) < 1e-4 and abs(c2["talent"]["theta"] - ind["theta"]) < 1e-5,
          f'C: Δ={c2["talent"]["delta"]:+.4f} θ={c2["talent"]["theta"]:.5f}  Python: Δ={ind["delta"]:+.4f} θ={ind["theta"]:.5f}')
    mean, sd, p = exact(npa, obs, ind["alpha"], ind["beta"], ind["delta"])
    se_mean = sd / math.sqrt(N); se_p = math.sqrt(p * (1 - p) / N)
    check("v2: media (Monte Carlo vs exacta)", abs(c2["mean"] - mean) < 4 * se_mean, f'{c2["mean"]:.3f} vs {mean:.3f}')
    check("v2: desviación", abs(c2["sd"] - sd) < 0.01 * sd, f'{c2["sd"]:.3f} vs {sd:.3f}')
    check("v2: p-value (dentro de 4 errores estándar)", abs(c2["pvalue"] - p) < 4 * se_p, f'{c2["pvalue"]:.3e} vs {p:.3e}')
    mean1, sd1, p1 = exact(npa, obs, theta=ind["r1"])
    se_p1 = math.sqrt(p1 * (1 - p1) / N)
    check("v1: p-value vs Binomial exacta", abs(c1["pvalue"] - p1) < 4 * se_p1 + 1e-12, f'{c1["pvalue"]:.3e} vs {p1:.3e}')

print("\nReproducibilidad (Bonds 2001, v2):")
a, b = run("Barry Bonds", 2001, "v2"), run("Barry Bonds", 2001, "v2")
check("misma semilla → mismo resultado", a == b, "")
c = run("Barry Bonds", 2001, "v2", threads=1)
check("1 hilo = varios hilos", a == c, "")
d = run("Barry Bonds", 2001, "v2", seed=7)
check("otra semilla → otro resultado (dentro del error)", d != a and abs(d["pvalue"] - a["pvalue"]) < 4 * a["se"] * 1.5, f'{d["pvalue"]:.3e}')

print("\nRESULTADO:", "todo correcto ✓" if fails == 0 else f"{fails} comprobaciones fallaron ✗")
sys.exit(1 if fails else 0)
