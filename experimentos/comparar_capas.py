"""
comparar_capas.py — Las capas contra la realidad, con el CSV de engine/src/experimentos/comparar.c.

Imprime, para cada capa, el error promedio y dónde cae lo real respecto al rango del 80 %, y muestra:
  izquierda: abajo / dentro / arriba del rango del 80 % por capa (lo ideal: 10 / 80 / 10)
  derecha:   regresión a la media: promedio real contra promedio esperado en 8 grupos, por capa

Uso:  python experimentos/comparar_capas.py                       (lee experimentos/capa1/comparacion.csv)
      python experimentos/comparar_capas.py otra/comparacion.csv
"""
import csv
import sys
from collections import defaultdict

import matplotlib.pyplot as plt

ruta = sys.argv[1] if len(sys.argv) > 1 else "experimentos/capa1/comparacion.csv"
capas = defaultdict(list)
for r in csv.DictReader(open(ruta, encoding="utf-8")):
    capas[int(r["capa"])].append(r)

fig, (izq, der) = plt.subplots(1, 2, figsize=(13, 5))
ancho = 0.8 / (len(capas) + 1)
izq.bar([x - 0.4 + ancho / 2 for x in range(3)], [0.10, 0.80, 0.10], width=ancho, color="#D9D9D9", label="lo ideal")
print("capa   temporadas   error promedio   dentro del 80 %   abajo    arriba")
for i, (capa, filas) in enumerate(sorted(capas.items())):
    n = len(filas)
    esperado = [float(r["media"]) for r in filas]
    real = [int(r["real"]) for r in filas]
    error = sum(abs(e - x) for e, x in zip(esperado, real)) / n
    abajo = sum(int(r["real"]) < int(r["p10"]) for r in filas) / n
    arriba = sum(int(r["real"]) > int(r["p90"]) for r in filas) / n
    dentro = 1 - abajo - arriba
    print(f"{capa:4d}   {n:10d}   {error:11.2f} HR   {100 * dentro:13.1f} %   {100 * abajo:5.1f} %  {100 * arriba:5.1f} %")
    izq.bar([x - 0.4 + ancho * (i + 1.5) for x in range(3)], [abajo, dentro, arriba], width=ancho,
            label=f"capa {capa} · error {error:.2f} HR")
    orden = sorted(range(n), key=lambda j: esperado[j])          # 8 grupos ordenados por lo esperado
    grupos = [orden[g * n // 8:(g + 1) * n // 8] for g in range(8)]
    der.plot([sum(esperado[j] for j in g) / len(g) for g in grupos],
             [sum(real[j] for j in g) / len(g) for g in grupos], "o-", label=f"capa {capa}")

izq.set_xticks(range(3), ["abajo", "dentro", "arriba"])
izq.set_title("¿Dónde cae lo real respecto al rango del 80 %?")
izq.legend()
der.plot([0, 35], [0, 35], "k--", label="acierto perfecto")
der.set_xlabel("HR esperados (promedio del grupo)")
der.set_ylabel("HR reales (promedio del grupo)")
der.set_title("Regresión a la media: 8 grupos ordenados por lo esperado")
der.legend()
plt.show()                     # abre la gráfica en una ventana; no guarda nada
