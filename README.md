# Detector de fisuras por corrientes de Eddy: procesamiento I/Q en un PIC16F18426

Firmware para **PIC16F18426** (MPLAB X + XC8 + **MCC Classic**) que lee las dos componentes DC **I** y **Q** de la tarjeta demoduladora P2 y reproduce el procesamiento del VI de LabVIEW `P2_IQ_SINAD.vi` para detectar fisuras en tubo de aluminio con una sonda de corrientes de Eddy diferencial:

- normalización ratiométrica a VCC_A = 3.7 V, división por la ganancia DC (×2, ×11, ×101) y corrección (fija) del signo invertido de I;
- cero (I0, Q0), ángulo de calibración θ (lift-off) y rotación → señales finales I′, Q′;
- ADC en modo básico disparado por hardware (PWM6, una conversión cada 100 µs) y **cada entrada muestreada a 1000 muestras/s** (una conversión por canal en cada ronda de 1 ms, como el DAQ del VI): el instante de muestreo no depende del software y su jitter es el del oscilador; autocero del ADC con una conversión de AVSS en cada ronda;
- captura de 200 conversiones seguidas de I_OUT a 1 kS/s (orden `m`) y un programa en Python que mide con ella el SNR del ADC y estima el jitter de muestreo;
- las etapas de cero, θ, ganancia, normalización a VCC_A y autocero se activan y desactivan en marcha con una tecla;
- indicadores por bloque de 0.1 s: |V|, Fase y d estimada (×10⁻⁴);
- traza del plano polar, envío por UART al PC y un programa en Python que dibuja el plano polar como el VI.

La especificación de partida está en [`docs/P2_IQ_procesamiento_para_PIC16F18426.md`](docs/P2_IQ_procesamiento_para_PIC16F18426.md); el código cita sus secciones (P1…P10, 10.4, 10.6…).

**Avance separado: solo el ADC a 1000 muestras/s.** El proyecto [`ADC_1kSPS_PIC16F18426.X`](ADC_1kSPS_PIC16F18426.X) muestrea una señal de un generador en RC0, disparada por el PWM6 cada 1 ms, y envía cada muestra por la UART. [`pc/adc_1ksps.py`](pc/adc_1ksps.py) la dibuja con su espectro, SNR y ENOB. Guía de montaje y presentación: [`docs/avance_adc_1ksps.md`](docs/avance_adc_1ksps.md).

**Reloj externo de 4 MHz (opcional):** el firmware completo funciona con el oscilador interno (32 MHz, por defecto) o con un cristal u oscilador de cuarzo de 4 MHz y el PLL ×4 (16 MHz), que se elige al compilar con la opción `RELOJ` o con las configuraciones `cristal_4MHz` y `oscilador_4MHz` de MPLAB X. Si el reloj externo falta o se para, sigue con el oscilador interno a 16 MHz y lo avisa. Conexiones, pines que cambian y registros: [`docs/reloj_externo_4MHz.md`](docs/reloj_externo_4MHz.md).

**Prueba en la placa con LabVIEW y una NI USB-6211** (los dos proyectos): AO 0 como generador, AI 0 como referencia y un contador en RA1 miden la frecuencia de muestreo real, la amplitud y el SNR. Diagrama de bloques del VI en [`docs/pruebas_labview_usb6211.md`](docs/pruebas_labview_usb6211.md); lo mismo en Python con `nidaqmx` en [`pc/prueba_usb6211.py`](pc/prueba_usb6211.py).

## Estado de la verificación

| Qué | Cómo | Resultado |
|---|---|---|
| Procesamiento (P1–P10) | `tests/test_iq_proc.c`: el mismo `iq_proc.c` del PIC compilado con gcc, con los vectores de la sección 9.1, las etapas activables, el autocero y casos extra | 102/102 correctas |
| Firmware completo (`main.c` y `app/`) | `tests/sim`: se compila sin cambios contra un hardware simulado (ADC disparado cada 100 µs con +3 LSB de offset; el simulador falla si alguna entrada no se convierte exactamente cada 1 ms; fisura sintética, UART, EEPROM, botones, paradas del lazo de 30 y 150 ms, captura `m` con un tono de 115 Hz, etapas desactivadas) | 36/36 comprobaciones |
| Medida del jitter | `tests/test_medir_jitter.py`: ADC ideal de 12 bits (SINAD = 74 dB) y capturas simuladas con σ conocido (10 y 30 ns: error < 10 %) | 19/19 |
| Avance ADC 1 kS/s | `tests/adc1k`: firmware simulado con la UART a su velocidad real (cada línea enviada se compara con su conversión); `tests/test_adc_1ksps.py` | 19/19 y 18/18 |
| Nombres de registros y bits | todos los `.c` de los dos proyectos contra el `pic16f18426.h` oficial (DFP 1.32.471) con gcc (repetido tras el cambio a 1 kS/s); las 146 macros de pines; los `#pragma config` contra el `cfgdata` del dispositivo | sin errores |
| Programa del PC | `tests/test_plano_polar.py` (19/19) y prueba sin ventana con `--demo` y `--archivo` | correcto |
| Prueba con la USB-6211 (`pc/prueba_usb6211.py`) | `tests/test_prueba_usb6211.py`: generador limitado a 0–3.3 V, fs por el contador y por el cociente de frecuencias, y fallos provocados (fs +3 %, firmware sin `# ADC:`, amplitud, pérdidas, saturación). La API de `nidaqmx` se revisó contra su código fuente (1.7.0); sin la tarjeta no se ha ejecutado | 25/25 |
| Opción de reloj externo de 4 MHz (`RELOJ`) | `make reloj`: registros de MCC en cada modo (FOSC, TMR2 de 100 µs, PWM6, TAD de 1 µs, baudios, bits de configuración) y puerto A en 9 casos. Firmware simulado con cristal y con oscilador: reloj bien, ausente y parándose. Los `.c` contra el `pic16f18426.h` oficial en 12 combinaciones con `-Wundef` | todo correcto |
| **Compilación con XC8** | **no se ha podido ejecutar XC8 en el entorno donde se escribió** | **pendiente: compilar en MPLAB X** |
| Hardware real (incluido el jitter del HFINTOSC) | — | pendiente: `pc/prueba_usb6211.py` o el VI (fs = 1000 S/s), `m` y `pc/medir_jitter.py` (jitter) |

Al compilar por primera vez revisa el *Memory Summary* de la salida (la estimación es ~50 % de RAM y menos de la mitad de la Flash) y comprueba en el PC que `muestras perdidas` sigue en 0 (orden `?`).

## Contenido

```
P2_IQ_PIC16F18426.X/            proyecto de MPLAB X (abrir esta carpeta)
├── main.c                      lazo principal: tramas de 10 ms, bloques, LED
├── app/
│   ├── app_config.h            constantes de la especificación y opciones
│   ├── iq_proc.c/.h            procesamiento P1-P10 (sin dependencias del hardware)
│   ├── adquisicion.c/.h        ADC disparado por el PWM6: I_OUT, Q_OUT, V_REF, VCC_A, FVR, AVSS
│   ├── comunicacion.c/.h       protocolo UART: muestras, bloques y órdenes
│   ├── traza.c/.h              traza del plano polar (P9), buffer circular
│   ├── ajustes.c/.h            I0, Q0, θ, G y etapas en EEPROM (escritura sin bloqueo)
│   ├── botones.c/.h            pulsadores con antirrebote
│   ├── reloj.c/.h              reloj externo de 4 MHz con PLL x4 y reserva con el HFINTOSC (opción RELOJ)
│   └── formato.c/.h            números a texto sin printf
├── mcc_generated_files/        drivers de MCC Classic (ver docs/configuracion_MCC_Classic.md)
└── nbproject/, Makefile        archivos del proyecto de MPLAB X
pc/plano_polar.py               plano polar, gráficas, botones y grabación CSV en el PC
pc/medir_jitter.py              SNR, ENOB y jitter de muestreo con las capturas de la orden m
pc/prueba_usb6211.py            prueba con la NI USB-6211: generador, referencia, contador y SNR (nidaqmx)
ADC_1kSPS_PIC16F18426.X/        avance separado: solo el ADC a 1000 muestras/s con salida serie
pc/adc_1ksps.py                 su programa del PC: señal, espectro, SNR/ENOB y CSV
tests/                          pruebas en el PC (make)
docs/                           especificación, guía de MCC Classic y análisis del ADC y el jitter
```

## Puesta en marcha

1. **Abrir** `P2_IQ_PIC16F18426.X` en MPLAB X (*File > Open Project*). Si no tienes XC8 v2.50, elige el que tengas (v2.40 o posterior) en *Project Properties > Compiler Toolchain*.
2. En *Project Properties > XC8 Global Options* comprueba **C99** y **float y double de 32 bits** (si no, la compilación se detiene con un error en `iq_proc.c`).
3. Elige la configuración del reloj en la barra de herramientas: `default` (oscilador interno), `cristal_4MHz` u `oscilador_4MHz` ([`docs/reloj_externo_4MHz.md`](docs/reloj_externo_4MHz.md)). Elige la herramienta (PICkit 4/5, MPLAB Snap…) y pulsa *Make and Program Device*.
4. En `app/app_config.h`, `GANANCIA_INICIAL` debe coincidir con los jumpers JP701/JP702 (o cámbiala luego con las órdenes `2`, `1`, `0`; queda guardada en la EEPROM).
5. En el PC: `pip install -r pc/requirements.txt` y `python pc/plano_polar.py --puerto COM5` (Windows) o `--puerto /dev/ttyUSB0` (Linux). `--listar` muestra los puertos.
6. Calibración, como en el VI:
   - sonda quieta sobre metal sano → **Fijar cero** (pulsador RA2, tecla `z` o botón);
   - sonda quieta sobre papel (lift-off conocido) → **Calibrar θ** (pulsador RA4, o RA1 con el cristal; tecla `c` o botón). Desde entonces el lift-off cae sobre 0° y una fisura se aparta de 0°/180°.

Para probar el programa del PC sin hardware: `python pc/plano_polar.py --demo`.

La configuración de los periféricos está documentada paso a paso en [`docs/configuracion_MCC_Classic.md`](docs/configuracion_MCC_Classic.md), por si quieres regenerar con MCC.

## Conexiones

Esquema completo con lista de componentes, para los tres modos de reloj: [`docs/esquema_conexiones.html`](docs/esquema_conexiones.html) (abrir en el navegador).
Para grabar el firmware con un PICkit 3/4/5 o un MPLAB Snap en una tarjeta aparte con zócalo: [`docs/tarjeta_programacion.html`](docs/tarjeta_programacion.html).

| PIC16F18426 (14 pines) | Señal | Conexión |
|---|---|---|
| 1 VDD | alimentación | VCC de la tarjeta (no VCC_A) |
| 14 VSS | tierra | J805.6 (GND), común con el adaptador USB-serie |
| 10 RC0 / ANC0 | I_OUT | J805.1 |
| 9 RC1 / ANC1 | Q_OUT | J805.3 |
| 8 RC2 / ANC2 | V_REF | J805.2 |
| 7 RC3 / ANC3 | VCC_A | J805.5 |
| 6 RC4 | TX (115200 8N1) | RX del adaptador USB-serie |
| 5 RC5 | RX | TX del adaptador USB-serie |
| 11 RA2 | pulsador "Fijar cero" | a GND (pull-up interno) |
| 3 RA4 | pulsador "Calibrar θ" | a GND (pull-up interno). Con el cristal de 4 MHz: OSC2 |
| 2 RA5 | LED | LED + resistencia de 1 kΩ a GND. Con reloj externo de 4 MHz: OSC1/CLKIN, y el LED pasa a RA0 |
| 4 RA3 / MCLR | reset | 10 kΩ a VDD; al programador |
| 13 RA0 | ICSPDAT; con reloj externo, LED | al programador |
| 12 RA1 | ICSPCLK; en producción, salida del disparo (PWM6, 10 kHz); con el cristal, pulsador "Calibrar θ" | al programador; punta del osciloscopio para medir el jitter |

- **VCC_A no debe superar VDD del PIC**: si lo hace, el ADC satura en ese canal (el PC muestra el aviso "VCC_A ≥ VDD del PIC"). Si puede pasar, monta un divisor 1:2 (2 × 10 kΩ) en la entrada de VCC_A y pon `DIVISOR_VCCA 2` en `app_config.h`.
- El adaptador USB-serie debe trabajar con niveles de 3.3 V a 5 V y compartir la tierra.
- Opcional: un filtro RC (1 kΩ + 100 nF) cerca de cada entrada analógica si el cable es largo. Además de quitar ruido, recorta lo que quede de la portadora: el jitter solo afecta a las componentes de alta frecuencia que lleguen al pin.
- RA1 saca el disparo del ADC solo en las compilaciones de producción con `SALIDA_DISPARO 1`; en las de depuración queda para el depurador.

## Funcionamiento

El ADC trabaja en **modo básico** y cada conversión la dispara el flanco de subida del **PWM6** (TMR2, cada 100 µs). La interrupción de fin de conversión suma el resultado en su canal y elige el siguiente; el software no decide cuándo se muestrea. Los disparos se agrupan en **rondas de 1 ms** (10 disparos): I_OUT, Q_OUT, V_REF, VCC_A, FVR, AVSS (autocero) y 4 disparos libres. Cada entrada ocupa siempre la misma posición de la ronda, así que **se muestrea a 1000 muestras/s con un periodo fijo de 1 ms**, como el DAQ Assistant del VI (1 kS/s por canal). Una **trama de 10 ms** son 10 rondas: la muestra de salida de cada canal es la suma de sus 10 conversiones. Las sumas de cada trama pasan a una cola de 4 tramas que vacía `main()`, que procesa la muestra. Cada 10 muestras (0.1 s: 100 conversiones por canal, como el bloque de 100 muestras de LabVIEW) cierra un bloque. La orden `?` lo confirma con la línea `# ADC: modo basico, disparo PWM6 cada 100 us, 1000 muestras/s por canal, 10 por trama`. El análisis del ADC, de las referencias y del jitter está en [`docs/analisis_adc_jitter.md`](docs/analisis_adc_jitter.md).

| Etapa del VI | Dónde |
|---|---|
| Autocero del ADC | `escalar()`: el offset medido en AVSS (cZ) se resta de todos los canales antes de nada |
| P1 VCC_A y protección de 2.5 V | `iq_proc.c: escalar()` y `cerrar_bloque()`; VCC_A en voltios = 1.024 · cV / cFVR |
| P2–P3 k = 3.7/VCC_A/G·1000, signo de I | `escalar()`: Ip = −(cI − cR)·3700 / (cV·G); VDD se cancela. El signo de I se corrige siempre (la tarjeta fabricada lo invierte, sección 1.3); si se arregla la tarjeta, quitar el signo menos en `escalar()` |
| P4 medias del bloque | `iq_procesar()` / `cerrar_bloque()` |
| P5 Fijar cero / Borrar 0 | `cerrar_bloque()` ("Borrar 0" manda) |
| P6–P8 sin cero y rotación por −θ | `iq_procesar()`: I′ = Ic·cosθ + Qc·sinθ, Q′ = Qc·cosθ − Ic·sinθ |
| P7 Calibrar θ / θ = 0 | `cerrar_bloque()`: θ = atan2(mQc, mIc), con el cero ya actualizado |
| P9 traza del plano polar | `traza.c` (64 puntos en el PIC; la traza larga la guarda el PC) |
| P10 \|V\|, Fase, d | `cerrar_bloque()` |

**Diferencias con LabVIEW** (todas de un bloque, 0.1 s, como mucho):

- El cero y θ capturados en un bloque se aplican a las muestras desde el bloque siguiente (diferencia aceptada en la sección 10.6). Los indicadores |V|, Fase y d de ese bloque sí usan el cero y θ nuevos, como el VI (la media de I′, Q′ se calcula exactamente desde las medias de Ip, Qp).
- La decisión "VCC_A > 2.5 V" se toma con la media del bloque anterior; la normalización usa el VCC_A de cada muestra (10.4). El LED de VCC_A refleja la media del propio bloque, como en el VI.
- SINAD, THD y espectro no se portan (sección 8).

**LED (RA5):** destello de 50 ms cada segundo si todo va bien; encendido fijo si el último bloque tuvo VCC_A fuera de rango, VCC_A ≥ VDD o I/Q saturadas.

**Pulsadores:** cada pulsación (antirrebote de 30 ms) vale un bloque, como los botones *latch* del VI. Las demás órdenes van por UART.

**EEPROM:** I0, Q0, θ, G y las etapas activas se guardan al cambiar y se recuperan al encender (17 bytes con suma de control; un byte por trama, sin parar el muestreo). Se desactiva con `USAR_EEPROM 0`.

## Etapas que se pueden desactivar

Cada etapa se activa o desactiva en marcha con una tecla (por UART o en el programa del PC), con las casillas *Etapas activas* del programa del PC o, para el arranque, con su opción en `app_config.h`. El estado se guarda en la EEPROM y se informa en cada línea `B` y en la línea de configuración.

| Etapa | Tecla | Desactivada |
|---|---|---|
| Cero (P5–P6) | `Z` | no se resta I0, Q0: I′, Q′ salen de Ip, Qp; «Fijar cero» y «Borrar 0» se ignoran |
| θ (P7–P8) | `T` | no se rota: I′ = Ic, Q′ = Qc; «Calibrar θ» y «θ = 0» se ignoran |
| Ganancia (P2) | `G` | no se divide por G: se ve la tensión a la salida de la tarjeta (d estimada deja de ser válida) |
| Normalización a VCC_A (P1–P2) | `N` | no se escala a 3.7 V: voltios absolutos medidos con el FVR (con `USAR_FVR 0` no se puede desactivar) |
| Autocero del ADC | `A` | no se resta el offset medido en AVSS (el ADC sigue convirtiendo AVSS para poder reactivarlo) |

Desactivar una etapa no borra lo guardado (I0, Q0, θ, G): al volver a activarla se aplica otra vez. Las órdenes de una etapa desactivada (por UART o con los pulsadores) se contestan con `-> ignorada (etapa desactivada)`.

## Protocolo UART (115200 8N1)

Líneas del PIC (texto, CR LF):

```
t_ms,Ip_uV,Qp_uV,I1_uV,Q1_uV                                          100 por segundo
B,V_uV,fase_cdeg,d_milesimas,I0_uV,Q0_uV,theta_cdeg,vcc_mV,G,banderas,etapas   10 por segundo
P,I1_uV,Q1_uV                                                         volcado de la traza (orden p)
M,c1,c2,...,c10                                                       captura de códigos del ADC (orden m)
# ...                                                                 respuestas y comentarios
```

Los seis primeros campos de la línea `B` son los de la sección 10.7; se añaden `vcc_mV`, `G`, `banderas` (1 = VCC_A fuera de rango, se usa 3.7 V; 2 = I saturada; 4 = Q saturada; 8 = VCC_A ≥ VDD; 16 = sin lectura del FVR; 32 = se descartaron tramas: la cola estaba llena; 64 = el reloj externo de 4 MHz no está y se usa el HFINTOSC de reserva) y `etapas` (1 = cero, 2 = θ, 4 = ganancia, 8 = normalización, 16 = autocero; 31 = todas activas). La configuración se informa con líneas `# G=2 cero=ON theta=ON ganancia=ON norm=ON autocero=ON I0_uV=… Q0_uV=… theta_cdeg=…`.

La orden `m` copia 200 conversiones seguidas de I_OUT, las mismas de las rondas (una por ms: 200 ms a 1 kS/s), y las envía como 20 líneas `M,…` entre `# m: captura de 200 conversiones de I_OUT, una cada 1000 us` y `# fin captura`. Las tramas y las muestras siguen sin hueco durante la captura. La traza del PIC se vacía y no crece mientras dura, porque la captura usa su memoria.

Órdenes de un carácter (también desde cualquier terminal serie):

| Orden | Acción | Orden | Acción |
|---|---|---|---|
| `z` | Fijar cero | `2` / `1` / `0` | ganancia ×2 / ×11 / ×101 |
| `x` | Borrar 0 | `Z` | activar / desactivar la etapa de cero |
| `c` | Calibrar θ | `T` | activar / desactivar la etapa de θ |
| `t` | θ = 0 | `G` | activar / desactivar la ganancia |
| `b` | Borrar traza | `N` | activar / desactivar la normalización a VCC_A |
| `p` | volcar la traza del PIC | `A` | activar / desactivar el autocero del ADC |
| `m` | captura para medir el jitter | `?` | estado (configuración, puntos, muestras perdidas, offset del ADC) y ayuda |

## Programa del PC (`pc/plano_polar.py`)

Dibuja el plano polar (traza de los últimos `--puntos` puntos, 2000 por defecto y hasta 12000 como en la medida real, unidos en orden de llegada y con autoescala: el anillo exterior vale el r máximo), las gráficas "I y Q (mV)" e "I y Q procesadas (mV)" de los últimos 10 s y los indicadores. Los botones, las casillas *Etapas activas* y las teclas (`z x c t b 2 1 0 Z T G N A m`) envían las órdenes (con `m` imprime la media y la desviación de la captura); `g` o el botón *Grabar* guardan un CSV (`t_ms, Ip, Qp, I1, Q1` en mV, y |V|, Fase y d en la fila que cierra cada bloque), equivalente a *Write To Measurement File*.

```
python pc/plano_polar.py --puerto COM5 --puntos 12000 --csv medida.csv --comentario "dc ganancia x2"
python pc/plano_polar.py --archivo captura.txt     # reproduce una captura del puerto serie
python pc/plano_polar.py --demo                    # sin hardware
```

## Medida del jitter (`pc/medir_jitter.py`)

Con un generador senoidal en RC0 (I_OUT desconectado, offset VDD/2, 3.4 Vpp) se hacen dos series de capturas, una a 230 Hz y otra a 51 230 Hz (que a 1 kS/s se ve en 230 Hz por submuestreo). El jitter solo degrada la segunda, y de la diferencia de ruido sale σ:

```
python pc/medir_jitter.py capturar --puerto COM5 --capturas 10 -o baja.txt     # generador a 230 Hz
python pc/medir_jitter.py capturar --puerto COM5 --capturas 10 -o alta.txt     # generador a 51 230 Hz
python pc/medir_jitter.py jitter baja.txt alta.txt --fin-baja 230 --fin-alta 51230
python pc/medir_jitter.py analizar baja.txt --fin 230 --png espectro.png       # SNR, THD, SINAD, ENOB
python pc/medir_jitter.py simular --sigma 10e-9                                # sin hardware
python pc/medir_jitter.py tabla                                                # SNR del jitter frente a f
```

El otro método, con un osciloscopio en RA1, y los detalles están en [`docs/analisis_adc_jitter.md`](docs/analisis_adc_jitter.md).

## Opciones (`app/app_config.h`)

| Opción | Por defecto | Efecto |
|---|---|---|
| `GANANCIA_INICIAL` | 2 | ganancia DC si la EEPROM está vacía (debe coincidir con los jumpers) |
| `CERO_ACTIVO` | 1 | etapa de cero activa al arrancar sin datos en la EEPROM |
| `THETA_ACTIVO` | 1 | etapa de θ activa al arrancar |
| `GANANCIA_ACTIVA` | 1 | división por G activa al arrancar |
| `NORMALIZACION_ACTIVA` | 1 | normalización a VCC_A activa al arrancar |
| `AUTOCERO_ACTIVO` | 1 | resta del offset del ADC medido en AVSS activa al arrancar |
| `RELOJ` | 0 | 0 = oscilador interno (32 MHz); 1 = cristal de 4 MHz en RA5/RA4; 2 = oscilador de cuarzo de 4 MHz en RA5. Con 1 y 2, FOSC = 16 MHz (PLL ×4) ([`docs/reloj_externo_4MHz.md`](docs/reloj_externo_4MHz.md)) |
| `CRISTAL_HS` | 0 | modo del cristal: 0 = XT (el de la hoja de datos para 4 MHz), 1 = HS |
| `SALIDA_DISPARO` | 1 (0 con el cristal) | saca el PWM6 (instante de cada conversión) por RA1 en las compilaciones de producción |
| `USAR_FVR` | 1 | mide el FVR para conocer VCC_A en voltios (protección de 2.5 V) |
| `DIVISOR_VCCA` | 1 | 2 si se monta un divisor 1:2 en la entrada de VCC_A |
| `N_TRAZA` | 64 | puntos de la traza en el PIC (0 = sin traza; 512 B de RAM con 64) |
| `USAR_EEPROM` | 1 | guardar la calibración y la configuración |

También se pueden definir en *Project Properties > XC8 Compiler > Preprocessing > Define macros*.

## Recursos y tiempos (estimación)

- **RAM:** unos 1000 de 2048 bytes (traza 512 B, que comparte memoria con la captura `m`; buffer de transmisión 128 B; cola de tramas 64 B; el resto estado y pila compilada).
- **ADC:** cada disparo son 8 µs de adquisición + ~14.3 µs de conversión; la interrupción ocupa del orden del 10 % de la CPU (el doble con el reloj externo de 4 MHz, a 16 MHz).
- **Tiempo por trama de 10 ms:** ~2–3 ms (~4–6 ms a 16 MHz, con el reloj externo) de cálculo y formato en coma flotante por software; el cierre de bloque añade `sqrtf` y `atan2f`, y "Calibrar θ" además `sinf` y `cosf`. El muestreo no depende de `main()`: si el lazo se retrasa, las tramas esperan en la cola (hasta 4, 40 ms); solo si va más atrasado se descartan, lo que el PC muestra como aviso y la orden `?` como "muestras perdidas".
- **UART:** ~45 caracteres por muestra y ~65 por bloque: la mitad de los 115200 baudios.

## Decisiones abiertas de la sección 12

| Decisión | Elegido |
|---|---|
| Salida | UART al PC (opción A) con el programa en Python |
| Entradas | dos pulsadores (Fijar cero, Calibrar θ); el resto por UART |
| Ganancia | por orden UART, guardada en la EEPROM |
| Alimentación | desde VCC de la tarjeta (hardware) |
| Formato numérico | `float` de 32 bits (versión 1) |
| Programador | cualquiera; también funciona en el simulador de MPLAB X |
| Muestreo de las entradas | 1000 muestras/s por canal, como el VI (rondas de 1 ms; la sección 10.4 proponía 16 conversiones por canal y trama) |
| Frecuencia de salida | 100 muestras/s (suma de 10 conversiones por canal), bloques de 10 |

## Pruebas en el PC

```
cd tests
make          # todo
make test     # procesamiento con los vectores de la sección 9.1 (gcc)
make sim      # firmware completo con hardware simulado y comprobación de su salida
make reloj    # opción de reloj externo de 4 MHz: registros de cada modo y firmware simulado
make adc1k    # avance ADC 1 kS/s: firmware simulado y comprobación de lo que envía
make pc       # programas del PC: intérprete de líneas, CSV, análisis del jitter, adc_1ksps y prueba_usb6211 (numpy)
```

`make sim` deja la salida del firmware simulado en `tests/build/sim/arranque1.txt`; se puede ver con `python pc/plano_polar.py --archivo tests/build/sim/arranque1.txt`.

## Siguientes pasos posibles

- Opción B de la sección 10.7: pantalla OLED SSD1306 por I2C (MSSP) a partir de `traza.c`.
- Versión 2 en punto fijo (sección 10.5) si hiciera falta más margen de tiempo.
- SNR DC con sumas acumuladas (Welford) en lugar del SINAD (sección 8).
