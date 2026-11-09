"""
build_db.py — Paso 3 de la etapa de datos: CARGAR.

Crea data/aod.sqlite con el esquema del proyecto y lo llena:
  teams, parks, players, games, plate_appearances  ← datos leídos
  player_seasons, pitcher_seasons, league_seasons  ← sumas calculadas con SQL

SQLite es una base de datos en un solo archivo y viene incluida con Python:
no hay que instalar ningún servidor.
Uso:  python etl/build_db.py
"""
import csv
import sqlite3
import time

from config import (DB_PATH, FIRST_SEASON, LAST_SEASON, REGISTER_DIR,
                    REGISTER_FILES, RETRO_DIR)
from parse_events import load_rosters, parse_file

SCHEMA = """
CREATE TABLE teams (                 -- equipo por temporada y su liga
  season SMALLINT, team_id CHAR(3), league CHAR(1), city TEXT, name TEXT,
  PRIMARY KEY (season, team_id));

CREATE TABLE parks (
  park_id CHAR(5) PRIMARY KEY, name TEXT, city TEXT, state TEXT);

CREATE TABLE players (
  player_id VARCHAR(8) PRIMARY KEY,  -- ID de Retrosheet (ej. bondb001)
  bbref_id VARCHAR(9), mlbam_id INTEGER,
  name_first TEXT, name_last TEXT,
  birth_date DATE,                   -- la edad se calcula, no se guarda
  bats CHAR(1), throws CHAR(1));

CREATE TABLE games (
  game_id CHAR(12) PRIMARY KEY, season SMALLINT, game_date DATE,
  park_id CHAR(5) REFERENCES parks, home_team CHAR(3), away_team CHAR(3),
  temp_f SMALLINT NULL,              -- NULL = desconocida (Retrosheet usa 0)
  wind_mph SMALLINT NULL, wind_dir TEXT NULL, is_night BOOLEAN);

CREATE TABLE plate_appearances (     -- tabla de hechos: una fila por PA
  pa_id INTEGER PRIMARY KEY,
  game_id CHAR(12) REFERENCES games, season SMALLINT,
  batter_id VARCHAR(8), pitcher_id VARCHAR(8),
  bat_team CHAR(3), bat_home BOOLEAN,
  bat_side CHAR(1), pitch_hand CHAR(1),
  event TEXT,                        -- resultado corto: S, D, T, HR, K, W, OUT…
  is_hr BOOLEAN);                    -- variable objetivo del modelo (1/0)
"""

AGGREGATES = """
CREATE INDEX ix_pa_batter ON plate_appearances(batter_id, season);
CREATE INDEX ix_pa_pitcher ON plate_appearances(pitcher_id, season);
CREATE INDEX ix_pa_game ON plate_appearances(game_id);

CREATE TABLE player_seasons AS       -- totales del bateador por temporada
  SELECT batter_id AS player_id, season,
         COUNT(*) AS pa, SUM(is_hr) AS hr,
         SUM(CASE WHEN bat_home THEN is_hr ELSE 0 END) AS hr_home,
         COUNT(DISTINCT game_id) AS g
  FROM plate_appearances GROUP BY batter_id, season;

CREATE TABLE pitcher_seasons AS      -- bateadores enfrentados y HR permitidos
  SELECT pitcher_id AS player_id, season,
         COUNT(*) AS batters_faced, SUM(is_hr) AS hr_allowed
  FROM plate_appearances WHERE pitcher_id IS NOT NULL
  GROUP BY pitcher_id, season;

CREATE TABLE league_seasons AS       -- ambiente de la liga (λ_t = hr / pa)
  SELECT pa.season, t.league, COUNT(*) AS pa, SUM(pa.is_hr) AS hr
  FROM plate_appearances pa JOIN teams t ON t.season = pa.season AND t.team_id = pa.bat_team
  GROUP BY pa.season, t.league;
"""


def load_reference(con):
    # Estadios
    with open(RETRO_DIR / "ballparks.csv", encoding="latin-1") as fh:
        rows = [(r["PARKID"], r["NAME"], r["CITY"], r["STATE"]) for r in csv.DictReader(fh)]
    con.executemany("INSERT INTO parks VALUES (?,?,?,?)", rows)

    # Equipos por temporada
    for season in range(FIRST_SEASON, LAST_SEASON + 1):
        for line in (RETRO_DIR / str(season) / f"TEAM{season}").read_text(encoding="latin-1").splitlines():
            p = line.strip().split(",")
            if len(p) >= 4 and p[1] in ("A", "N"):
                con.execute("INSERT INTO teams VALUES (?,?,?,?,?)", (season, p[0], p[1], p[2], p[3]))

    # Jugadores: IDs y nacimiento del Chadwick Register; manos del roster más reciente
    hands = {}
    for season in range(FIRST_SEASON, LAST_SEASON + 1):
        hands.update(load_rosters(RETRO_DIR / str(season)))
    rows = []
    for name in REGISTER_FILES:
        with open(REGISTER_DIR / name, encoding="utf-8") as fh:
            for r in csv.DictReader(fh):
                pid = r["key_retro"]
                if not pid or pid not in hands:
                    continue
                y, m, d = r["birth_year"], r["birth_month"], r["birth_day"]
                birth = f"{int(y):04d}-{int(m):02d}-{int(d):02d}" if y and m and d else None
                rows.append((pid, r["key_bbref"] or None,
                             int(r["key_mlbam"]) if r["key_mlbam"] else None,
                             r["name_first"], r["name_last"], birth, *hands[pid]))
    con.executemany("INSERT OR REPLACE INTO players VALUES (?,?,?,?,?,?,?,?)", rows)
    print(f"  referencia: {len(rows)} jugadores con ID de Retrosheet")


def main():
    t0 = time.time()
    DB_PATH.parent.mkdir(parents=True, exist_ok=True)
    if DB_PATH.exists():
        DB_PATH.unlink()                 # se reconstruye desde cero: idempotente
    con = sqlite3.connect(DB_PATH)
    con.executescript("PRAGMA journal_mode=OFF; PRAGMA synchronous=OFF;" + SCHEMA)
    load_reference(con)

    total = 0
    for season in range(FIRST_SEASON, LAST_SEASON + 1):
        sdir = RETRO_DIR / str(season)
        hands = load_rosters(sdir)
        n = 0
        for ev in sorted(list(sdir.glob("*.EVA")) + list(sdir.glob("*.EVN"))):
            games, pas = parse_file(ev, season, hands)
            con.executemany("INSERT INTO games VALUES (:game_id,:season,:game_date,:park_id,:home_team,"
                            ":away_team,:temp_f,:wind_mph,:wind_dir,:is_night)", games)
            con.executemany("INSERT INTO plate_appearances (game_id,season,batter_id,pitcher_id,bat_team,"
                            "bat_home,bat_side,pitch_hand,event,is_hr) VALUES (?,?,?,?,?,?,?,?,?,?)", pas)
            n += len(pas)
        con.commit()
        total += n
        print(f"  {season}: {n:>7,} apariciones al bate")

    print("  calculando índices y totales por temporada…")
    con.executescript(AGGREGATES)
    con.commit()
    con.execute("VACUUM")
    con.close()
    print(f"Listo: {total:,} PA en {DB_PATH.name} "
          f"({DB_PATH.stat().st_size / 1e6:.0f} MB) en {time.time() - t0:.0f} s")


if __name__ == "__main__":
    main()
