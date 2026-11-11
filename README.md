# Ace of Diamond

**Autor:** Emmanuel Gutierrez Cruz

**Materia:** Modelado y Simulación

**Universidad:** ENES Morelia

**Contacto:** emmanuelgc241@gmail.com


## Objetivo
Simulador estocástico en C para analizar récords de Home Runs en la MLB. Utiliza el método de Monte Carlo para simular hasta un millón de temporadas posibles de un jugador y responder dos preguntas:

1. **Análisis:** ¿qué tan inusual fue una temporada histórica (p. ej. Bonds 2001, Judge 2022)? ¿Era predecible o fue una anomalía estadística?
2. **Predicción:** ¿cuántos jonrones podría pegar un jugador en una temporada que todavía no empieza?

## Modelo matemático
Cada turno al bate se modela como un experimento aleatorio con una pequeña probabilidad de jonrón, y una temporada es la suma de todos sus turnos:

```math
HR = \sum_{j=1}^{PA} Y_j, \qquad Y_j \sim \text{Bernoulli}(p_j)
```

La probabilidad de jonrón en cada turno, $`p_j`$, combina el talento del jugador con el contexto de ese turno:

```math
\text{logit}(p_j) = \text{logit}(\theta) + c_j
```

donde el talento del jugador sigue una distribución Beta:

```math
\theta \sim \text{Beta}(\alpha, \beta)
```

1. **Talento del jugador ($`\theta`$).** No se conoce con exactitud, así que se modela como una distribución Beta. 
2. **Contexto del turno ($`c_j`$).** El estadio, el pitcher rival y el clima aumentan o reducen la probabilidad en cada turno. Se combinan en escala logit para que la probabilidad siempre quede entre 0 y 1.
3. **Simulación Monte Carlo.** Se repite la temporada $`N = 10^6`$ veces: en cada una se sortea un talento posible y se juega cada turno. La probabilidad de un resultado es la proporción de temporadas simuladas en que ocurre:

```math
\hat{P}(HR \ge x) = \frac{\text{temporadas simuladas con } HR \ge x}{N}
```

De ahí salen el número esperado de jonrones, un intervalo de confianza y el p-value, que indica qué tan inusual fue la temporada real.

## Datos
Datos públicos de **Retrosheet** (cada jugada de cada partido de la MLB, 1995–2025) y del **Chadwick Baseball Bureau Register** (IDs y fechas de nacimiento).

## Créditos
*The information used here was obtained free of charge from and is copyrighted by Retrosheet. Interested parties may contact Retrosheet at "[www.retrosheet.org](https://www.retrosheet.org)".*

Licencia del código: GPL v3 (ver `LICENSE`).
