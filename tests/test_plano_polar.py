#!/usr/bin/env python3
"""Pruebas del intérprete de líneas y del grabador CSV de pc/plano_polar.py.

No necesita matplotlib ni pyserial: solo usa las funciones que leen las
líneas que envía el PIC.
"""
import os
import sys
import tempfile

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "pc"))
import plano_polar as pp    # noqa: E402

fallos = 0


def comprobar(nombre, ok):
    global fallos
    if not ok:
        fallos += 1
    print(("OK     " if ok else "FALLO  ") + nombre)


tipo, m = pp.interpretar("1230,-500000,-250000,212132,-7")
comprobar("muestra: t_ms y valores en mV", tipo == "muestra" and m == (1230, -500.0, -250.0, 212.132, -0.007))

tipo, b = pp.interpretar("B,212132,9000,52638,-500000,-250000,13500,3651,2,5")
comprobar("bloque: |V|, Fase, d", tipo == "bloque" and b["V"] == 212.132 and b["fase"] == 90.0 and b["d"] == 52.638)
comprobar("bloque: I0, Q0, theta", b["I0"] == -500.0 and b["Q0"] == -250.0 and b["theta"] == 135.0)
comprobar("bloque: VCC_A, G y banderas", b["vcc"] == 3.651 and b["G"] == 2 and b["banderas"] == 5)

tipo, b = pp.interpretar("B,24329,5938,6037,62328,21135,0")
comprobar("bloque con solo los 6 campos de la especificacion", tipo == "bloque" and b["d"] == 6.037 and b["G"] == 0)

comprobar("punto del volcado de la traza", pp.interpretar("P,-1500,2500") == ("punto", (-1.5, 2.5)))
comprobar("comentario", pp.interpretar("# z: fijar cero") == ("texto", "# z: fijar cero"))
comprobar("lineas vacias o rotas se ignoran",
          pp.interpretar("") is None and pp.interpretar("12,3") is None and pp.interpretar("B,1,2") is None
          and pp.interpretar("\x00\xff basura") is None)

comprobar("configuracion del PIC",
          pp.interpretar_configuracion("# G=101 signo_I=OFF I0_uV=1 Q0_uV=2 theta_cdeg=3") == {"G": 101, "signo": False})
comprobar("otras lineas no son configuracion", pp.interpretar_configuracion("# z: fijar cero") is None)

with tempfile.TemporaryDirectory() as carpeta:
    ruta = os.path.join(carpeta, "medida.csv")
    g = pp.Grabador(ruta, "dc ganancia x2", "# G=2 signo_I=ON I0_uV=0 Q0_uV=0 theta_cdeg=0")
    for k in range(1, 11):
        g.muestra((k * 10, 1.0, 2.0, 3.0, 4.0))
    g.bloque({"V": 5.0, "fase": 53.13, "d": 1.241})
    g.muestra((110, 1.0, 2.0, 3.0, 4.0))
    g.cerrar()
    with open(ruta, encoding="utf-8") as f:
        filas = f.read().splitlines()
    datos = [f for f in filas if not f.startswith("#")]
    comprobar("CSV: comentario y configuracion en la cabecera",
              "# comentario: dc ganancia x2" in filas and any(f.startswith("# configuracion: G=2") for f in filas))
    comprobar("CSV: una fila por muestra", len(datos) == 1 + 11)
    comprobar("CSV: la fila que cierra el bloque lleva |V|, Fase y d",
              datos[10] == "100,1.000,2.000,3.000,4.000,5.000,53.13,1.241" and datos[9].endswith(",,,"))

print(f"\n{fallos} fallos")
sys.exit(1 if fallos else 0)
