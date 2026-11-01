# Reloj externo de 4 MHz (opción `RELOJ`)

El firmware completo (`P2_IQ_PIC16F18426.X`, versión 1.4) puede funcionar con el **oscilador interno** del PIC, como hasta ahora, o con un **reloj de cuarzo externo de 4 MHz**, que se conectará más adelante. Es una opción de compilación con tres valores:

| `RELOJ` | Reloj | FOSC | Hardware |
|---|---|---|---|
| `0` = `RELOJ_INTERNO` (por defecto) | HFINTOSC del PIC | 32 MHz | el actual |
| `1` = `RELOJ_CRISTAL_4MHZ` | cristal de cuarzo de 4 MHz (2 patas) entre RA5 y RA4, con dos condensadores | 4 MHz × PLL 4 = **16 MHz** | ocupa RA4 y RA5: el LED pasa a RA0 y el pulsador "Calibrar θ" a RA1 |
| `2` = `RELOJ_OSCILADOR_4MHZ` | oscilador de cuarzo de 4 MHz con salida CMOS (módulo de 4 patas) a RA5 | 4 MHz × PLL 4 = **16 MHz** | ocupa RA5: el LED pasa a RA0 |

Con los tres, el ADC muestrea igual: disparo del PWM6 cada 100 µs, cada entrada a **1000 muestras/s**, TAD de 1 µs y UART a 115200 baudios. Solo cambian los registros que dependen de FOSC.

## 1. Cómo se elige

Cualquiera de estas tres formas:

1. **MPLAB X:** en la lista de configuraciones de la barra de herramientas, elegir **`default`** (interno), **`cristal_4MHz`** o **`oscilador_4MHz`**. Las dos últimas solo añaden `RELOJ=1` o `RELOJ=2` en *XC8 Compiler › Preprocessing › Define macros*.
2. A mano en *Project Properties › XC8 Compiler › Define macros*: `RELOJ=1`.
3. En `app/app_config.h`: `#define RELOJ RELOJ_CRISTAL_4MHZ`.

No se puede cambiar en marcha (por ejemplo, con una orden por la UART). El tipo de oscilador externo (HS, XT o EC) lo fijan los **bits de configuración**, que se graban con el programa, y con el cristal RA4 y RA5 dejan de ser pines de E/S. Por eso el pulsador y el LED tienen que estar en otro sitio.

Opciones relacionadas:

| Opción | Por defecto | Efecto |
|---|---|---|
| `CRISTAL_HS` | 0 | 0 = modo **XT** para el cristal (ganancia media). Es el que la hoja de datos indica para 4 MHz: sección 8.2.1.2 y parámetro OS8, FXT ≤ 4 MHz. 1 = modo **HS** (ganancia alta, hasta 20 MHz, OS9), por si un cristal de ESR alta no arranca en XT |
| `SALIDA_DISPARO` | 1 (0 con `RELOJ_CRISTAL_4MHZ`) | Con el cristal, RA1 es el pulsador "Calibrar θ". `SALIDA_DISPARO=1` saca por RA1 el disparo del PWM6 (osciloscopio, contador de la USB-6211) y deja el pulsador sin pin: queda la orden `c` |

## 2. Por qué 16 MHz y no 4 MHz directos

Con 4 MHz sin PLL (1 MIPS):

- **UART:** 115200 baudios no salen con precisión. El más cercano tiene un error del −3.5 %, demasiado para la UART. Habría que bajar a 38400, y entonces no caben las ~5800 letras/s que envía el firmware.
- **CPU:** la interrupción del ADC (10 000 por segundo) y el cálculo en coma flotante de cada trama de 10 ms no caben.

El **PLL ×4** del PIC admite entradas de 4 a 16 MHz (PLL01) y salidas de 16 a 32 MHz (PLL02). Con 4 MHz da 16 MHz (4 MIPS). Es la mitad que con el oscilador interno, así que **el margen de CPU se reduce** (sección 6).

## 3. Conexiones

### 3.1 Cristal (`RELOJ=1`)

```
            PIC16F18426
 RA5 / OSC1 (pin 2) ──┬────────┐
                      │      [XTAL 4 MHz]
 RA4 / OSC2 (pin 3) ──┼────────┤
                     C1        C2
                      │        │
                     VSS      VSS        C1 = C2 ≈ 2·(CL − 5 pF): 22–27 pF para CL = 16–18 pF

 RA0 (pin 13) ── 1 kΩ ── LED ── VSS       (antes en RA5)
 RA1 (pin 12) ── pulsador "Calibrar θ" ── VSS   (antes en RA4; pull-up interno)
 RA2 (pin 11) ── pulsador "Fijar cero" ── VSS   (sin cambios)
```

- Cristal y condensadores lo más cerca posible de los pines 2 y 3, con pistas cortas y el plano de masa debajo.
- CL es la capacidad de carga que indica la hoja del cristal. Si el cristal se calienta o el fabricante limita la potencia (*drive level*), poner una resistencia en serie (RS de la figura 8-3 de la hoja de datos, 0.5–1 kΩ) en OSC2. Si no arranca, probar `CRISTAL_HS=1`.

### 3.2 Oscilador de cuarzo (`RELOJ=2`)

```
 oscilador 4 MHz: salida ── RA5 / CLKIN (pin 2)      VDD y VSS del oscilador a los del PIC, con 100 nF
 RA0 (pin 13) ── 1 kΩ ── LED ── VSS                   (antes en RA5)
 RA4 (pin 3)  ── pulsador "Calibrar θ" (sin cambios)
```

El oscilador tiene que funcionar con la VDD del PIC (3.3–3.7 V) y dar una salida CMOS de 0 a VDD.

### 3.3 Pines de programación

RA0 (ICSPDAT) y RA1 (ICSPCLK) son también los pines del programador:

- **El LED en RA0** carga la línea de datos. Con 1 kΩ suele programar bien. Si da errores, desconectarlo al programar o subir la resistencia.
- **No pulsar el botón de RA1** mientras se programa.
- **En las compilaciones de depuración** (*Debug*), RA0 y RA1 son del depurador. Con reloj externo no hay LED y, con el cristal, tampoco el pulsador "Calibrar θ" (queda la orden `c`).

## 4. Arranque y reserva

1. El PIC arranca siempre con el **HFINTOSC** (bits de configuración: RSTOSC = HFINT32). FEXTOSC = XT/HS/ECM prepara el oscilador externo.
2. `OSCILLATOR_Initialize()` pone el HFINTOSC a **16 MHz** (OSCFRQ). Los demás módulos de MCC (TMR2, ADC, EUSART) ya están configurados para 16 MHz.
3. `reloj_inicializar()` (`app/reloj.c`) pide el cambio al externo con el PLL ×4 (OSCCON1 = 0x20). El HFINTOSC sigue funcionando hasta que el externo ha superado su temporizador de arranque (1024 ciclos) y el PLL está enganchado. Entonces el cambio es automático y COSC lo indica.
4. Si en **100 ms** no ha arrancado (cristal no montado o roto), se abandona el cambio (OSCCON1 = 0x60) y el firmware sigue con el **HFINTOSC a 16 MHz**. Como da la misma frecuencia, todo funciona igual, con la exactitud del HFINTOSC (±2 %).
5. Si el reloj externo se para después, el **monitor de fallos** (FSCM, FCMEN = ON) pasa el sistema al **HFINTOSC a 1 MHz** y activa OSFIF (hoja de datos, 8.4.2). A 1 MHz todo iría 16 veces más lento: disparos cada 1.6 ms y UART a ~7200 baudios. Por eso el fallo se atiende en la **interrupción OSFIF** (OSFIE se activa al arrancar con el externo): `reloj_fallo_isr()` pone OSCFRQ = 16 MHz y OSCCON1 = HFINTOSC 1:1 en pocos µs, lo que además despeja la condición de fallo (8.4.3). Solo se pierde el instante del disparo en curso. Después, `reloj_tarea()` avisa por la UART.

   Al arrancar, si el PLL no engancha, el FSCM también salta (8.3.2). La espera termina en cuanto aparece OSFIF y se pasa a la reserva igual que en el punto 4.

   El texto de 8.4.2 sitúa OSFIF en PIR3, pero el mapa de registros de la misma hoja de datos (y el `pic16f18426.h` oficial) lo ponen en **PIR1** (0x070D, bit 7), que es lo que usa el código.

El firmware lo comunica así:

| Situación | UART | Bloques `B` |
|---|---|---|
| Reloj interno | `# reloj: interno (HFINTOSC) a 32 MHz` (orden `?` y arranque) | — |
| Externo en marcha | `# reloj: cristal de 4 MHz (RA5/RA4) x PLL 4 = 16 MHz` (u `oscilador de 4 MHz (RA5)`) | — |
| No arrancó | `# reloj: FALLO del cristal de 4 MHz (RA5/RA4): HFINTOSC de reserva a 16 MHz (+-2 %)` | bandera **64** en cada bloque |
| Se paró en marcha | una vez: `# reloj: el cristal de 4 MHz (RA5/RA4) se ha parado; sigue el HFINTOSC a 16 MHz (+-2 %)`; luego `?` da la línea de FALLO | bandera **64** desde ese momento |

El programa del PC (`pc/plano_polar.py`) muestra la bandera 64 como aviso: "sin el reloj externo de 4 MHz: HFINTOSC de reserva".

## 5. Registros en cada modo

| Registro (archivo de MCC) | Interno (32 MHz) | Externo (16 MHz) | Resultado |
|---|---|---|---|
| FEXTOSC (`device_config.c`) | OFF | XT (HS con `CRISTAL_HS=1`) / ECM | tipo de oscilador externo (OS8: XT ≤ 4 MHz; OS3: ECM ≤ 4 MHz) |
| RSTOSC, CSWEN, FCMEN | HFINT32, ON, ON | igual | arranque con el HFINTOSC; cambio y monitor de fallos permitidos |
| OSCFRQ (`mcc.c`) | 0x06 (32 MHz) | 0x05 (16 MHz) | HFINTOSC: reloj o reserva |
| OSCCON1 (`app/reloj.c`) | 0x60 | 0x20 → externo ×4; 0x60 (y OSCFRQ = 0x05) si falla | |
| PIE1.OSFIE (`app/reloj.c`) e `interrupt_manager.c` | — | 1 con el externo en marcha; la interrupción llama a `reloj_fallo_isr()` | vuelta de 1 MHz a 16 MHz en µs |
| `_XTAL_FREQ` (`device_config.h`) | 32000000 | 16000000 | `__delay_us()` |
| T2CON (`tmr2.c`) | 0xA0 (1:4) | 0x90 (1:2) | T2PR = 199: **100 µs** en los dos |
| ADCLK (`adcc.c`) | 0x0F (FOSC/32) | 0x07 (FOSC/16) | **TAD = 1 µs**; 100 µs = 100 TAD exactos |
| SP1BRGL (`eusart1.c`) | 68 → 115942 (+0.6 %) | 34 → 114286 (−0.8 %) | **115200 baudios** |
| TRISA / ANSELA / WPUA (`pin_manager.c`) | 0x1F / 0x03 / 0x14 | cristal 0x3E / 0x00 / 0x06; oscilador 0x3E / 0x02 / 0x14 | RA5 (y RA4) nunca como salida; LED en RA0 |

El PWM6 (50 %) no cambia, porque su ciclo de trabajo es relativo a T2PR.

**Si se regenera con MCC Classic**, los archivos de `mcc_generated_files/` se sustituyen y se pierden los `#if RELOJ`. En ese caso:

- generar con los valores del modo que se vaya a usar, siguiendo la tabla anterior: en *System Module*, External Oscillator HS o EC y 16 MHz;
- o recuperar los archivos con `git checkout -- P2_IQ_PIC16F18426.X/mcc_generated_files`.

`make reloj` comprueba los valores de cada modo.

## 6. Qué falta comprobar en la placa

- **Margen de CPU a 16 MHz.** Con el oscilador interno se estimaron 2–3 ms de cálculo por trama de 10 ms y ~10 % de CPU en la interrupción del ADC. A 4 MIPS es el doble: ~4–6 ms y ~20 %, sin medir. El muestreo no depende del lazo, y la cola de 4 tramas absorbe los picos (cierre de bloque, "Calibrar θ"). Hay que compilar con XC8 y, en la placa, comprobar con `?` que **"muestras perdidas" sigue en 0**. Si no, la solución sería bajar la frecuencia de las interrupciones (por ejemplo, rondas de 8 disparos de 125 µs: siguen siendo 1 ms por canal).
- **Arranque del cristal en XT** (y en HS si no arranca) con los condensadores elegidos. Con `?` debe aparecer `x PLL 4 = 16 MHz`, no `FALLO`.
- **Frecuencia real:** con el cristal, la frecuencia de muestreo será 1000 S/s con la exactitud del cristal (±50 ppm típicos) en vez del ±2 % del HFINTOSC. La frecuencia media es la del cristal; el PLL añade su inestabilidad a corto plazo (PLL04, ±0.25 %), así que el jitter no es necesariamente el de picosegundos del cristal y conviene medirlo. `pc/prueba_usb6211.py` lo mide; si se usa el contador en RA1 con el cristal, hace falta `SALIDA_DISPARO=1`. Su jitter debería ser de picosegundos (`docs/analisis_adc_jitter.md`, sección 5.1).

## 7. Verificación hecha (sin XC8 ni placa)

| Qué | Resultado |
|---|---|
| Todos los `.c` contra el `pic16f18426.h` oficial con gcc y `-Wundef`, en 12 combinaciones (RELOJ 0/1/2 × producción, depuración, `SALIDA_DISPARO=1`, `CRISTAL_HS=1`) | sin errores ni avisos |
| `make reloj`, registros: FOSC, TMR2 (100 µs), PWM6 (50 %), TAD (1 µs, 100 TAD por periodo), baudios (±1 %), bits de configuración y secuencia de `reloj.c` e interrupción OSFIF, en interno, cristal XT/HS y oscilador | todo correcto |
| `make reloj`, pines: puerto A en los 9 casos (3 modos × producción, depuración, `SALIDA_DISPARO=1`) | RA5/RA4 nunca como salida; LED y pulsadores donde deben |
| `make reloj`, firmware simulado con cristal y con oscilador: reloj bien, ausente y parándose a los 0.7 s (el FSCM lo deja a 1 MHz, como dice 8.4.2) | mensajes, bandera 64 y base de tiempo sin huecos; se vuelve a 16 MHz antes del disparo siguiente. Tres fallos provocados en `reloj.c` (no volver al HFINTOSC tras el fallo, no abandonar el cambio y no activar la interrupción OSFIF) los detecta el simulador |
| Valores contra la hoja de datos DS40001985C | ver sección 8 |
| `make sim` (reloj interno) | sin cambios: 36/36 más la línea `# reloj: interno` |

## 8. Comprobado con la hoja de datos (DS40001985C)

| Dato que usa el código | Hoja de datos | |
|---|---|---|
| RA5 = CLKIN/OSC1 (pin 2), RA4 = CLKOUT/OSC2 (pin 3) | tabla de asignación de pines (14 pines) | ✔ |
| XT para un cristal de 4 MHz | 8.2.1.2 ("XT … up to 4 MHz"); OS8: FXT ≤ 4 MHz; OS9: FHS ≤ 20 MHz | ✔ (antes se usaba HS por defecto: corregido) |
| ECM para un oscilador de 4 MHz | OS3: FECM ≤ 4 MHz (justo en el límite; ECH, ≤ 32 MHz, también valdría) | ✔ |
| PLL ×4 con 4 MHz | PLL01: entrada 4–16 MHz; PLL02: salida 16–32 MHz; PLL03: enganche en ~200 µs | ✔ |
| OSCCON1: NOSC 010 = EXTOSC + 4x PLL, 110 = HFINTOSC; NDIV 0000 = 1:1 | tablas 8-1 y 8-2 | ✔ |
| OSCFRQ: 101 = 16 MHz, 110 = 32 MHz | registro OSCFRQ | ✔ |
| Cambio de reloj: el anterior sigue hasta que el nuevo está listo; COSC se actualiza al cambiar | 8.3.1 | ✔ |
| FSCM: pasa al HFINTOSC **a 1 MHz** y activa OSFIF; se despeja cambiando NOSC/NDIV | 8.4.2 y 8.4.3 | ✔ con el cambio a interrupción (antes se suponía la frecuencia de OSCFRQ y se atendía cada 10 ms: corregido) |
| OSFIF / OSFIE en PIR1 / PIE1, bit 7 | mapa de registros (0x070D / 0x0717) | ✔ (el texto de 8.4.2 dice PIR3: errata) |
| OST de 1024 ciclos con cristal, sin OST en EC | 8.2.1.3 y 8.4.4 | ✔ (la espera de 100 ms sobra de largo) |
| TMR2: CKPS 001 = 1:2, 010 = 1:4; T2CLKCON 0001 = FOSC/4 | registros T2CON y T2CLKCON | ✔ |
| ADCLK: FOSC/(2·(n+1)) → 0x07 = FOSC/16, 0x0F = FOSC/32; TAD 0.5–9 µs | tabla 32-1; AD20 | ✔ |
| EUSART: BRG16 = BRGH = 1 → FOSC/[4(n+1)] | tabla de fórmulas de la EUSART | ✔ |
| Tensión: FOSC ≤ 16 MHz desde 1.8–2.3 V; > 16 MHz desde 2.5 V | D002, figuras 39-1 y 39-2 | ✔ con VDD de 3.3–3.7 V |
