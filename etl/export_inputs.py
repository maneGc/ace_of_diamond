"""
export_inputs.py — Puente entre la base de datos y el motor en C.

Exporta de data/aod.sqlite tres archivos CSV pequeños (~2 MB en total) a data/inputs/:
  - player_seasons.csv : HR y PA de cada bateador por temporada, y su liga principal
  - league_seasons.csv : HR y PA de cada liga por temporada (el ambiente λ_t)
  - players.csv        : nombre y fecha de nacimiento (para calcular la edad)

Este script NO hace cálculos del modelo: solo entrega los datos.
Todo el modelo matemático está en C (engine/).
Como los CSV se incluyen en el repositorio, quien descargue el proyecto
puede usar el motor sin generar la base de datos completa.
Uso:  python etl/export_inputs.py
"""
import csv
import sqlite3

from config import DATA_DIR, DB_PATH

OUT = DATA_DIR / "inputs"


def write(name, header, rows):
    OUT.mkdir(parents=True, exist_ok=True)
    with open(OUT / name, "w", newline="", encoding="utf-8") as fh:
        w = csv.writer(fh)
        w.writerow(header)
        w.writerows(rows)
    print(f"  {name}: {len(rows):,} filas")


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
    write("player_seasons.csv", ["player_id", "season", "pa", "hr", "league"], rows)

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
