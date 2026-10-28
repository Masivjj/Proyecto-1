# Detector de fisuras por corrientes de Eddy: procesamiento I/Q en un PIC16F18426

Firmware para **PIC16F18426** (MPLAB X + XC8 + **MCC Classic**) que lee las dos componentes DC **I** y **Q** de la tarjeta demoduladora P2 y reproduce el procesamiento del VI de LabVIEW `P2_IQ_SINAD.vi` para detectar fisuras en tubo de aluminio con una sonda de corrientes de Eddy diferencial:

- normalización ratiométrica a VCC_A = 3.7 V, división por la ganancia DC (×2, ×11, ×101) y corrección (fija) del signo invertido de I;
- cero (I0, Q0), ángulo de calibración θ (lift-off) y rotación → señales finales I′, Q′;
- las etapas de cero, θ, ganancia y normalización a VCC_A se activan y desactivan en marcha con una tecla;
- indicadores por bloque de 0.1 s: |V|, Fase y d estimada (×10⁻⁴);
- traza del plano polar, envío por UART al PC y un programa en Python que dibuja el plano polar como el VI.

La especificación de partida está en [`docs/P2_IQ_procesamiento_para_PIC16F18426.md`](docs/P2_IQ_procesamiento_para_PIC16F18426.md); el código cita sus secciones (P1…P10, 10.4, 10.6…).

## Estado de la verificación

| Qué | Cómo | Resultado |
|---|---|---|
| Procesamiento (P1–P10) | `tests/test_iq_proc.c`: el mismo `iq_proc.c` del PIC compilado con gcc, con los vectores de la sección 9.1, las etapas activables y casos extra | 97/97 correctas |
| Firmware completo (`main.c` y `app/`) | `tests/sim`: se compila sin cambios contra un hardware simulado (ADC con una fisura sintética, UART, EEPROM, botones, retrasos del lazo, etapas desactivadas) | 27/27 comprobaciones |
| Nombres de registros y bits | todos los `.c` contra el `pic16f18426.h` oficial (DFP 1.32.471) con gcc; las 146 macros de pines; los `#pragma config` contra el `cfgdata` del dispositivo | sin errores |
| Programa del PC | `tests/test_plano_polar.py` y prueba sin ventana con `--demo` y `--archivo` | correcto |
| **Compilación con XC8** | **no se ha podido ejecutar XC8 en el entorno donde se escribió** | **pendiente: compilar en MPLAB X** |
| Hardware real | — | pendiente |

Al compilar por primera vez revisa el *Memory Summary* de la salida (la estimación es ~50 % de RAM y menos de la mitad de la Flash) y comprueba en el PC que `muestras perdidas` sigue en 0 (orden `?`).

## Contenido

```
P2_IQ_PIC16F18426.X/            proyecto de MPLAB X (abrir esta carpeta)
├── main.c                      lazo principal: tick de 10 ms, bloques, LED
├── app/
│   ├── app_config.h            constantes de la especificación y opciones
│   ├── iq_proc.c/.h            procesamiento P1-P10 (sin dependencias del hardware)
│   ├── adquisicion.c/.h        ADCC en Burst Average: I_OUT, Q_OUT, V_REF, VCC_A, FVR
│   ├── comunicacion.c/.h       protocolo UART: muestras, bloques y órdenes
│   ├── traza.c/.h              traza del plano polar (P9), buffer circular
│   ├── ajustes.c/.h            I0, Q0, θ, G y etapas en EEPROM (escritura sin bloqueo)
│   ├── botones.c/.h            pulsadores con antirrebote
│   └── formato.c/.h            números a texto sin printf
├── mcc_generated_files/        drivers de MCC Classic (ver docs/configuracion_MCC_Classic.md)
└── nbproject/, Makefile        archivos del proyecto de MPLAB X
pc/plano_polar.py               plano polar, gráficas, botones y grabación CSV en el PC
tests/                          pruebas en el PC (make)
docs/                           especificación y guía de MCC Classic
```

## Puesta en marcha

1. **Abrir** `P2_IQ_PIC16F18426.X` en MPLAB X (*File > Open Project*). Si no tienes XC8 v2.50, elige el que tengas (v2.40 o posterior) en *Project Properties > Compiler Toolchain*.
2. En *Project Properties > XC8 Global Options* comprueba **C99** y **float y double de 32 bits** (si no, la compilación se detiene con un error en `iq_proc.c`).
3. Elige la herramienta (PICkit 4/5, MPLAB Snap…) y pulsa *Make and Program Device*.
4. En `app/app_config.h`, `GANANCIA_INICIAL` debe coincidir con los jumpers JP701/JP702 (o cámbiala luego con las órdenes `2`, `1`, `0`; queda guardada en la EEPROM).
5. En el PC: `pip install -r pc/requirements.txt` y `python pc/plano_polar.py --puerto COM5` (Windows) o `--puerto /dev/ttyUSB0` (Linux). `--listar` muestra los puertos.
6. Calibración, como en el VI:
   - sonda quieta sobre metal sano → **Fijar cero** (pulsador RA2, tecla `z` o botón);
   - sonda quieta sobre papel (lift-off conocido) → **Calibrar θ** (pulsador RA4, tecla `c` o botón). Desde entonces el lift-off cae sobre 0° y una fisura se aparta de 0°/180°.

Para probar el programa del PC sin hardware: `python pc/plano_polar.py --demo`.

La configuración de los periféricos está documentada paso a paso en [`docs/configuracion_MCC_Classic.md`](docs/configuracion_MCC_Classic.md), por si quieres regenerar con MCC.

## Conexiones

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
| 3 RA4 | pulsador "Calibrar θ" | a GND (pull-up interno) |
| 2 RA5 | LED | LED + resistencia de 1 kΩ a GND |
| 4 RA3 / MCLR | reset | 10 kΩ a VDD; al programador |
| 13 RA0, 12 RA1 | ICSPDAT, ICSPCLK | al programador |

- **VCC_A no debe superar VDD del PIC**: si lo hace, el ADC satura en ese canal (el PC muestra el aviso "VCC_A ≥ VDD del PIC"). Si puede pasar, monta un divisor 1:2 (2 × 10 kΩ) en la entrada de VCC_A y pon `DIVISOR_VCCA 2` en `app_config.h`.
- El adaptador USB-serie debe trabajar con niveles de 3.3 V a 5 V y compartir la tierra.
- Opcional: un filtro RC (1 kΩ + 100 nF) cerca de cada entrada analógica si el cable es largo.

## Funcionamiento

Cada 10 ms (TMR2) el PIC convierte los cinco canales con el ADCC en modo *Burst Average* (16 conversiones por canal, suma en ADFLTR, ~1.8 ms en total) y procesa la muestra. Cada 10 muestras (0.1 s, como el bloque de 100 muestras de LabVIEW) cierra un bloque.

| Etapa del VI | Dónde |
|---|---|
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

**EEPROM:** I0, Q0, θ, G y las etapas activas se guardan al cambiar y se recuperan al encender (17 bytes con suma de control; un byte por tick, sin parar el muestreo). Se desactiva con `USAR_EEPROM 0`.

## Etapas que se pueden desactivar

Cada etapa se activa o desactiva en marcha con una tecla (por UART o en el programa del PC), con las casillas *Etapas activas* del programa del PC o, para el arranque, con su opción en `app_config.h`. El estado se guarda en la EEPROM y se informa en cada línea `B` y en la línea de configuración.

| Etapa | Tecla | Desactivada |
|---|---|---|
| Cero (P5–P6) | `Z` | no se resta I0, Q0: I′, Q′ salen de Ip, Qp; «Fijar cero» y «Borrar 0» se ignoran |
| θ (P7–P8) | `T` | no se rota: I′ = Ic, Q′ = Qc; «Calibrar θ» y «θ = 0» se ignoran |
| Ganancia (P2) | `G` | no se divide por G: se ve la tensión a la salida de la tarjeta (d estimada deja de ser válida) |
| Normalización a VCC_A (P1–P2) | `N` | no se escala a 3.7 V: voltios absolutos medidos con el FVR (con `USAR_FVR 0` no se puede desactivar) |

Desactivar una etapa no borra lo guardado (I0, Q0, θ, G): al volver a activarla se aplica otra vez. Las órdenes de una etapa desactivada (por UART o con los pulsadores) se contestan con `-> ignorada (etapa desactivada)`.

## Protocolo UART (115200 8N1)

Líneas del PIC (texto, CR LF):

```
t_ms,Ip_uV,Qp_uV,I1_uV,Q1_uV                                          100 por segundo
B,V_uV,fase_cdeg,d_milesimas,I0_uV,Q0_uV,theta_cdeg,vcc_mV,G,banderas,etapas   10 por segundo
P,I1_uV,Q1_uV                                                         volcado de la traza (orden p)
# ...                                                                 respuestas y comentarios
```

Los seis primeros campos de la línea `B` son los de la sección 10.7; se añaden `vcc_mV`, `G`, `banderas` (1 = VCC_A fuera de rango, se usa 3.7 V; 2 = I saturada; 4 = Q saturada; 8 = VCC_A ≥ VDD; 16 = sin lectura del FVR; 32 = se descartaron muestras por retraso) y `etapas` (1 = cero, 2 = θ, 4 = ganancia, 8 = normalización; 15 = todas activas). La configuración se informa con líneas `# G=2 cero=ON theta=ON ganancia=ON norm=ON I0_uV=… Q0_uV=… theta_cdeg=…`.

Órdenes de un carácter (también desde cualquier terminal serie):

| Orden | Acción | Orden | Acción |
|---|---|---|---|
| `z` | Fijar cero | `2` / `1` / `0` | ganancia ×2 / ×11 / ×101 |
| `x` | Borrar 0 | `Z` | activar / desactivar la etapa de cero |
| `c` | Calibrar θ | `T` | activar / desactivar la etapa de θ |
| `t` | θ = 0 | `G` | activar / desactivar la ganancia |
| `b` | Borrar traza | `N` | activar / desactivar la normalización a VCC_A |
| `p` | volcar la traza del PIC | `?` | estado (configuración, puntos, muestras perdidas) y ayuda |

## Programa del PC (`pc/plano_polar.py`)

Dibuja el plano polar (traza de los últimos `--puntos` puntos, 2000 por defecto y hasta 12000 como en la medida real, unidos en orden de llegada y con autoescala: el anillo exterior vale el r máximo), las gráficas "I y Q (mV)" e "I y Q procesadas (mV)" de los últimos 10 s y los indicadores. Los botones, las casillas *Etapas activas* y las teclas (`z x c t b 2 1 0 Z T G N`) envían las órdenes; `g` o el botón *Grabar* guardan un CSV (`t_ms, Ip, Qp, I1, Q1` en mV, y |V|, Fase y d en la fila que cierra cada bloque), equivalente a *Write To Measurement File*.

```
python pc/plano_polar.py --puerto COM5 --puntos 12000 --csv medida.csv --comentario "dc ganancia x2"
python pc/plano_polar.py --archivo captura.txt     # reproduce una captura del puerto serie
python pc/plano_polar.py --demo                    # sin hardware
```

## Opciones (`app/app_config.h`)

| Opción | Por defecto | Efecto |
|---|---|---|
| `GANANCIA_INICIAL` | 2 | ganancia DC si la EEPROM está vacía (debe coincidir con los jumpers) |
| `CERO_ACTIVO` | 1 | etapa de cero activa al arrancar sin datos en la EEPROM |
| `THETA_ACTIVO` | 1 | etapa de θ activa al arrancar |
| `GANANCIA_ACTIVA` | 1 | división por G activa al arrancar |
| `NORMALIZACION_ACTIVA` | 1 | normalización a VCC_A activa al arrancar |
| `USAR_FVR` | 1 | mide el FVR para conocer VCC_A en voltios (protección de 2.5 V) |
| `DIVISOR_VCCA` | 1 | 2 si se monta un divisor 1:2 en la entrada de VCC_A |
| `N_TRAZA` | 64 | puntos de la traza en el PIC (0 = sin traza; 512 B de RAM con 64) |
| `USAR_EEPROM` | 1 | guardar la calibración y la configuración |

También se pueden definir en *Project Properties > XC8 Compiler > Preprocessing > Define macros*.

## Recursos y tiempos (estimación)

- **RAM:** unos 1000 de 2048 bytes (traza 512 B, buffer de transmisión 128 B, el resto estado y pila compilada).
- **Tiempo por tick de 10 ms:** ~1.8 ms de ADC + ~2–3 ms de cálculo y formato en coma flotante por software; el cierre de bloque añade `sqrtf` y `atan2f`, y "Calibrar θ" además `sinf` y `cosf`. Si un tick no se atiende a tiempo se atiende justo después (no se pierde la muestra); solo si el lazo va más de 100 ms por detrás se descartan muestras, lo que el PC muestra como aviso y la orden `?` como "muestras perdidas".
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
| Frecuencia de salida | 100 muestras/s, bloques de 10 |

## Pruebas en el PC

```
cd tests
make          # todo
make test     # procesamiento con los vectores de la sección 9.1 (gcc)
make sim      # firmware completo con hardware simulado y comprobación de su salida
make pc       # intérprete de líneas y CSV del programa del PC
```

`make sim` deja la salida del firmware simulado en `tests/build/sim/arranque1.txt`; se puede ver con `python pc/plano_polar.py --archivo tests/build/sim/arranque1.txt`.

## Siguientes pasos posibles

- Opción B de la sección 10.7: pantalla OLED SSD1306 por I2C (MSSP) a partir de `traza.c`.
- Versión 2 en punto fijo (sección 10.5) si hiciera falta más margen de tiempo.
- SNR DC con sumas acumuladas (Welford) en lugar del SINAD (sección 8).
