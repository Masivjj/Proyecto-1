#!/usr/bin/env python3
"""Comprueba la salida del firmware simulado del avance "ADC a 1000 muestras/s"
(tests/adc1k, "make adc1k").

Uso: verificar_adc1k.py <salida_uart> <conversiones>

El guion del simulador (tests/adc1k/sim.c) es:
  generador en RC0: 1.65 V + 1.5 V·sin(2π·117 Hz·t), VDD = 3.30 V
  1.0 s '?' estado          2.0 s el lazo se para 50 ms (la FIFO lo absorbe)
  3.0 s el lazo se para 200 ms (se pierden muestras)
  4.0 s 'd' detener         4.5 s 'i' reanudar       4.8 s orden desconocida
  5.5 s '?' estado
<conversiones> tiene "k,codigo" de cada conversion disparada por el PWM6: las
lineas de datos de la UART deben ser exactamente esas, en orden, salvo las
perdidas y las del tiempo detenido.
"""
import math
import os
import sys

import numpy as np

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "pc"))
import medir_jitter as mj    # noqa: E402

VDD = 3.30
fallos = 0


def comprobar(nombre, ok, detalle=""):
    global fallos
    if not ok:
        fallos += 1
    print(("OK     " if ok else "FALLO  ") + nombre + (f"  ({detalle})" if detalle else ""))


def alinear(datos, conversiones, tramo=20):
    """Recorre las dos listas y devuelve los huecos [(k, longitud)] de las
    conversiones que no salieron por la UART, o None si no encajan."""
    huecos, i, j = [], 0, 0
    while i < len(datos):
        if j < len(conversiones) and datos[i] == conversiones[j]:
            i += 1
            j += 1
            continue
        muestra = datos[i:i + tramo]
        for g in range(1, 3000):
            if conversiones[j + g:j + g + len(muestra)] == muestra:
                huecos.append((j, g))
                j += g
                break
        else:
            return None, j
    return huecos, j


def main():
    with open(sys.argv[1], encoding="ascii") as f:
        lineas = f.read().splitlines()
    with open(sys.argv[2], encoding="ascii") as f:
        conversiones = [int(x.split(",")[1]) for x in f.read().splitlines()]

    datos, posicion = [], []                # posicion: indice de linea de cada dato
    for n, linea in enumerate(lineas):
        if not linea.startswith("#"):
            datos.append(int(linea))
            posicion.append(n)
    comentarios = [x for x in lineas if x.startswith("#")]

    comprobar("todas las lineas son '#...' o un codigo de 0 a 4095",
              all(x.startswith("#") or (x.isdigit() and int(x) <= 4095) for x in lineas))
    comprobar("cabecera al arrancar", lineas[0] == "# ADC 1 kS/s - PIC16F18426 - firmware 1.0")
    vdd = [x for x in comentarios if "referencia VDD = " in x]
    vdd_mv = int(vdd[0].split("VDD = ")[1].split()[0]) if vdd else 0
    comprobar("VDD medida con el FVR: 3300 mV +-5", abs(vdd_mv - 3300) <= 5, f"{vdd_mv} mV")
    comprobar("muestreo cada 1000 us", any("1 conversion cada 1000 us" in x for x in comentarios))

    huecos, fin = alinear(datos, conversiones)
    comprobar("los datos son las conversiones, en orden y sin cambios", huecos is not None,
              f"se pierde la pista en la conversion {fin}" if huecos is None else f"huecos {huecos}")
    huecos = huecos or []
    comprobar("la primera linea de datos es la primera conversion", bool(datos) and datos[0] == conversiones[0])
    comprobar("parada de 50 ms a los 2 s: sin huecos", not any(1900 <= k <= 2200 for k, _ in huecos))
    perdida = [h for h in huecos if 2900 <= h[0] <= 3300]
    aviso = [int(x.split(": ")[1]) for x in comentarios if x.startswith("# perdidas: ")]
    comprobar("parada de 200 ms a los 3 s: un hueco de 200 - 127 (FIFO) muestras, mas o menos",
              len(perdida) == 1 and 60 <= perdida[0][1] <= 90, f"{perdida}")
    comprobar("aviso '# perdidas: N' una vez, con N = el hueco",
              len(aviso) == 1 and perdida and aviso[0] == perdida[0][1], f"{aviso}")
    detenido = [h for h in huecos if 3900 <= h[0] <= 4600]
    comprobar("'d' ... 'i': un hueco de ~500 muestras", len(detenido) == 1 and 450 <= detenido[0][1] <= 560,
              f"{detenido}")
    comprobar("no hay otros huecos", len(huecos) == 2, f"{huecos}")
    comprobar("al terminar quedan menos de 128 conversiones por enviar", len(conversiones) - fin < 128,
              f"{len(conversiones) - fin}")

    i_d = lineas.index("# d: envio detenido") if "# d: envio detenido" in lineas else -1
    i_i = lineas.index("# i: envio en marcha") if "# i: envio en marcha" in lineas else -1
    comprobar("ninguna linea de datos entre 'd' e 'i'",
              0 < i_d < i_i and not any(i_d < p < i_i for p in posicion))
    comprobar("orden desconocida contestada", "# orden desconocida (? = ayuda)" in lineas)
    estados = [x for x in comentarios if x.startswith("# estado: ")]
    comprobar("estado a 1 s: ~1000 convertidas y 0 perdidas", len(estados) == 2
              and "perdidas 0" in estados[0]
              and 990 <= int(estados[0].split("convertidas ")[1].split(",")[0]) <= 1040, f"{estados[:1]}")
    comprobar("estado a 5.5 s: las perdidas de la parada", len(estados) == 2 and aviso
              and estados[1].endswith(f"perdidas {aviso[0]}"), f"{estados[1:]}")

    # La senal: el primer segundo, ajustado a una senoide
    x = np.array(datos[:1000], dtype=float)
    r = mj.resumir([x], 1000.0)
    amplitud = math.sqrt(2 * r["senal"]) * VDD / 4095
    offset = r["offset"] * VDD / 4095
    comprobar("senal: 117 Hz +-0.05", abs(r["f_aparente"] - 117) < 0.05, f"{r['f_aparente']:.3f} Hz")
    comprobar("senal: 1.5 V de amplitud y 1.65 V de offset (+-0.5 %)",
              abs(amplitud / 1.5 - 1) < 0.005 and abs(offset / 1.65 - 1) < 0.005,
              f"{amplitud:.4f} V, {offset:.4f} V")
    comprobar("senal: SINAD > 65 dB (cuantificacion y 0.3 LSB de ruido)", r["sinad"] > 65,
              f"SINAD {r['sinad']:.1f} dB, ENOB {r['enob']:.2f}")

    print(f"\n{fallos} fallos")
    return 1 if fallos else 0


if __name__ == "__main__":
    sys.exit(main())
