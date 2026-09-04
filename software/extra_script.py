import os

Import("env")

# os.path.basename, nu split('\\'): separatorul de cale este backslash doar pe
# Windows. Pe macOS/Linux split-ul nu gaseste nimic si intoarce calea intreaga,
# iar PROGNAME devine un drum absolut - binarul ajunge la
# .pio/build/uno/Users/.../comanda_pompa_uno.hex in loc de comanda_pompa_uno.hex.
prj_name = os.path.basename(env["PROJECT_DIR"])

env.Replace(PROGNAME=f"{prj_name}_{env.subst('$BOARD')}")
