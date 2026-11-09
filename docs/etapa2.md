# Etapa 2 · Motor Monte Carlo en C y modelos v1 y v2

**Objetivo:** programar en C el nivel 1 del modelo (el talento del jugador) y el motor de simulación, y medir qué tan inusuales fueron cinco temporadas históricas.

---

## Cómo ejecutarlo

```bash
cd engine
make
cd ..
engine/build/aod analizar --jugador "Barry Bonds" --temporada 2001
engine/build/aod casos
```

Opciones de `analizar`: `--modelo v1|v2`, `--n` (simulaciones, por defecto 1 000 000), `--semilla`, `--sin-era`, `--sin-edad` y `--json`.

El motor lee tres archivos pequeños de `data/inputs/` (832 KB, incluidos en el repositorio). Por eso **funciona sin generar la base de datos**. Para regenerarlos: `python etl/export_inputs.py`.

### Compilar en cada sistema

| Sistema | Qué instalar | Comando |
|---|---|---|
| Linux | `gcc` y `make` (normalmente ya vienen) | `make` |
| Windows | MSYS2 con `pacman -S mingw-w64-ucrt-x86_64-gcc make`, o WSL | `make` (desde la terminal de MSYS2) |
| macOS | `xcode-select --install` | `make OPENMP=0` (el compilador de Apple no trae OpenMP) |

Sin OpenMP el motor funciona igual, solo que con un hilo: tarda unos 2 s en lugar de 0.5 s.

---

## Qué hace cada archivo

| Archivo | Qué hace, en palabras | Ecuación de la lección |
|---|---|---|
| `etl/export_inputs.py` | Saca de la base de datos los totales de cada jugador y de cada liga. No calcula nada del modelo. | — |
| `engine/src/data.c` | Lee esos CSV y busca jugadores por nombre o por ID. | — |
| `engine/src/prior.c` | Con los bateadores regulares de las 3 temporadas anteriores, calcula el promedio de la liga, separa el talento real de la suerte y construye el prior Beta. | Ec. 8–9 |
| `engine/src/aging.c` | Mide cuánto cambia la tasa de HR de un año al siguiente según la edad y le ajusta una recta: la curva de edad. | Ec. 14 |
| `engine/src/talent.c` | Traduce las 3 temporadas previas al ambiente de la liga actual, las suma con pesos 1, 0.8 y 0.6 al prior y aplica el ajuste por edad. | Ec. 10–14 |
| `engine/src/rng.c` | Genera números aleatorios y sortea distribuciones (uniforme, normal, Gamma y, con dos Gamma, la Beta). | Ec. 22 |
| `engine/src/montecarlo.c` | Simula N temporadas en paralelo: sortea el talento, juega cada turno y cuenta los jonrones en un histograma. | Ec. 22 |
| `engine/src/stats.c` | Del histograma saca la media, la desviación, los percentiles, el p-value y su error. | Ec. 23–24 |
| `engine/src/main.c` | El programa que se usa desde la terminal. | — |
| `tests/verify_etapa2.py` | Recalcula todo de forma independiente y exacta y lo compara con el motor. | — |

### Los dos modelos

- **v1 · Talento fijo:** el talento es la tasa ponderada de las 3 temporadas anteriores, sin incertidumbre ni ajustes. Equivale a una Binomial. Es la línea base contra la que se comparan las demás versiones.
- **v2 · Talento bayesiano:** el talento es una distribución Beta (prior de la liga + temporadas previas, con ajuste por era y por edad) y se sortea en cada simulación.

### Decisiones de diseño

- **Solo información anterior.** Para analizar la temporada *t*, el prior, la curva de edad y el talento usan únicamente temporadas hasta *t − 1*. Así no hay fuga de datos.
- **Reproducible.** Las simulaciones se reparten en bloques de 4 096, y cada bloque tiene su propia secuencia aleatoria derivada de la semilla. Con la misma semilla el resultado es idéntico, se use 1 hilo o 16.
- **Rápido.** El talento se sortea como odds (X/Y con dos Gamma) y cada turno se decide con una multiplicación y una comparación, sin exponenciales ni divisiones. Un millón de temporadas tarda ~0.5 s con 4 núcleos.
- **El contexto ya tiene su lugar.** El motor acepta un factor por turno, *e<sup>c<sub>j</sub></sup>*. Por ahora vale 1 (contexto neutro); en la etapa 3 se llenará con estadio, pitcher y clima.

---

## Cómo se comprobó que el motor está bien

`python tests/verify_etapa2.py` ejecuta **38 comprobaciones** y todas pasan:

1. **Mismo modelo, otro código.** Un script en Python, escrito aparte, recalcula el prior, la posterior, la curva de edad y el talento. Coinciden con el motor en C hasta el cuarto decimal.
2. **Simulación contra cálculo exacto.** La distribución de HR se calcula también de forma exacta, integrando sobre todos los valores posibles del talento. La media, la desviación y el p-value del Monte Carlo quedan dentro de su error estadístico.
3. **v1 contra la Binomial.** Con el talento fijo, la simulación da el mismo p-value que la fórmula de la Binomial.
4. **Reproducibilidad.** La misma semilla da el mismo resultado exacto con 1 o con varios hilos; otra semilla da otro resultado, dentro del error esperado.

La prueba sirvió para algo: en su primera versión daba diferencias en el ajuste por edad. El error estaba **en el script de prueba**, que buscaba la fecha de nacimiento por nombre. Hay 67 nombres repetidos entre los jugadores (por ejemplo, dos "Josh Bell"). El motor la busca por ID, que es lo correcto.

---

## Resultados

Con N = 1 000 000 simulaciones por caso:

| Jugador | Temporada | HR reales | v1: media | v1: IC 80 % | v1: p-value | v2: media | v2: IC 80 % | v2: p-value |
|---|---|---|---|---|---|---|---|---|
| Barry Bonds | 2001 | 73 | 47.6 | 39–56 | 2.3·10⁻⁴ | 42.1 | 33–52 | **1.3·10⁻⁴** (1 en 7 600) |
| Mark McGwire | 1998 | 70 | 62.1 | 53–72 | 0.16 | 47.9 | 38–58 | **5.5·10⁻³** (1 en 180) |
| Sammy Sosa | 1998 | 66 | 43.0 | 35–51 | 4.6·10⁻⁴ | 41.3 | 32–51 | **1.6·10⁻³** (1 en 630) |
| Aaron Judge | 2022 | 62 | 43.8 | 36–52 | 4.1·10⁻³ | 33.5 | 25–43 | **2.7·10⁻⁴** (1 en 3 700) |
| Shohei Ohtani | 2024 | 54 | 47.7 | 39–56 | 0.19 | 42.9 | 33–53 | **8.7·10⁻²** (1 en 12) |

### Cómo leerlos

- **Bonds 2001 es la temporada más inusual de las cinco:** el modelo esperaba ~42 HR y pegó 73.
- **Judge 2022 también es muy inusual según v2.** Su talento previo se estimó con 2019–2021: 2020 fue una temporada de 60 partidos y en 2019 la liga pegó muchos más jonrones que en 2022, así que el ajuste por era reduce sus números previos.
- **McGwire 1998 cambia mucho entre v1 y v2.** v1 solo mira sus 58 HR de 1997 y casi no lo encuentra raro. v2 lo acerca a la liga, tiene en cuenta que tenía 34 años y ajusta por era, y entonces sí resulta inusual.
- **Ohtani 2024 no es una anomalía:** pasa en 1 de cada 12 temporadas simuladas.
- **v1 y v2 no siempre van en la misma dirección.** v2 no es "más estricto": es más completo. Qué versión predice mejor lo dirá el backtesting de la etapa 4.

> El p-value mide qué tan improbable era la temporada según lo que se sabía antes de ella. No dice por qué ocurrió.

---

## Qué falta (etapas siguientes)

- **Etapa 3:** el contexto de cada turno (estadio, pitcher, mano, clima) con regresión logística en C, y el modo predicción con turnos aleatorios.
- **Etapa 4:** backtesting, comparación de las versiones y la web local.
