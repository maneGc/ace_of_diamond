"""
config.py — Rutas y constantes compartidas por toda la etapa de datos.

Solo usa la biblioteca estándar de Python, para que el proyecto funcione
en cualquier computadora sin instalar nada extra en esta etapa.
"""
from pathlib import Path

# Carpeta raíz del proyecto (la que contiene etl/, engine/, data/...)
ROOT = Path(__file__).resolve().parent.parent

DATA_DIR = ROOT / "data"
RAW_DIR = DATA_DIR / "raw"                 # descargas originales (no se editan)
RETRO_DIR = RAW_DIR / "retrosheet"         # un subdirectorio por temporada
REGISTER_DIR = RAW_DIR / "register"        # Chadwick Register (IDs y nacimientos)
DB_PATH = DATA_DIR / "aod.sqlite"          # base de datos generada

# Rango de temporadas del proyecto escolar.
# 1995 deja 3 años previos a Bonds 2001 y McGwire/Sosa 1998,
# y 2025 es la última temporada publicada por Retrosheet.
FIRST_SEASON = 1995
LAST_SEASON = 2025

# Copia de Retrosheet mantenida por Chadwick Baseball Bureau en GitHub.
RETRO_URL = "https://raw.githubusercontent.com/chadwickbureau/retrosheet/master"
REGISTER_URL = "https://raw.githubusercontent.com/chadwickbureau/register/master/data"
REGISTER_FILES = [f"people-{c}.csv" for c in "0123456789abcdef"]

# Aviso que Retrosheet exige mostrar al usar sus datos.
RETROSHEET_NOTICE = (
    "The information used here was obtained free of charge from and is "
    "copyrighted by Retrosheet. Interested parties may contact Retrosheet "
    'at "www.retrosheet.org".'
)
