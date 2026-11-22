"""
graficar_simulacion.py — Grafica las temporadas simuladas por engine/src/experimentos/simular.c.
Con varios CSV (por ejemplo, la misma temporada con la capa 0 y con la capa 1) los dibuja juntos.

Uso:  python experimentos/graficar_simulacion.py experimentos/capa1/bondb001_2001.csv
      python experimentos/graficar_simulacion.py experimentos/capa0/bondb001_2001.csv experimentos/capa1/bondb001_2001.csv
"""
import csv
import sys

import matplotlib.pyplot as plt

for ruta in sys.argv[1:]:
    filas = list(csv.DictReader(open(ruta, encoding="utf-8")))
    hr = [int(r["hr"]) for r in filas]
    temporadas = [int(r["temporadas"]) for r in filas]
    n = sum(temporadas)
    media = sum(k * c for k, c in zip(hr, temporadas)) / n
    f0 = filas[0]
    real = int(f0["real"])
    etiqueta = f"capa {f0['capa']}: θ = {100 * float(f0['theta']):.2f} %, promedio {media:.1f} HR"
    if real >= 0:              # comparar con lo que de verdad pasó
        p_value = sum(c for k, c in zip(hr, temporadas) if k >= real) / n
        etiqueta += f", p-value = {p_value:.2e}"
    plt.bar(hr, [c / n for c in temporadas], alpha=0.55, label=etiqueta)

if real >= 0:
    plt.axvline(real, color="black", linestyle="--", label=f"real {real} HR")
plt.title(f"{f0['jugador']} {f0['temporada']} · {f0['pa']} turnos · {n} temporadas simuladas")
plt.xlabel("HR en la temporada")
plt.ylabel("proporción de temporadas")
plt.legend(loc="upper left")
plt.show()                     # abre la gráfica en una ventana; no guarda nada
