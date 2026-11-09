"""
download.py — Paso 1 de la etapa de datos: DESCARGAR.

Baja de la copia de Retrosheet en GitHub, para cada temporada:
  - TEAMaaaa         → lista de equipos y su liga (A = Americana, N = Nacional)
  - aaaaEEE.EVA/.EVN → archivo de jugadas del equipo EEE como local
  - EEEaaaa.ROS      → roster: con qué mano batea y lanza cada jugador
Y además:
  - reference/ballparks.csv → nombres de los estadios
  - Chadwick Register       → equivalencia de IDs y fechas de nacimiento

Es "idempotente": si un archivo ya existe, no lo vuelve a bajar.
Uso:  python etl/download.py            (todas las temporadas)
      python etl/download.py 2001 2001  (solo una)
"""
import sys
import urllib.request
from concurrent.futures import ThreadPoolExecutor

from config import (FIRST_SEASON, LAST_SEASON, REGISTER_DIR, REGISTER_FILES,
                    REGISTER_URL, RETRO_DIR, RETRO_URL)


def fetch(url, dest, required=True):
    """Descarga url → dest (si no existe). Devuelve True si el archivo quedó en disco."""
    if dest.exists() and dest.stat().st_size > 0:
        return True
    dest.parent.mkdir(parents=True, exist_ok=True)
    for intento in range(4):
        try:
            with urllib.request.urlopen(url, timeout=60) as r:
                data = r.read()
            dest.write_bytes(data)
            return True
        except urllib.error.HTTPError as e:
            if e.code == 404:          # el archivo no existe (p. ej. equipo All-Star)
                if required:
                    print(f"  [falta] {url}")
                return False
        except Exception as e:         # error de red: reintentar
            if intento == 3:
                print(f"  [error] {url}: {e}")
                return False
    return False


def teams_of(season):
    """Lee TEAMaaaa y devuelve [(equipo, liga), ...]."""
    dest = RETRO_DIR / str(season) / f"TEAM{season}"
    fetch(f"{RETRO_URL}/seasons/{season}/TEAM{season}", dest)
    out = []
    for line in dest.read_text(encoding="latin-1").splitlines():
        parts = line.strip().split(",")
        if len(parts) >= 2 and parts[1] in ("A", "N"):
            out.append((parts[0], parts[1]))
    return out


def season_jobs(season):
    """Lista de (url, destino, obligatorio) para una temporada."""
    jobs = []
    for team, league in teams_of(season):
        ext = "EVA" if league == "A" else "EVN"
        d = RETRO_DIR / str(season)
        # Los equipos All-Star (ALS/NLS) no tienen archivo de temporada: no son obligatorios.
        jobs.append((f"{RETRO_URL}/seasons/{season}/{season}{team}.{ext}", d / f"{season}{team}.{ext}", False))
        jobs.append((f"{RETRO_URL}/seasons/{season}/{team}{season}.ROS", d / f"{team}{season}.ROS", False))
    return jobs


def main():
    first = int(sys.argv[1]) if len(sys.argv) > 1 else FIRST_SEASON
    last = int(sys.argv[2]) if len(sys.argv) > 2 else LAST_SEASON

    jobs = [(f"{RETRO_URL}/reference/ballparks.csv", RETRO_DIR / "ballparks.csv", True)]
    jobs += [(f"{REGISTER_URL}/{f}", REGISTER_DIR / f, True) for f in REGISTER_FILES]
    for season in range(first, last + 1):
        jobs += season_jobs(season)

    print(f"Descargando {len(jobs)} archivos ({first}–{last})…")
    with ThreadPoolExecutor(max_workers=12) as pool:
        ok = list(pool.map(lambda j: fetch(*j), jobs))
    n_events = sum(1 for (u, d, r), k in zip(jobs, ok) if k and d.suffix in (".EVA", ".EVN"))
    print(f"Listo: {sum(ok)} archivos en disco, {n_events} archivos de jugadas.")


if __name__ == "__main__":
    main()
