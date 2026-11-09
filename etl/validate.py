"""
validate.py — Paso 4 de la etapa de datos: VERIFICAR.

Compara lo que hay en la base de datos con cifras oficiales muy conocidas.
Si el parser tuviera un error (contar mal los HR, confundir jugadas…),
estas pruebas lo detectarían.
Uso:  python etl/validate.py
"""
import sqlite3
import sys

from config import DB_PATH

# (nombre, apellido, temporada, HR oficiales, PA oficiales)
SEASONS = [
    ("Barry", "Bonds", 2001, 73, 664),
    ("Mark", "McGwire", 1998, 70, 681),
    ("Sammy", "Sosa", 1998, 66, 722),
    ("Aaron", "Judge", 2022, 62, 696),
    ("Shohei", "Ohtani", 2024, 54, 731),
]
# HR totales de toda MLB en una temporada (cifras oficiales)
LEAGUE_HR = {2001: 5458, 2019: 6776}


def main():
    con = sqlite3.connect(DB_PATH)
    q = lambda sql, *a: con.execute(sql, a).fetchone()
    fails = 0

    def check(label, got, expected, tol=0):
        nonlocal fails
        ok = got is not None and abs(got - expected) <= tol
        fails += not ok
        print(f"  {'✓' if ok else '✗'} {label:<44} obtenido {got!s:>6} · oficial {expected}")

    print("Temporadas históricas:")
    for first, last, season, hr, pa in SEASONS:
        pid = q("SELECT player_id FROM players WHERE name_first=? AND name_last=?", first, last)[0]
        got_hr, got_pa, hr_home = q("SELECT hr, pa, hr_home FROM player_seasons "
                                    "WHERE player_id=? AND season=?", pid, season)
        check(f"{first} {last} {season} · HR", got_hr, hr)
        check(f"{first} {last} {season} · PA", got_pa, pa, tol=2)
        if (last, season) == ("Bonds", 2001):
            check("Barry Bonds 2001 · HR en casa", hr_home, 37)

    print("Totales de la liga:")
    for season, hr in LEAGUE_HR.items():
        check(f"HR de toda MLB en {season}", q("SELECT SUM(hr) FROM league_seasons WHERE season=?", season)[0], hr)

    print("Calidad de los datos:")
    total = q("SELECT COUNT(*) FROM plate_appearances")[0]
    no_pit = q("SELECT COUNT(*) FROM plate_appearances WHERE pitcher_id IS NULL")[0]
    no_hand = q("SELECT COUNT(*) FROM plate_appearances WHERE pitch_hand NOT IN ('L','R') "
                "OR bat_side NOT IN ('L','R')")[0]
    g, no_temp = q("SELECT COUNT(*), SUM(temp_f IS NULL) FROM games")
    print(f"  PA sin pitcher identificado: {no_pit} de {total:,}")
    print(f"  PA sin mano de bateo/lanzamiento conocida: {no_hand} ({no_hand / total:.3%})")
    print(f"  Partidos sin temperatura (NULL): {no_temp} de {g:,} ({no_temp / g:.1%})")
    fails += no_pit > 0

    print("\nRESULTADO:", "todo correcto ✓" if fails == 0 else f"{fails} comprobaciones fallaron ✗")
    sys.exit(1 if fails else 0)


if __name__ == "__main__":
    main()
