#!/usr/bin/env python3
"""Comprueba la salida del firmware simulado en el PC (tests/sim, "make sim").

Uso: verificar_sim.py <salida_primer_arranque> <salida_segundo_arranque>

El guion del simulador (tests/sim/sim.c) es:
  0.0-2.0 s  metal sano con el desequilibrio del puente (62.33, 21.14) mV
  0.5 s      'z' fijar cero          1.0 s  '?' estado
  2.0-3.0 s  fisura: dos lobulos de 65 mV a 60 grados
  3.0 s      el lazo se retrasa 3 ticks (se recupera sin perder muestras)
  3.5-4.5 s  lift-off de 30 mV a 20 grados;  4.0 s 'c' calibrar theta
  5.0 s      'p' volcar la traza     5.8 / 5.9 s  ganancia x11 y vuelta a x2
  6.2 s      orden desconocida       6.5 s  el lazo se retrasa 15 ticks
  7.0 s      pulsador "Fijar cero" 150 ms;  8.0 s  rebote de 20 ms en "Calibrar"
  9.0 s      'b' borrar traza
  9.3 s      'Z' desactiva el cero;  9.45 s 'z' (ignorada);  9.6 s 'T' desactiva theta
El segundo arranque usa la EEPROM que dejo el primero.
"""
import math
import sys

fallos = 0


def comprobar(nombre, ok, detalle=""):
    global fallos
    if not ok:
        fallos += 1
    print(("OK     " if ok else "FALLO  ") + nombre + (f"  ({detalle})" if detalle else ""))


def leer(ruta):
    with open(ruta, encoding="ascii") as f:
        lineas = f.read().splitlines()
    muestras, bloques = [], []
    for linea in lineas:
        if linea[:1].isdigit():
            muestras.append([int(x) for x in linea.split(",")])
        elif linea.startswith("B,"):
            # el bloque se cierra con la muestra anterior: se guarda su t_ms
            bloques.append([muestras[-1][0]] + [int(x) for x in linea[2:].split(",")])
    return lineas, muestras, bloques


def fase(i, q):
    return math.degrees(math.atan2(q, i))


def main():
    lineas, m, b = leer(sys.argv[1])
    lineas2, _, _ = leer(sys.argv[2])
    entre = lambda t0, t1: [r for r in m if t0 <= r[0] < t1]

    comprobar("todas las lineas de muestra tienen 5 campos", all(len(r) == 5 for r in m))
    comprobar("todas las lineas de bloque tienen 10 campos", all(len(r) == 11 for r in b))
    comprobar("primer arranque sin datos en la EEPROM", "# EEPROM: sin datos, valores por defecto" in lineas)

    t = [r[0] for r in m]
    saltos = [(t[i], t[i + 1]) for i in range(len(t) - 1) if t[i + 1] - t[i] != 10]
    comprobar("t_ms avanza de 10 en 10 salvo un hueco de 150 ms hacia 6.5 s",
              len(saltos) == 1 and saltos[0][1] - saltos[0][0] == 150 and 6400 < saltos[0][0] < 6600,
              f"saltos {saltos}")
    con_retraso = [r[0] for r in b if r[9] & 0x20]
    comprobar("bandera de retraso solo en el bloque del hueco", len(con_retraso) == 1 and 6500 <= con_retraso[0] <= 6800,
              f"bloques {con_retraso}")
    comprobar("muestras perdidas = 0 en el estado de t = 1 s",
              any("muestras perdidas: 0" in x for x in lineas))

    sano = entre(600, 2000)
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
    comprobar("VCC_A medido ~ 3.65 V", all(abs(r[7] - 3650) < 20 for r in b))

    i0 = lineas.index(next(x for x in lineas if x.startswith("# traza:")))
    i1 = lineas.index("# fin traza")
    puntos = [x for x in lineas[i0:i1] if x.startswith("P,")]
    comprobar("volcado de la traza: 64 puntos", len(puntos) == 64 and "# traza: 64 puntos" in lineas[i0])

    comprobar("ganancia x11 y vuelta a x2", any(x.startswith("# G=11 ") for x in lineas)
              and any(x.startswith("# G=2 ") for x in lineas[lineas.index(next(x for x in lineas if x.startswith("# G=11 "))):]))
    comprobar("orden desconocida contestada", "# orden desconocida (? = ayuda)" in lineas)
    comprobar("pulsador de 150 ms: un evento 'fijar cero'", lineas.count("# boton: fijar cero") == 1)
    comprobar("rebote de 20 ms ignorado", lineas.count("# boton: calibrar theta") == 0)
    comprobar("'b' borrar traza contestado", "# b: borrar traza" in lineas)

    # etapas: cero desactivado desde 9.3 s y theta desde 9.6 s
    comprobar("campo etapas: 15 hasta 9.3 s, 14 hasta 9.6 s y 12 al final",
              all(r[10] == 15 for r in b if r[0] <= 9300) and all(r[10] == 14 for r in b if 9400 <= r[0] <= 9600)
              and all(r[10] == 12 for r in b if r[0] >= 9700), f"{sorted(set(r[10] for r in b))}")
    sin_cero = entre(9310, 9600)
    comprobar("cero desactivado: I', Q' = Ip, Qp rotadas por theta",
              all(abs(r[3] - (r[1] * math.cos(math.radians(19.7)) + r[2] * math.sin(math.radians(19.7)))) < 30
                  for r in sin_cero), f"{len(sin_cero)} muestras")
    comprobar("'z' con el cero desactivado: contestada como ignorada y el cero no cambia",
              "# z: fijar cero -> ignorada (etapa desactivada)" in lineas
              and len({r[4] for r in b if r[0] >= [x[0] for x in b if x[0] <= 9300][-1]}) == 1)
    comprobar("cero y theta desactivados: I' = Ip y Q' = Qp", all(r[3] == r[1] and r[4] == r[2] for r in entre(9610, 10000)))
    comprobar("la configuracion informa de las etapas",
              any(x.startswith("# G=2 cero=OFF theta=OFF ganancia=ON norm=ON ") for x in lineas))

    fin = b[-1]     # estado al terminar el primer arranque
    esperada = (f"# G={fin[8]} cero=OFF theta=OFF ganancia=ON norm=ON "
                f"I0_uV={fin[4]} Q0_uV={fin[5]} theta_cdeg={fin[6]}")
    comprobar("segundo arranque: ajustes cargados de la EEPROM", "# EEPROM: ajustes cargados" in lineas2)
    comprobar("segundo arranque: misma G, etapas, I0, Q0 y theta", esperada in lineas2, esperada)

    print(f"\n{fallos} fallos")
    return 1 if fallos else 0


if __name__ == "__main__":
    sys.exit(main())
