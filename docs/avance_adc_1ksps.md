# Avance: ADC del PIC16F18426 a 1000 muestras/s con salida serie

Proyecto separado del firmware completo, solo con la parte del ADC: el PIC muestrea una señal de un **generador de señales** en RC0 a **1000 muestras/s** y envía cada muestra por la **UART** (115200 8N1) al PC, donde `pc/adc_1ksps.py` la dibuja, mide su espectro y la graba.

| Qué | Dónde |
|---|---|
| Proyecto de MPLAB X (XC8 + MCC Classic) | `ADC_1kSPS_PIC16F18426.X/` (`main.c`, `config.h`, `mcc_generated_files/`) |
| Programa del PC | `pc/adc_1ksps.py` (gráfica en vivo, espectro, SNR/ENOB, CSV) |
| Pruebas en el PC | `tests/adc1k/`, `tests/verificar_adc1k.py`, `tests/test_adc_1ksps.py` (`make adc1k`, `make pc`) |

El ADC se configura igual que en el firmware completo: modo básico, disparo por hardware con el PWM6 y reloj síncrono con FOSC (ver `docs/analisis_adc_jitter.md`). Cambian el periodo, que pasa de 100 µs a 1 ms, y que solo hay un canal.

---

## 1. Montaje

```
 Generador ──[1 kΩ]──┬── RC0 (pin 10)            PIC16F18426
  (salida)           │                            RC4 (pin 6)  ──► RX del adaptador USB-serie
                   [100 nF]  (opcional)           RC5 (pin 5)  ◄── TX del adaptador
  masa ──────────────┴── VSS (pin 14) ── GND del adaptador y del programador
                                                  RA5 (pin 2)  ── LED + 1 kΩ a GND
                                                  RA1 (pin 12) ── PWM6: disparo de cada muestra (osciloscopio)
                                                  VDD (pin 1)  ── 3.3 V o 5 V, con 0.1 µF junto al pin
                                                  MCLR (pin 4) ── 10 kΩ a VDD y al programador
```

**Ajustes del generador** (con VDD = 3.3 V):

| Parámetro | Valor | Por qué |
|---|---|---|
| Forma | senoidal | para medir SNR, SINAD y ENOB; con cuadrada o triangular también se ve, pero el ajuste solo mide la fundamental |
| Offset | **VDD / 2 = 1.65 V** | el ADC solo convierte de 0 a VDD |
| Amplitud | **hasta 3.0 Vpp** (−0.8 dBFS) | sin llegar a 0 ni a VDD; el programa avisa si se satura |
| Frecuencia | **10 a 450 Hz**, p. ej. 117 Hz | por debajo de fs/2 = 500 Hz (Nyquist). Mejor una que no divida a 1000 Hz |
| Carga de salida | **High-Z** | si el generador se configura para 50 Ω, entrega el doble de la amplitud que indica al PIC (alta impedancia) |

La entrada nunca debe pasar de 0–VDD (más de 0.3 V fuera puede dañar el pin). La resistencia de 1 kΩ limita la corriente si ocurre. El condensador de 100 nF forma con ella un filtro de 1.6 kHz que quita ruido y, a 117 Hz, atenúa un 0.3 %. No es un filtro antialiasing completo: por eso hay que quedarse por debajo de 500 Hz.

---

## 2. Configuración en MCC Classic

`mcc_generated_files/` ya contiene el código equivalente, así que el proyecto **compila sin abrir MCC**. Para regenerarlo o crearlo desde cero:

| Módulo | Ajustes |
|---|---|
| Proyecto | PIC16F18426, XC8 v2.40 o posterior, C99 |
| System Module | HFINTOSC, 32 MHz, divisor 1 (FOSC = 32 MHz). Bits de configuración como el firmware completo (`docs/configuracion_MCC_Classic.md`, sección 2) |
| Pin Module | RC0 → ADCC ANC0, nombre **`SENAL`**, analógico; RC4 → EUSART1 TX1; RC5 → EUSART1 RX1; RA5 → GPIO salida, nombre **`LED`**. RA1 no se asigna (lo hace `main.c`) |
| ADCC | **Basic_mode**; referencias VDD / VSS; justificado a la derecha; **Auto-conversion Trigger = PWM6_OUT**; reloj **FOSC/ADCLK, FOSC/32** (TAD = 1 µs); **Acquisition Count = 8**; **ADC Interrupt** activada; sin modo continuo ni doble muestreo |
| FVR | habilitado, buffer al ADC 1x (1.024 V): `main.c` mide VDD con él al arrancar |
| TMR2 | FOSC/4, **prescaler 1:32**, postscaler 1:1, **periodo 1 ms** (T2PR = 249), Roll over pulse / Software control, **sin interrupción** |
| PWM6 | habilitado, **Timer2**, ciclo de trabajo **50 %**, active_hi |
| EUSART1 | asíncrona, **115200** baudios, TX y RX, interrupciones activadas; buffers TX 128 y RX 8 |
| Interrupt Module | ADC (ADI) y EUSART1 (TX y RX) |

Valores de registro que deben salir:

| Archivo | Registro | Valor |
|---|---|---|
| `mcc.c` | OSCCON1 / OSCFRQ | 0x60 / 0x06 |
| `adcc.c` | ADCON0 / ADCON2 / ADCLK / ADACQL / ADACT | 0x84 / 0x00 / 0x0F / 0x08 / 0x0E |
| `tmr2.c` | T2CLKCON / T2PR / T2CON | 0x01 / 0xF9 / 0xD0 |
| `pwm6.c` | PWM6CON / PWM6DCH / PWM6DCL / CCPTMRS1.P6TSEL | 0x80 / 0x7D / 0x00 / 1 |
| `fvr.c` | FVRCON | 0x81 |
| `eusart1.c` | BAUD1CON / RC1STA / TX1STA / SP1BRGL | 0x08 / 0x90 / 0x24 / 0x44 |
| `pin_manager.c` | ANSELA / ANSELC / TRISA / TRISC / WPUA | 0x17 / 0x0F / 0x1F / 0x2F / 0x00 |
| `pin_manager.c` | RX1PPS / RC4PPS | 0x15 / 0x0F |

Periodo de muestreo: 32 × 250 / 8 MHz = **1.000 ms** = 32 000 ciclos de FOSC = 1000 TAD exactos.

---

## 3. Funcionamiento

```
 cada 1 ms   flanco del PWM6 ─► ADC: 8 µs de adquisición ─► muestreo ─► 14 µs de conversión ─► ADIF
                                                                                              │
 interrupción: lee el código y lo pone en la FIFO (128 posiciones) ◄────────────────────────────┘
 lazo principal: saca un código de la FIFO y lo envía como texto "2047\r\n"
```

- **El software no decide cuándo se muestrea**: el instante lo marca el PWM6, así que su jitter es el del oscilador.
- **Presupuesto de la UART:** a 115200 baudios caben 11 520 caracteres/s. 1000 líneas de hasta 6 caracteres ("4095\r\n") ocupan como mucho el **52 %**.
- **FIFO:** si el lazo se retrasa (por ejemplo, mientras envía la cabecera), las muestras esperan hasta 127 ms sin perderse. Si se llena, se descartan, se cuentan y se avisa con `# perdidas: N`.
- **VDD:** al arrancar, antes de habilitar el disparo, se convierte 16 veces el FVR: VDD = 1.024 V × 4095 / código. La cabecera lo informa con la tolerancia del FVR, ±4 % (FVR01). El PC lo usa para pasar los códigos a voltios.
- **LED (RA5):** cambia cada 500 muestras, un parpadeo por segundo mientras el ADC convierte.

## 4. Protocolo serie (115200 8N1)

```
# ADC 1 kS/s - PIC16F18426 - firmware 1.0
# entrada RC0 (ANC0), 12 bits, referencia VDD = 3301 mV (medida con el FVR, +-4 %)
# muestreo: 1 conversion cada 1000 us, disparada por el PWM6
# formato: una linea por muestra con el codigo del ADC (0-4095)
# ordenes: d=detener envio  i=reanudar  ?=estado
2047
2851
...
```

| Orden | Respuesta |
|---|---|
| `d` | `# d: envio detenido`: deja de enviar muestras (el ADC sigue convirtiendo) |
| `i` | `# i: envio en marcha` |
| `?` | la cabecera y `# estado: envio en marcha, convertidas N, perdidas M` |

Voltaje de cada muestra: **V = código × VDD / 4095**.

---

## 5. Cómo presentarlo

1. Abrir `ADC_1kSPS_PIC16F18426.X` en MPLAB X y pulsar *Make and Program Device*. Para usar RA1 con el osciloscopio, hay que programar así y no con *Debug*.
2. Generador: senoide de 117 Hz, 3 Vpp, offset 1.65 V, salida High-Z. Conectarlo a RC0 (sección 1).
3. **Datos crudos:** cualquier terminal serie a 115200 (PuTTY, RealTerm, el monitor serie de Arduino…) muestra la cabecera y una línea por muestra. Un trazador serie (por ejemplo, el *Serial Plotter* de Arduino) puede dibujar esas líneas de números.
4. **Programa del PC:**
   ```
   pip install -r pc/requirements.txt
   python pc/adc_1ksps.py --listar
   python pc/adc_1ksps.py --puerto COM5
   ```
   Muestra la señal (cada punto es una muestra), el espectro del último segundo y las medidas: frecuencia, Vpp, offset, SNR, THD, SINAD, ENOB y muestras/s. Los botones *Detener/Reanudar* y *Estado* envían `d`, `i` y `?`; *Grabar CSV* guarda `n,t_s,codigo,V`.
5. Cambiar la frecuencia y la amplitud en el generador y ver cómo responden la gráfica y las medidas. Por encima de 500 Hz la señal aparece en su alias: con 900 Hz se ve una senoide de 100 Hz.
6. Opcional: osciloscopio en RA1 (pin 12). Cuadrada de 1 kHz; cada flanco de subida es el instante de una muestra.

Sin hardware (para ensayar la presentación): `python pc/adc_1ksps.py --demo` imita al PIC con un generador de 117 Hz (`--demo-f` cambia la frecuencia).

**Desde LabVIEW:** *VISA Configure Serial Port* a 115200 8N1 con carácter de terminación `\n`. Luego, en un bucle, *VISA Read*: si la línea empieza por `#` es un mensaje; si no, *Decimal String To Number* × VDD / 4095.

## 6. Qué resultados esperar

- **Muestras/s:** 1000 ± 2 %. El reloj es el HFINTOSC, con ±2 % de exactitud entre 0 y 60 °C. El programa del PC lo mide con su reloj en una ventana de 5 s.
- **Frecuencia:** la que marca el generador × 1000 / (muestras/s reales). Si el PIC muestrea a 1010 S/s, una senoide de 117 Hz se lee 115.8 Hz. La forma y el espectro no cambian.
- **Amplitud y offset:** con el error de la VDD medida con el FVR (±4 %) y el del ADC (offset y ganancia de hasta ±6 LSB).
- **SNR y ENOB:** un ADC ideal de 12 bits da 74 dB (12 bits). La hoja de datos no especifica el SNR. En la simulación, con 0.3 LSB de ruido, sale 69.9 dB (11.3 bits). En la placa dependerá del ruido del generador, de VDD y del montaje. Este es el dato que este avance permite medir.

## 7. Verificación hecha

| Qué | Resultado |
|---|---|
| `main.c` y `mcc_generated_files/*.c` contra el `pic16f18426.h` oficial (gcc, en producción, depuración y sin salida del disparo) | sin errores |
| Firmware simulado en el PC (`make adc1k`): disparo cada 1 ms, UART a su velocidad real, generador de 117 Hz, paradas del lazo de 50 y 200 ms, órdenes `?`, `d`, `i` | 19/19. Cada línea enviada es su conversión, en orden; la parada de 50 ms no pierde nada; la de 200 ms pierde 74 muestras y lo avisa; 117.000 Hz, 1.5000 V, 1.6500 V |
| Programa del PC (`tests/test_adc_1ksps.py`) | 19/19 |
| **Compilación con XC8 y prueba en la placa** | **pendiente** |
