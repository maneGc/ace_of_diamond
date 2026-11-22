"""
graficar_simulacion.py — Capa 0: grafica las temporadas simuladas por engine/src/capa0/simular.c.

Uso:  python experimentos/graficar_simulacion.py experimentos/capa0/bondb001_2001.csv
"""
import csv
import sys

import matplotlib.pyplot as plt

filas = list(csv.DictReader(open(sys.argv[1], encoding="utf-8")))
hr = [int(r["hr"]) for r in filas]
temporadas = [int(r["temporadas"]) for r in filas]
n = sum(temporadas)
media = sum(k * c for k, c in zip(hr, temporadas)) / n
f0 = filas[0]

plt.bar(hr, [c / n for c in temporadas], color="#D86A3C")
plt.axvline(media, color="black", label=f"promedio {media:.1f} HR")
real = int(f0["real"])
if real >= 0:                  # comparar con lo que de verdad pasó
    p_value = sum(c for k, c in zip(hr, temporadas) if k >= real) / n
    plt.axvline(real, color="#1F5FA8", linestyle="--", label=f"real {real} HR · p-value = {p_value:.2e}")
plt.title(f"{f0['jugador']} {f0['temporada']} · θ = {100 * float(f0['theta']):.2f} % · {f0['pa']} turnos · {n} temporadas")
plt.xlabel("HR en la temporada")
plt.ylabel("proporción de temporadas")
plt.legend(loc="upper left")
plt.show()                     # abre la gráfica en una ventana; no guarda nada
