"""
comparar_capas.py — Las capas contra la realidad, con el CSV de engine/src/experimentos/comparar.c.

Para cada capa dibuja las mismas dos gráficas de la capa 0, una fila por capa y con los mismos ejes:
  izquierda: HR esperados contra HR reales (la diagonal es el acierto perfecto)
  derecha:   cuántas temporadas caen abajo, dentro o arriba del rango del 80 % (lo ideal: 10 / 80 / 10)
e imprime el error promedio y dónde cae lo real de cada capa.

Uso:  python experimentos/comparar_capas.py                       (lee experimentos/capa3/comparacion.csv)
      python experimentos/comparar_capas.py otra/comparacion.csv
"""
import csv
import sys
from collections import defaultdict

import matplotlib.pyplot as plt

ruta = sys.argv[1] if len(sys.argv) > 1 else "experimentos/capa3/comparacion.csv"
capas = defaultdict(list)
for r in csv.DictReader(open(ruta, encoding="utf-8")):
    capas[r["capa"]].append(r)                                   # "0", "1", …, "3 predicción"

fig, ejes = plt.subplots(len(capas), 2, figsize=(12, 5 * len(capas)), squeeze=False)
tope = max(float(r["media"]) for filas in capas.values() for r in filas) + 5     # mismo eje x para todas
print("          capa   temporadas   error promedio   dentro del 80 %   abajo    arriba")
for (izq, der), (capa, filas) in zip(ejes, capas.items()):
    n = len(filas)
    esperado = [float(r["media"]) for r in filas]
    real = [int(r["real"]) for r in filas]
    error = sum(abs(e - x) for e, x in zip(esperado, real)) / n
    abajo = sum(int(r["real"]) < int(r["p10"]) for r in filas) / n
    arriba = sum(int(r["real"]) > int(r["p90"]) for r in filas) / n
    dentro = 1 - abajo - arriba
    print(f"{capa:>14s}   {n:10d}   {error:11.2f} HR   {100 * dentro:13.1f} %   {100 * abajo:5.1f} %  {100 * arriba:5.1f} %")

    izq.scatter(esperado, real, s=4, alpha=0.2, color="#D86A3C")
    izq.plot([0, 75], [0, 75], "k--")
    izq.set_xlim(0, tope)
    izq.set_xlabel("HR esperados (promedio de la simulación)")
    izq.set_ylabel("HR reales")
    izq.set_title(f"Capa {capa}: {n} temporadas · error promedio {error:.2f} HR")

    der.bar(["abajo", "dentro", "arriba"], [0.10, 0.80, 0.10], color="#D9D9D9", label="lo ideal")
    der.bar(["abajo", "dentro", "arriba"], [abajo, dentro, arriba], width=0.5, color="#D86A3C", label=f"capa {capa}")
    der.set_ylim(0, 0.85)
    der.set_title(f"Capa {capa}: ¿dónde cae lo real respecto al rango del 80 %?")
    der.legend()

plt.tight_layout()
plt.show()                     # abre la gráfica en una ventana; no guarda nada
