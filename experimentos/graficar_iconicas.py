"""
graficar_iconicas.py — ¿Qué tan anómalas fueron las temporadas icónicas? (CSV de engine/src/experimentos/iconicas.c)

Abre tres ventanas:
  1. el p-value de cada temporada en cada capa (escala logarítmica: más arriba = más anómala)
  2. el ranking con el modelo completo (la última capa), como "1 en N"
  3. la campana de temporadas simuladas de cada una (modelo completo) contra lo que de verdad pegó
e imprime una tabla.

Uso:  python experimentos/graficar_iconicas.py
"""
import csv
from collections import defaultdict

import matplotlib.pyplot as plt

N_SIM = 1_000_000
N_TEMPORADAS = 7144            # temporadas de regulares analizadas en comparar.c (1998–2025)

res = list(csv.DictReader(open("experimentos/iconicas/resumen.csv", encoding="utf-8")))
hist = defaultdict(list)
for r in csv.DictReader(open("experimentos/iconicas/histogramas.csv", encoding="utf-8")):
    hist[(r["capa"], r["jugador"], r["temporada"])].append((int(r["hr"]), int(r["temporadas"])))
ultima = max(r["capa"] for r in res)
temporadas = [(r["jugador"], r["temporada"]) for r in res if r["capa"] == ultima]
p = {(r["capa"], r["jugador"], r["temporada"]): max(float(r["pvalue"]), 0.5 / N_SIM) for r in res}   # 0 → "menos de 1 en N"
nombre = lambda j, t: f"{j.split()[-1]} {t}"
uno_en = lambda x: "menos de 1 en un millón" if x < 1 / N_SIM else f"1 en {round(1 / x):,}".replace(",", " ")

# ---- tabla
print(f"{'temporada':24s} {'HR':>3s} {'esperaba':>9s}  {'rango 80 %':>10s}  {'p-value':>9s}  {'qué tan raro':>24s}  {'veces esperadas en 7 144':>8s}")
orden = sorted(temporadas, key=lambda x: p[(ultima, *x)])
for j, t in orden:
    r = next(r for r in res if r["capa"] == ultima and r["jugador"] == j and r["temporada"] == t)
    x = p[(ultima, j, t)]
    print(f"{j + ' ' + t:24s} {r['real']:>3s} {float(r['media']):9.1f}  {r['p10']:>4s} a {r['p90']:>3s}  {x:9.1e}  {uno_en(x):>24s}  {N_TEMPORADAS * x:8.2f}")

# ---- 1. p-value por capa
fig, eje = plt.subplots(figsize=(10, 7))
capas = sorted({r["capa"] for r in res})
destacadas = [x for x in orden[:5]]
import math
etiquetas = []
for j, t in temporadas:
    ys = [p[(c, j, t)] for c in capas]
    color, ancho = ("#D86A3C", 2.4) if (j, t) in destacadas else ("#B8B8B8", 1.2)
    eje.plot(range(len(capas)), ys, "o-", color=color, linewidth=ancho, markersize=4)
    etiquetas.append([math.log10(ys[-1]), nombre(j, t), (j, t) in destacadas])
etiquetas.sort()                                     # separar etiquetas que caerían encimadas
for i in range(1, len(etiquetas)):
    etiquetas[i][0] = max(etiquetas[i][0], etiquetas[i - 1][0] + 0.13)
for y, texto, dest in etiquetas:
    eje.text(len(capas) - 1 + 0.08, 10 ** y, texto, va="center", fontsize=9, color="black" if dest else "#808080")
eje.set_yscale("log")
eje.invert_yaxis()                                  # más arriba = más anómala
eje.set_xticks(range(len(capas)), [f"capa {c}" for c in capas])
eje.set_xlim(-0.2, len(capas) - 0.2)
eje.set_ylabel("p-value (escala logarítmica, más arriba = más anómala)")
eje.set_title("¿Qué tan anómala fue cada temporada según cada capa?\n(si ninguna de las 1 000 000 simuladas llegó, se dibuja en 5e-7)")

# ---- 2. ranking con el modelo completo
fig, eje = plt.subplots(figsize=(10, 7))
ys = list(range(len(orden)))[::-1]
vals = [1 / p[(ultima, j, t)] for j, t in orden]
eje.barh(ys, vals, color="#D86A3C")
for y, v, (j, t) in zip(ys, vals, orden):
    eje.text(v * 1.15, y, uno_en(1 / v), va="center", fontsize=9)
eje.axvline(N_TEMPORADAS, color="black", linestyle="--")
eje.text(N_TEMPORADAS * 1.1, -0.4, "1 en 7 144: tan raro como para esperar\nuna así entre todas las temporadas de regulares", fontsize=9, va="bottom")
eje.set_xscale("log")
eje.set_yticks(ys, [f"{j} {t}" for j, t in orden])
eje.set_xlabel("1 en N temporadas (escala logarítmica)")
eje.set_title(f"Ranking de anomalías con el modelo completo (capa {ultima})")
eje.set_xlim(1, N_SIM * 8)
eje.set_ylim(-1.2, len(orden) - 0.4)

# ---- 3. las campanas
cols = 5
filas = (len(orden) + cols - 1) // cols
fig, ejes = plt.subplots(filas, cols, figsize=(18, 3.4 * filas), squeeze=False)
for i, (j, t) in enumerate(orden):
    eje = ejes[i // cols][i % cols]
    h = sorted(hist[(ultima, j, t)])
    n = sum(c for _, c in h)
    eje.bar([k for k, _ in h], [c / n for _, c in h], width=1.0, color="#D86A3C")
    r = next(r for r in res if r["capa"] == ultima and r["jugador"] == j and r["temporada"] == t)
    eje.axvline(int(r["real"]), color="#1F5FA8", linestyle="--", linewidth=2)
    eje.set_title(f"{j} {t}: {r['real']} HR\np = {float(r['pvalue']):.1e}", fontsize=10)
    eje.set_yticks([])
for k in range(len(orden), filas * cols):
    ejes[k // cols][k % cols].set_visible(False)
fig.suptitle(f"Modelo completo (capa {ultima}): temporadas simuladas y lo que de verdad pegó (línea azul)")
fig.tight_layout()
plt.show()                     # abre las tres ventanas; no guarda nada
