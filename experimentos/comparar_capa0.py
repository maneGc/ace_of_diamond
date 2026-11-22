"""
comparar_capa0.py — Capa 0 contra la realidad, con el CSV de engine/src/capa0/comparar.c.

Imprime el error promedio y qué tan seguido lo real cae dentro del rango del 80 %, y muestra:
  izquierda: HR esperados contra HR reales (la diagonal es el acierto perfecto)
  derecha:   cuántas temporadas caen abajo, dentro o arriba del rango del 80 % (lo ideal: 10 / 80 / 10)

Uso:  python experimentos/comparar_capa0.py
"""
import csv

import matplotlib.pyplot as plt

filas = list(csv.DictReader(open("experimentos/capa0/comparacion.csv", encoding="utf-8")))
n = len(filas)
esperado = [float(r["media"]) for r in filas]
real = [int(r["real"]) for r in filas]
error = sum(abs(e - x) for e, x in zip(esperado, real)) / n
abajo = sum(int(r["real"]) < int(r["p10"]) for r in filas) / n
arriba = sum(int(r["real"]) > int(r["p90"]) for r in filas) / n
dentro = 1 - abajo - arriba

print(f"{n} temporadas")
print(f"error promedio: {error:.2f} HR")
print(f"lo real cae dentro del rango del 80 %: {100 * dentro:.1f} %  (abajo {100 * abajo:.1f} %, arriba {100 * arriba:.1f} %)")

fig, (izq, der) = plt.subplots(1, 2, figsize=(12, 5))
izq.scatter(esperado, real, s=4, alpha=0.2, color="#D86A3C")
izq.plot([0, 75], [0, 75], "k--")
izq.set_xlabel("HR esperados (promedio de la simulación)")
izq.set_ylabel("HR reales")
izq.set_title(f"Capa 0: {n} temporadas · error promedio {error:.2f} HR")

der.bar(["abajo", "dentro", "arriba"], [0.10, 0.80, 0.10], color="#D9D9D9", label="lo ideal")
der.bar(["abajo", "dentro", "arriba"], [abajo, dentro, arriba], width=0.5, color="#D86A3C", label="capa 0")
der.set_title("¿Dónde cae lo real respecto al rango del 80 %?")
der.legend()
plt.show()                     # abre la gráfica en una ventana; no guarda nada
