#!/usr/bin/env python3
"""Pruebas de pc/prueba_usb6211.py sin la tarjeta (necesita numpy).

Comprueba el generador (límites de 0-3.3 V, periodos enteros), las tres
medidas de la frecuencia de muestreo, la estadística del contador de 80 MHz
y que las comprobaciones fallan cuando deben, con datos simulados.
"""
import math
import os
import sys

import numpy as np

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "pc"))
import prueba_usb6211 as pu    # noqa: E402

fallos = 0


def comprobar(nombre, ok, detalle=""):
    global fallos
    if not ok:
        fallos += 1
    print(("OK     " if ok else "FALLO  ") + nombre + (f"  ({detalle})" if detalle else ""))


def lanza(f, *args):
    try:
        f(*args)
    except ValueError:
        return True
    return False


# Generador
onda, f_real = pu.onda_generador(117.0, 1.65, 3.0)
comprobar("generador: 1 s a 100 kS/s con 117 periodos enteros", len(onda) == 100000 and f_real == 117.0)
comprobar("generador: entre 0.15 y 3.15 V", abs(onda.min() - 0.15) < 1e-3 and abs(onda.max() - 3.15) < 1e-3)
siguiente = 1.65 + 1.5 * math.sin(2 * math.pi * 117.0 * len(onda) / pu.FS_AO)
comprobar("generador: el buffer se regenera sin salto", abs(siguiente - onda[0]) < 1e-9)
_, f_real = pu.onda_generador(117.4, 1.65, 3.0)
comprobar("generador: la frecuencia se redondea a periodos enteros", f_real == 117.0)
comprobar("limites: 3.5 V de pico se rechaza", lanza(pu.comprobar_limites, 2.0, 3.0))
comprobar("limites: tension negativa se rechaza", lanza(pu.comprobar_limites, 1.0, 2.2))
comprobar("limites: 0.15-3.15 V se acepta", not lanza(pu.comprobar_limites, 1.65, 3.0))

# Frecuencia de muestreo por el cociente
comprobar("cociente: 117 Hz vistos en 116.187 Hz -> 1007 S/s",
          abs(pu.fs_por_cociente(117.0, 117.0 * 1000 / 1007) - 1007) < 1e-9)
comprobar("cociente: no vale con alias (600 Hz)", pu.fs_por_cociente(600.0, 400.0) is None)

# Contador
periodos = np.full(1000, 1e-4)
c = pu.estadistica_periodos(periodos, 10)
comprobar("contador: RA1 a 10 kHz / 10 = 1000 S/s por canal", abs(c["fs"] - 1000) < 1e-9 and c["n"] == 1000)
comprobar("contador: suelo de cuantificacion q/sqrt(6) = 5.1 ns", abs(c["suelo"] - 12.5e-9 / math.sqrt(6)) < 1e-15)
d = pu.simular("completo", sigma=0.0, semilla=2)
c = pu.estadistica_periodos(d["periodos"], 10)
comprobar("contador sin jitter: no pasa del suelo y no se resuelve", c["sigma_muestra"] <= 1.15 * c["suelo"]
          and not c["resuelve"], f"{c['sigma_muestra'] * 1e9:.2f} ns")
d = pu.simular("completo", sigma=30e-9, semilla=3)
c = pu.estadistica_periodos(d["periodos"], 10)
esperado = 30e-9 * math.sqrt(2)          # dos flancos independientes por intervalo
comprobar("contador con 30 ns de jitter: se resuelve y se estima +-15 %",
          c["resuelve"] and abs(c["jitter_muestra"] / esperado - 1) < 0.15, f"{c['jitter_muestra'] * 1e9:.1f} ns")


def todas(d):
    med, pruebas = pu.evaluar(d)
    return med, all(ok for _, ok, _ in pruebas), {n: ok for n, ok, _ in pruebas}


# Pruebas completas simuladas
for firmware, fs in (("avance", 985.0), ("completo", 1015.0)):
    d = pu.simular(firmware, fs_pic=fs, semilla=4)
    med, ok, _ = todas(d)
    comprobar(f"{firmware} a {fs:.0f} S/s: todas las comprobaciones correctas", ok)
    comprobar(f"{firmware}: fs por el contador y por el cociente +-0.01 %",
              abs(med["contador"]["fs"] / fs - 1) < 1e-4 and abs(med["fs_cociente"] / fs - 1) < 1e-4,
              f"{med['contador']['fs']:.3f} y {med['fs_cociente']:.3f}")
    comprobar(f"{firmware}: ENOB del PIC entre 10.5 y 12 bits", 10.5 < med["pic"]["enob"] < 12,
              f"{med['pic']['enob']:.2f}")

d = pu.simular("avance", fs_pic=1030.0, semilla=5)
_, ok, r = todas(d)
comprobar("fs de 1030 S/s (+3 %): falla la de +-2 %", not ok and not r["fs por el contador (RA1): 1000 S/s ±2 %"])
d = pu.simular("completo", semilla=6)
d["linea_adc"] = ""
_, ok, _ = todas(d)
comprobar("firmware completo sin la linea '# ADC:' (anterior a 1.3): falla", not ok)
d = pu.simular("avance", semilla=7)
d["ai"] = 1.65 + (np.asarray(d["ai"]) - 1.65) * 1.10
_, ok, _ = todas(d)
comprobar("amplitud de la AI un 10 % mayor: falla la comparacion", not ok)
d = pu.simular("avance", semilla=8)
d["perdidas"] = 3
_, ok, _ = todas(d)
comprobar("muestras perdidas: falla", not ok)
d = pu.simular("avance", vpp=3.2, offset=1.65, vdd=3.2, semilla=9)
_, ok, r = todas(d)
comprobar("senoide mas ancha que VDD: avisa de la saturacion", not r["sin saturar el ADC"])
texto = pu.informe(*((d,) + pu.evaluar(d)))
comprobar("informe: incluye el aviso de que el contador no certifica 10 ns", "no certifica" in texto)

print(f"\n{fallos} fallos")
sys.exit(1 if fallos else 0)
