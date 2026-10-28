# Configuración de MCC Classic para el PIC16F18426

El proyecto ya trae en `P2_IQ_PIC16F18426.X/mcc_generated_files/` el código equivalente al que genera MCC Classic con la configuración de esta guía, así que **compila sin abrir MCC**. Esta guía sirve para:

- reproducir la configuración en MCC Classic (por ejemplo, para cambiar algo desde el entorno gráfico y volver a generar), o
- crear el proyecto desde cero.

Si vuelves a generar con MCC, los archivos de `mcc_generated_files/` se sustituyen. La aplicación (`main.c` y la carpeta `app/`) solo depende de las funciones de MCC y de los nombres de pin `LED`, `BTN_CERO` y `BTN_THETA`. Los canales del ADC se seleccionan por número, así que no dependen de los nombres que les des.

> En MPLAB X 6.x, al pulsar el botón de MCC por primera vez en un proyecto se pregunta qué contenido usar: elige **MCC Classic** (no Melody). Las librerías son las de *PIC10 / PIC12 / PIC16 / PIC18 MCUs*.

---

## 1. Proyecto

| Opción | Valor |
|---|---|
| Dispositivo | PIC16F18426 |
| Compilador | XC8 v2.40 o posterior (el proyecto pide la v2.50; si tienes otra, elígela en Project Properties) |
| Project Properties > XC8 Global Options > C standard | **C99** |
| Project Properties > XC8 Global Options > Size of float / Size of double | **32 bit** (obligatorio: el código lo comprueba al compilar) |
| Project Properties > XC8 Compiler > Optimizations | Nivel 2 (disponible en la versión gratuita) |
| Herramienta | Simulator, PICkit 4/5, MPLAB Snap o ICD |

---

## 2. System Module

**Easy Setup**

| Campo | Valor |
|---|---|
| Oscillator Select | HFINTOSC |
| HF Internal Clock | 32_MHz |
| Clock Divider | 1 |
| Resultado | Current System clock: 32 MHz (8 MIPS) |
| Low-voltage programming enable | ✔ |

**Registers / Configuration Bits** (los demás, por defecto)

| Palabra | Ajustes |
|---|---|
| CONFIG1 | FEXTOSC = OFF, RSTOSC = HFINT32, CLKOUTEN = OFF, CSWEN = ON, FCMEN = ON |
| CONFIG2 | MCLRE = ON, PWRTS = OFF, LPBOREN = OFF, BOREN = ON, BORV = LO (2.45 V), ZCD = OFF, PPS1WAY = ON, STVREN = ON |
| CONFIG3 | WDTCPS = WDTCPS_31, WDTE = OFF, WDTCWS = WDTCWS_7, WDTCCS = SC |
| CONFIG4 | BBSIZE = BB512, BBEN/SAFEN/WRTAPP/WRTB/WRTC/WRTD/WRTSAF = OFF, LVP = ON |
| CONFIG5 | CP = OFF |

---

## 3. Pin Module

En *Pin Manager: Grid View* marca los pines y, en *Pin Module*, pon el nombre (**Custom Name**) y las opciones:

| Pin | Puerto | Módulo / función | Custom Name | Dirección | Analog | WPU |
|---|---|---|---|---|---|---|
| 10 | RC0 | ADCC → ANC0 | `I_OUT` | in | ✔ | |
| 9 | RC1 | ADCC → ANC1 | `Q_OUT` | in | ✔ | |
| 8 | RC2 | ADCC → ANC2 | `V_REF` | in | ✔ | |
| 7 | RC3 | ADCC → ANC3 | `VCC_A` | in | ✔ | |
| 6 | RC4 | EUSART1 → TX1 | | out | | |
| 5 | RC5 | EUSART1 → RX1 | | in | | |
| 11 | RA2 | Pin Module → GPIO input | `BTN_CERO` | in | | ✔ |
| 3 | RA4 | Pin Module → GPIO input | `BTN_THETA` | in | | ✔ |
| 2 | RA5 | Pin Module → GPIO output | `LED` | out | | |

RA0 (ICSPDAT) queda sin usar, en analógico. RA3 es MCLR (pull-up externo de 10 kΩ).

RA1 (ICSPCLK) **no se asigna en MCC**: en las compilaciones de producción `app/adquisicion.c` saca por él la salida del PWM6 (`RA1PPS = 0x0D`, digital, salida, sin limitar la pendiente) para medir el jitter del disparo con un osciloscopio. En las de depuración (`__DEBUG`) o con `SALIDA_DISPARO = 0` queda libre para el depurador. Ver `docs/analisis_adc_jitter.md`.

---

## 4. ADCC (ADC with Computation)

Modo básico (sección 32.2.7 de la hoja de datos): una conversión por cada flanco de subida del PWM6, sin intervención del software. El porqué de cada valor está en `docs/analisis_adc_jitter.md`.

| Campo | Valor | Por qué |
|---|---|---|
| Enable ADC | ✔ | |
| Operating Mode | **Basic_mode** | una conversión por disparo (MD = 000) |
| Result Alignment | right | |
| Positive Reference | VDD | normalización ratiométrica: VDD se cancela (sección 10.4) |
| Negative Reference | VSS | |
| Auto-conversion Trigger | **PWM6_OUT** | el instante de muestreo lo marca el hardware: su jitter es el del oscilador |
| Clock Source | **FOSC/ADCLK** | síncrono con FOSC y con el disparo (con FRC no lo sería) |
| Clock | **FOSC/32** | TAD = 1.0 µs (hoja de datos: 0.5 a 9 µs); 100 µs de disparo = 100 TAD exactos |
| Acquisition Count | **8** | 8 µs de adquisición (la hoja de datos pide 7.8 µs con 10 kΩ de fuente) |
| Precharge Time | 0 | |
| Repeat (ADRPT) | 0 | no se usa en modo básico |
| Error Calculation / Threshold | por defecto | no se usan |
| Enable ADC Interrupt | **✔** | ADIF al terminar cada conversión: la interrupción suma el resultado y elige el canal siguiente |
| ADC Threshold Interrupt | sin marcar | |
| Continuous operation | sin marcar | |
| Double sampling (ADDSEN) | sin marcar | |

`app/adquisicion.c` vuelve a fijar ADCONT, ADCS, el modo y ADACT al arrancar, porque la secuencia de canales depende de ellos, e instala su función de interrupción con `ADCC_SetADIInterruptHandler()`.

---

## 5. FVR (Fixed Voltage Reference)

| Campo | Valor |
|---|---|
| Enable FVR | ✔ |
| FVR buffer gain to ADC | 1x (1.024 V) |
| FVR buffer gain to Comparator/DAC | off |
| Temperature indicator | disabled |

Con él se conoce VCC_A en voltios (`VCC_A = 1.024 · cV / cFVR`) para la protección de 2.5 V y el LED "VCC_A fuera de rango" del VI.

---

## 6. TMR2 (base de tiempo del PWM6)

| Campo | Valor |
|---|---|
| Enable Timer | ✔ |
| Control Mode | Roll over pulse |
| Start/Reset Option | Software control |
| Clock Source | FOSC/4 |
| Prescaler | **1:4** |
| Postscaler | 1:1 |
| Timer Period | **100 µs** (MCC pone T2PR = 199) |
| Enable Timer Interrupt | **sin marcar** |

4 · 200 / 8 MHz = 100.000 µs exactos = 3200 ciclos de FOSC = 100 TAD. Una trama de 10 ms son 100 periodos.

---

## 7. PWM6 (disparo del ADC)

Añade **PWM6** desde Device Resources (PWM).

| Campo | Valor |
|---|---|
| Enable PWM | ✔ |
| Select a Timer | **Timer2** |
| Duty Cycle | **50 %** (50 µs; MCC pone PWM6DCH = 0x64, PWM6DCL = 0x00) |
| PWM Polarity | active_hi |

No asignes su salida a ningún pin en el Pin Manager (ver la nota de RA1 en la sección 3). El ADC usa la señal interna PWM6_OUT aunque no salga por ningún pin.

---

## 8. EUSART1

| Campo | Valor |
|---|---|
| Mode | asynchronous |
| Enable EUSART | ✔ |
| Enable Transmit | ✔ |
| Enable Receive / Continuous Receive | ✔ |
| Baud Rate | **115200** (MCC elige BRG16 = 1, BRGH = 1, SP1BRG = 68; error +0.6 %) |
| Transmission / Reception bits | 8-bit |
| Enable EUSART Interrupts | ✔ |
| Redirect STDIO to USART | sin marcar (no se usa printf) |
| Software Settings: Transmit Buffer Size | **128** |
| Software Settings: Receive Buffer Size | 8 |

---

## 9. Memory

Añade el módulo **Memory** (Device Resources). Genera `DATAEE_ReadByte()` y `DATAEE_WriteByte()` (y las funciones FLASH_x, que no se usan). La EEPROM de datos está en 0xF000–0xF0FF; el firmware guarda ahí I0, Q0, θ, G y las etapas activas (17 bytes).

---

## 10. Interrupt Module

Deja habilitadas las interrupciones del ADC (ADI) y de la EUSART1 (TX y RX); la del TMR2, no. MCC genera `interrupt_manager.c`; el del proyecto atiende primero el ADC. `main.c` activa GIE y PEIE después de inicializar.

La interrupción del ADC no decide el instante de muestreo (lo decide el PWM6): solo tiene que leer el resultado y elegir el canal antes del disparo siguiente, con ~75 µs de margen.

---

## 11. Generar

Pulsa **Generate**. Si MCC propone cambios en `main.c`, conserva el `main.c` del proyecto. Después comprueba que estos valores coinciden con los de los archivos generados:

| Archivo | Registro | Valor |
|---|---|---|
| `mcc.c` | OSCCON1 / OSCFRQ | 0x60 / 0x06 |
| `adcc.c` | ADCON0 / ADCON2 / ADCON3 | 0x84 / 0x00 / 0x00 |
| `adcc.c` | ADCLK / ADACQL / ADACT | 0x0F / 0x08 / 0x0E |
| `adcc.c` | PIE1bits.ADIE | 1 |
| `fvr.c` | FVRCON | 0x81 |
| `tmr2.c` | T2CLKCON / T2PR / T2CON | 0x01 / 0xC7 / 0xA0 |
| `pwm6.c` | PWM6CON / PWM6DCH / PWM6DCL / CCPTMRS1.P6TSEL | 0x80 / 0x64 / 0x00 / 1 |
| `eusart1.c` | BAUD1CON / RC1STA / TX1STA / SP1BRGL | 0x08 / 0x90 / 0x24 / 0x44 |
| `pin_manager.c` | ANSELA / ANSELC / TRISA / TRISC / WPUA | 0x03 / 0x0F / 0x1F / 0x2F / 0x14 |
| `pin_manager.c` | RX1PPS / RC4PPS | 0x15 / 0x0F |
