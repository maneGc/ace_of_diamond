"""
graficar_simulacion.py — Grafica las temporadas simuladas por engine/src/experimentos/simular.c.
Con varios CSV (por ejemplo, la misma temporada con la capa 0 y con la capa 1) dibuja la misma
gráfica para cada uno, lado a lado y con los mismos ejes.

Uso:  python experimentos/graficar_simulacion.py experimentos/capa1/bondb001_2001.csv
      python experimentos/graficar_simulacion.py experimentos/capa0/bondb001_2001.csv experimentos/capa1/bondb001_2001.csv
"""
import csv
import sys

import matplotlib.pyplot as plt

rutas = sys.argv[1:]
fig, ejes = plt.subplots(1, len(rutas), figsize=(7 * len(rutas), 5), sharex=True, sharey=True, squeeze=False)
for eje, ruta in zip(ejes[0], rutas):
    filas = list(csv.DictReader(open(ruta, encoding="utf-8")))
    hr = [int(r["hr"]) for r in filas]
    temporadas = [int(r["temporadas"]) for r in filas]
    n = sum(temporadas)
    media = sum(k * c for k, c in zip(hr, temporadas)) / n
    f0 = filas[0]

    eje.bar(hr, [c / n for c in temporadas], color="#D86A3C")
    eje.axvline(media, color="black", label=f"promedio {media:.1f} HR")
    real = int(f0["real"])
    if real >= 0:              # comparar con lo que de verdad pasó
        p_value = sum(c for k, c in zip(hr, temporadas) if k >= real) / n
        eje.axvline(real, color="#1F5FA8", linestyle="--", label=f"real {real} HR · p-value = {p_value:.2e}")
    eje.set_title(f"Capa {f0['capa']} · {f0['jugador']} {f0['temporada']} · θ = {100 * float(f0['theta']):.2f} % · {f0['pa']} turnos")
    eje.set_xlabel("HR en la temporada")
    eje.set_ylabel("proporción de temporadas")
    eje.legend(loc="upper left")

plt.tight_layout()
plt.show()                     # abre la gráfica en una ventana; no guarda nada
