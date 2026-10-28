#!/usr/bin/env python3
"""Comprueba la salida del firmware simulado en el PC (tests/sim, "make sim").

Uso: verificar_sim.py <salida_primer_arranque> <salida_segundo_arranque>

El guion del simulador (tests/sim/sim.c) es:
  0.0-2.0 s  metal sano con el desequilibrio del puente (62.33, 21.14) mV
  0.5 s      'z' fijar cero          1.0 s  '?' estado
  2.0-3.0 s  fisura: dos lobulos de 65 mV a 60 grados
  3.0 s      main se para 30 ms (la cola de tramas lo absorbe)
  3.5-4.5 s  lift-off de 30 mV a 20 grados;  4.0 s 'c' calibrar theta
  5.0 s      'p' volcar la traza     5.8 / 5.9 s  ganancia x11 y vuelta a x2
  6.2 s      orden desconocida       6.5 s  main se para 150 ms (se pierden tramas)
  7.0 s      pulsador "Fijar cero" 150 ms;  7.5 s 'm' captura;  8.0 s rebote de 20 ms
  9.0 s      'b' borrar traza
  9.3 s      'Z' desactiva el cero;  9.45 s 'z' (ignorada);  9.6 s 'T' desactiva theta
  9.7 s      'A' desactiva el autocero;  9.8 s '?' estado
El ADC simulado tiene un offset de +3 LSB. El segundo arranque usa la EEPROM
que dejo el primero.
"""
import math
import statistics
import sys

OFFSET_ADC = 3
VDD, VCC_A, G = 3.70, 3.65, 2.0

fallos = 0


def comprobar(nombre, ok, detalle=""):
    global fallos
    if not ok:
        fallos += 1
    print(("OK     " if ok else "FALLO  ") + nombre + (f"  ({detalle})" if detalle else ""))


def leer(ruta):
    with open(ruta, encoding="ascii") as f:
        lineas = f.read().splitlines()
    muestras, bloques, captura = [], [], []
    for linea in lineas:
        if linea[:1].isdigit():
            muestras.append([int(x) for x in linea.split(",")])
        elif linea.startswith("B,"):
            # el bloque se cierra con la muestra anterior: se guarda su t_ms
            bloques.append([muestras[-1][0]] + [int(x) for x in linea[2:].split(",")])
        elif linea.startswith("M,"):
            captura += [int(x) for x in linea[2:].split(",")]
    return lineas, muestras, bloques, captura


def fase(i, q):
    return math.degrees(math.atan2(q, i))


def main():
    lineas, m, b, captura = leer(sys.argv[1])
    lineas2, _, _, _ = leer(sys.argv[2])
    entre = lambda t0, t1: [r for r in m if t0 <= r[0] < t1]

    comprobar("todas las lineas de muestra tienen 5 campos", all(len(r) == 5 for r in m))
    comprobar("todas las lineas de bloque tienen 10 campos", all(len(r) == 11 for r in b))
    comprobar("primer arranque sin datos en la EEPROM", "# EEPROM: sin datos, valores por defecto" in lineas)

    # Base de tiempo: una trama cada 10 ms, tambien durante capturas y retrasos
    t = [r[0] for r in m]
    saltos = [(t[i], t[i + 1] - t[i]) for i in range(len(t) - 1) if t[i + 1] - t[i] != 10]
    comprobar("parada de 30 ms a los 3 s: sin huecos (la cola la absorbe)",
              not any(2900 < s[0] < 3200 for s in saltos))
    perdida = [s for s in saltos if 6400 < s[0] < 6800]
    comprobar("parada de 150 ms a los 6.5 s: hueco de 11 tramas perdidas",
              len(perdida) == 1 and perdida[0][1] == 10 * (1 + 11), f"{perdida}")
    hueco_captura = [s for s in saltos if 7400 < s[0] < 7700]
    comprobar("captura a los 7.5 s: hueco de 2 tramas (200 disparos)",
              len(hueco_captura) == 1 and hueco_captura[0][1] == 30, f"{hueco_captura}")
    comprobar("no hay otros huecos", len(saltos) == 2, f"{saltos}")
    comprobar("la ultima muestra sigue en la base de tiempo (t = 9990 ms)", t[-1] == 9990)
    con_retraso = [r[0] for r in b if r[9] & 0x20]
    comprobar("bandera de retraso solo en el bloque de la perdida",
              len(con_retraso) == 1 and perdida and perdida[0][0] - 200 <= con_retraso[0] <= perdida[0][0] + 200,
              f"bloques {con_retraso}")
    comprobar("estado a 1 s: 0 muestras perdidas y offset de 3.00 LSB",
              any("muestras perdidas: 0  offset ADC (AVSS): 3.00 LSB" in x for x in lineas))
    comprobar("estado a 9.8 s: 11 muestras perdidas",
              any("muestras perdidas: 11  offset ADC (AVSS): 3.00 LSB" in x for x in lineas))

    sano = entre(610, 2000)     # el cero se aplica al cerrar el bloque de t = 600 ms
    comprobar("tras 'z' el metal sano queda en el origen (|I'|, |Q'| < 1 mV)",
              all(abs(r[3]) < 1000 and abs(r[4]) < 1000 for r in sano))
    cero = [r for r in b if r[0] == 600]
    comprobar("cero fijado I0 ~ 62.3 mV, Q0 ~ 21.1 mV", bool(cero) and abs(cero[0][4] / 1000 - 62.33) < 1
              and abs(cero[0][5] / 1000 - 21.14) < 1, f"{cero[0][4:6] if cero else '-'}")

    fisura = [r for r in entre(2000, 3000) if math.hypot(r[3], r[4]) > 10000]
    rmax = max(math.hypot(r[3], r[4]) for r in entre(2000, 3000)) / 1000
    comprobar("fisura: r maximo ~ 65 mV", abs(rmax - 65) < 2, f"{rmax:.2f} mV")
    desvio = max(min(abs(fase(r[3], r[4]) - 60), abs(fase(r[3], r[4]) + 120)) for r in fisura)
    comprobar("fisura: puntos sobre la recta de 60 / 240 grados", desvio < 3, f"desvio maximo {desvio:.2f} grados")

    theta = [r[6] / 100 for r in b if r[0] >= 4100]
    comprobar("'c' durante el lift-off: theta ~ 20 grados", bool(theta) and abs(theta[0] - 20) < 1,
              f"{theta[0] if theta else '-'}")
    lift = [fase(r[3], r[4]) for r in entre(4200, 4500)]
    comprobar("tras calibrar, el lift-off cae sobre 0 grados", abs(sum(lift) / len(lift)) < 1.5,
              f"{sum(lift) / len(lift):.2f} grados")
    comprobar("d estimada = |V| / 4.03 en todos los bloques",
              all(abs(r[3] - r[1] / 4.03) <= 1.5 for r in b))
    vcc_con = [r[7] for r in b if r[0] <= 9700]
    vcc_sin = [r[7] for r in b if r[0] >= 9800]
    comprobar("autocero activo: VCC_A = 3.650 V +-2 mV pese al offset", all(abs(v - 3650) <= 2 for v in vcc_con),
              f"{sorted(set(vcc_con))}")
    comprobar("autocero desactivado: VCC_A con el error del offset (-5 a -9 mV)",
              bool(vcc_sin) and all(-9 <= v - 3650 <= -5 for v in vcc_sin), f"{sorted(set(vcc_sin))}")

    i0 = lineas.index(next(x for x in lineas if x.startswith("# traza:")))
    i1 = lineas.index("# fin traza")
    puntos = [x for x in lineas[i0:i1] if x.startswith("P,")]
    comprobar("volcado de la traza: 64 puntos", len(puntos) == 64 and "# traza: 64 puntos" in lineas[i0])

    comprobar("captura: 200 codigos entre '# m: ...' y '# fin captura'",
              len(captura) == 200 and any(x.startswith("# m: captura de 200 conversiones de I_OUT") for x in lineas)
              and "# fin captura" in lineas, f"{len(captura)} codigos")
    v_i = VCC_A / 2 - 62.3276 * G / 1000 * (VCC_A / 3.7)
    esperado = round(v_i / VDD * 4095) + OFFSET_ADC
    comprobar("captura: codigos de I_OUT del metal sano (con el offset)",
              bool(captura) and abs(statistics.median(captura) - esperado) <= 1 and max(captura) - min(captura) <= 2,
              f"mediana {statistics.median(captura) if captura else '-'}, esperado {esperado}")

    comprobar("ganancia x11 y vuelta a x2", any(x.startswith("# G=11 ") for x in lineas)
              and any(x.startswith("# G=2 ") for x in lineas[lineas.index(next(x for x in lineas if x.startswith("# G=11 "))):]))
    comprobar("orden desconocida contestada", "# orden desconocida (? = ayuda)" in lineas)
    comprobar("pulsador de 150 ms: un evento 'fijar cero'", lineas.count("# boton: fijar cero") == 1)
    comprobar("rebote de 20 ms ignorado", not any(x.startswith("# boton: calibrar theta") for x in lineas))
    comprobar("'b' borrar traza contestado", "# b: borrar traza" in lineas)

    # Etapas: todas (31) hasta 9.3 s; sin cero (30); sin theta (28); sin autocero (12)
    comprobar("campo etapas: 31, 30 tras 'Z', 28 tras 'T' y 12 tras 'A'",
              all(r[10] == 31 for r in b if r[0] <= 9300) and all(r[10] == 30 for r in b if 9400 <= r[0] <= 9600)
              and all(r[10] == 28 for r in b if r[0] == 9700) and all(r[10] == 12 for r in b if r[0] >= 9800),
              f"{sorted(set(r[10] for r in b))}")
    sin_cero = entre(9310, 9600)
    comprobar("cero desactivado: I', Q' = Ip, Qp rotadas por theta",
              all(abs(r[3] - (r[1] * math.cos(math.radians(19.75)) + r[2] * math.sin(math.radians(19.75)))) < 30
                  for r in sin_cero), f"{len(sin_cero)} muestras")
    comprobar("'z' con el cero desactivado: contestada como ignorada y el cero no cambia",
              "# z: fijar cero -> ignorada (etapa desactivada)" in lineas
              and len({r[4] for r in b if r[0] >= [x[0] for x in b if x[0] <= 9300][-1]}) == 1)
    comprobar("cero y theta desactivados: I' = Ip y Q' = Qp", all(r[3] == r[1] and r[4] == r[2] for r in entre(9610, 10000)))

    fin = b[-1]     # estado al terminar el primer arranque
    esperada = (f"# G={fin[8]} cero=OFF theta=OFF ganancia=ON norm=ON autocero=OFF "
                f"I0_uV={fin[4]} Q0_uV={fin[5]} theta_cdeg={fin[6]}")
    comprobar("segundo arranque: ajustes cargados de la EEPROM", "# EEPROM: ajustes cargados" in lineas2)
    comprobar("segundo arranque: misma G, etapas, I0, Q0 y theta", esperada in lineas2, esperada)

    print(f"\n{fallos} fallos")
    return 1 if fallos else 0


if __name__ == "__main__":
    sys.exit(main())
