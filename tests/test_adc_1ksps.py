#!/usr/bin/env python3
"""Pruebas de pc/adc_1ksps.py sin ventana (necesita numpy, no matplotlib).

Usa la salida del firmware simulado ("make adc1k") si existe: el programa
del PC debe leerla entera, con la VDD de la cabecera y las perdidas.
"""
import math
import os
import queue
import sys
import tempfile
import time

import numpy as np

AQUI = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(AQUI, "..", "pc"))
import adc_1ksps as a    # noqa: E402

fallos = 0


def comprobar(nombre, ok, detalle=""):
    global fallos
    if not ok:
        fallos += 1
    print(("OK     " if ok else "FALLO  ") + nombre + (f"  ({detalle})" if detalle else ""))


class FuenteLista:
    def __init__(self, lineas):
        self.cola = queue.Queue()
        for linea in lineas:
            self.cola.put(linea)


comprobar("muestra", a.interpretar("2047\r") == ("muestra", 2047))
comprobar("comentario", a.interpretar("# d: envio detenido") == ("texto", "# d: envio detenido"))
comprobar("lineas no validas", a.interpretar("4096") is None and a.interpretar("20x7") is None
          and a.interpretar("") is None and a.interpretar("-5") is None)
comprobar("VDD de la cabecera",
          a.vdd_de_cabecera("# entrada RC0 (ANC0), 12 bits, referencia VDD = 3301 mV (medida con el FVR, +-4 %)")
          == 3.301 and a.vdd_de_cabecera("# perdidas: 3") is None)
comprobar("aviso de perdidas", a.perdidas_de_aviso("# perdidas: 74") == 74 and a.perdidas_de_aviso("# d:") is None)

# Analisis de una senoide conocida
t = np.arange(1000) / 1000
codigos = np.round((1.65 + 1.2 * np.sin(2 * np.pi * 60 * t)) / 3.3 * 4095).astype(int)
r = a.analizar(list(codigos), 3.3)
comprobar("analisis: 60 Hz, 2.4 Vpp, offset 1.65 V",
          abs(r["f_aparente"] - 60) < 0.01 and abs(2 * r["amplitud_v"] - 2.4) < 0.005
          and abs(r["offset_v"] - 1.65) < 0.003, a.resumen_texto(r))
comprobar("analisis: senoide sin saturar", not r["saturada"])
r = a.analizar([0, 4095] * 100, 3.3)
comprobar("analisis: saturacion detectada", r["saturada"])
r = a.analizar([2000, 2001] * 100, 3.3)
comprobar("analisis: entrada continua", r["dc"] and abs(r["media_v"] - 2000.5 * 3.3 / 4095) < 1e-6)
comprobar("analisis: pocas muestras", a.analizar([1, 2, 3], 3.3) is None)

# Receptor y CSV con lineas del PIC
with tempfile.TemporaryDirectory() as carpeta:
    ruta = os.path.join(carpeta, "senal.csv")
    rx = a.Receptor(eco=False)
    rx.grabador = a.Grabador(ruta, "prueba")
    rx.procesar(FuenteLista(["# entrada RC0 (ANC0), 12 bits, referencia VDD = 3000 mV (medida con el FVR)",
                             "4095", "0", "basura", "# perdidas: 2", "# d: envio detenido", "2048"]))
    rx.grabador.cerrar()
    with open(ruta, encoding="utf-8") as f:
        filas = f.read().splitlines()
    comprobar("receptor: muestras, VDD, perdidas, lineas no validas y estado",
              rx.total == 3 and rx.vdd == 3.0 and rx.perdidas_pic == 2 and rx.invalidas == 1 and not rx.enviando)
    comprobar("CSV: cabecera, comentarios y n,t_s,codigo,V",
              "n,t_s,codigo,V" in filas and "0,0.000,4095,3.00000" in filas and "2,0.002,2048,1.50037" in filas
              and "# perdidas: 2" in filas and "# comentario: prueba" in filas, f"{filas[-4:]}")

# Salida del firmware simulado
salida = os.path.join(AQUI, "build", "adc1k", "salida.txt")
if os.path.exists(salida):
    with open(salida, encoding="ascii") as f:
        lineas = f.read().splitlines()
    rx = a.Receptor(memoria=10000, eco=False)
    rx.procesar(FuenteLista(lineas), maximo=len(lineas) + 1)
    datos = sum(1 for x in lineas if not x.startswith("#"))
    r = a.analizar(rx.ultimas(1000), rx.vdd_v())
    comprobar("firmware simulado: todas las muestras, VDD 3.300 V y 74 perdidas",
              rx.total == datos and rx.vdd == 3.3 and rx.perdidas_pic == 74 and rx.invalidas == 0,
              f"{rx.total} muestras")
    comprobar("firmware simulado: 117 Hz, 3.00 Vpp", abs(r["f_aparente"] - 117) < 0.05
              and abs(2 * r["amplitud_v"] - 3.0) < 0.01, a.resumen_texto(r))
else:
    print("(sin build/adc1k/salida.txt: ejecute antes 'make adc1k')")

# Demostracion: genera a 1000 muestras/s y atiende 'd'
demo = a.FuenteDemo(50.0)
rx = a.Receptor(eco=False)
time.sleep(0.6)
rx.procesar(demo)
n1 = rx.total
demo.enviar("d")
time.sleep(0.3)
rx.procesar(demo)
n2 = rx.total
time.sleep(0.3)
rx.procesar(demo)
comprobar("demostracion: ~1000 muestras/s", 300 <= n1 <= 800, f"{n1} en 0.6 s")
comprobar("demostracion: 'd' detiene el envio", rx.total == n2 and not rx.enviando, f"{n2} -> {rx.total}")
comprobar("demostracion: la senal es la del generador",
          abs(a.analizar(rx.ultimas(400), rx.vdd_v())["f_aparente"] - 50) < 0.1)
comprobar("demostracion: VDD de su cabecera", rx.vdd == 3.3 and math.isclose(rx.vdd_v(), 3.3))

print(f"\n{fallos} fallos")
sys.exit(1 if fallos else 0)
