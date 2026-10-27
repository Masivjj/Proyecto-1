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

RA0 y RA1 (ICSPDAT/ICSPCLK) quedan sin usar, en analógico. RA3 es MCLR (pull-up externo de 10 kΩ).

---

## 4. ADCC (ADC with Computation)

| Campo | Valor | Por qué |
|---|---|---|
| Enable ADC | ✔ | |
| Operating Mode | **Burst_average_mode** | el hardware repite la conversión ADRPT veces seguidas |
| Result Alignment | right | |
| Positive Reference | VDD | normalización ratiométrica: VDD se cancela (sección 10.4) |
| Negative Reference | VSS | |
| Auto-conversion Trigger | disabled | la conversión la lanza el programa |
| Clock Source | FOSC/ADCLK | |
| Clock | **FOSC/32** | TAD = 1.0 µs (hoja de datos: 0.5 a 9 µs) |
| Acquisition Count | **8** | 8 µs de adquisición: sobra para fuentes de baja impedancia |
| Precharge Time | 0 | |
| Repeat (ADRPT) | **16** | `N_SOBREMUESTREO` en `app/app_config.h` |
| Accumulator Right Shift (ADCRS) | **0** | ADFLTR = suma de las 16 conversiones (0..65520) |
| Error Calculation (ADCALC) | First derivative of single measurement | no se usa |
| Threshold Interrupt Mode (ADTMD) | **Interrupt regardless of threshold test results** ("always") | ADTIF marca el final de cada ráfaga |
| ADC Interrupt / ADC Threshold Interrupt | sin marcar | el programa consulta ADTIF |
| Continuous operation | sin marcar | |
| Double sampling (ADDSEN) | sin marcar | la errata del Burst Average solo afecta con doble muestreo |

`app/adquisicion.c` vuelve a fijar ADRPT, ADCRS, el modo y ADTMD al arrancar, porque el procesamiento depende de que el resultado sea la suma de 16 conversiones.

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

## 6. TMR2 (tick de 10 ms)

| Campo | Valor |
|---|---|
| Enable Timer | ✔ |
| Control Mode | Roll over pulse |
| Start/Reset Option | Software control |
| Clock Source | FOSC/4 |
| Prescaler | 1:128 |
| Postscaler | 1:5 |
| Timer Period | **10 ms** (MCC pone T2PR = 124) |
| Enable Timer Interrupt | ✔ |

128 · 125 · 5 / 8 MHz = 10.000 ms exactos.

---

## 7. EUSART1

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

## 8. Memory

Añade el módulo **Memory** (Device Resources). Genera `DATAEE_ReadByte()` y `DATAEE_WriteByte()` (y las funciones FLASH_x, que no se usan). La EEPROM de datos está en 0xF000–0xF0FF; el firmware guarda ahí I0, Q0, θ, G y la corrección de signo (17 bytes).

---

## 9. Interrupt Module

Deja habilitadas las interrupciones de TMR2 y EUSART1 (TX y RX); MCC genera `interrupt_manager.c`. `main.c` activa GIE y PEIE después de inicializar.

---

## 10. Generar

Pulsa **Generate**. Si MCC propone cambios en `main.c`, conserva el `main.c` del proyecto. Después comprueba que estos valores coinciden con los de los archivos generados:

| Archivo | Registro | Valor |
|---|---|---|
| `mcc.c` | OSCCON1 / OSCFRQ | 0x60 / 0x06 |
| `adcc.c` | ADCON0 / ADCON2 / ADCON3 | 0x84 / 0x03 / 0x07 |
| `adcc.c` | ADCLK / ADRPT / ADACQL | 0x0F / 0x10 / 0x08 |
| `fvr.c` | FVRCON | 0x81 |
| `tmr2.c` | T2CLKCON / T2PR / T2CON | 0x01 / 0x7C / 0xF4 |
| `eusart1.c` | BAUD1CON / RC1STA / TX1STA / SP1BRGL | 0x08 / 0x90 / 0x24 / 0x44 |
| `pin_manager.c` | ANSELA / ANSELC / TRISA / TRISC / WPUA | 0x03 / 0x0F / 0x1F / 0x2F / 0x14 |
| `pin_manager.c` | RX1PPS / RC4PPS | 0x15 / 0x0F |
