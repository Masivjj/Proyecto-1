#!/usr/bin/env python3
"""
medir_jitter.py - SNR del ADC del PIC16F18426 y estimación del jitter de
muestreo a partir de las capturas de la orden 'm' del firmware.

El firmware convierte I_OUT (RC0 / ANC0) 200 veces seguidas, una por flanco
del PWM6 (cada 100 µs, fs = 10 kS/s), y las envía como líneas "M,c1,...,c10"
entre "# m: captura de ..." y "# fin captura".

Método (docs/analisis_adc_jitter.md, sección 6):
  - Cada captura se ajusta a una senoide (ajuste de 4 parámetros, IEEE 1057):
    amplitud, frecuencia, fase y offset. Con la frecuencia conocida se ajustan
    también los armónicos 2 a 5. El residuo es el ruido; los armónicos, la
    distorsión. De ahí salen SINAD, SNR, THD, ENOB y el ruido en LSB.
  - El jitter añade un ruido de potencia P_s·(2π·f·σ)², proporcional a f²,
    donde f es la frecuencia REAL de la entrada (no la que se ve tras el
    submuestreo). Con dos medidas, una a frecuencia baja y otra alta:
        σ² = (N_alta − N_baja) / ((2π)²·(P_alta·f_alta² − P_baja·f_baja²))
    con N el ruido y P la potencia de la señal (LSB²). Equivale a
        σ = sqrt(10^(−SNR_alta/10) − 10^(−SNR_baja/10)) / (2π f_alta)
    cuando las amplitudes son iguales y f_baja << f_alta.

Montaje: desconectar I_OUT de la tarjeta y llevar a RC0 un generador senoidal
con offset VDD/2 (1.85 V) y 3.4 Vpp (−0.7 dBFS) desde 50 Ω. Las frecuencias
por defecto, 1230 Hz y 51 230 Hz, se ven las dos en 1230 Hz (51 230 − 5·10 000).
Con σ = 10 ns, a 51 230 Hz el jitter limita el SNR a 49.8 dB, muy por debajo
de los 74 dB de un ADC ideal de 12 bits: se mide con holgura.

Uso:
    python medir_jitter.py capturar --puerto COM5 --capturas 10 -o baja.txt
    python medir_jitter.py analizar baja.txt --fin 1230 [--png baja.png]
    python medir_jitter.py jitter baja.txt alta.txt --fin-baja 1230 --fin-alta 51230
    python medir_jitter.py simular --sigma 10e-9          valida el método sin hardware
    python medir_jitter.py tabla                          SNR del jitter frente a f

Requisitos: numpy (y pyserial para "capturar", matplotlib para --png).
"""
import argparse
import math
import os
import re
import sys
import time

import numpy as np

CUENTAS = 4096                  # ADC de 12 bits
FS_RMS = CUENTAS / 2 / math.sqrt(2)     # senoide de fondo de escala: 1448 LSB eficaces
PERIODO_US = 100.0              # PERIODO_DISPARO_US del firmware
N_ARMONICOS = 5
SNR_OBJETIVO_DB = 80.0          # límite superior pedido para el SNR del jitter
SIGMA_OBJETIVO = 10e-9


# --------------------------------------------------------------------------
# Lectura de capturas
# --------------------------------------------------------------------------

def leer_capturas(lineas):
    """Devuelve ([capturas], periodo_us) de las líneas del PIC.

    Una captura son los códigos de las líneas "M," entre "# m: captura" y
    "# fin captura"; si una línea llega rota se descarta la captura entera.
    """
    capturas, actual, periodo = [], None, PERIODO_US
    for linea in lineas:
        linea = linea.strip()
        if linea.startswith("# m: captura"):
            actual = []
            m = re.search(r"una cada (\d+) us", linea)
            if m:
                periodo = float(m.group(1))
        elif linea.startswith("M,") and actual is not None:
            try:
                actual.extend(int(x) for x in linea[2:].split(","))
            except ValueError:
                actual = None
        elif linea.startswith("# fin captura") and actual is not None:
            if actual:
                capturas.append(np.array(actual, dtype=float))
            actual = None
    return capturas, periodo


def leer_archivo(ruta):
    with open(ruta, encoding="ascii", errors="replace") as f:
        return leer_capturas(f.read().splitlines())


# --------------------------------------------------------------------------
# Análisis de una captura
# --------------------------------------------------------------------------

def ventana_blackman_harris(n):
    k = np.arange(n)
    a = (0.35875, 0.48829, 0.14128, 0.01168)
    return (a[0] - a[1] * np.cos(2 * np.pi * k / n) + a[2] * np.cos(4 * np.pi * k / n)
            - a[3] * np.cos(6 * np.pi * k / n))


def frecuencia_inicial(x, fs):
    """Pico del espectro (Blackman-Harris) con interpolación parabólica."""
    n = len(x)
    espectro = np.abs(np.fft.rfft((x - x.mean()) * ventana_blackman_harris(n)))
    espectro[:3] = 0.0                  # lejos de DC
    k = int(np.argmax(espectro))
    if 1 <= k < len(espectro) - 1:
        a, b, c = np.log(espectro[k - 1:k + 2] + 1e-12)
        k += 0.5 * (a - c) / (a - 2 * b + c) if (a - 2 * b + c) != 0 else 0.0
    return k * fs / n


def ajuste_senoide(x, fs, f0=None, iteraciones=30):
    """Ajuste de 4 parámetros (IEEE 1057). Devuelve (f, A, B, C) con
    x ≈ A·cos(2πft) + B·sin(2πft) + C."""
    t = np.arange(len(x)) / fs
    f = f0 if f0 is not None else frecuencia_inicial(x, fs)
    for _ in range(iteraciones):
        w = 2 * np.pi * f
        c, s = np.cos(w * t), np.sin(w * t)
        a, b, cc = np.linalg.lstsq(np.column_stack([c, s, np.ones_like(t)]), x, rcond=None)[0]
        d = 2 * np.pi * t * (-a * s + b * c)            # derivada respecto a f
        sol = np.linalg.lstsq(np.column_stack([c, s, np.ones_like(t), d]), x, rcond=None)[0]
        f += sol[3]
        if abs(sol[3]) < 1e-9 * fs:
            break
    w = 2 * np.pi * f
    a, b, cc = np.linalg.lstsq(np.column_stack([np.cos(w * t), np.sin(w * t), np.ones_like(t)]),
                               x, rcond=None)[0]
    return f, a, b, cc


def alias(f, fs):
    """Frecuencia aparente de f muestreada a fs (0 .. fs/2)."""
    return abs((f + fs / 2) % fs - fs / 2)


def analizar_captura(x, fs):
    """Potencias (LSB²) de señal, ruido y armónicos de una captura.

    Si la entrada es continua (amplitud < 2 LSB) solo hay ruido."""
    n = len(x)
    t = np.arange(n) / fs
    if np.ptp(x) < 4 or n < 16:
        return {"dc": True, "media": float(x.mean()), "ruido": float(x.var(ddof=1)), "n": n,
                "gl": n - 1}
    f, a, b, c = ajuste_senoide(x, fs)
    resto = x - (a * np.cos(2 * np.pi * f * t) + b * np.sin(2 * np.pi * f * t) + c)
    p_s = (a * a + b * b) / 2
    p_nd = float(np.sum(resto ** 2) / (n - 4))

    # armónicos 2..5 en su alias, salvo los que caen sobre DC, la fundamental
    # u otro armónico (a menos de 2 casillas: no se podrían separar)
    casilla = fs / n
    ocupadas = [0.0, alias(f, fs)]
    columnas = [np.ones(n), np.cos(2 * np.pi * f * t), np.sin(2 * np.pi * f * t)]
    for h in range(2, N_ARMONICOS + 1):
        fh = alias(h * f, fs)
        if all(abs(fh - o) > 2 * casilla for o in ocupadas) and fh < fs / 2 - casilla:
            ocupadas.append(fh)
            columnas += [np.cos(2 * np.pi * h * f * t), np.sin(2 * np.pi * h * f * t)]
    m = np.column_stack(columnas)
    coef = np.linalg.lstsq(m, x, rcond=None)[0]
    ruido = x - m @ coef
    gl = n - m.shape[1] - 1
    p_ruido = float(np.sum(ruido ** 2) / gl)
    p_h = float(np.sum(coef[3:] ** 2) / 2)
    # solo con ruido, cada armonico ajustado ya recoge ~2·ruido/n
    return {"dc": False, "f": f, "senal": p_s, "ruido": p_ruido, "ruido_dist": p_nd,
            "armonicos": p_h, "armonicos_ruido": (m.shape[1] - 3) * p_ruido / n,
            "offset": c, "n": n, "gl": gl}


def db(x):
    return 10 * math.log10(x) if x > 0 else float("inf")


def resumir(capturas, fs):
    """Analiza todas las capturas y promedia las potencias."""
    res = [analizar_captura(x, fs) for x in capturas]
    if not res:
        raise SystemExit("no hay capturas completas en los datos")
    if all(r["dc"] for r in res):
        ruido = sum(r["ruido"] * r["gl"] for r in res) / sum(r["gl"] for r in res)
        return {"dc": True, "capturas": len(res), "media": float(np.mean([r["media"] for r in res])),
                "ruido": ruido, "gl": sum(r["gl"] for r in res)}
    res = [r for r in res if not r["dc"]]
    gl = sum(r["gl"] for r in res)
    prom = lambda clave: float(np.mean([r[clave] for r in res]))
    ruido = sum(r["ruido"] * r["gl"] for r in res) / gl
    s = {"dc": False, "capturas": len(res), "f_aparente": prom("f"), "senal": prom("senal"),
         "ruido": ruido, "ruido_dist": prom("ruido_dist"), "armonicos": prom("armonicos"),
         "offset": prom("offset"), "gl": gl}
    s["snr"] = db(s["senal"] / s["ruido"])
    s["sinad"] = db(s["senal"] / s["ruido_dist"])
    # THD: si los armonicos no destacan del ruido (3 veces lo esperable) solo
    # se puede dar una cota
    umbral = 3 * prom("armonicos_ruido")
    s["thd_bajo_ruido"] = s["armonicos"] <= umbral
    s["thd"] = db(max(s["armonicos"], umbral) / s["senal"]) if max(s["armonicos"], umbral) > 0 \
        else -float("inf")
    s["enob"] = (s["sinad"] - 1.76) / 6.02
    s["dbfs"] = db(s["senal"] / FS_RMS ** 2)
    return s


def informe(s, fs, f_entrada=None):
    lineas = []
    if s["dc"]:
        sigma = math.sqrt(s["ruido"])
        lineas += [f"{s['capturas']} captura{'s' if s['capturas'] != 1 else ''} de entrada continua (sin senoide)",
                   f"  media           {s['media']:.2f} LSB",
                   f"  ruido           {sigma:.3f} LSB eficaces",
                   f"  SNR a fondo de escala  {20 * math.log10(FS_RMS / max(sigma, 1 / math.sqrt(12))):.1f} dB"
                   "  (ruido mínimo: el de cuantificación, 0.289 LSB)"]
        return "\n".join(lineas)
    lineas += [f"{s['capturas']} captura{'s' if s['capturas'] != 1 else ''} a {fs:.0f} S/s",
               f"  frecuencia aparente   {s['f_aparente']:.1f} Hz"
               + (f"   (entrada real {f_entrada:.0f} Hz, alias {alias(f_entrada, fs):.0f} Hz)" if f_entrada else ""),
               f"  amplitud              {math.sqrt(2 * s['senal']):.1f} LSB pico ({s['dbfs']:.2f} dBFS),"
               f" offset {s['offset']:.1f} LSB",
               f"  ruido                 {math.sqrt(s['ruido']):.3f} LSB eficaces",
               f"  SNR                   {s['snr']:.2f} dB",
               f"  THD                   {'< ' if s['thd_bajo_ruido'] else ''}{s['thd']:.2f} dB"
               f"  (armónicos 2 a {N_ARMONICOS}{', bajo el ruido' if s['thd_bajo_ruido'] else ''})",
               f"  SINAD                 {s['sinad']:.2f} dB",
               f"  ENOB                  {s['enob']:.2f} bits"]
    if f_entrada and alias(f_entrada, fs) > 0 and abs(alias(f_entrada, fs) - s["f_aparente"]) > 0.02 * fs:
        lineas.append(f"  AVISO: la entrada de {f_entrada:.0f} Hz debería verse en {alias(f_entrada, fs):.0f} Hz")
    return "\n".join(lineas)


# --------------------------------------------------------------------------
# Jitter
# --------------------------------------------------------------------------

def snr_jitter_db(f, sigma):
    """Límite del SNR por jitter (TI, nota sobre el ruido del ADC)."""
    return -20 * math.log10(2 * math.pi * f * sigma)


def estimar_jitter(baja, alta, f_baja, f_alta):
    """σ (s) y su incertidumbre a partir de dos resúmenes (señal baja y alta).

    Devuelve (sigma, cota): sigma es None si el aumento del ruido no supera
    dos veces su incertidumbre; cota es entonces el σ máximo compatible."""
    den = (2 * math.pi) ** 2 * (alta["senal"] * f_alta ** 2 - baja["senal"] * f_baja ** 2)
    if den <= 0:
        raise SystemExit("la frecuencia alta debe ser mayor que la baja")
    dif = alta["ruido"] - baja["ruido"]
    # incertidumbre de una varianza estimada con gl grados de libertad
    u = math.sqrt(2 * alta["ruido"] ** 2 / alta["gl"] + 2 * baja["ruido"] ** 2 / baja["gl"])
    if dif > 2 * u:
        return math.sqrt(dif / den), math.sqrt((dif + 2 * u) / den)
    return None, math.sqrt(max(dif, 0.0) + 2 * u) / math.sqrt(den)


def informe_jitter(sigma, cota):
    lineas = []
    if sigma is None:
        lineas.append(f"jitter: no se distingue del ruido del ADC; σ < {cota * 1e9:.2f} ns (2 incertidumbres)")
        s = cota
    else:
        lineas.append(f"jitter: σ = {sigma * 1e9:.2f} ns  (cota superior {cota * 1e9:.2f} ns)")
        s = sigma
    ok = cota < SIGMA_OBJETIVO
    lineas.append(f"  objetivo σ < {SIGMA_OBJETIVO * 1e9:.0f} ns: {'CUMPLE' if ok else 'NO CUMPLE'}")
    f80 = 10 ** (-SNR_OBJETIVO_DB / 20) / (2 * math.pi * s)
    lineas.append(f"  SNR del jitter ≥ {SNR_OBJETIVO_DB:.0f} dB para señales de hasta"
                  f"{' al menos' if sigma is None else ''} {f80:,.0f} Hz")
    for f in (10, 100, 1000, 10000):
        lineas.append(f"    a {f:>6} Hz: {snr_jitter_db(f, s):6.1f} dB")
    return "\n".join(lineas)


# --------------------------------------------------------------------------
# Simulación (valida el método)
# --------------------------------------------------------------------------

def simular_capturas(f, sigma, capturas=10, n=200, fs=1e4, amplitud=1900.0, ruido=0.5,
                     offset=2048.0, generador=None):
    """Capturas de una senoide de frecuencia real f muestreada con jitter σ
    (gaussiano, independiente en cada muestra), ruido térmico y cuantificación."""
    g = generador or np.random.default_rng(1)
    resultado = []
    for _ in range(capturas):
        t = np.arange(n) / fs + g.normal(0.0, sigma, n)
        x = offset + amplitud * np.sin(2 * np.pi * f * t + g.uniform(0, 2 * np.pi)) + g.normal(0.0, ruido, n)
        resultado.append(np.clip(np.round(x), 0, CUENTAS - 1))
    return resultado


def formato_pic(capturas, periodo_us=PERIODO_US):
    """Las capturas como las envía el firmware (para guardarlas en un archivo)."""
    lineas = []
    for x in capturas:
        lineas.append(f"# m: captura de {len(x)} conversiones de I_OUT, una cada {periodo_us:.0f} us")
        codigos = [str(int(c)) for c in x]
        lineas += ["M," + ",".join(codigos[k:k + 10]) for k in range(0, len(codigos), 10)]
        lineas.append("# fin captura")
    return lineas


# --------------------------------------------------------------------------
# Puerto serie
# --------------------------------------------------------------------------

def capturar(puerto, baudios, n_capturas, espera_s=3.0):
    """Envía 'm' n_capturas veces y devuelve las líneas de las capturas."""
    import serial                       # pyserial
    lineas = []
    with serial.Serial(puerto, baudios, timeout=0.1) as p:
        resto = b""
        for k in range(n_capturas):
            p.reset_input_buffer()
            p.write(b"m")
            fin = time.monotonic() + espera_s
            dentro = False
            while time.monotonic() < fin:
                resto += p.read(512)
                *nuevas, resto = resto.split(b"\n")
                for cruda in nuevas:
                    linea = cruda.decode("ascii", "replace").strip()
                    if linea.startswith("# m: espere"):
                        time.sleep(0.2)
                        p.write(b"m")
                    elif linea.startswith("# m: captura"):
                        dentro = True
                    if dentro and (linea.startswith("#") or linea.startswith("M,")):
                        lineas.append(linea)
                    if dentro and linea.startswith("# fin captura"):
                        fin = 0.0
                        break
            print(f"captura {k + 1}/{n_capturas}: {'bien' if fin == 0.0 else 'sin respuesta'}")
    return lineas


# --------------------------------------------------------------------------
# Gráfica
# --------------------------------------------------------------------------

def dibujar(capturas, fs, titulo, ruta):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    tinta, tenue, serie = "#0b0b0b", "#52514e", "#2a78d6"
    n = len(capturas[0])
    w = ventana_blackman_harris(n)
    # se quita la media ponderada con la ventana: la casilla de DC queda vacía
    potencia = np.mean([np.abs(np.fft.rfft((x - np.dot(x, w) / w.sum()) * w)) ** 2 for x in capturas
                        if len(x) == n], axis=0)
    # 0 dBFS = pico de una senoide de fondo de escala con la misma ventana
    ref = (CUENTAS / 2 * w.sum() / 2) ** 2
    f = np.arange(len(potencia)) * fs / n

    fig, (ax_t, ax_f) = plt.subplots(2, 1, figsize=(9, 6.5), facecolor="#fcfcfb")
    fig.suptitle(titulo, color=tinta, fontsize=11)
    t_ms = np.arange(n) / fs * 1000
    ax_t.plot(t_ms, capturas[0], color=serie, lw=1.5, marker="o", ms=2.5)
    ax_t.set_title("Primera captura (códigos del ADC)", fontsize=10, color=tinta, loc="left")
    ax_t.set_xlabel("tiempo (ms)", color=tenue)
    dbfs = 10 * np.log10(potencia / ref + 1e-20)
    ax_f.plot(f, dbfs, color=serie, lw=1.5)
    ax_f.set_title(f"Espectro promedio de {len(capturas)} capturas (Blackman-Harris)", fontsize=10,
                   color=tinta, loc="left")
    ax_f.set_xlabel("frecuencia aparente (Hz)", color=tenue)
    ax_f.set_ylabel("dBFS", color=tenue)
    ax_f.set_ylim(max(-160.0, float(np.percentile(dbfs, 5)) - 20), 5)
    for ax in (ax_t, ax_f):
        ax.set_facecolor("#fcfcfb")
        ax.grid(True, color="#e6e5e0", lw=0.8)
        ax.tick_params(colors=tenue, labelsize=8)
        for lado in ("top", "right"):
            ax.spines[lado].set_visible(False)
        for lado in ("left", "bottom"):
            ax.spines[lado].set_color("#c3c2b7")
    fig.tight_layout()
    fig.savefig(ruta, dpi=100)
    print(f"gráfica guardada en {ruta}")


# --------------------------------------------------------------------------

def main():
    p = argparse.ArgumentParser(description="SNR del ADC del PIC y jitter de muestreo (orden 'm').")
    sub = p.add_subparsers(dest="accion", required=True)

    c = sub.add_parser("capturar", help="pide capturas al PIC por el puerto serie")
    c.add_argument("--puerto", required=True)
    c.add_argument("--baudios", type=int, default=115200)
    c.add_argument("--capturas", type=int, default=10)
    c.add_argument("-o", "--salida", required=True, help="archivo de texto con las capturas")

    a = sub.add_parser("analizar", help="SNR, THD, SINAD y ENOB de un archivo de capturas")
    a.add_argument("archivo")
    a.add_argument("--fin", type=float, help="frecuencia real de la entrada (Hz)")
    a.add_argument("--png", help="guarda la captura y el espectro en esta imagen")

    j = sub.add_parser("jitter", help="estima σ con una captura a frecuencia baja y otra alta")
    j.add_argument("baja")
    j.add_argument("alta")
    j.add_argument("--fin-baja", type=float, default=1230.0)
    j.add_argument("--fin-alta", type=float, default=51230.0)

    s = sub.add_parser("simular", help="valida el método con capturas sintéticas de σ conocido")
    s.add_argument("--sigma", type=float, default=10e-9, help="jitter simulado (s)")
    s.add_argument("--ruido", type=float, default=0.5, help="ruido del ADC (LSB eficaces)")
    s.add_argument("--capturas", type=int, default=10)
    s.add_argument("--fin-baja", type=float, default=1230.0)
    s.add_argument("--fin-alta", type=float, default=51230.0)
    s.add_argument("--semilla", type=int, default=1)
    s.add_argument("--guardar", metavar="PREFIJO",
                   help="guarda las capturas en PREFIJO_baja.txt y PREFIJO_alta.txt (formato del PIC)")

    t = sub.add_parser("tabla", help="SNR límite del jitter frente a la frecuencia")
    t.add_argument("--sigma", type=float, nargs="*", default=[1e-9, 10e-9, 100e-9, 1e-6])

    args = p.parse_args()
    fs = 1e6 / PERIODO_US

    if args.accion == "capturar":
        lineas = capturar(args.puerto, args.baudios, args.capturas)
        with open(args.salida, "w", encoding="ascii") as f:
            f.write("\n".join(lineas) + "\n")
        capturas, _ = leer_capturas(lineas)
        print(f"{len(capturas)} capturas guardadas en {args.salida}")
        return 0 if capturas else 1

    if args.accion == "analizar":
        capturas, periodo = leer_archivo(args.archivo)
        fs = 1e6 / periodo
        print(informe(resumir(capturas, fs), fs, args.fin))
        if args.png and capturas:
            dibujar(capturas, fs, os.path.basename(args.archivo), args.png)
        return 0

    if args.accion == "jitter":
        cb, periodo = leer_archivo(args.baja)
        ca, _ = leer_archivo(args.alta)
        fs = 1e6 / periodo
        rb, ra = resumir(cb, fs), resumir(ca, fs)
        if rb["dc"] or ra["dc"]:
            raise SystemExit("las dos capturas deben ser de una senoide")
        print(f"señal baja ({args.fin_baja:.0f} Hz):\n" + informe(rb, fs, args.fin_baja))
        print(f"señal alta ({args.fin_alta:.0f} Hz):\n" + informe(ra, fs, args.fin_alta))
        print(informe_jitter(*estimar_jitter(rb, ra, args.fin_baja, args.fin_alta)))
        return 0

    if args.accion == "simular":
        g = np.random.default_rng(args.semilla)
        cb = simular_capturas(args.fin_baja, args.sigma, args.capturas, ruido=args.ruido, generador=g)
        ca = simular_capturas(args.fin_alta, args.sigma, args.capturas, ruido=args.ruido, generador=g)
        if args.guardar:
            for nombre, capturas in (("baja", cb), ("alta", ca)):
                with open(f"{args.guardar}_{nombre}.txt", "w", encoding="ascii") as f:
                    f.write("\n".join(formato_pic(capturas)) + "\n")
            print(f"capturas guardadas en {args.guardar}_baja.txt y {args.guardar}_alta.txt")
        rb, ra = resumir(cb, fs), resumir(ca, fs)
        print(f"simulación: σ = {args.sigma * 1e9:.2f} ns, ruido {args.ruido} LSB, {args.capturas} capturas")
        print(f"señal baja ({args.fin_baja:.0f} Hz):\n" + informe(rb, fs, args.fin_baja))
        print(f"señal alta ({args.fin_alta:.0f} Hz):\n" + informe(ra, fs, args.fin_alta))
        print(informe_jitter(*estimar_jitter(rb, ra, args.fin_baja, args.fin_alta)))
        return 0

    frecuencias = (10, 100, 1000, 1592, 10000, 20000, 51230)
    print("SNR límite por jitter, -20·log10(2π·f·σ), en dB")
    print("     f (Hz) " + "".join(f"{s * 1e9:>10.0f} ns" for s in args.sigma))
    for f in frecuencias:
        print(f"{f:>11} " + "".join(f"{snr_jitter_db(f, s):>13.1f}" for s in args.sigma))
    print(" f máx. para 80 dB:" + "".join(f"{10 ** -4 / (2 * math.pi * s):>11,.0f} Hz" for s in args.sigma))
    return 0


if __name__ == "__main__":
    sys.exit(main())
