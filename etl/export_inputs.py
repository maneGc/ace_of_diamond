"""
export_inputs.py — Puente entre la base de datos y el motor en C.

Exporta de data/aod.sqlite tres archivos CSV pequeños (~2 MB en total) a data/inputs/:
  - player_seasons.csv : HR y PA de cada bateador por temporada, su liga principal y "parque":
                         el promedio, sobre sus turnos, del logaritmo del factor de estadio (capa 4)
  - league_seasons.csv : HR y PA de cada liga por temporada (el ambiente λ_t)
  - players.csv        : nombre y fecha de nacimiento (para calcular la edad)

Este script NO hace cálculos del modelo: solo entrega los datos.
Todo el modelo matemático está en C (engine/).
Como los CSV se incluyen en el repositorio, quien descargue el proyecto
puede usar el motor sin generar la base de datos completa.
Uso:  python etl/export_inputs.py
"""
import csv
import math
import sqlite3
from collections import defaultdict

from config import DATA_DIR, DB_PATH

OUT = DATA_DIR / "inputs"


def write(name, header, rows):
    OUT.mkdir(parents=True, exist_ok=True)
    with open(OUT / name, "w", newline="", encoding="utf-8") as fh:
        w = csv.writer(fh)
        w.writerow(header)
        w.writerows(rows)
    print(f"  {name}: {len(rows):,} filas")


def factores_estadio(con, k=3000):
    """Factor de jonrones de cada estadio por temporada: HR/PA en los juegos del equipo local
    en casa ÷ HR/PA en sus juegos de visitante (así se cancela la calidad del equipo).
    Se suman la temporada y sus vecinas (±1) y se encoge hacia 1 con k PA imaginarios."""
    casa, fuera, local = defaultdict(lambda: [0, 0]), defaultdict(lambda: [0, 0]), defaultdict(lambda: defaultdict(int))
    for park, s, home, away, n, hr in con.execute("""
            SELECT g.park_id, g.season, g.home_team, g.away_team, COUNT(*), SUM(pa.is_hr)
            FROM plate_appearances pa JOIN games g ON g.game_id = pa.game_id GROUP BY 1, 2, 3, 4"""):
        casa[(park, s)][0] += n; casa[(park, s)][1] += hr; local[(park, s)][home] += n
        fuera[(away, s)][0] += n; fuera[(away, s)][1] += hr

    def pf(park, s):
        hn = hh = rn = rh = 0
        for d in (-1, 0, 1):
            if (park, s + d) not in casa:
                continue
            equipo = max(local[(park, s + d)].items(), key=lambda x: x[1])[0]
            a, b = casa[(park, s + d)]
            c, e = fuera.get((equipo, s + d), (0, 0))
            hn += a; hh += b; rn += c; rh += e
        if not rn:
            return 1.0
        tasa_fuera = rh / rn
        return (hh + k * tasa_fuera) / (hn * tasa_fuera + k * tasa_fuera)

    return {key: math.log(pf(*key)) for key in casa}


def parque_por_temporada(con):
    """Promedio de log(factor de estadio) sobre los turnos de cada bateador en cada temporada."""
    lpf = factores_estadio(con)
    suma, n = defaultdict(float), defaultdict(int)
    for bat, s, park, c in con.execute("""
            SELECT pa.batter_id, pa.season, g.park_id, COUNT(*)
            FROM plate_appearances pa JOIN games g ON g.game_id = pa.game_id GROUP BY 1, 2, 3"""):
        suma[(bat, s)] += c * lpf.get((park, s), 0.0)
        n[(bat, s)] += c
    return {key: suma[key] / n[key] for key in n}


def main():
    con = sqlite3.connect(DB_PATH)

    # Liga principal del bateador en cada temporada = donde tuvo más PA
    # (si lo cambiaron de liga a mitad de temporada).
    rows = con.execute("""
        WITH by_league AS (
          SELECT pa.batter_id, pa.season, t.league, COUNT(*) AS n
          FROM plate_appearances pa
          JOIN teams t ON t.season = pa.season AND t.team_id = pa.bat_team
          GROUP BY pa.batter_id, pa.season, t.league),
        main_league AS (
          SELECT batter_id, season, league,
                 ROW_NUMBER() OVER (PARTITION BY batter_id, season ORDER BY n DESC) AS rk
          FROM by_league)
        SELECT ps.player_id, ps.season, ps.pa, ps.hr, ml.league
        FROM player_seasons ps
        JOIN main_league ml ON ml.batter_id = ps.player_id AND ml.season = ps.season AND ml.rk = 1
        ORDER BY ps.player_id, ps.season""").fetchall()
    parque = parque_por_temporada(con)
    rows = [(*r, f"{parque.get((r[0], r[1]), 0.0):.5f}") for r in rows]
    write("player_seasons.csv", ["player_id", "season", "pa", "hr", "league", "parque"], rows)

    rows = con.execute("SELECT season, league, pa, hr FROM league_seasons ORDER BY season, league").fetchall()
    write("league_seasons.csv", ["season", "league", "pa", "hr"], rows)

    rows = con.execute("""
        SELECT player_id, REPLACE(name_first, ',', ' '), REPLACE(name_last, ',', ' '), COALESCE(birth_date, '')
        FROM players WHERE player_id IN (SELECT DISTINCT player_id FROM player_seasons)
        ORDER BY player_id""").fetchall()
    write("players.csv", ["player_id", "name_first", "name_last", "birth_date"], rows)
    con.close()


if __name__ == "__main__":
    main()
