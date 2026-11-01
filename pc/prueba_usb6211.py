#!/usr/bin/env python3
"""
prueba_usb6211.py - Prueba del ADC del PIC16F18426 a 1000 muestras/s con una
NI USB-6211. Hace lo mismo que el VI descrito en
docs/pruebas_labview_usb6211.md.

La tarjeta de NI hace de generador, de referencia y de frecuencímetro:
  AO 0   senoide de prueba (117 Hz, offset 1.65 V, 3 Vpp por defecto) hacia
         RC0, a través de 1 kΩ con 100 nF a masa
  AI 0   la misma señal en RC0 (RSE): referencia de 16 bits
  AI 1   VDD del PIC (opcional, --ai-vdd): pasa los códigos a voltios sin
         el ±4 % del FVR
  PFI 0  RA1 del PIC (PWM6, un flanco por conversión): periodo de muestreo
El PIC envía sus muestras por el puerto serie (115200 8N1):
  --firmware avance    ADC_1kSPS_PIC16F18426.X: una línea por muestra
  --firmware completo  P2_IQ_PIC16F18426.X (1.3 o posterior): capturas de la
                       orden 'm', 200 conversiones de I_OUT, una por ms.
                       Antes hay que desconectar I_OUT de la tarjeta P2.

Resultados:
  - la frecuencia de muestreo de cada canal por tres caminos: el contador
    (RA1), el cociente de frecuencias AI/PIC y, en el avance, el reloj del PC
  - la amplitud y el offset del PIC frente a la AI
  - SNR, THD, SINAD y ENOB del PIC (ajuste de senoide, medir_jitter.py)
  - el jitter de periodo que el contador puede resolver. Con su base de
    tiempo de 80 MHz (12.5 ns) no puede certificar σ < 10 ns: para eso, el
    osciloscopio o el método del SNR (docs/analisis_adc_jitter.md, sección 6)

Uso:
  python prueba_usb6211.py --puerto COM5 --firmware avance
  python prueba_usb6211.py --puerto COM5 --firmware completo --capturas 5 --ai-vdd
  python prueba_usb6211.py --puerto COM5 --sin-generador   (generador de laboratorio)
  python prueba_usb6211.py --demo [--firmware completo]    sin hardware

Requisitos: numpy y pyserial (pc/requirements.txt), y nidaqmx (pip install
nidaqmx) con el controlador NI-DAQmx instalado. --demo no los necesita.
"""
import argparse
import math
import sys
import time

import numpy as np

import medir_jitter as mj

FS_NOMINAL = 1000.0             # muestras/s de cada entrada del PIC
CUENTAS = 4095                  # fondo de escala del ADC de 12 bits (VDD)
FS_AO = 100_000.0               # actualizaciones/s de AO 0
FS_AI = 10_000.0                # muestras/s de AI 0 (y AI 1)
V_MAX = 3.3                     # la salida nunca pasa de aquí (pin del PIC)
BASE_CONTADOR_HZ = 80e6         # base de tiempo de los contadores de la USB-6211
DISPAROS_POR_MUESTRA = {"avance": 1, "completo": 10}   # flancos de RA1 por muestra de un canal
TOLERANCIA_FS = 0.02            # HFINTOSC: ±2 % entre 0 y 60 °C
BAUDIOS = 115200


# --------------------------------------------------------------------------
# Generador y análisis (sin hardware)
# --------------------------------------------------------------------------

def comprobar_limites(offset, vpp, v_max=V_MAX):
    """Error si la senoide se sale de 0..v_max (protege el pin del PIC)."""
    bajo, alto = offset - vpp / 2, offset + vpp / 2
    if vpp <= 0 or bajo < 0 or alto > v_max:
        raise ValueError(f"la senoide va de {bajo:.3f} a {alto:.3f} V: debe quedar entre 0 y {v_max} V")


def onda_generador(f, offset, vpp, fs_ao=FS_AO, duracion=1.0):
    """Un buffer de `duracion` segundos con un número entero de periodos, para
    regenerarlo sin saltos. Devuelve (muestras en V, frecuencia real)."""
    comprobar_limites(offset, vpp)
    n = int(round(fs_ao * duracion))
    ciclos = max(1, int(round(f * duracion)))
    f_real = ciclos / duracion
    t = np.arange(n) / fs_ao
    onda = offset + vpp / 2 * np.sin(2 * np.pi * f_real * t)
    return np.clip(onda, 0.0, V_MAX), f_real


def analizar_lsb(capturas, fs):
    """medir_jitter.resumir de una o varias capturas en LSB del PIC."""
    return mj.resumir([np.asarray(x, dtype=float) for x in capturas], fs)


def fs_por_cociente(f_ref, f_pic, fs_nominal=FS_NOMINAL):
    """Frecuencia de muestreo real del PIC. Si el PIC muestrea a fs, la
    señal de f_ref se ve en f_ref · fs_nominal / fs. Solo vale sin alias."""
    if f_pic <= 0 or f_ref >= 0.49 * fs_nominal:
        return None
    return fs_nominal * f_ref / f_pic


def estadistica_periodos(periodos, disparos_por_muestra, base_hz=BASE_CONTADOR_HZ):
    """Periodos de RA1 (s, consecutivos) -> frecuencia de muestreo por canal y jitter.

    Cada lectura del contador cuantifica sus dos flancos con la base de
    tiempo. Si la fase entre el PIC y la base de 80 MHz es aleatoria, el error
    es triangular, de σ = q/√6 (5.1 ns): es el "suelo". Con un periodo fijo
    el error se repite con el patrón de ticks y puede quedar por debajo. Lo
    que pase claramente del suelo es jitter del PIC (o del propio contador)."""
    p = np.asarray(periodos, dtype=float)
    k = disparos_por_muestra
    q = 1.0 / base_hz
    suelo = q / math.sqrt(6)
    m = len(p) // k
    muestra = p[:m * k].reshape(m, k).sum(axis=1)       # intervalo entre muestras del mismo canal
    s_disparo = float(p.std(ddof=1)) if len(p) > 1 else float("nan")
    s_muestra = float(muestra.std(ddof=1)) if m > 1 else float("nan")
    estima = lambda s: math.sqrt(max(s * s - suelo * suelo, 0.0))
    return {"n": len(p), "f_disparo": 1.0 / float(p.mean()), "fs": 1.0 / float(muestra.mean()),
            "sigma_disparo": s_disparo, "sigma_muestra": s_muestra, "suelo": suelo,
            "jitter_disparo": estima(s_disparo), "jitter_muestra": estima(s_muestra),
            "resuelve": s_muestra > 2 * suelo}


def evaluar(d):
    """Medidas y comprobaciones a partir de los datos crudos de una prueba
    (de la tarjeta o de simular()). Devuelve (medidas, [(nombre, ok, detalle)])."""
    k = DISPAROS_POR_MUESTRA[d["firmware"]]
    vdd = d["vdd_ai"] if d.get("vdd_ai") else d.get("vdd_pic") or d.get("vdd_supuesta", 3.3)
    origen_vdd = "AI 1" if d.get("vdd_ai") else ("FVR del PIC" if d.get("vdd_pic") else "supuesta")
    med = {"vdd": vdd, "origen_vdd": origen_vdd, "k": k}
    pruebas = []

    pic = analizar_lsb(d["pic"], FS_NOMINAL)
    med["pic"] = pic
    if pic["dc"]:
        pruebas.append(("el PIC ve una senoide", False, "entrada continua: ¿generador conectado a RC0?"))
        return med, pruebas
    med["pic_vpp"] = 2 * math.sqrt(2 * pic["senal"]) * vdd / CUENTAS
    med["pic_offset"] = pic["offset"] * vdd / CUENTAS

    f_ref = d.get("f_generador")
    if d.get("ai") is not None:
        ai = analizar_lsb([np.asarray(d["ai"]) * CUENTAS / vdd], d["fs_ai"])
        med["ai"] = ai
        if not ai["dc"]:
            f_ref = ai["f_aparente"]
            med["ai_vpp"] = 2 * math.sqrt(2 * ai["senal"]) * vdd / CUENTAS
            med["ai_offset"] = ai["offset"] * vdd / CUENTAS
            tol = 0.01 if origen_vdd == "AI 1" else 0.05     # el FVR tiene ±4 %
            dif = med["pic_vpp"] / med["ai_vpp"] - 1
            pruebas.append((f"amplitud del PIC = la de la AI (±{tol:.0%})", abs(dif) <= tol,
                            f"{med['pic_vpp']:.4f} frente a {med['ai_vpp']:.4f} Vpp ({dif:+.2%})"))
            dif_off = med["pic_offset"] - med["ai_offset"]
            tol_off = 0.010 + (0.0 if origen_vdd == "AI 1" else 0.04 * med["ai_offset"])
            pruebas.append((f"offset del PIC = el de la AI (±{tol_off * 1000:.0f} mV)", abs(dif_off) <= tol_off,
                            f"{med['pic_offset']:.4f} frente a {med['ai_offset']:.4f} V ({dif_off * 1000:+.1f} mV)"))
    med["f_ref"] = f_ref

    if f_ref:
        fs_c = fs_por_cociente(f_ref, pic["f_aparente"])
        med["fs_cociente"] = fs_c
        if fs_c:
            pruebas.append(("fs por el cociente de frecuencias: 1000 S/s ±2 %",
                            abs(fs_c / FS_NOMINAL - 1) <= TOLERANCIA_FS,
                            f"{fs_c:.2f} S/s ({f_ref:.3f} Hz se ve en {pic['f_aparente']:.3f} Hz)"))
    if d.get("periodos") is not None and len(d["periodos"]) > 2 * k:
        c = estadistica_periodos(d["periodos"], k)
        med["contador"] = c
        pruebas.append(("fs por el contador (RA1): 1000 S/s ±2 %", abs(c["fs"] / FS_NOMINAL - 1) <= TOLERANCIA_FS,
                        f"{c['fs']:.2f} S/s; RA1 a {c['f_disparo']:.1f} Hz / {k}"))
        if med.get("fs_cociente"):
            dif = med["fs_cociente"] / c["fs"] - 1
            pruebas.append(("contador y cociente coinciden (±0.1 %)", abs(dif) <= 0.001, f"{dif:+.4%}"))
    if d.get("fs_pc"):
        pruebas.append(("fs por el reloj del PC (aprox. ±0.5 %): 1000 S/s ±2.5 %",
                        abs(d["fs_pc"] / FS_NOMINAL - 1) <= TOLERANCIA_FS + 0.005, f"{d['fs_pc']:.1f} S/s"))
    if d.get("perdidas") is not None:
        pruebas.append(("muestras perdidas = 0", d["perdidas"] == 0, f"{d['perdidas']}"))
    if d.get("linea_adc") is not None:
        pruebas.append(("el firmware informa 1000 muestras/s por canal", "1000 muestras/s por canal" in d["linea_adc"],
                        d["linea_adc"] or "sin la línea '# ADC:' (¿firmware anterior a 1.3?)"))
    saturada = any(np.min(x) <= 0 or np.max(x) >= CUENTAS for x in d["pic"])
    pruebas.append(("sin saturar el ADC", not saturada, "bajar la amplitud" if saturada else ""))
    return med, pruebas


def informe(d, med, pruebas):
    lineas = [f"Prueba del ADC del PIC16F18426 con la USB-6211 ({d['firmware']}, {d.get('origen', '')})"]
    if d.get("f_generador"):
        lineas.append(f"  generador AO 0: {d['f_generador']:.3f} Hz, {d['vpp']:.3f} Vpp, offset {d['offset']:.3f} V")
    lineas.append(f"  VDD = {med['vdd']:.4f} V ({med['origen_vdd']})")
    pic = med["pic"]
    if not pic["dc"]:
        n = sum(len(x) for x in d["pic"])
        lineas += [f"PIC ({n} muestras en {len(d['pic'])} bloque{'s' if len(d['pic']) != 1 else ''}):",
                   f"  {pic['f_aparente']:.3f} Hz aparentes, {med['pic_vpp']:.4f} Vpp, offset {med['pic_offset']:.4f} V",
                   f"  SNR {pic['snr']:.1f} dB, THD {'< ' if pic['thd_bajo_ruido'] else ''}{pic['thd']:.1f} dB, "
                   f"SINAD {pic['sinad']:.1f} dB, ENOB {pic['enob']:.2f} bits, "
                   f"ruido {math.sqrt(pic['ruido']):.2f} LSB eficaces"]
    if "ai_vpp" in med:
        ai = med["ai"]
        lineas += ["AI 0 (referencia):",
                   f"  {ai['f_aparente']:.4f} Hz, {med['ai_vpp']:.4f} Vpp, offset {med['ai_offset']:.4f} V, "
                   f"SNR {ai['snr']:.1f} dB, THD {'< ' if ai['thd_bajo_ruido'] else ''}{ai['thd']:.1f} dB"]
    if "contador" in med:
        c = med["contador"]
        lineas += [f"Contador en RA1 ({c['n']} periodos):",
                   f"  RA1 a {c['f_disparo']:.2f} Hz -> {c['fs']:.3f} muestras/s por canal",
                   f"  σ del periodo entre muestras de un canal: {c['sigma_muestra'] * 1e9:.1f} ns"
                   f" (suelo del contador {c['suelo'] * 1e9:.1f} ns)",
                   "  " + (f"jitter estimado ≈ {c['jitter_muestra'] * 1e9:.1f} ns (orientativo)" if c["resuelve"] else
                           f"por debajo de lo que resuelve el contador: σ < {2 * c['suelo'] * 1e9:.0f} ns aprox."),
                   "  (el contador no certifica σ < 10 ns: usar el osciloscopio o el método del SNR)"]
    lineas.append("Comprobaciones:")
    for nombre, ok, detalle in pruebas:
        lineas.append(("  OK     " if ok else "  FALLO  ") + nombre + (f"  ({detalle})" if detalle else ""))
    return "\n".join(lineas)


# --------------------------------------------------------------------------
# Simulación (--demo y pruebas)
# --------------------------------------------------------------------------

def simular(firmware="avance", f=117.0, offset=1.65, vpp=3.0, segundos=5.0, capturas=5,
            vdd=3.30, fs_pic=1007.0, sigma=3e-9, error_fvr=0.012, semilla=1):
    """Datos como los de una prueba real: PIC con su fs y su jitter, AI de
    16 bits y contador de 80 MHz con su cuantificación."""
    g = np.random.default_rng(semilla)
    _, f_real = onda_generador(f, offset, vpp)
    k = DISPAROS_POR_MUESTRA[firmware]

    def pic(n, t0):
        t = t0 + np.arange(n) / fs_pic + g.normal(0.0, sigma, n)
        v = offset + vpp / 2 * np.sin(2 * np.pi * f_real * t)
        return np.clip(np.round(v / vdd * CUENTAS + 1.5 + g.normal(0.0, 0.4, n)), 0, CUENTAS)

    n_ai = int(FS_AI * segundos)
    t_ai = np.arange(n_ai) / FS_AI
    d = {"firmware": firmware, "origen": "simulación", "f_generador": f_real, "vpp": vpp, "offset": offset,
         "ai": offset + vpp / 2 * np.sin(2 * np.pi * f_real * t_ai + 0.3) + g.normal(0.0, 150e-6, n_ai),
         "fs_ai": FS_AI}
    # Flancos de RA1 con jitter, medidos con una base de 80 MHz de fase aleatoria
    n_p = 2000
    flancos = np.arange(n_p + 1) / (fs_pic * k) + g.normal(0.0, sigma, n_p + 1)
    ticks = np.floor((flancos + g.uniform(0, 1 / BASE_CONTADOR_HZ)) * BASE_CONTADOR_HZ)
    d["periodos"] = np.diff(ticks) / BASE_CONTADOR_HZ
    if firmware == "avance":
        d["pic"] = [pic(int(fs_pic * segundos), 0.0)]
        d["vdd_pic"] = round(vdd * (1 + error_fvr), 3)
        d["fs_pc"] = fs_pic * (1 + g.normal(0.0, 0.001))
        d["perdidas"] = 0
    else:
        d["pic"] = [pic(200, g.uniform(0, 1)) for _ in range(capturas)]
        d["vdd_supuesta"] = vdd
        d["linea_adc"] = "# ADC: modo basico, disparo PWM6 cada 100 us, 1000 muestras/s por canal, 10 por trama"
    return d


# --------------------------------------------------------------------------
# Tarjeta USB-6211 (nidaqmx) y puerto serie
# --------------------------------------------------------------------------

def generador_iniciar(dev, canal, onda, fs_ao=FS_AO):
    import nidaqmx
    from nidaqmx.constants import AcquisitionType
    t = nidaqmx.Task()
    t.ao_channels.add_ao_voltage_chan(f"{dev}/{canal}", min_val=0.0, max_val=V_MAX)
    t.timing.cfg_samp_clk_timing(fs_ao, sample_mode=AcquisitionType.CONTINUOUS, samps_per_chan=len(onda))
    t.write(onda, auto_start=False)          # se regenera el buffer: periodos enteros, sin saltos
    t.start()
    return t


def generador_parar(tarea, dev, canal):
    import nidaqmx
    tarea.stop()
    tarea.close()
    with nidaqmx.Task() as t:                # deja la salida a 0 V
        t.ao_channels.add_ao_voltage_chan(f"{dev}/{canal}", min_val=0.0, max_val=V_MAX)
        t.write(0.0, auto_start=True)


def leer_ai(dev, canales, fs, segundos):
    import nidaqmx
    from nidaqmx.constants import AcquisitionType, TerminalConfiguration
    n = int(fs * segundos)
    with nidaqmx.Task() as t:
        for c in canales:                    # 0..5 V: la USB-6211 elige el rango de ±5 V
            t.ai_channels.add_ai_voltage_chan(f"{dev}/{c}", terminal_config=TerminalConfiguration.RSE,
                                              min_val=0.0, max_val=5.0)
        t.timing.cfg_samp_clk_timing(fs, sample_mode=AcquisitionType.FINITE, samps_per_chan=n)
        datos = t.read(number_of_samples_per_channel=n, timeout=segundos + 10)
    return np.atleast_2d(np.asarray(datos, dtype=float))


def medir_periodos(dev, contador, terminal, n, periodo_nominal):
    import nidaqmx
    from nidaqmx.constants import AcquisitionType, CounterFrequencyMethod, Edge, TimeUnits
    with nidaqmx.Task() as t:
        ch = t.ci_channels.add_ci_period_chan(f"{dev}/{contador}", min_val=periodo_nominal / 2,
                                              max_val=periodo_nominal * 2, units=TimeUnits.SECONDS,
                                              edge=Edge.RISING,
                                              meas_method=CounterFrequencyMethod.LOW_FREQUENCY_1_COUNTER)
        ch.ci_period_term = f"/{dev}/{terminal}"
        t.timing.cfg_implicit_timing(sample_mode=AcquisitionType.FINITE, samps_per_chan=n + 1)
        datos = t.read(number_of_samples_per_channel=n + 1, timeout=(n + 1) * periodo_nominal * 2 + 10)
    return np.asarray(datos[1:], dtype=float)    # la primera lectura puede empezar a medias


def leer_avance(puerto, segundos):
    """Muestras del avance durante `segundos` (tras 0.3 s de margen), VDD de la
    cabecera, avisos de pérdidas y la fs aproximada con el reloj del PC."""
    import serial
    import adc_1ksps as a1k
    codigos, vdd, perdidas, t_prim, t_ult = [], None, 0, None, None
    with serial.Serial(puerto, BAUDIOS, timeout=0.05) as p:
        p.reset_input_buffer()
        p.write(b"i?")
        inicio, resto = time.monotonic(), b""
        while time.monotonic() - inicio < segundos + 0.3:
            resto += p.read(4096)
            *lineas, resto = resto.split(b"\n")
            ahora = time.monotonic() - inicio
            for cruda in lineas:
                r = a1k.interpretar(cruda.decode("ascii", "replace"))
                if r is None:
                    continue
                if r[0] == "texto":
                    vdd = a1k.vdd_de_cabecera(r[1]) or vdd
                    perdidas += a1k.perdidas_de_aviso(r[1]) or 0
                elif ahora >= 0.3:
                    codigos.append(r[1])
                    t_prim = ahora if t_prim is None else t_prim
                    t_ult = ahora
    fs_pc = (len(codigos) - 1) / (t_ult - t_prim) if t_prim is not None and t_ult > t_prim else None
    return np.asarray(codigos, dtype=float), vdd, perdidas, fs_pc


def leer_completo(puerto, n_capturas):
    """Línea '# ADC:' de la orden '?' y las capturas de la orden 'm'."""
    import serial
    linea_adc = ""
    with serial.Serial(puerto, BAUDIOS, timeout=0.1) as p:
        p.reset_input_buffer()
        p.write(b"?")
        fin, resto = time.monotonic() + 1.5, b""
        while time.monotonic() < fin and not linea_adc:
            resto += p.read(4096)
            *lineas, resto = resto.split(b"\n")
            linea_adc = next((x.decode("ascii", "replace").strip() for x in lineas if x.startswith(b"# ADC:")), "")
    capturas, periodo = mj.leer_capturas(mj.capturar(puerto, BAUDIOS, n_capturas))
    if capturas and periodo != 1e6 / FS_NOMINAL:
        print(f"AVISO: el firmware captura una conversión cada {periodo:.0f} us, no cada 1000 us")
    return capturas, linea_adc


def prueba_real(a):
    k = DISPAROS_POR_MUESTRA[a.firmware]
    d = {"firmware": a.firmware, "origen": f"{a.dispositivo} y {a.puerto}", "vpp": a.vpp, "offset": a.offset}
    tarea = None
    if not a.sin_generador:
        onda, d["f_generador"] = onda_generador(a.frecuencia, a.offset, a.vpp)
        tarea = generador_iniciar(a.dispositivo, "ao0", onda)
        time.sleep(0.5)                      # asentamiento del filtro RC
    try:
        canales = ["ai0", "ai1"] if a.ai_vdd else ["ai0"]
        ai = leer_ai(a.dispositivo, canales, FS_AI, a.segundos)
        d["ai"], d["fs_ai"] = ai[0], FS_AI
        if a.ai_vdd:
            d["vdd_ai"] = float(ai[1].mean())
        if not a.sin_contador:
            d["periodos"] = medir_periodos(a.dispositivo, "ctr0", a.pfi, 200 * k,
                                           1.0 / (FS_NOMINAL * k))
        if a.firmware == "avance":
            codigos, d["vdd_pic"], d["perdidas"], d["fs_pc"] = leer_avance(a.puerto, a.segundos)
            d["pic"] = [codigos]
        else:
            d["pic"], d["linea_adc"] = leer_completo(a.puerto, a.capturas)
            d["vdd_supuesta"] = a.vdd
    finally:
        if tarea is not None:
            generador_parar(tarea, a.dispositivo, "ao0")
    if not d["pic"] or not len(d["pic"][0]):
        raise SystemExit("el PIC no envió muestras: revisar el puerto, el firmware y la conexión")
    return d


def main():
    p = argparse.ArgumentParser(description="Prueba del ADC del PIC (1000 S/s) con una NI USB-6211.")
    p.add_argument("--puerto", help="puerto serie del PIC (COM5, /dev/ttyUSB0...)")
    p.add_argument("--firmware", choices=sorted(DISPAROS_POR_MUESTRA), default="avance")
    p.add_argument("--dispositivo", default="Dev1", help="nombre en NI MAX (Dev1)")
    p.add_argument("--frecuencia", type=float, default=117.0, help="Hz (menos de 490)")
    p.add_argument("--offset", type=float, default=1.65, help="V")
    p.add_argument("--vpp", type=float, default=3.0, help="V pico a pico")
    p.add_argument("--segundos", type=float, default=5.0, help="duración de la AI y del avance")
    p.add_argument("--capturas", type=int, default=5, help="capturas 'm' (firmware completo)")
    p.add_argument("--ai-vdd", action="store_true", help="medir la VDD del PIC con AI 1")
    p.add_argument("--vdd", type=float, default=3.3, help="VDD supuesta si no se mide (completo)")
    p.add_argument("--pfi", default="PFI0", help="entrada del contador conectada a RA1")
    p.add_argument("--sin-generador", action="store_true", help="la señal viene de un generador externo")
    p.add_argument("--sin-contador", action="store_true", help="no medir RA1")
    p.add_argument("--demo", action="store_true", help="sin hardware: datos simulados")
    a = p.parse_args()

    if not a.sin_generador:
        try:
            comprobar_limites(a.offset, a.vpp)
        except ValueError as error:
            raise SystemExit(str(error))
        if a.frecuencia >= 0.49 * FS_NOMINAL:
            raise SystemExit("la frecuencia debe ser menor de 490 Hz (Nyquist del PIC: 500 Hz)")
    if a.demo:
        d = simular(a.firmware, a.frecuencia, a.offset, a.vpp, a.segundos, a.capturas)
    else:
        if not a.puerto:
            raise SystemExit("falta --puerto (o --demo)")
        d = prueba_real(a)
    med, pruebas = evaluar(d)
    print(informe(d, med, pruebas))
    return 0 if all(ok for _, ok, _ in pruebas) else 1


if __name__ == "__main__":
    sys.exit(main())
