"""
run_all.py — Ejecuta la etapa de datos completa, en orden:
  1. download.py  → descarga Retrosheet y el Chadwick Register   (~1 min)
  2. build_db.py  → crea data/aod.sqlite                          (~1 min)
  3. validate.py  → compara con cifras oficiales
Uso:  python etl/run_all.py
"""
import runpy
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

for step in ("download.py", "build_db.py", "validate.py"):
    print(f"\n=== {step} ===")
    sys.argv = [step]
    runpy.run_path(str(HERE / step), run_name="__main__")
