"""
parse_events.py — Paso 2 de la etapa de datos: TRANSFORMAR.

Convierte los archivos de jugadas de Retrosheet en dos listas de filas:
  - un partido por fila (fecha, estadio, clima…)
  - una APARICIÓN AL BATE (PA) por fila (bateador, pitcher, manos, ¿fue HR?)

Cómo es un archivo de Retrosheet (formato de texto, una línea por registro):
  id,SFN200104020                      ← empieza un partido
  info,temp,59                         ← datos del partido (clima, estadio…)
  start,bondb001,"Barry Bonds",1,3,7   ← alineación inicial (1 = equipo local)
  sub,...                              ← sustituciones (aquí se detecta el cambio de pitcher)
  play,1,1,bondb001,22,BCFB,HR/F89     ← una jugada: entrada, equipo, bateador, conteo, lanzamientos, EVENTO
El evento dice qué pasó: HR = jonrón, K = ponche, W = base por bolas, SB2 = robo de base…
"""
import re

# Eventos que NO terminan una aparición al bate (pasan mientras el bateador sigue en turno).
NO_PA = ("NP", "SB", "CS", "PO", "WP", "PB", "BK", "DI", "OA", "FLE")

# Jonrón: "HR…" o "H" sola (notación antigua), pero no "HP" (golpeado por lanzamiento).
HR_RE = re.compile(r"^(HR|H(?!P))")
CODE_RE = re.compile(r"^[A-Z]+")


def event_token(event):
    """Parte principal del evento: 'HR/F89.1-H' → 'HR' ; 'K+SB2' → 'K'."""
    return re.split(r"[/.+]", event, maxsplit=1)[0]


def is_pa(token):
    """True si el evento termina la aparición al bate."""
    return not token.startswith(NO_PA)


def event_code(token):
    """Código corto del resultado: 'S8' → 'S', 'FC6' → 'FC', '63' (out) → 'OUT'."""
    m = CODE_RE.match(token)
    return m.group(0) if m else "OUT"


def load_rosters(season_dir):
    """Mano de bateo y de lanzamiento de cada jugador: {id: (bats, throws)}."""
    hands = {}
    for ros in season_dir.glob("*.ROS"):
        for line in ros.read_text(encoding="latin-1").splitlines():
            p = line.strip().split(",")
            if len(p) >= 5:
                hands[p[0]] = (p[3], p[4])
    return hands


def _none_if(value, *unknown):
    return None if value in unknown else value


def parse_file(path, season, hands):
    """Lee un archivo .EVA/.EVN y devuelve (partidos, apariciones_al_bate)."""
    games, pas = [], []
    game = None
    pitcher = {0: None, 1: None}      # pitcher en el montículo de cada equipo (0 visitante, 1 local)
    bat_adj, pit_adj = {}, {}          # ajustes excepcionales de mano (badj / padj)

    for raw in path.read_text(encoding="latin-1").splitlines():
        f = raw.rstrip("\r").split(",")
        kind = f[0]

        if kind == "id":
            game = {"game_id": f[1], "season": season, "park_id": None, "home_team": None,
                    "away_team": None, "game_date": None, "temp_f": None, "wind_mph": None,
                    "wind_dir": None, "is_night": None}
            games.append(game)
            pitcher = {0: None, 1: None}
            bat_adj, pit_adj = {}, {}

        elif kind == "info" and game is not None and len(f) >= 3:
            key, val = f[1], f[2].strip('"')
            if key == "site":
                game["park_id"] = val
            elif key == "hometeam":
                game["home_team"] = val
            elif key == "visteam":
                game["away_team"] = val
            elif key == "date":
                game["game_date"] = val.replace("/", "-")
            elif key == "temp":                       # 0 = desconocida → NULL
                game["temp_f"] = _none_if(int(val) if val.lstrip("-").isdigit() else None, 0, None)
            elif key == "windspeed":                  # -1 = desconocida → NULL
                game["wind_mph"] = _none_if(int(val) if val.lstrip("-").isdigit() else None, -1, None)
            elif key == "winddir":
                game["wind_dir"] = _none_if(val, "unknown", "")
            elif key == "daynight":
                game["is_night"] = {"night": 1, "day": 0}.get(val)

        elif kind in ("start", "sub") and len(f) >= 6:
            # start/sub,jugador,"nombre",equipo,orden_bateo,posición  (posición 1 = pitcher)
            if f[-1].strip() == "1":
                pitcher[int(f[-3])] = f[1]

        elif kind == "badj" and len(f) >= 3:
            bat_adj[f[1]] = f[2]
        elif kind == "padj" and len(f) >= 3:
            pit_adj[f[1]] = f[2]

        elif kind == "play" and len(f) >= 7 and game is not None:
            team, batter, event = int(f[2]), f[3], f[6]
            token = event_token(event)
            if not is_pa(token):
                continue
            pid = pitcher[1 - team]                    # lanza el equipo contrario
            p_hand = pit_adj.pop(pid, None) or hands.get(pid, ("?", "?"))[1]
            b_hand = bat_adj.pop(batter, None) or hands.get(batter, ("?", "?"))[0]
            if b_hand == "B":                          # ambidiestro: batea del lado contrario al pitcher
                b_hand = "L" if p_hand == "R" else "R"
            pas.append((game["game_id"], season, batter, pid,
                        game["home_team"] if team == 1 else game["away_team"],
                        team, b_hand, p_hand, event_code(token),
                        1 if HR_RE.match(token) else 0))
    return games, pas
