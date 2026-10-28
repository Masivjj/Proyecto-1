#!/usr/bin/env python3
"""Pruebas de pc/medir_jitter.py (necesita numpy).

Comprueba el análisis con señales de resultado conocido: el SINAD de un ADC
ideal de 12 bits (6.02·12 + 1.76 = 74 dB), la estimación del jitter con
capturas simuladas de σ conocido y la lectura de las líneas del PIC.
"""
import math
import os
import sys

import numpy as np

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "pc"))
import medir_jitter as mj    # noqa: E402

fallos = 0
FS = 1e4


def comprobar(nombre, ok, detalle=""):
    global fallos
    if not ok:
        fallos += 1
    print(("OK     " if ok else "FALLO  ") + nombre + (f"  ({detalle})" if detalle else ""))


# Teoria
comprobar("SNR del jitter: 10 ns a 1592 Hz = 80 dB", abs(mj.snr_jitter_db(1592, 10e-9) - 80.0) < 0.01)
comprobar("SNR del jitter: 10 ns a 10 Hz = 124 dB", abs(mj.snr_jitter_db(10, 10e-9) - 124.0) < 0.1)
comprobar("alias de 51230 Hz a 10 kS/s = 1230 Hz", abs(mj.alias(51230, FS) - 1230) < 1e-6)

# ADC ideal: solo cuantificacion
g = np.random.default_rng(7)
ideal = mj.simular_capturas(1230.0, 0.0, capturas=10, amplitud=2047.0, ruido=0.0, offset=2047.5, generador=g)
r = mj.resumir(ideal, FS)
comprobar("ADC ideal de 12 bits: SINAD = 74 dB +-0.5", abs(r["sinad"] - 74.0) < 0.5, f"{r['sinad']:.2f} dB")
comprobar("ADC ideal: ENOB = 12 +-0.1", abs(r["enob"] - 12) < 0.1, f"{r['enob']:.2f} bits")
comprobar("ADC ideal: ruido = 1/sqrt(12) LSB +-5 %", abs(math.sqrt(r["ruido"]) / (1 / math.sqrt(12)) - 1) < 0.05,
          f"{math.sqrt(r['ruido']):.3f} LSB")
comprobar("ajuste de la senoide: frecuencia +-0.1 Hz", abs(r["f_aparente"] - 1230) < 0.1, f"{r['f_aparente']:.3f} Hz")
comprobar("ajuste de la senoide: amplitud y offset",
          abs(math.sqrt(2 * r["senal"]) - 2047) < 1 and abs(r["offset"] - 2047.5) < 0.5)

# Distorsion: 2o armonico a -60 dBc
t = np.arange(200) / FS
dist = [np.round(2048 + 1800 * np.sin(2 * np.pi * 1230 * t + k) + 1.8 * np.sin(2 * np.pi * 2460 * t + 2 * k)
                 + g.normal(0, 0.5, 200)) for k in range(10)]
r = mj.resumir(dist, FS)
comprobar("THD de un 2o armonico a -60 dBc", abs(r["thd"] + 60) < 1.5 and not r["thd_bajo_ruido"],
          f"{r['thd']:.2f} dB")
comprobar("el armonico no cuenta como ruido: SNR > SINAD", r["snr"] > r["sinad"] + 3,
          f"SNR {r['snr']:.1f}, SINAD {r['sinad']:.1f}")

# Jitter conocido
for sigma in (10e-9, 30e-9):
    g = np.random.default_rng(3)
    baja = mj.resumir(mj.simular_capturas(1230.0, sigma, generador=g), FS)
    alta = mj.resumir(mj.simular_capturas(51230.0, sigma, generador=g), FS)
    est, cota = mj.estimar_jitter(baja, alta, 1230.0, 51230.0)
    comprobar(f"jitter simulado de {sigma * 1e9:.0f} ns: estimado +-10 %",
              est is not None and abs(est / sigma - 1) < 0.10 and cota >= est, f"{est * 1e9:.2f} ns")

g = np.random.default_rng(4)
baja = mj.resumir(mj.simular_capturas(1230.0, 10e-9, amplitud=1900.0, generador=g), FS)
alta = mj.resumir(mj.simular_capturas(51230.0, 10e-9, amplitud=1500.0, generador=g), FS)
est, _ = mj.estimar_jitter(baja, alta, 1230.0, 51230.0)
comprobar("amplitudes distintas en baja y alta: el estimado no cambia", abs(est / 10e-9 - 1) < 0.10,
          f"{est * 1e9:.2f} ns")

g = np.random.default_rng(5)
baja = mj.resumir(mj.simular_capturas(1230.0, 0.0, generador=g), FS)
alta = mj.resumir(mj.simular_capturas(51230.0, 0.0, generador=g), FS)
est, cota = mj.estimar_jitter(baja, alta, 1230.0, 51230.0)
comprobar("sin jitter: no se detecta y la cota es < 1 ns", est is None and cota < 1e-9, f"cota {cota * 1e9:.2f} ns")
comprobar("informe: cumple el objetivo de 10 ns", "CUMPLE" in mj.informe_jitter(est, cota)
          and "NO CUMPLE" not in mj.informe_jitter(est, cota))

# Lineas del PIC
lineas = mj.formato_pic([np.arange(200) % 7 + 1000, np.full(200, 1887.0)])
capturas, periodo = mj.leer_capturas(["1230,1,2,3,4", "B,1,2,3,4,5,6,3651,2,0,31"] + lineas)
comprobar("formato del PIC: ida y vuelta de 2 capturas",
          len(capturas) == 2 and list(capturas[0][:8]) == [1000, 1001, 1002, 1003, 1004, 1005, 1006, 1000]
          and periodo == 100.0)
rota = list(lineas)
rota[3] = "M,1887,18x7"
capturas, _ = mj.leer_capturas(rota)
comprobar("una linea rota descarta solo su captura", len(capturas) == 1 and capturas[0][0] == 1887)
capturas, periodo = mj.leer_capturas(["# m: captura de 20 conversiones de I_OUT, una cada 50 us",
                                       "M," + ",".join(["5"] * 10), "M," + ",".join(["5"] * 10), "# fin captura"])
comprobar("periodo leido de la cabecera", periodo == 50.0 and len(capturas[0]) == 20)
r = mj.resumir([np.full(200, 1887.0), np.full(200, 1887.0)], FS)
comprobar("entrada continua: media y ruido", r["dc"] and r["media"] == 1887.0 and r["ruido"] == 0.0)

print(f"\n{fallos} fallos")
sys.exit(1 if fallos else 0)
