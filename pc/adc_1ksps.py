#!/usr/bin/env python3
"""
adc_1ksps.py - Recibe, dibuja y graba las muestras del avance "ADC a 1000
muestras/s" (ADC_1kSPS_PIC16F18426.X, docs/avance_adc_1ksps.md).

El PIC muestrea RC0 cada 1 ms (disparo por hardware del PWM6) y envía por la
UART (115200 8N1) una línea por muestra con el código del ADC (0-4095). Las
líneas con '#' son la cabecera (con VDD medida con el FVR), respuestas y
avisos ("# perdidas: N"). Órdenes: d = detener el envío, i = reanudarlo,
? = estado.

Este programa muestra:
  - la señal en voltios (código · VDD / 4095), con cada muestra marcada
  - su espectro (último segundo, ventana Blackman-Harris)
  - frecuencia, amplitud, offset, SNR, THD, SINAD y ENOB (ajuste de una
    senoide, como medir_jitter.py), muestras/s recibidas y avisos
y graba un CSV con "n,t_s,codigo,V".

Uso:
    python adc_1ksps.py --listar                      puertos serie disponibles
    python adc_1ksps.py --puerto COM5                 Windows (/dev/ttyUSB0 en Linux)
    python adc_1ksps.py --puerto COM5 --csv senal.csv graba desde el principio
    python adc_1ksps.py --demo                        sin hardware: imita al PIC con un generador
    python adc_1ksps.py --archivo captura.txt         reproduce lo grabado de la UART
    python adc_1ksps.py --demo --png avance.png --segundos 4    sin ventana: guarda una imagen
    python adc_1ksps.py --puerto COM5 --sin-ventana --segundos 10 --csv senal.csv
                                                      sin gráficas: un resumen por segundo

Requisitos: pip install -r requirements.txt   (pyserial, matplotlib y numpy)
"""
import argparse
import collections
import datetime
import math
import queue
import re
import sys
import threading
import time

import numpy as np

import medir_jitter as mj

FS = 1000.0                     # muestras por segundo del firmware
CUENTAS = 4095
VDD_POR_DEFECTO = 3.3           # si no llega la cabecera del PIC
BAUDIOS = 115200


# --------------------------------------------------------------------------
# Líneas del PIC
# --------------------------------------------------------------------------

def interpretar(linea):
    """("muestra", código) o ("texto", línea) o None si la línea no vale."""
    linea = linea.strip()
    if not linea:
        return None
    if linea.startswith("#"):
        return ("texto", linea)
    if linea.isdigit() and int(linea) <= CUENTAS:
        return ("muestra", int(linea))
    return None


def vdd_de_cabecera(texto):
    """VDD en voltios de "# entrada ..., referencia VDD = 3301 mV ...", o None."""
    m = re.search(r"VDD = (\d+) mV", texto)
    return int(m.group(1)) / 1000 if m else None


def perdidas_de_aviso(texto):
    m = re.match(r"# perdidas: (\d+)", texto)
    return int(m.group(1)) if m else None


def analizar(codigos, vdd):
    """Resumen de los últimos códigos: diccionario con las medidas en voltios
    y en dB (ver medir_jitter.resumir), o None si hay pocos."""
    if len(codigos) < 64:
        return None
    x = np.asarray(codigos, dtype=float)
    r = mj.resumir([x], FS)
    r["saturada"] = bool(x.min() <= 0 or x.max() >= CUENTAS)
    r["vpp"] = float(np.ptp(x)) * vdd / CUENTAS
    r["media_v"] = float(x.mean()) * vdd / CUENTAS
    if not r["dc"]:
        r["amplitud_v"] = math.sqrt(2 * r["senal"]) * vdd / CUENTAS
        r["offset_v"] = r["offset"] * vdd / CUENTAS
    return r


def resumen_texto(r):
    if r is None:
        return "esperando muestras..."
    if r["dc"]:
        return (f"continua {r['media_v']:.4f} V, ruido {math.sqrt(r['ruido']):.2f} LSB eficaces"
                + ("  ¡SATURADA!" if r["saturada"] else ""))
    return (f"{r['f_aparente']:.2f} Hz, {2 * r['amplitud_v']:.3f} Vpp, offset {r['offset_v']:.3f} V, "
            f"SNR {r['snr']:.1f} dB, SINAD {r['sinad']:.1f} dB, ENOB {r['enob']:.2f}"
            + ("  ¡SATURADA!" if r["saturada"] else ""))


# --------------------------------------------------------------------------
# Fuentes: puerto serie, archivo o demostración
# --------------------------------------------------------------------------

class FuenteSerie:
    def __init__(self, puerto, baudios):
        import serial                       # pyserial
        self.puerto = serial.Serial(puerto, baudios, timeout=0.05)
        self.descripcion = f"{puerto} a {baudios} baudios"
        self.cola = queue.Queue()
        threading.Thread(target=self._leer, daemon=True).start()

    def _leer(self):
        resto = b""
        while True:
            try:
                datos = self.puerto.read(4096)
            except Exception as error:      # puerto desconectado
                self.cola.put(f"# ERROR del puerto serie: {error}")
                return
            if datos:
                resto += datos
                *lineas, resto = resto.split(b"\n")
                for linea in lineas:
                    self.cola.put(linea.decode("ascii", "replace"))

    def enviar(self, orden):
        self.puerto.write(orden.encode("ascii"))

    def cerrar(self):
        self.puerto.close()


class FuenteArchivo:
    """Reproduce lo grabado de la UART a 1000 muestras/s."""

    def __init__(self, ruta):
        with open(ruta, encoding="ascii", errors="replace") as f:
            self.lineas = f.read().splitlines()
        self.descripcion = f"archivo {ruta}"
        self.cola = queue.Queue()
        threading.Thread(target=self._reproducir, daemon=True).start()

    def _reproducir(self):
        inicio, n = time.monotonic(), 0
        for linea in self.lineas:
            if linea[:1].isdigit():
                n += 1
                if n % 10 == 0:
                    espera = inicio + n / FS - time.monotonic()
                    if espera > 0:
                        time.sleep(espera)
            self.cola.put(linea)
        self.cola.put("# fin del archivo")

    def enviar(self, orden):
        print(f"(reproducción de un archivo: la orden '{orden}' no se envía)")

    def cerrar(self):
        pass


class FuenteDemo:
    """Imita al PIC y a un generador de señales, sin hardware: senoide de
    f_hz con 1.5 V de amplitud sobre 1.65 V, VDD = 3.3 V y 0.4 LSB de ruido."""

    def __init__(self, f_hz=117.0, amplitud_v=1.5, offset_v=1.65, vdd=3.3):
        self.descripcion = f"demostración: generador de {f_hz:g} Hz (sin hardware)"
        self.f, self.a, self.o, self.vdd = f_hz, amplitud_v, offset_v, vdd
        self.cola = queue.Queue()
        self.ordenes = queue.Queue()
        self.enviando = True
        self.convertidas = 0
        threading.Thread(target=self._generar, daemon=True).start()

    def enviar(self, orden):
        self.ordenes.put(orden)

    def cerrar(self):
        pass

    def _cabecera(self):
        self.cola.put("# ADC 1 kS/s - demostracion (sin hardware)")
        self.cola.put(f"# entrada RC0 (ANC0), 12 bits, referencia VDD = {round(self.vdd * 1000)} mV "
                      "(medida con el FVR, +-4 %)")
        self.cola.put("# muestreo: 1 conversion cada 1000 us, disparada por el PWM6")
        self.cola.put("# ordenes: d=detener envio  i=reanudar  ?=estado")

    def _orden(self, o):
        if o == "d":
            self.enviando = False
            self.cola.put("# d: envio detenido")
        elif o == "i":
            self.cola.put("# i: envio en marcha")
            self.enviando = True
        elif o in "?h":
            self._cabecera()
            self.cola.put(f"# estado: envio {'en marcha' if self.enviando else 'detenido'}, "
                          f"convertidas {self.convertidas}, perdidas 0")
        else:
            self.cola.put("# orden desconocida (? = ayuda)")

    def _generar(self):
        g = np.random.default_rng()
        self._cabecera()
        inicio = time.monotonic()
        while True:
            espera = inicio + (self.convertidas + 10) / FS - time.monotonic()
            if espera > 0:
                time.sleep(espera)
            while not self.ordenes.empty():
                self._orden(self.ordenes.get())
            t = (self.convertidas + np.arange(10)) / FS
            v = self.o + self.a * np.sin(2 * np.pi * self.f * t)
            codigos = np.clip(np.round(v / self.vdd * CUENTAS + g.normal(0, 0.4, 10)), 0, CUENTAS)
            self.convertidas += 10
            if self.enviando:
                for c in codigos:
                    self.cola.put(str(int(c)))


# --------------------------------------------------------------------------
# Recepción, estadísticas y CSV
# --------------------------------------------------------------------------

class Receptor:
    """Lo que llega del PIC: últimas muestras, contadores y grabación."""

    def __init__(self, memoria=2000, eco=True):
        self.eco = eco                          # imprimir los mensajes '#' del PIC
        self.codigos = collections.deque(maxlen=memoria)
        self.total = 0
        self.invalidas = 0
        self.perdidas_pic = 0
        self.vdd = None
        self.enviando = True
        self.llegadas = collections.deque()     # (instante, total) para muestras/s
        self.grabador = None
        self.mensajes = []

    def procesar(self, fuente, maximo=20000):
        for _ in range(maximo):
            try:
                linea = fuente.cola.get_nowait()
            except queue.Empty:
                break
            self.linea(linea)
        ahora = time.monotonic()
        self.llegadas.append((ahora, self.total))
        while self.llegadas and ahora - self.llegadas[0][0] > 5.0:
            self.llegadas.popleft()

    def linea(self, linea):
        dato = interpretar(linea)
        if dato is None:
            if linea.strip():
                self.invalidas += 1
            return
        tipo, valor = dato
        if tipo == "muestra":
            self.codigos.append(valor)
            if self.grabador:
                self.grabador.muestra(self.total, valor, self.vdd_v())
            self.total += 1
            return
        self.mensajes.append(valor)
        if self.eco:
            print(valor)
        if self.grabador:
            self.grabador.comentario(valor)
        vdd = vdd_de_cabecera(valor)
        if vdd:
            self.vdd = vdd
        perdidas = perdidas_de_aviso(valor)
        if perdidas is not None:
            self.perdidas_pic = perdidas
        if valor.startswith("# d:"):
            self.enviando = False
        if valor.startswith("# i:"):
            self.enviando = True

    def vdd_v(self):
        return self.vdd or VDD_POR_DEFECTO

    def muestras_por_segundo(self):
        if len(self.llegadas) < 2 or self.llegadas[-1][0] - self.llegadas[0][0] < 0.5:
            return 0.0
        (t0, n0), (t1, n1) = self.llegadas[0], self.llegadas[-1]
        return (n1 - n0) / (t1 - t0)

    def ultimas(self, n):
        return list(self.codigos)[-n:]


class Grabador:
    def __init__(self, ruta, comentario=""):
        self.ruta = ruta
        self.archivo = open(ruta, "w", encoding="utf-8", newline="")
        self.archivo.write(f"# ADC 1 kS/s PIC16F18426 - {datetime.datetime.now().isoformat(timespec='seconds')}\n")
        if comentario:
            self.archivo.write(f"# comentario: {comentario}\n")
        self.archivo.write("n,t_s,codigo,V\n")

    def muestra(self, n, codigo, vdd):
        self.archivo.write(f"{n},{n / FS:.3f},{codigo},{codigo * vdd / CUENTAS:.5f}\n")

    def comentario(self, texto):
        self.archivo.write(texto.rstrip() + "\n")

    def cerrar(self):
        self.archivo.close()


# --------------------------------------------------------------------------
# Ventana
# --------------------------------------------------------------------------

class Panel:
    def __init__(self, plt, fuente, receptor, ventana_s, comentario):
        from matplotlib.widgets import Button

        self.plt, self.fuente, self.rx = plt, fuente, receptor
        self.ventana = max(10, int(ventana_s * FS))
        self.comentario = comentario
        tinta, self.tenue, serie = "#0b0b0b", "#52514e", "#2a78d6"

        self.fig = plt.figure("ADC 1 kS/s - PIC16F18426", figsize=(12, 7.5), facecolor="#fcfcfb")
        self.titulo = self.fig.text(0.04, 0.965, "", fontsize=10, color=self.tenue)
        self.ax_t = self.fig.add_axes([0.07, 0.56, 0.62, 0.36])
        self.ax_f = self.fig.add_axes([0.07, 0.10, 0.62, 0.35])
        for ax in (self.ax_t, self.ax_f):
            ax.set_facecolor("#fcfcfb")
            ax.grid(True, color="#e6e5e0", lw=0.8)
            ax.tick_params(colors=self.tenue, labelsize=8)
            for lado in ("top", "right"):
                ax.spines[lado].set_visible(False)
            for lado in ("left", "bottom"):
                ax.spines[lado].set_color("#c3c2b7")
        self.ax_t.set_title(f"Señal en RC0 (V): últimas {self.ventana} muestras", fontsize=10,
                            color=tinta, loc="left")
        self.ax_t.set_xlabel("tiempo (ms)", color=self.tenue, fontsize=9)
        self.ax_f.set_title("Espectro del último segundo (dBFS)", fontsize=10, color=tinta, loc="left")
        self.ax_f.set_xlabel("frecuencia (Hz)", color=self.tenue, fontsize=9)
        self.ax_f.set_xlim(0, FS / 2)
        self.linea_t, = self.ax_t.plot([], [], color=serie, lw=1.5, marker="o", ms=3)
        self.linea_f, = self.ax_f.plot([], [], color=serie, lw=1.5)
        self.texto = self.fig.text(0.73, 0.92, "", family="monospace", fontsize=9.5, va="top", color=tinta)
        self.avisos = self.fig.text(0.73, 0.30, "", fontsize=9.5, va="top", color="#c62828", wrap=True)

        self.botones = []
        for k, (nombre, accion) in enumerate((("Detener", self._alternar_envio), ("Estado (?)", self._estado),
                                              ("Grabar CSV", self._alternar_grabacion))):
            ax_b = self.fig.add_axes([0.73 + k * 0.085, 0.10, 0.08, 0.05])
            boton = Button(ax_b, nombre)
            boton.label.set_fontsize(9)
            boton.on_clicked(lambda _evento, a=accion: a())
            self.botones.append(boton)
        self.fig.canvas.mpl_connect("key_press_event", self._tecla)

    def _tecla(self, evento):
        if evento.key in ("d", "i", "?"):
            self.fuente.enviar(evento.key)
        elif evento.key == "g":
            self._alternar_grabacion()

    def _alternar_envio(self):
        self.fuente.enviar("d" if self.rx.enviando else "i")

    def _estado(self):
        self.fuente.enviar("?")

    def _alternar_grabacion(self):
        if self.rx.grabador:
            self.rx.grabador.cerrar()
            print(f"Grabación terminada: {self.rx.grabador.ruta}")
            self.rx.grabador = None
        else:
            ruta = datetime.datetime.now().strftime("adc1k_%Y%m%d_%H%M%S.csv")
            self.rx.grabador = Grabador(ruta, self.comentario)
            print(f"Grabando en {ruta}")

    def actualizar(self, _cuadro=None):
        self.rx.procesar(self.fuente)
        vdd = self.rx.vdd_v()
        ultimas = self.rx.ultimas(self.ventana)
        if ultimas:
            t = np.arange(len(ultimas)) / FS * 1000
            v = np.asarray(ultimas) * vdd / CUENTAS
            self.linea_t.set_data(t, v)
            self.ax_t.set_xlim(0, max(t[-1], 1))
            bajo, alto = float(v.min()), float(v.max())
            margen = max(0.05 * (alto - bajo), 0.01)
            self.ax_t.set_ylim(bajo - margen, alto + margen)

        segundo = self.rx.ultimas(int(FS))
        r = analizar(segundo, vdd)
        if len(segundo) >= 64:
            x = np.asarray(segundo, dtype=float)
            w = mj.ventana_blackman_harris(len(x))
            p = np.abs(np.fft.rfft((x - np.dot(x, w) / w.sum()) * w)) ** 2
            dbfs = 10 * np.log10(p / (CUENTAS / 2 * w.sum() / 2) ** 2 + 1e-20)
            self.linea_f.set_data(np.arange(len(p)) * FS / len(x), dbfs)
            self.ax_f.set_ylim(max(-160.0, float(np.percentile(dbfs, 5)) - 20), 5)

        self._indicadores(r)
        return []

    def _indicadores(self, r):
        rx = self.rx
        estado = f"Fuente: {self.fuente.descripcion}"
        if rx.grabador:
            estado += f"   ·   grabando en {rx.grabador.ruta}"
        self.titulo.set_text(estado)
        self.botones[0].label.set_text("Detener" if rx.enviando else "Reanudar")
        self.botones[2].label.set_text("Grabando..." if rx.grabador else "Grabar CSV")
        filas = [f"muestras/s   {rx.muestras_por_segundo():8.1f}",
                 f"recibidas    {rx.total:8d}",
                 f"VDD          {rx.vdd_v():8.3f} V" + ("" if rx.vdd else " (supuesta)"),
                 ""]
        if r is None:
            filas.append("esperando muestras...")
        elif r["dc"]:
            filas += [f"continua     {r['media_v']:8.4f} V",
                      f"ruido        {math.sqrt(r['ruido']):8.2f} LSB"]
        else:
            filas += [f"frecuencia   {r['f_aparente']:8.2f} Hz",
                      f"amplitud     {2 * r['amplitud_v']:8.3f} Vpp",
                      f"offset       {r['offset_v']:8.3f} V",
                      f"SNR          {r['snr']:8.1f} dB",
                      f"THD        {'<' if r['thd_bajo_ruido'] else ' '} {r['thd']:8.1f} dB",
                      f"SINAD        {r['sinad']:8.1f} dB",
                      f"ENOB         {r['enob']:8.2f} bits"]
        self.texto.set_text("\n".join(filas))
        avisos = []
        if not rx.enviando:
            avisos.append("envío detenido (d)")
        if r and r["saturada"]:
            avisos.append("señal saturada: ajuste amplitud u offset del generador (0 a VDD)")
        if rx.perdidas_pic:
            avisos.append(f"el PIC perdió {rx.perdidas_pic} muestras")
        if rx.invalidas:
            avisos.append(f"{rx.invalidas} líneas no válidas")
        self.avisos.set_text("\n".join(avisos))

    def cerrar(self):
        if self.rx.grabador:
            self.rx.grabador.cerrar()
            print(f"Grabación guardada en {self.rx.grabador.ruta}")
            self.rx.grabador = None
        self.fuente.cerrar()


# --------------------------------------------------------------------------

def main():
    p = argparse.ArgumentParser(description="Señal del ADC del PIC16F18426 a 1000 muestras/s.")
    fuente = p.add_mutually_exclusive_group()
    fuente.add_argument("--puerto", help="puerto serie del adaptador USB-serie (COM5, /dev/ttyUSB0...)")
    fuente.add_argument("--demo", action="store_true", help="sin hardware: imita al PIC con un generador")
    fuente.add_argument("--archivo", help="reproduce lo grabado de la UART")
    fuente.add_argument("--listar", action="store_true", help="muestra los puertos serie y sale")
    p.add_argument("--baudios", type=int, default=BAUDIOS)
    p.add_argument("--demo-f", type=float, default=117.0, help="frecuencia del generador de --demo (Hz)")
    p.add_argument("--ventana", type=float, default=0.1, help="segundos de señal en la gráfica (0.1)")
    p.add_argument("--csv", help="graba desde el principio en este archivo")
    p.add_argument("--comentario", default="", help="comentario para el CSV")
    p.add_argument("--png", help="sin ventana: guarda una imagen al cabo de --segundos y sale")
    p.add_argument("--sin-ventana", action="store_true", help="sin gráficas: un resumen por segundo")
    p.add_argument("--segundos", type=float, default=5.0)
    args = p.parse_args()

    if args.listar:
        from serial.tools import list_ports
        for puerto in list_ports.comports():
            print(f"{puerto.device:15s} {puerto.description}")
        return 0
    if not (args.puerto or args.demo or args.archivo):
        p.error("indique --puerto, --demo o --archivo (o --listar para ver los puertos)")

    if args.puerto:
        datos = FuenteSerie(args.puerto, args.baudios)
        datos.enviar("?")               # el PIC responde con la cabecera (VDD)
    elif args.archivo:
        datos = FuenteArchivo(args.archivo)
    else:
        datos = FuenteDemo(args.demo_f)
    rx = Receptor()
    if args.csv:
        rx.grabador = Grabador(args.csv, args.comentario)

    if args.sin_ventana:
        fin = time.monotonic() + args.segundos
        siguiente = time.monotonic() + 1.0
        while time.monotonic() < fin:
            time.sleep(0.05)
            rx.procesar(datos)
            if time.monotonic() >= siguiente:
                siguiente += 1.0
                print(f"{rx.muestras_por_segundo():7.1f} muestras/s  "
                      + resumen_texto(analizar(rx.ultimas(int(FS)), rx.vdd_v())))
        if rx.grabador:
            rx.grabador.cerrar()
            print(f"CSV guardado en {rx.grabador.ruta} ({rx.total} muestras)")
        datos.cerrar()
        return 0

    import matplotlib
    if args.png:
        matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from matplotlib.animation import FuncAnimation
    for clave, valor in plt.rcParams.items():  # las teclas d, i, g son órdenes
        if clave.startswith("keymap.") and isinstance(valor, list):
            plt.rcParams[clave] = [t for t in valor if t not in ("d", "i", "g", "?")]

    panel = Panel(plt, datos, rx, args.ventana, args.comentario)
    if args.png:
        fin = time.monotonic() + args.segundos
        while time.monotonic() < fin:
            time.sleep(0.05)
            rx.procesar(datos)
        panel.actualizar()
        panel.fig.savefig(args.png, dpi=90)
        print(resumen_texto(analizar(rx.ultimas(int(FS)), rx.vdd_v())))
        panel.cerrar()
        print(f"Imagen guardada en {args.png}")
        return 0

    animacion = FuncAnimation(panel.fig, panel.actualizar, interval=100, cache_frame_data=False)
    plt.show()
    del animacion
    panel.cerrar()
    return 0


if __name__ == "__main__":
    sys.exit(main())
