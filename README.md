# Ace of Diamond

| | |
|---|---|
| **Autor** | Emmanuel Gutierrez Cruz |
| **Materia** | Modelado y Simulación |
| **Universidad** | ENES Morelia |
| **Contacto** | emmanuelgc241@gmail.com |

## Objetivo
Simulador estocástico en C para analizar récords de Home Runs en la MLB. Utiliza el método de Monte Carlo para simular hasta un millon de temporadas posibles de un jugador y responder dos preguntas:

1. **Análisis:** ¿qué tan inusual fue una temporada histórica (p. ej. Bonds 2001, Judge 2022)? ¿Era predecible o fue una anomalía estadística?
2. **Predicción:** ¿cuántos jonrones podría pegar un jugador en una temporada que todavía no empieza?

## ¿Qué hace?
* **Simulación en C:** el motor Monte Carlo y todo el modelo matemático están escritos en C.
* **Modelo en tres niveles:**
  * **Talento del jugador**, estimado con estadística bayesiana (distribución Beta) a partir de sus temporadas anteriores.
  * **Contexto de cada turno al bate:** estadio (*Park Factors*), pitcher rival, mano del pitcher y clima, combinados con regresión logística.
  * **Tiempo de juego:** cuántas apariciones al bate tendrá (en el modo predicción).
* **Validación:** backtesting contra temporadas reales y comparación entre versiones del modelo.
* **Web local:** gráficas y resultados en el navegador.

## Datos
Datos públicos de **Retrosheet** (cada jugada de cada partido de la MLB, 1995–2025) y del **Chadwick Baseball Bureau Register** (IDs y fechas de nacimiento).

La base de datos no se incluye en el repositorio (pesa ~665 MB). Se genera con un comando, que solo necesita Python 3.10+ y tarda unos 2 minutos:

```bash
python etl/run_all.py
```

## Créditos
*The information used here was obtained free of charge from and is copyrighted by Retrosheet. Interested parties may contact Retrosheet at "www.retrosheet.org".*

Licencia del código: GPL v3 (ver `LICENSE`).
