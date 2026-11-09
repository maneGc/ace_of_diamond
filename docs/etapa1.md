# Etapa 1 · Datos, explicada en palabras

**Objetivo:** tener en la computadora cada aparición al bate (PA) de la MLB entre 1995 y 2025, con quién bateó, quién lanzó, con qué mano, en qué estadio, con qué clima y si terminó en jonrón. Es la materia prima de todo el modelo.

**Resultado:** `data/aod.sqlite`, con **5 593 806 apariciones al bate** de **73 044 partidos**, verificada contra cifras oficiales.

---

## El recorrido en 4 pasos (ETL + verificación)

```
Retrosheet (texto) ──descargar──► data/raw/ ──transformar──► filas ──cargar──► aod.sqlite ──verificar──► ✓
     download.py                        parse_events.py           build_db.py            validate.py
```

### 1. Descargar (`download.py`)

Para cada temporada baja tres tipos de archivo:

| Archivo | Ejemplo | Qué contiene |
|---|---|---|
| `TEAMaaaa` | `TEAM2001` | Los equipos y su liga (A = Americana, N = Nacional) |
| `aaaaEEE.EVA` / `.EVN` | `2001SFN.EVN` | Todas las jugadas de los partidos del equipo como local |
| `EEEaaaa.ROS` | `SFN2001.ROS` | El roster: con qué mano batea y lanza cada jugador |

Además baja la lista de estadios y el **Chadwick Register**, que traduce el ID de Retrosheet (`bondb001`) a otros sistemas y trae la fecha de nacimiento.

Son 924 archivos de jugadas: 28 equipos de 1995 a 1997 y 30 desde 1998. No falta ninguno.

> **Idempotente:** si se ejecuta dos veces, no descarga de nuevo lo que ya existe.

### 2. Transformar (`parse_events.py`)

Un archivo de Retrosheet es texto con una línea por registro:

```
id,SFN200104020                       ← empieza un partido
info,temp,59                          ← clima, estadio, fecha…
start,bondb001,"Barry Bonds",1,3,7    ← alineación inicial
sub,...                               ← sustituciones
play,1,1,bondb001,22,BCFB,HR/F89      ← una jugada
```

El parser recorre las líneas y hace cuatro cosas:

1. **Recuerda quién está lanzando.** Cada vez que aparece un `start` o un `sub` con posición 1 (pitcher), anota el nuevo pitcher de ese equipo. Así sabe contra quién batea cada bateador.
2. **Decide si la jugada es una aparición al bate.** Algunas jugadas ocurren *durante* el turno sin terminarlo: un robo de base (`SB`), un wild pitch (`WP`), un balk (`BK`)… Esas se descartan. Todas las demás terminan el turno.
3. **Detecta el jonrón.** El evento empieza por `HR` (o `H` en la notación antigua), pero no por `HP`, que es "golpeado por lanzamiento".
4. **Resuelve las manos.** Toma la mano de bateo del roster. Si el bateador es ambidiestro (`B`), batea del lado contrario al pitcher: contra un derecho batea zurdo. Ese es el dato que necesita el término *platoon* del modelo.

**Detalle importante:** cuando Retrosheet no conoce la temperatura escribe `0`. El parser lo convierte en **NULL** ("desconocido"), porque un 0 haría creer al modelo que hubo partidos bajo cero (ver la demostración en la guía).

### 3. Cargar (`build_db.py`)

Crea la base de datos **SQLite**: un solo archivo, incluido con Python, sin servidor que instalar.

| Tabla | Filas | Para qué sirve en el modelo |
|---|---|---|
| `plate_appearances` | 5,6 M | La tabla central: una fila por turno. `is_hr` es la variable que el modelo intenta explicar. |
| `games` | 73 044 | Fecha, estadio, temperatura y viento de cada partido (nivel 2, contexto) |
| `players` | 8 031 | Nombre, manos y fecha de nacimiento (para la edad) |
| `parks` | todos | Nombre de cada estadio |
| `teams` | por temporada | Liga de cada equipo |
| `player_seasons` | calculada | HR y PA de cada bateador por año → **talento θ (nivel 1)** |
| `pitcher_seasons` | calculada | HR permitidos por cada pitcher → **término del pitcher (nivel 2)** |
| `league_seasons` | calculada | HR y PA de cada liga por año → **ambiente λ<sub>t</sub> y prior de la liga** |

Las tres tablas "calculadas" se obtienen sumando la tabla central con SQL. Por eso ya no hace falta la base de datos Lahman: todo sale de una sola fuente.

### 4. Verificar (`validate.py`)

Un parser puede tener errores silenciosos, como contar mal un tipo de jugada. Para detectarlos, se compara la base de datos con cifras oficiales muy conocidas:

| Comprobación | Base de datos | Oficial |
|---|---|---|
| Bonds 2001 · HR / PA / HR en casa | 73 / 664 / 37 | 73 / 664 / 37 ✓ |
| McGwire 1998 · HR / PA | 70 / 681 | 70 / 681 ✓ |
| Sosa 1998 · HR / PA | 66 / 722 | 66 / 722 ✓ |
| Judge 2022 · HR / PA | 62 / 696 | 62 / 696 ✓ |
| Ohtani 2024 · HR / PA | 54 / 731 | 54 / 731 ✓ |
| HR de toda la MLB en 2001 / 2019 | 5 458 / 6 776 | 5 458 / 6 776 ✓ |

Coinciden **exactamente**. Además:
- el 100 % de las PA tiene pitcher y manos identificados;
- solo el 0,8 % de los partidos no tiene temperatura, y esos quedan como NULL.

---

## Preguntas que te pueden hacer

**¿Por qué Retrosheet?** Es la única fuente gratuita con *cada jugada* de cada partido, incluido quién lanzaba y el clima. Con totales por temporada no se podría modelar el contexto de cada turno.

**¿Por qué desde 1995?** Deja 3 temporadas previas para McGwire y Sosa (1998) y Bonds (2001), y suficientes años para el backtesting. Ampliar el rango solo requiere cambiar `FIRST_SEASON` en `config.py`.

**¿Cómo sabes que los datos están bien?** Por la verificación del paso 4: si el parser tuviera un error en cómo cuenta turnos o jonrones, los totales no coincidirían exactamente con las cifras oficiales.

**¿Por qué 1995 y 2020 tienen menos turnos?** Fueron temporadas acortadas: la huelga de 1994–95 y la pandemia de 2020 (60 partidos).
