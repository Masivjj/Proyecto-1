# Prueba del ADC a 1000 muestras/s con LabVIEW y una NI USB-6211

Cómo comprobar en la placa que el ADC del PIC16F18426 muestrea a **1000 muestras/s** y qué SNR da, usando una **NI USB-6211** como generador, referencia y frecuencímetro. La prueba sirve para los dos proyectos:

| Firmware | Qué envía el PIC | Flancos de RA1 por muestra de un canal |
|---|---|---|
| `ADC_1kSPS_PIC16F18426.X` (avance) | una línea por muestra de RC0 | 1 (RA1 a 1 kHz) |
| `P2_IQ_PIC16F18426.X` (1.3 o posterior) | capturas de la orden `m`: 200 conversiones de I_OUT (RC0), una por ms | 10 (RA1 a 10 kHz; cada canal se convierte 1 vez cada 10 disparos) |

No se pueden generar archivos `.vi` desde aquí, así que se describe el diagrama de bloques para montarlo a mano. [`pc/prueba_usb6211.py`](../pc/prueba_usb6211.py) hace lo mismo en Python con `nidaqmx` y sirve de referencia: si el VI y el script dan lo mismo, el VI está bien hecho.

---

## 1. Montaje

```
 USB-6211                                         PIC16F18426
 AO 0 ──[1 kΩ]──┬──────────────────────────────── RC0 (pin 10)  (I_OUT desconectado de la tarjeta P2)
                │
              [100 nF]
                │
 AI 0 ──────────┘ (en el nodo de RC0)
 AI 1 ─────────────────────────────────────────── VDD (pin 1)   (opcional: VDD exacta)
 PFI 0 ────────────────────────────────────────── RA1 (pin 12)  (PWM6: un flanco por conversión)
 AO GND / AI GND / D GND ──────────────────────── VSS (pin 14)
                                                  RC4 (pin 6) ──► RX del adaptador USB-serie (115200 8N1)
```

| Punto | Detalle |
|---|---|
| **Orden de encendido** | Alimentar el PIC **antes** de generar y parar el generador antes de quitar la alimentación: con el PIC apagado, la señal lo alimentaría por los diodos del pin. La resistencia de 1 kΩ limita esa corriente a ~3 mA. |
| **Filtro RC (1 kΩ + 100 nF)** | Obligatorio con la AO. La AO es una escalera de 10 µs (100 kS/s). El filtro de 1.6 kHz deja esos escalones en ~0.03 LSB y a 117 Hz atenúa un 0.3 %, igual en el PIC y en la AI, porque las dos miden después del filtro. |
| **AI 0** | Referenciada (RSE) contra AI GND, en el mismo nodo que RC0. Rango de ±5 V (16 bits: 0.15 mV, unos 0.2 LSB del PIC). |
| **PFI 0** | Entrada digital de 5 V con umbral de ~2.2 V: acepta los 3.3–3.7 V de RA1. Hay que programar en modo producción (*Make and Program Device*, no *Debug*) y desconectar el PICkit, porque RA1 es ICSPCLK. |
| **Firmware completo** | Desconectar **I_OUT** (J805.1) de RC0. Los demás canales pueden seguir conectados a la tarjeta P2. |
| **Masas** | La USB-6211 y el adaptador USB-serie comparten la masa del PC: basta con unir todas las masas en VSS. |

**Señal por defecto:** senoide de **117 Hz**, offset **1.65 V**, **3.0 Vpp** (de 0.15 a 3.15 V). Debe quedar dentro de 0–VDD y nunca pasar de 3.3 V. 117 Hz está por debajo de fs/2 = 500 Hz y no divide a 1000. La AO de la USB-6211 tiene una impedancia de salida de 0.2 Ω y no tiene ajuste "High-Z": la tensión que se programa es la que sale.

---

## 2. Antes de empezar: NI MAX

1. *Devices and Interfaces* → la USB-6211 aparece como **Dev1** (o el nombre que tenga: ponerlo en el VI y en `--dispositivo`). *Self-Test*.
2. *Test Panels* → *Analog Output*: 1.65 V en AO 0, y comprobar con *Analog Input* (AI 0, RSE) que llega 1.65 V a RC0.
3. *Test Panels* → *Counter I/O*: *Frequency* en ctr0 con la entrada en PFI 0. Con el PIC en marcha, debe marcar ~1 kHz (avance) o ~10 kHz (firmware completo).

---

## 3. El VI

### 3.1 Panel frontal

| Controles | Indicadores |
|---|---|
| `Dispositivo` (DAQmx Physical Channel o texto, Dev1) | Gráfica *Waveform Graph*: señal del PIC (V) y de la AI superpuestas |
| `Puerto VISA` (COM del adaptador) | Espectro del PIC (dBFS) |
| `Firmware` (enum: avance / completo) | `fs contador`, `fs cociente`, `fs reloj PC` (S/s) |
| `Frecuencia` (117 Hz), `Offset` (1.65 V), `Vpp` (3.0 V) | `Vpp PIC`, `Vpp AI`, `Offset PIC`, `Offset AI` |
| `Segundos` (5), `Capturas` (5), `Medir VDD con AI 1` (booleano) | `SNR`, `THD`, `SINAD`, `ENOB` del PIC |
| `VDD supuesta` (3.3 V, firmware completo sin AI 1) | `σ periodo` (ns), `Muestras perdidas`, LEDs de cada comprobación |

### 3.2 Diagrama de bloques

Una *Flat Sequence* de cinco marcos. Los *task in*/*error in* se encadenan de un VI al siguiente, y el error final va a *Simple Error Handler.vi*.

**Marco 1: generador (AO 0)**

1. *DAQmx Create Virtual Channel.vi*, *Analog Output » Voltage*: `Dev1/ao0`, **min 0, max 3.3 V**.
2. *Sine Waveform.vi* (paleta *Signal Processing » Waveform Generation*): frecuencia = `round(Frecuencia)`, amplitud = Vpp/2, offset = Offset, *sampling info* Fs = **100 000**, #s = **100 000**. Es un buffer de 1 s con un número entero de periodos, así que se regenera sin saltos. Para proteger el pin, antes del VI: si `Offset − Vpp/2 < 0` o `Offset + Vpp/2 > 3.3`, abortar con un error.
3. *DAQmx Timing.vi* (*Sample Clock*): rate 100 000, *Continuous Samples*, samples per channel 100 000.
4. *DAQmx Write.vi* (*Analog Wfm 1Chan NSamp*), *auto start* = F.
5. *DAQmx Start Task.vi* y *Wait (ms)* de 500 ms, para que se asiente el filtro RC.

La tarea queda en marcha y su cable pasa hasta el marco 5.

**Marco 2: referencia (AI 0 y AI 1)**

1. *DAQmx Create Virtual Channel.vi*, *Analog Input » Voltage*: `Dev1/ai0` (y `Dev1/ai1` si se mide VDD), *terminal configuration* **RSE**, min 0, max 5 V.
2. *DAQmx Timing.vi* (*Sample Clock*): **10 000 S/s**, *Finite Samples*, samples = 10 000 × Segundos.
3. *DAQmx Read.vi* (*Analog 1D Wfm NChan NSamp*), timeout = Segundos + 10. Después, *DAQmx Clear Task.vi*.
4. VDD = media de la forma de onda de AI 1 (*Mean.vi*). Si no se mide, se usa la del PIC (avance) o la supuesta (completo).

**Marco 3: frecuencia de muestreo real (contador en RA1)**

1. *DAQmx Create Virtual Channel.vi*, *Counter Input » Period*: `Dev1/ctr0`, units **Seconds**, starting edge **Rising**, measurement method **Low Frequency with 1 Counter**. Rango: min = periodo/2 y max = 2 × periodo, con periodo = 1 ms (avance) o 100 µs (completo).
2. *DAQmx Channel* (property node): **CI.Period.Term = /Dev1/PFI0**.
3. *DAQmx Timing.vi* (*Implicit*): *Finite Samples*, samples = 200 × k + 1, con k = 1 (avance) o 10 (completo).
4. *DAQmx Read.vi* (*Counter 1D DBL 1Chan NSamp*). Se descarta la primera lectura (*Array Subset* desde el índice 1).
5. fs = 1 / (k × *Mean* de los periodos). σ: se suman los periodos de k en k (*Reshape Array* a [m, k] y suma por filas) y se aplica *Std Deviation and Variance.vi*.

**Marco 4: datos del PIC por VISA**

1. *VISA Configure Serial Port.vi*: 115200 baudios, 8 bits, sin paridad, 1 bit de parada, *enable termination char* = F (se lee en bloque), timeout 200 ms.
2. **Avance:** *VISA Write.vi* con `i?` (reanuda el envío y pide la cabecera con VDD). Después, un *While Loop* de Segundos + 0.3 s:
   - *VISA* property node *Bytes at Port* → *VISA Read.vi* con ese número de bytes.
   - *Concatenate Strings* con el resto de la vuelta anterior (*shift register*), y *Spreadsheet String To Array.vi* con delimitador `\n` (tipo *string*). El último elemento (línea a medias) vuelve al *shift register*.
   - Por cada línea: *Trim Whitespace*. Si empieza por `#` (*String Subset* 0,1 = "#"), es un mensaje: *Match Pattern* `VDD = [0-9]+` → VDD en mV; `# perdidas: ` → sumar a *Muestras perdidas*. Si no, *Decimal String To Number.vi* → código (0–4095).
   - Se descartan los primeros 0.3 s. Con *Tick Count (ms)* de la primera y la última muestra: fs reloj PC = (N − 1) / Δt.
3. **Completo:** *VISA Write.vi* con `?` y buscar la línea que empieza por `# ADC:`. Debe contener `1000 muestras/s por canal`; si no, el firmware es anterior a 1.3. Después, por cada captura: *VISA Write.vi* con `m` y leer como arriba hasta `# fin captura`. Solo se guardan las líneas `M,`: se quita `M,` y *Spreadsheet String To Array* con delimitador `,` da 10 códigos por línea. Si llega `# m: espere`, esperar 200 ms y repetir `m`. Las líneas de muestra y de bloque que siguen llegando se ignoran.
4. *VISA Close.vi*.

**Marco 5: parada y análisis**

1. *DAQmx Stop Task.vi* y *DAQmx Clear Task.vi* del generador. Luego una tarea AO de una muestra a 0 V, para no dejar tensión en el pin.
2. Señal del PIC: *Build Waveform* con t0 = 0, **dt = 0.001 s** (el PIC dice que muestrea a 1 kS/s) e Y = código × VDD / 4095. En el firmware completo se analiza cada captura por separado y se promedian los resultados.
3. *Extract Single Tone Information.vi* sobre el PIC y sobre la AI: frecuencia, amplitud y fase. Vpp = 2 × amplitud. Offset con *Basic Averaged DC-RMS.vi* (DC).
4. **fs cociente** = 1000 × f_AI / f_PIC. Es independiente del contador: si el PIC muestrea a fs, la señal de f_AI se ve en f_AI × 1000 / fs. Solo vale si la frecuencia es menor de 500 Hz.
5. *SINAD Analyzer.vi* → SINAD; ENOB = (SINAD − 1.76) / 6.02 (*Formula Node*). *Harmonic Distortion Analyzer.vi* (armónicos hasta el 5.º) → THD.
6. Comprobaciones (LEDs): las mismas que imprime el script (sección 5).

---

## 4. Lo mismo en Python

```
pip install -r pc/requirements.txt
pip install nidaqmx                     # necesita el controlador NI-DAQmx instalado
python pc/prueba_usb6211.py --puerto COM5 --firmware avance
python pc/prueba_usb6211.py --puerto COM5 --firmware completo --capturas 5 --ai-vdd
python pc/prueba_usb6211.py --puerto COM5 --sin-generador         # con un generador de laboratorio
python pc/prueba_usb6211.py --demo --firmware completo            # sin hardware
```

Opciones: `--dispositivo Dev1`, `--frecuencia 117`, `--offset 1.65`, `--vpp 3.0`, `--segundos 5`, `--pfi PFI0`, `--vdd 3.3`, `--sin-contador`. El script se niega a generar fuera de 0–3.3 V y deja AO 0 a 0 V al terminar. Devuelve 0 si pasan todas las comprobaciones. Sus funciones de análisis tienen pruebas en `tests/test_prueba_usb6211.py` (`make pc`).

---

## 5. Qué esperar

| Comprobación | Criterio | Por qué |
|---|---|---|
| fs por el contador | 1000 S/s ± 2 % | HFINTOSC: ±2 % entre 0 y 60 °C |
| fs por el cociente de frecuencias | 1000 S/s ± 2 % | |
| Contador frente a cociente | ± 0.1 % | son dos medidas independientes de la misma fs |
| fs por el reloj del PC (avance) | ± 2.5 % | la latencia del USB añade ~0.5 % |
| Línea `# ADC:` (completo) | `1000 muestras/s por canal` | firmware 1.3 o posterior |
| Vpp del PIC frente a la AI | ± 1 % con VDD de AI 1; ± 5 % con la del FVR | FVR ±4 %, ganancia del ADC ±6 LSB |
| Offset del PIC frente a la AI | ± 10 mV (+4 % sin AI 1) | offset del ADC ±6 LSB (~5 mV) |
| Muestras perdidas (avance) | 0 | |
| SNR / ENOB del PIC | se mide (sin criterio) | 74 dB / 12 bits es el ideal; en la simulación, ~68.6 dB / 11.1 bits con 0.4 LSB de ruido |

Ejemplo de la salida, con datos simulados (`--demo --firmware completo`, PIC a 1007 S/s):

```
Contador en RA1 (2000 periodos):
  RA1 a 10070.00 Hz -> 1007.000 muestras/s por canal
Comprobaciones:
  OK     fs por el cociente de frecuencias: 1000 S/s ±2 %  (1007.00 S/s (117.000 Hz se ve en 116.187 Hz))
  OK     fs por el contador (RA1): 1000 S/s ±2 %  (1007.00 S/s; RA1 a 10070.0 Hz / 10)
  OK     contador y cociente coinciden (±0.1 %)  (-0.0000%)
  OK     el firmware informa 1000 muestras/s por canal  (# ADC: modo basico, ...)
```

---

## 6. Límites

- **El contador no certifica σ < 10 ns.** Su base de tiempo es de 80 MHz (12.5 ns). Cada periodo medido lleva un error de cuantificación de hasta q/√6 ≈ 5.1 ns eficaces, más el jitter propio de la tarjeta. Si la σ medida no pasa claramente de ese suelo, solo se puede decir que el jitter es menor de unos 10 ns. Para el requisito de 10 ns: osciloscopio en RA1 (medida TIE) o el método del SNR con 230 Hz y 51 230 Hz (`pc/medir_jitter.py`, `docs/analisis_adc_jitter.md`, sección 6). Con la AO no se puede medir el jitter por el SNR: a 100 kS/s y con un filtro de 1.6 kHz no llega a 51 kHz. Hace falta un generador de laboratorio con `--sin-generador`.
- **La AO también tiene ruido y distorsión.** Es de 16 bits y debería quedar por debajo del ADC de 12 bits, pero no está comprobado. El SNR y la THD de la AI 0 lo dicen: si los del PIC se acercan a los de la AI, el límite es la señal y no el ADC.
- **En el firmware completo** solo se compara I_OUT (RC0). Los demás canales se muestrean con la misma ronda y el mismo periodo: el simulador (`make sim`) comprueba que todos van exactamente a 1 ms.
