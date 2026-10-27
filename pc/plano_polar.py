#!/usr/bin/env python3
"""
plano_polar.py - Plano polar y gráficas en vivo del detector de fisuras por
corrientes de Eddy (tarjeta P2 + PIC16F18426).

Es la "opción A" de la especificación (sección 10.7): el PIC hace todo el
procesamiento del VI P2_IQ_SINAD.vi y envía por UART (115200 baudios):

    t_ms,Ip_uV,Qp_uV,I1_uV,Q1_uV                                  100 por segundo
    B,V_uV,fase_cdeg,d_milesimas,I0_uV,Q0_uV,theta_cdeg,vcc_mV,G,banderas
                                                                   10 por segundo
Este programa lo dibuja como el panel de LabVIEW:
  - plano polar: traza de los últimos N puntos (I', Q') unidos en orden de
    llegada, con autoescala (el anillo exterior vale el r máximo de la traza)
  - "I y Q (mV)" (finales I', Q') e "I y Q procesadas (mV)" frente al tiempo
  - indicadores |V|, Fase, d estimada, I0, Q0, θ, VCC_A y avisos
y envía las órdenes con botones o con el teclado:
    z Fijar cero   x Borrar 0   c Calibrar θ   t θ = 0   b Borrar traza
    2 / 1 / 0 ganancia ×2 / ×11 / ×101   s corrección de signo de I
    g Grabar (CSV)

Uso:
    python plano_polar.py --listar                      puertos serie disponibles
    python plano_polar.py --puerto COM5                 Windows
    python plano_polar.py --puerto /dev/ttyUSB0 --puntos 12000
    python plano_polar.py --puerto COM5 --csv medida.csv --comentario "dc x2, sano-defecto-sano"
    python plano_polar.py --demo                        sin hardware (imita al PIC)
    python plano_polar.py --archivo captura.txt         reproduce una captura
    python plano_polar.py --demo --png prueba.png --segundos 12   sin ventana

Requisitos: pip install -r requirements.txt   (pyserial y matplotlib)
"""
import argparse
import collections
import datetime
import math
import queue
import random
import sys
import threading
import time

BAUDIOS = 115200
PERIODO_S = 0.010           # una muestra cada 10 ms
VENTANA_S = 10.0            # segundos visibles en las gráficas temporales
MV_POR_1E4 = 4.03           # "mV por 10^-4" (sección 1.4)

AVISOS = (
    (0x01, "VCC_A fuera de rango: se usa 3.7 V"),
    (0x02, "I_OUT saturada"),
    (0x04, "Q_OUT saturada"),
    (0x08, "VCC_A ≥ VDD del PIC"),
    (0x10, "sin lectura del FVR"),
    (0x20, "se descartaron muestras"),
)

TECLAS = {
    "z": "Fijar cero", "x": "Borrar 0", "c": "Calibrar θ", "t": "θ = 0",
    "b": "Borrar traza", "2": "Ganancia ×2", "1": "Ganancia ×11",
    "0": "Ganancia ×101", "s": "Corrección signo I",
}
GANANCIAS = {2: "2", 11: "1", 101: "0"}         # ganancia -> orden


# --------------------------------------------------------------------------
# Interpretación de las líneas del PIC
# --------------------------------------------------------------------------

def interpretar(linea):
    """Devuelve (tipo, datos) de una línea del PIC, o None si no se entiende.

    tipo: "muestra" (t_ms, Ip, Qp, I1, Q1) en ms y mV
          "bloque"  dict con V, fase, d, I0, Q0, theta, vcc, G, banderas
          "punto"   (I1, Q1) del volcado de la traza (orden 'p')
          "texto"   comentario o respuesta (líneas con '#')
    """
    linea = linea.strip()
    if not linea:
        return None
    if linea.startswith("#"):
        return ("texto", linea)
    try:
        if linea[0].isdigit():
            t, ip, qp, i1, q1 = (int(x) for x in linea.split(","))
            return ("muestra", (t, ip / 1000, qp / 1000, i1 / 1000, q1 / 1000))
        if linea.startswith("B,"):
            # 6 campos de la especificación (10.7) + vcc_mV, G y banderas
            c = [int(x) for x in linea[2:].split(",")]
            if len(c) < 6:
                return None
            vcc, g, banderas = (c[6:] + [0, 0, 0])[:3]
            return ("bloque", {"V": c[0] / 1000, "fase": c[1] / 100, "d": c[2] / 1000,
                               "I0": c[3] / 1000, "Q0": c[4] / 1000, "theta": c[5] / 100,
                               "vcc": vcc / 1000, "G": g, "banderas": banderas})
        if linea.startswith("P,"):
            _, i1, q1 = linea.split(",")
            return ("punto", (int(i1) / 1000, int(q1) / 1000))
    except ValueError:
        return None
    return None


def interpretar_configuracion(texto):
    """'# G=2 signo_I=ON I0_uV=...' -> {'G': 2, 'signo': True}, o None."""
    if not texto.startswith("# G="):
        return None
    campos = dict(p.split("=", 1) for p in texto[1:].split() if "=" in p)
    try:
        return {"G": int(campos["G"]), "signo": campos.get("signo_I") == "ON"}
    except (KeyError, ValueError):
        return None


# --------------------------------------------------------------------------
# Fuentes de datos: puerto serie, archivo o demostración
# --------------------------------------------------------------------------

class FuenteSerie:
    def __init__(self, puerto, baudios):
        import serial                       # pyserial
        self.puerto = serial.Serial(puerto, baudios, timeout=0.1)
        self.descripcion = f"{puerto} a {baudios} baudios"
        self.cola = queue.Queue()
        threading.Thread(target=self._leer, daemon=True).start()

    def _leer(self):
        resto = b""
        while True:
            try:
                datos = self.puerto.read(512)
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
    """Reproduce una captura de texto del PIC a velocidad real."""

    def __init__(self, ruta, velocidad=1.0):
        with open(ruta, encoding="ascii", errors="replace") as f:
            self.lineas = f.read().splitlines()
        self.descripcion = f"archivo {ruta}"
        self.velocidad = velocidad
        self.cola = queue.Queue()
        threading.Thread(target=self._reproducir, daemon=True).start()

    def _reproducir(self):
        siguiente = time.monotonic()
        for linea in self.lineas:
            if linea[:1].isdigit():
                siguiente += PERIODO_S / self.velocidad
                espera = siguiente - time.monotonic()
                if espera > 0:
                    time.sleep(espera)
            self.cola.put(linea)
        self.cola.put("# fin del archivo")

    def enviar(self, orden):
        print(f"(reproducción de un archivo: la orden '{orden}' no se envía)")

    def cerrar(self):
        pass


class FuenteDemo:
    """Imita al PIC sin hardware: genera las mismas líneas y atiende las órdenes.

    La señal es una pasada de la sonda que se repite cada 8 s: metal sano con
    el desequilibrio del puente de la medida real (sección 9.2), una fisura
    (lóbulos de las bobinas A y B sobre la recta de 60°) y un lift-off a 20°.
    El procesamiento del cero, θ y los indicadores es el del firmware.
    """
    G_TARJETA = 2                           # jumpers JP701/JP702 de la "tarjeta"

    def __init__(self):
        self.descripcion = "demostración (sin hardware)"
        self.cola = queue.Queue()
        self.ordenes = queue.Queue()
        self.G, self.signo = 2, True
        self.I0 = self.Q0 = self.theta = 0.0
        self.eventos = set()
        threading.Thread(target=self._generar, daemon=True).start()

    def enviar(self, orden):
        self.ordenes.put(orden)

    def cerrar(self):
        pass

    def _configuracion(self):
        return (f"# G={self.G} signo_I={'ON' if self.signo else 'OFF'} "
                f"I0_uV={round(self.I0 * 1000)} Q0_uV={round(self.Q0 * 1000)} "
                f"theta_cdeg={round(self.theta * 100)}")

    def _senal(self, t):
        ip, qp = 62.3276, 21.1352           # cero de la medida real
        ciclo = t % 8.0
        if 2.0 <= ciclo < 3.0:              # fisura
            a = 65.0 * math.sin(2 * math.pi * (ciclo - 2.0))
            ip += a * math.cos(math.radians(60))
            qp += a * math.sin(math.radians(60))
        if 5.0 <= ciclo < 6.0:              # lift-off (papel)
            a = 30.0 * (0.5 - 0.5 * math.cos(2 * math.pi * (ciclo - 5.0)))
            ip += a * math.cos(math.radians(20))
            qp += a * math.sin(math.radians(20))
        ip += random.gauss(0, 0.05)
        qp += random.gauss(0, 0.05)
        if not self.signo:                  # sin corrección, I sale invertido
            ip = -ip
        escala = self.G_TARJETA / self.G    # ganancia distinta de los jumpers
        return ip * escala, qp * escala

    def _atender_orden(self, o):
        nombres = {"z": "fijar cero", "x": "borrar cero", "c": "calibrar theta",
                   "t": "theta = 0", "b": "borrar traza"}
        if o in nombres:
            self.eventos.add(o)
            self.cola.put(f"# {o}: {nombres[o]}")
        elif o in ("2", "1", "0"):
            self.G = {"2": 2, "1": 11, "0": 101}[o]
            self.cola.put(self._configuracion())
        elif o == "s":
            self.signo = not self.signo
            self.cola.put(self._configuracion())
        elif o in "?h":
            self.cola.put("# P2 IQ - demostracion")
            self.cola.put(self._configuracion())

    def _generar(self):
        self.cola.put("# P2 IQ - demostracion (sin hardware)")
        self.cola.put(self._configuracion())
        n, sum_i, sum_q = 0, 0.0, 0.0
        inicio = time.monotonic()
        while True:
            n += 1
            espera = inicio + n * PERIODO_S - time.monotonic()
            if espera > 0:
                time.sleep(espera)
            while not self.ordenes.empty():
                self._atender_orden(self.ordenes.get())
            ip, qp = self._senal(n * PERIODO_S)
            c, s = math.cos(math.radians(self.theta)), math.sin(math.radians(self.theta))
            ic, qc = ip - self.I0, qp - self.Q0
            i1, q1 = ic * c + qc * s, qc * c - ic * s
            self.cola.put(f"{n * 10},{round(ip * 1000)},{round(qp * 1000)},"
                          f"{round(i1 * 1000)},{round(q1 * 1000)}")
            sum_i += ip
            sum_q += qp
            if n % 10:
                continue
            # cierre del bloque: P4, P5, P7 y P10 como en el firmware
            mi, mq = sum_i / 10, sum_q / 10
            sum_i = sum_q = 0.0
            if "z" in self.eventos:
                self.I0, self.Q0 = mi, mq
            if "x" in self.eventos:
                self.I0 = self.Q0 = 0.0
            mic, mqc = mi - self.I0, mq - self.Q0
            if "c" in self.eventos:
                self.theta = math.degrees(math.atan2(mqc, mic))
            if "t" in self.eventos:
                self.theta = 0.0
            self.eventos.clear()
            c, s = math.cos(math.radians(self.theta)), math.sin(math.radians(self.theta))
            mi1, mq1 = mic * c + mqc * s, mqc * c - mic * s
            v = math.hypot(mi1, mq1)
            self.cola.put(f"B,{round(v * 1000)},{round(math.degrees(math.atan2(mq1, mi1)) * 100)},"
                          f"{round(v / MV_POR_1E4 * 1000)},{round(self.I0 * 1000)},"
                          f"{round(self.Q0 * 1000)},{round(self.theta * 100)},3653,{self.G},0")


# --------------------------------------------------------------------------
# Grabación en CSV ("Grabar" / Write To Measurement File)
# --------------------------------------------------------------------------

class Grabador:
    """Una fila por muestra; la fila que cierra cada bloque lleva además
    |V|, Fase y d estimada."""

    def __init__(self, ruta, comentario, configuracion):
        self.ruta = ruta
        self.archivo = open(ruta, "w", encoding="utf-8", newline="")
        ahora = datetime.datetime.now().isoformat(timespec="seconds")
        self.archivo.write(f"# P2 IQ PIC16F18426 - {ahora}\n")
        if comentario:
            self.archivo.write(f"# comentario: {comentario}\n")
        if configuracion:
            self.archivo.write(f"# configuracion: {configuracion.lstrip('# ')}\n")
        self.archivo.write("t_ms,Ip_mV,Qp_mV,I1_mV,Q1_mV,V_mV,fase_grados,d_x1e-4\n")
        self.pendiente = None

    def _escribir(self, m, b=None):
        fila = f"{m[0]},{m[1]:.3f},{m[2]:.3f},{m[3]:.3f},{m[4]:.3f}"
        fila += f",{b['V']:.3f},{b['fase']:.2f},{b['d']:.3f}\n" if b else ",,,\n"
        self.archivo.write(fila)

    def muestra(self, m):
        if self.pendiente:
            self._escribir(self.pendiente)
        self.pendiente = m

    def bloque(self, b):
        if self.pendiente:
            self._escribir(self.pendiente, b)
            self.pendiente = None

    def configuracion(self, texto):
        """Deja constancia en el CSV de un cambio de G, signo, cero o θ."""
        if self.pendiente:
            self._escribir(self.pendiente)
            self.pendiente = None
        self.archivo.write(f"# configuracion: {texto.lstrip('# ')}\n")

    def cerrar(self):
        if self.pendiente:
            self._escribir(self.pendiente)
        self.archivo.close()


# --------------------------------------------------------------------------
# Ventana
# --------------------------------------------------------------------------

class Panel:
    def __init__(self, plt, fuente, puntos, csv, comentario):
        from matplotlib.widgets import Button, CheckButtons, RadioButtons

        self.plt = plt
        self.fuente = fuente
        self.comentario = comentario
        self.csv_pedido = csv
        self.traza = collections.deque(maxlen=puntos)
        self.historia = collections.deque(maxlen=int(VENTANA_S / PERIODO_S))
        self.bloque = None
        self.configuracion = ""
        self.grabador = None
        self.llegadas = collections.deque()  # instantes de llegada (muestras/s)
        self.r_escala = 0.0
        self.sincronizando = False
        self.punto_volcado = 0

        self.fig = plt.figure("P2 IQ - PIC16F18426", figsize=(14, 8.2))
        self.fig.patch.set_facecolor("white")
        self.titulo = self.fig.text(0.03, 0.965, "", fontsize=10, color="0.25")

        # Plano polar: (I', Q') en ejes cartesianos con la misma escala
        self.ax = self.fig.add_axes([0.03, 0.20, 0.44, 0.73])
        self.ax.set_aspect("equal")
        self.ax.set_xticks([])
        self.ax.set_yticks([])
        for lado in self.ax.spines.values():
            lado.set_visible(False)
        self.ax.set_title("Plano polar  (I', Q')  -  0° = lift-off tras calibrar θ", fontsize=11)
        malla = dict(color="0.82", lw=0.8, zorder=1)
        self.anillos = [self.ax.plot([], [], **malla)[0] for _ in range(4)]
        self.radios = [self.ax.plot([], [], **malla)[0] for _ in range(12)]
        self.etiquetas_anillo = [self.ax.text(0, 0, "", fontsize=8, color="0.45") for _ in range(4)]
        self.etiquetas_angulo = [self.ax.text(0, 0, f"{a}°", fontsize=8, color="0.45",
                                              ha="center", va="center") for a in range(0, 360, 30)]
        self.linea_traza, = self.ax.plot([], [], color="#1f5fa8", lw=1.0, zorder=3)
        self.punto_actual, = self.ax.plot([], [], "o", color="#d1342b", ms=6, zorder=4)

        # Gráficas temporales
        self.ax_final = self.fig.add_axes([0.54, 0.64, 0.43, 0.29])
        self.ax_proc = self.fig.add_axes([0.54, 0.31, 0.43, 0.25])
        self.lineas_t = {}
        for ax, titulo, claves in ((self.ax_final, "I y Q (mV)  -  finales I', Q'", ("I1", "Q1")),
                                   (self.ax_proc, "I y Q procesadas (mV)  -  Ip, Qp", ("Ip", "Qp"))):
            ax.set_title(titulo, fontsize=10)
            ax.set_xlim(-VENTANA_S, 0)
            ax.grid(True, color="0.9")
            ax.tick_params(labelsize=8)
            self.lineas_t[claves[0]], = ax.plot([], [], color="#1f5fa8", lw=1, label=claves[0])
            self.lineas_t[claves[1]], = ax.plot([], [], color="#d1342b", lw=1, label=claves[1])
            ax.legend(loc="upper left", fontsize=8)
        self.ax_proc.set_xlabel("tiempo (s)", fontsize=8)

        # Indicadores
        self.texto_indicadores = self.fig.text(0.54, 0.235, "", family="monospace", fontsize=10, va="top")
        self.texto_avisos = self.fig.text(0.54, 0.035, "", fontsize=10, color="#c62828")

        # Botones (equivalen a los botones latch del VI)
        self.botones = []
        for k, (orden, nombre) in enumerate((("z", "Fijar cero"), ("x", "Borrar 0"), ("c", "Calibrar θ"),
                                             ("t", "θ = 0"), ("b", "Borrar traza"), ("g", "Grabar"))):
            ax_b = self.fig.add_axes([0.03 + (k % 3) * 0.085, 0.085 - (k // 3) * 0.06, 0.08, 0.05])
            boton = Button(ax_b, nombre)
            boton.label.set_fontsize(9)
            boton.on_clicked(lambda _evento, o=orden: self.orden(o))
            self.botones.append(boton)
        self.boton_grabar = self.botones[-1]

        ax_g = self.fig.add_axes([0.30, 0.025, 0.075, 0.12])
        ax_g.set_title("Ganancia DC", fontsize=9)
        self.radio_g = RadioButtons(ax_g, ("×2", "×11", "×101"))
        self.radio_g.on_clicked(self._ganancia)
        ax_s = self.fig.add_axes([0.385, 0.025, 0.09, 0.12])
        ax_s.set_title("Corrección", fontsize=9)
        self.check_signo = CheckButtons(ax_s, ("signo I",), (True,))
        self.check_signo.on_clicked(self._signo)

        self.fig.canvas.mpl_connect("key_press_event", self._tecla)
        self.fig.canvas.mpl_connect("close_event", lambda _evento: self.cerrar())
        if csv:
            self.alternar_grabacion()

    # ---- órdenes ----

    def orden(self, o):
        if o == "g":
            self.alternar_grabacion()
            return
        if o == "b":
            self.traza.clear()          # "Borrar traza" vacía también la del PC
        self.fuente.enviar(o)

    def _tecla(self, evento):
        if evento.key in TECLAS or evento.key == "g":
            self.orden(evento.key)

    def _ganancia(self, etiqueta):
        if not self.sincronizando:
            self.fuente.enviar(GANANCIAS[int(etiqueta[1:])])

    def _signo(self, _etiqueta):
        if not self.sincronizando:
            self.fuente.enviar("s")

    def _sincronizar(self, g=None, signo=None):
        """Pone los controles como dice el PIC sin volver a enviar órdenes."""
        self.sincronizando = True
        try:
            if g in GANANCIAS:
                indice = (2, 11, 101).index(g)
                if self.radio_g.value_selected != f"×{g}":
                    self.radio_g.set_active(indice)
            if signo is not None and self.check_signo.get_status()[0] != signo:
                self.check_signo.set_active(0)
        finally:
            self.sincronizando = False

    def alternar_grabacion(self):
        if self.grabador:
            self.grabador.cerrar()
            print(f"Grabación terminada: {self.grabador.ruta}")
            self.grabador = None
            self.boton_grabar.label.set_text("Grabar")
            return
        ruta = self.csv_pedido or datetime.datetime.now().strftime("medida_%Y%m%d_%H%M%S.csv")
        self.csv_pedido = None          # las siguientes grabaciones, con nombre nuevo
        self.grabador = Grabador(ruta, self.comentario, self.configuracion)
        self.boton_grabar.label.set_text("Grabando...")
        print(f"Grabando en {ruta}")

    # ---- datos ----

    def procesar_cola(self, maximo=5000):
        for _ in range(maximo):
            try:
                linea = self.fuente.cola.get_nowait()
            except queue.Empty:
                return
            dato = interpretar(linea)
            if dato is None:
                continue
            tipo, valor = dato
            if tipo == "muestra":
                self.traza.append((valor[3], valor[4]))
                self.historia.append(valor)
                self.llegadas.append(time.monotonic())
                if self.grabador:
                    self.grabador.muestra(valor)
            elif tipo == "bloque":
                self.bloque = valor
                if self.grabador:
                    self.grabador.bloque(valor)
                self._sincronizar(g=valor["G"])
            elif tipo == "punto":
                self.punto_volcado += 1
            else:
                print(valor)
                if valor.startswith("# fin traza"):
                    print(f"  (recibidos {self.punto_volcado} puntos de la traza del PIC)")
                    self.punto_volcado = 0
                config = interpretar_configuracion(valor)
                if config:
                    self.configuracion = valor
                    if self.grabador:
                        self.grabador.configuracion(valor)
                    self._sincronizar(g=config["G"], signo=config["signo"])

    def actualizar(self, _cuadro=None):
        self.procesar_cola()
        ahora = time.monotonic()
        while self.llegadas and ahora - self.llegadas[0] > 1.0:
            self.llegadas.popleft()

        # Plano polar con autoescala: anillo exterior = r máximo de la traza
        if self.traza:
            i1 = [p[0] for p in self.traza]
            q1 = [p[1] for p in self.traza]
            self.linea_traza.set_data(i1, q1)
            self.punto_actual.set_data([i1[-1]], [q1[-1]])
            r_max = max(math.hypot(i, q) for i, q in self.traza)
        else:
            self.linea_traza.set_data([], [])
            self.punto_actual.set_data([], [])
            r_max = 1.0
        self._escalar_polar(max(r_max, 0.01))

        # Gráficas temporales (últimos VENTANA_S segundos)
        if self.historia:
            t_fin = self.historia[-1][0]
            t = [(m[0] - t_fin) / 1000 for m in self.historia]
            columnas = {"Ip": 1, "Qp": 2, "I1": 3, "Q1": 4}
            for clave, linea in self.lineas_t.items():
                linea.set_data(t, [m[columnas[clave]] for m in self.historia])
            for ax, claves in ((self.ax_final, (3, 4)), (self.ax_proc, (1, 2))):
                valores = [m[k] for m in self.historia for k in claves]
                bajo, alto = min(valores), max(valores)
                margen = max(0.1 * (alto - bajo), 0.05)
                ax.set_ylim(bajo - margen, alto + margen)

        self._indicadores()
        return []

    def _escalar_polar(self, r):
        if abs(r - self.r_escala) <= 0.02 * self.r_escala:
            return
        self.r_escala = r
        angulos = [2 * math.pi * k / 180 for k in range(181)]
        for k, (anillo, etiqueta) in enumerate(zip(self.anillos, self.etiquetas_anillo), start=1):
            rk = r * k / 4
            anillo.set_data([rk * math.cos(a) for a in angulos], [rk * math.sin(a) for a in angulos])
            etiqueta.set_position((rk * math.cos(math.radians(75)), rk * math.sin(math.radians(75))))
            etiqueta.set_text(f"{rk:.3g} mV")
        for k, (radio, etiqueta) in enumerate(zip(self.radios, self.etiquetas_angulo)):
            a = math.radians(30 * k)
            radio.set_data([0, r * math.cos(a)], [0, r * math.sin(a)])
            etiqueta.set_position((1.08 * r * math.cos(a), 1.08 * r * math.sin(a)))
        self.ax.set_xlim(-1.16 * r, 1.16 * r)
        self.ax.set_ylim(-1.16 * r, 1.16 * r)

    def _indicadores(self):
        b = self.bloque
        estado = f"Fuente: {self.fuente.descripcion}   ·   {len(self.llegadas)} muestras/s"
        if self.grabador:
            estado += f"   ·   grabando en {self.grabador.ruta}"
        self.titulo.set_text(estado)
        if b is None:
            self.texto_indicadores.set_text("Esperando datos del PIC...")
            return
        signo = "ON" if self.check_signo.get_status()[0] else "OFF"
        self.texto_indicadores.set_text(
            f"|V|    {b['V']:9.3f} mV        I0       {b['I0']:9.3f} mV\n"
            f"Fase   {b['fase']:9.2f} °         Q0       {b['Q0']:9.3f} mV\n"
            f"d      {b['d']:9.3f} ×10⁻⁴     θ        {b['theta']:9.2f} °\n"
            f"VCC_A  {b['vcc']:9.3f} V         G        ×{b['G']}  (signo I {signo})\n"
            f"puntos actuales {len(self.traza):6d}     grieta sola |Q'|/4.03 ="
            f" {abs(self.historia[-1][4]) / MV_POR_1E4 if self.historia else 0:.3f} ×10⁻⁴")
        avisos = [texto for bit, texto in AVISOS if b["banderas"] & bit]
        self.texto_avisos.set_text("  ·  ".join(avisos))

    def cerrar(self):
        if self.grabador:
            self.grabador.cerrar()
            print(f"Grabación guardada en {self.grabador.ruta}")
            self.grabador = None
        self.fuente.cerrar()


# --------------------------------------------------------------------------

def liberar_teclas(plt):
    """Quita los atajos de matplotlib que chocan con las órdenes (s, g, c...)."""
    propias = set(TECLAS) | {"g"}
    for clave, valor in plt.rcParams.items():
        if clave.startswith("keymap.") and isinstance(valor, list):
            plt.rcParams[clave] = [t for t in valor if t not in propias]


def main():
    p = argparse.ArgumentParser(description="Plano polar en vivo del detector de fisuras (PIC16F18426).")
    fuente = p.add_mutually_exclusive_group()
    fuente.add_argument("--puerto", help="puerto serie del adaptador USB-serie (COM5, /dev/ttyUSB0...)")
    fuente.add_argument("--demo", action="store_true", help="datos simulados, sin hardware")
    fuente.add_argument("--archivo", help="reproduce una captura de texto del PIC")
    fuente.add_argument("--listar", action="store_true", help="muestra los puertos serie y sale")
    p.add_argument("--baudios", type=int, default=BAUDIOS)
    p.add_argument("--puntos", type=int, default=2000, help="'Puntos en traza' (2000 a 12000)")
    p.add_argument("--csv", help="graba desde el principio en este archivo")
    p.add_argument("--comentario", default="", help="comentario para el archivo CSV")
    p.add_argument("--png", help="sin ventana: guarda una imagen al cabo de --segundos y sale")
    p.add_argument("--segundos", type=float, default=10.0)
    args = p.parse_args()

    if args.listar:
        from serial.tools import list_ports
        for puerto in list_ports.comports():
            print(f"{puerto.device:15s} {puerto.description}")
        return 0
    if not (args.puerto or args.demo or args.archivo):
        p.error("indique --puerto, --demo o --archivo (o --listar para ver los puertos)")

    import matplotlib
    if args.png:
        matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from matplotlib.animation import FuncAnimation
    liberar_teclas(plt)

    if args.puerto:
        datos = FuenteSerie(args.puerto, args.baudios)
        datos.enviar("?")               # el PIC responde con G y el signo
    elif args.archivo:
        datos = FuenteArchivo(args.archivo)
    else:
        datos = FuenteDemo()
    panel = Panel(plt, datos, args.puntos, args.csv, args.comentario)

    if args.png:
        fin = time.monotonic() + args.segundos
        while time.monotonic() < fin:
            time.sleep(0.1)
            panel.procesar_cola()
        panel.actualizar()
        panel.fig.savefig(args.png, dpi=90)
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
