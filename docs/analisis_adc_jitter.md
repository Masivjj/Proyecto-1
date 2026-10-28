# ADC del PIC16F18426: referencias, muestreo en modo básico, jitter y autocero

Este documento responde a los requisitos de la pestaña PROYECTO 1:

1. un ADC con un **jitter de muestreo (desviación estándar) menor de 10 ns**, para que el jitter limite el SNR a **80 dB** como mucho (20 dB de margen sobre los 60 dB que dejan los demás ruidos), y **maneras de medir ese jitter**;
2. **analizar el ADC del PIC** (un SAR) con la hoja de datos y decidir **qué referencias usar**;
3. usar la **configuración básica de muestreo** (sección 32.2.7) con el circuito de muestreo y retención de la hoja de datos;
4. **autocero con el conversor y el PWM**.

Las referencias a la hoja de datos son al documento DS40001985B (PIC16(L)F18426/46). Las del ruido y el jitter siguen la nota de Texas Instruments sobre el ruido del ADC citada en la pestaña.

## 0. Resumen

| Requisito | Qué se ha hecho | Dónde |
|---|---|---|
| Jitter < 10 ns | Cada conversión la dispara por hardware el flanco del **PWM6** (ADACT = PWM6_OUT), con el reloj del ADC derivado de FOSC y un periodo de disparo de 100 µs = 100 TAD exactos. El instante de muestreo no depende del software (interrupciones, lazo principal): su jitter es el del oscilador. | `app/adquisicion.c`, `mcc_generated_files/adcc.c`, `pwm6.c`, `tmr2.c` |
| Medir el jitter | (a) El PWM6 sale por **RA1** para medirlo con un osciloscopio. (b) La orden **`m`** captura 200 conversiones seguidas y **`pc/medir_jitter.py`** calcula SNR, SINAD, THD y ENOB y estima σ con dos frecuencias de entrada. | sección 6 |
| Analizar el ADC y las referencias | Análisis del SAR, errores de la hoja de datos, tiempo de adquisición. Referencia: **VDD/VSS** con normalización ratiométrica y el FVR medido como un canal más. | secciones 1 y 2 |
| Muestreo en modo básico | ADC en **modo básico** (MD = 000), una conversión por disparo, adquisición de 8 TAD. | sección 3 |
| Autocero con conversor y PWM | En cada ronda de disparos del PWM se convierte **AVSS** (0 V interno). El offset medido se resta de todos los canales. Se activa o desactiva con la orden **`A`**. | sección 7 |

Un punto importante para leer el requisito: **la relación «10 ns → 80 dB» solo vale para señales de hasta 1.59 kHz**. El SNR que permite el jitter depende de la frecuencia de la señal, no solo de σ. En este firmware el PIC convierte las salidas **DC** I y Q de la tarjeta, que varían a pocos hercios. Con 10 ns, el jitter limita su SNR a 110 dB a 50 Hz, así que no es el factor limitante. El requisito pesa de verdad si el ADC llega a muestrear la señal de alterna (ver la sección 4.3).

---

## 1. El ADC del PIC16F18426 (ADC2, aproximaciones sucesivas de 12 bits)

### 1.1 Cómo convierte

El ADC2 (capítulo 32) es un convertidor de **aproximaciones sucesivas (SAR)** con un **condensador de muestreo y retención** (figura 32-4, *Analog Input Model*):

```
          Rs          RIC ≈ 1 kΩ      SS (RSS ≈ 7 kΩ)
  VA ──/\/\/──┬──────/\/\/────────────o/ o──────┬──── al comparador del SAR
             CPIN 5 pF                         CHOLD = 28 pF
              │                                 │
             VSS                              Ref-
```

Una conversión tiene tres fases (figura 32-2, *Analog-to-Digital Conversion TAD Cycles*):

1. **Adquisición** (ADACQ periodos de TAD): el interruptor SS está cerrado y CHOLD se carga a la tensión de la entrada a través de Rs + RIC + RSS.
2. **Muestreo**: al empezar la conversión el interruptor se abre (*CHOLD is disconnected from analog input, typically 100 ns*; parámetro AD24, THCD = 2 TAD + 1 TCY). **Ese instante es el instante de muestreo.** Su incertidumbre es el jitter que nos interesa (la flecha «Jitter aquí» del diagrama de bloques).
3. **Conversión** (AD22: 14 TAD + 2 TCY con reloj FOSC): el SAR compara la carga retenida con fracciones de la referencia, un bit por TAD (b11 a b0). Durante este tiempo la entrada puede cambiar sin afectar al resultado.

En este diseño: TAD = 1 µs (FOSC/32, dentro de los 0.5–9 µs de AD20); adquisición de 8 µs; conversión de ~14.3 µs; ADIF al terminar.

### 1.2 Errores según la hoja de datos (tabla 40-13, VDD = 3.0 V, TAD = 1 µs)

| Parámetro | Típico | Máximo | Qué hace este diseño con él |
|---|---|---|---|
| AD01 resolución | — | 12 bits | 4096 códigos; ruido de cuantificación q/√12 = 0.29 LSB |
| AD02 EIL, no linealidad integral | ±1.0 LSB | ±2.0 LSB | queda; ±0.05 % del fondo de escala |
| AD03 EDL, no linealidad diferencial | ±1.0 LSB | ±1.0 LSB | sin códigos perdidos (nota 2 de la tabla) |
| AD04 EOFF, offset | 0.5 LSB | ±6.0 LSB | **autocero**: se mide con AVSS y se resta (sección 7) |
| AD05 EGN, ganancia | ±0.2 LSB | ±6.0 LSB | **se cancela**: la normalización usa cocientes de códigos (cI/cV, cV/cFVR) y el error de ganancia afecta igual a numerador y denominador |
| AD08 impedancia de la fuente recomendada | 10 kΩ | — | ver 1.3 |
| AD09 escalera de la referencia | 50 kΩ | — | solo importa con referencia externa (sección 2) |

La hoja de datos **no da SNR, SINAD ni ENOB**. Por eso el firmware trae la orden `m` y `pc/medir_jitter.py`, para medirlos en la tarjeta (sección 6.2).

### 1.3 Tiempo de adquisición

Con la ecuación 32-1, CHOLD debe cargarse hasta ½ LSB de un escalón de fondo de escala:

TC = −CHOLD · (RIC + RSS + Rs) · ln(1/8191) y TACQ = 2 µs (amplificador) + TC + (T − 25 °C) · 0.05 µs/°C

| Fuente Rs | TC | TACQ a 50 °C | Con ADACQ = 8 µs |
|---|---|---|---|
| 1 kΩ (salida de un operacional) | 2.27 µs | 5.5 µs | sobra |
| 5 kΩ (divisor 2 × 10 kΩ de VCC_A) | 3.28 µs | 6.5 µs | sobra |
| 10 kΩ (máximo recomendado) | 4.54 µs | 7.8 µs | justo |

Además, la interrupción elige el canal siguiente ~75 µs antes de su disparo. El orden de los canales (I_OUT, Q_OUT, V_REF, VCC_A, FVR, AVSS) incluye el peor escalón, de AVSS (0 V) a I_OUT. La ecuación ya supone un escalón completo. CHOLD no se descarga entre conversiones (nota 2 de la ecuación), pero con este tiempo no queda memoria del canal anterior.

---

## 2. Referencias del ADC

ADREF permite como referencia positiva VDD, el pin VREF+ (RA1 en el encapsulado de 14 pines) o el buffer del FVR (1.024, 2.048 o 4.096 V); como negativa, VSS o VREF−. AD06 exige ADREF+ − ADREF− ≥ 1.8 V.

| Opción | Ventajas | Inconvenientes | Decisión |
|---|---|---|---|
| **VDD / VSS** | Rango completo (0–3.7 V) para I_OUT y Q_OUT, que están centradas en VCC_A/2 ≈ 1.83 V. Ningún pin ocupado. | VDD no es una referencia de precisión. | **Elegida**: VDD se cancela porque el procesamiento usa cocientes (cI − cR)/cV (sección 10.4 de la especificación). |
| FVR 2.048 V | Referencia estable. | Recorta las señales por encima de 2.048 V (I_OUT llega a 1.83 V ± la señal). Rompe la ratiometría con VCC_A. | No |
| FVR 4.096 V | — | Necesita VDD > 4.096 V; el PIC va a 3.7 V. | No |
| VCC_A en VREF+ | Ratiometría por hardware: el código ya es señal/VCC_A. | Ocupa RA1 (ICSPCLK, que ahora saca el disparo). Carga VCC_A con los 50 kΩ de la escalera. Falla si VCC_A ≥ VDD. | Alternativa si se rediseña la tarjeta |

Con VDD como referencia, el FVR (1.024 V) se mide como un canal más. Así se conoce VCC_A en voltios (VCC_A = 1.024 · cV / cFVR) para la protección de 2.5 V y el aviso de VCC_A fuera de rango. Las conversiones de I, Q, V_REF y VCC_A de una misma ronda están separadas por 100 µs. Para que el ruido de VDD se cancele bien, **VDD debe estar bien desacoplada** (0.1 µF + 10 µF junto al PIC; mejor aún, un filtro LC o un regulador propio).

---

## 3. Muestreo en modo básico disparado por el PWM6

Es el procedimiento de la sección 32.2.7 (*ADC Conversion Procedure, Basic Mode*), con el inicio de cada conversión dado por el disparo de autoconversión (sección 32.2.6: «*When a rising edge of the selected source occurs, the GO bit is set by hardware*»).

| Registro | Valor | Significado |
|---|---|---|
| ADCON0 | 0x84 | ADON = 1, ADCONT = 0 (una conversión por disparo), **ADCS = 0 (reloj FOSC/ADCLK)**, justificado a la derecha |
| ADCON2 | 0x00 | **MD = 000: modo básico** |
| ADCLK | 0x0F | FOSC/32 → TAD = 1 µs |
| ADACQ | 8 | 8 TAD de adquisición |
| ADREF | 0x00 | VDD / VSS |
| ADACT | **0x0E** | disparo = PWM6_OUT |
| PIE1.ADIE | 1 | interrupción al terminar cada conversión |
| T2CLKCON / T2CON / T2PR | 0x01 / 0xA0 / 199 | TMR2 a FOSC/4 con prescaler 1:4: periodo de 200 × 0.5 µs = **100 µs** |
| PWM6CON / PWM6DC | 0x80 / 400 | PWM6 con el TMR2, 50 % (50 µs en alto) |

Cronograma de cada disparo (cada 100 µs):

```
 t = 0        flanco de subida del PWM6 → GO = 1 (hardware)
 0 … 8 µs     adquisición (8 TAD)
 ≈ 8 µs       se abre el interruptor de muestreo: INSTANTE DE MUESTREO
 8 … 22.3 µs  conversión SAR (14 TAD + 2 TCY)
 22.3 µs      ADIF → interrupción: suma el resultado en su canal y elige el canal siguiente
 … 100 µs     el canal nuevo se asienta (~75 µs) hasta el disparo siguiente
```

Una **trama de 10 ms** son 100 disparos: 16 rondas de los 6 canales (96 conversiones) y 4 disparos libres que convierten AVSS y se descartan. Al terminar la trama, las seis sumas pasan a una cola de 4 tramas que vacía `main()`. Si el lazo principal se retrasa (por ejemplo, al formatear números en coma flotante), las conversiones siguen a su hora y solo se pierden tramas si el retraso supera 40 ms. Las tramas perdidas se cuentan y se avisan.

---

## 4. Ruido, SNR y jitter

### 4.1 Fórmulas (nota de TI sobre el ruido del ADC)

- Cuantificación de un ADC ideal de N bits con una senoide de fondo de escala: **SNR_q = 6.02·N + 1.76 dB** = 74.0 dB con 12 bits.
- Jitter de muestreo σ (desviación estándar del instante de muestreo) con una senoide de frecuencia f: **SNR_j = −20·log10(2π·f·σ)**.
- Las fuentes independientes se suman en potencia: **SNR_total = −10·log10(10^(−SNR_q/10) + 10^(−SNR_térmico/10) + 10^(−SNR_j/10))**.
- **ENOB = (SINAD − 1.76) / 6.02**.

### 4.2 Qué significa «σ < 10 ns → 80 dB»

| f de la señal | σ = 1 ns | **σ = 10 ns** | σ = 100 ns | σ = 1 µs |
|---|---|---|---|---|
| 10 Hz | 144 dB | **124 dB** | 104 dB | 84 dB |
| 100 Hz | 124 dB | **104 dB** | 84 dB | 64 dB |
| 1 kHz | 104 dB | **84 dB** | 64 dB | 44 dB |
| 1.59 kHz | 100 dB | **80 dB** | 60 dB | 40 dB |
| 10 kHz | 84 dB | **64 dB** | 44 dB | 24 dB |
| 20 kHz | 78 dB | **58 dB** | 38 dB | 18 dB |
| f máxima para 80 dB | 15.9 kHz | **1.59 kHz** | 159 Hz | 16 Hz |

(`python pc/medir_jitter.py tabla` reproduce esta tabla.)

Con 10 ns, el jitter cuesta 0.04 dB sobre un objetivo total de 60 dB: −10·log10(10⁻⁶ + 10⁻⁸) = 59.96 dB. Ese es el margen de 20 dB del requisito. Pero solo se cumple **hasta 1.59 kHz**: a 20 kHz harían falta 0.8 ns.

### 4.3 En este sistema

- **Señales I y Q (DC, ≤ 50 Hz):** el jitter es despreciable. Con 10 ns da 110 dB a 50 Hz; incluso con 1 µs serían 70 dB. Lo que limita es la cuantificación (74 dB por conversión), el ruido del ADC y el de la tarjeta. La suma de 16 conversiones por canal y trama gana hasta 12 dB (10·log10 16) si el ruido es de al menos ~0.5 LSB eficaces (hace de *dither*).
- **Restos de alta frecuencia en el pin** (por ejemplo, la portadora mal filtrada): el jitter actúa sobre ellos. El ruido que añade es A_resto · 2π · f · σ / √2 eficaces. Por ejemplo, 10 mV de amplitud de portadora a 50 kHz con σ = 10 ns dan 22 µV eficaces (95 dB bajo una senoide de fondo de escala). Conviene el filtro RC en cada entrada.
- **Si en el futuro el PIC muestrea la señal de alterna** (demodulación digital), el requisito de 10 ns es el adecuado y la frecuencia de la señal decide el SNR (tabla de 4.2). El firmware ya muestrea con disparo por hardware, y la sección 6 permite comprobarlo.

---

## 5. Fuentes de jitter y cómo se evitan

| Fuente | Orden de magnitud | Solución en este firmware |
|---|---|---|
| Inicio por software (ADGO en el lazo o en una interrupción) | De la latencia de interrupción (varios TCY, ≥ 0.4 µs) a decenas de µs con las secciones críticas de `main()` | **Disparo por hardware** (ADACT = PWM6_OUT): ninguna instrucción interviene en el instante de muestreo |
| Reloj del ADC FRC (ADCS = 1), asíncrono a FOSC | Hasta 1 TAD de FRC (AD21: 2 µs típicos): σ ≈ 0.6 µs | **ADCS = 0**: reloj FOSC/32, síncrono con el disparo |
| Fase variable entre el disparo y el divisor del reloj del ADC | Hasta 1 TAD (1 µs) si el periodo de disparo no es múltiplo de TAD y el divisor corre libre (la hoja de datos no detalla este punto) | Periodo de 100 µs = 3200 ciclos de FOSC = **100 TAD exactos**: la fase se repite en cada disparo |
| Oscilador (HFINTOSC) | **La hoja de datos no lo especifica** | Medirlo (sección 6). Si no basta, usar un reloj externo (5.1) |
| Ruido de VDD y de masa | Desplaza el umbral del comparador: aparece como ruido de tensión, no de tiempo | Desacoplo de VDD (sección 2) |
| Apertura del interruptor de muestreo | Picosegundos en un interruptor CMOS | Despreciable |

### 5.1 HFINTOSC frente a un reloj externo

- **HFINTOSC a 32 MHz** (configuración actual: RSTOSC = HFINT32, sin PLL). La hoja de datos solo da su exactitud (figura 40-6: ±2 % entre 0 y 60 °C; ±3 % hasta 85 °C; ±5 % en los extremos) y ningún dato de jitter. El ±0.25 % de PLL04 (*PLL Output Frequency Stability (Jitter)*) es del PLL, que no se usa a 32 MHz con HFINTOSC.
- **Exactitud no es jitter.** Un error de frecuencia del 2 % solo cambia la frecuencia de muestreo (10 kS/s ± 2 %) y la escala de tiempo, sin añadir ruido. Lo que añade ruido son las variaciones rápidas del periodo. En un oscilador interno se acumulan de un ciclo al siguiente, así que el jitter entre dos disparos (3200 ciclos) puede ser bastante mayor que el de un ciclo. No se puede predecir con la hoja de datos: hay que **medirlo**.
- **Si la medida no da < 10 ns:** reloj externo en modo EC por CLKIN (**RA5, pin 2**), por ejemplo un oscilador de cristal CMOS de 32 MHz (FEXTOSC = ECH, RSTOSC = EXT1X) o uno de 8 MHz con el PLL ×4 (EXT4X). Los osciladores de cristal tienen un jitter de picosegundos. Hay que mover el LED, que ahora está en RA5. Es un cambio de hardware y no está hecho.

---

## 6. Cómo medir el jitter

Un temporizador interno (SMT, TMR1…) **no sirve**. Su resolución es 1/FOSC = 31.25 ns, mayor que lo que se quiere medir. Además, mediría el oscilador con el propio oscilador. Hace falta una referencia externa: un osciloscopio o una señal de un generador.

### 6.1 Con un osciloscopio en RA1

1. Compilar en modo producción (*Make and Program Device*, no *Debug*) con `SALIDA_DISPARO 1` (por defecto). RA1 (pin 12) saca el PWM6: 10 kHz, 50 %. Cada flanco de subida es el disparo de una conversión. El firmware quita la limitación de pendiente de RA1: un flanco lento convierte el ruido de tensión en jitter aparente.
2. Punta con el muelle de masa corto, osciloscopio de ≥ 1 GS/s.
3. **Jitter de periodo:** disparar en un flanco de subida y mirar el flanco siguiente con un retardo horizontal de 100 µs, con persistencia infinita o histograma. La desviación estándar del histograma es σ del periodo de muestreo. Repetir con 10 periodos (1 ms). Si σ crece como √10, el jitter se acumula (ruido de frecuencia del oscilador).
4. Si el osciloscopio tiene la medida **TIE** (*time interval error*, con recuperación de reloj), da directamente la desviación de cada flanco respecto a una rejilla ideal. Es lo que degrada el SNR.

El instante de muestreo real está 8 TAD + THCD después del flanco, con un número fijo de ciclos de FOSC. Lo que se mide en RA1 es, por tanto, el jitter del muestreo (más el de esos ~8 µs, despreciable frente al de 100 µs).

### 6.2 Por el SNR: orden `m` y `pc/medir_jitter.py`

Es el método de la nota de TI aplicado al propio ADC. Mide el jitter efectivo **tal como lo ve el convertidor**, incluido todo lo que hay entre el oscilador y el interruptor de muestreo.

1. Desconectar I_OUT de la tarjeta y conectar a **RC0** un generador senoidal de baja distorsión. Offset VDD/2 (1.85 V), 3.4 Vpp (−0.7 dBFS), salida de 50 Ω. Sin superar nunca 0–VDD.
2. **Frecuencia baja, 1230 Hz:** el jitter no influye (82 dB con 10 ns).
   `python pc/medir_jitter.py capturar --puerto COM5 --capturas 10 -o baja.txt`
3. **Frecuencia alta, 51 230 Hz:** a 10 kS/s se ve en 1230 Hz (submuestreo), así que el espectro es el mismo que en 2. Con σ = 10 ns, el jitter limita el SNR a 49.8 dB, muy por debajo del suelo del ADC.
   `python pc/medir_jitter.py capturar --puerto COM5 --capturas 10 -o alta.txt`
4. `python pc/medir_jitter.py jitter baja.txt alta.txt --fin-baja 1230 --fin-alta 51230`

Cada captura (200 conversiones, 20 ms) se ajusta a una senoide de 4 parámetros (IEEE 1057) y a sus armónicos 2 a 5. El residuo es el ruido N y la senoide, la señal P (en LSB²). El jitter añade P·(2π·f·σ)², así que:

**σ² = (N_alta − N_baja) / ((2π)² · (P_alta·f_alta² − P_baja·f_baja²))**

Con amplitudes iguales y f_baja ≪ f_alta, equivale a σ = √(10^(−SNR_alta/10) − 10^(−SNR_baja/10)) / (2π·f_alta).

El programa da σ con su cota superior (dos incertidumbres) y dice si cumple 10 ns. Si el aumento de ruido no es significativo, da solo la cota. También informa hasta qué frecuencia el SNR del jitter es ≥ 80 dB. `analizar` da SNR, THD, SINAD, ENOB, ruido en LSB y el espectro (`--png`).

Notas:

- El generador debe tener un jitter muy inferior a 10 ns a 51 kHz (cualquier generador DDS de laboratorio lo tiene).
- A 51 kHz el filtro RC del muestreo (RIC + RSS + Rs con CHOLD: ~0.7 MHz de ancho de banda) atenúa menos del 1 %. Aun así la fórmula usa las amplitudes medidas en cada caso.
- `python pc/medir_jitter.py simular --sigma 10e-9` valida el método sin hardware: con capturas sintéticas de σ = 10 ns estima 9.8 ns (las pruebas de `tests/test_medir_jitter.py` exigen un error < 10 %).
- Con la entrada DC normal de la tarjeta, `analizar` da el ruido del ADC en LSB y el SNR referido al fondo de escala.

---

## 7. Autocero

### 7.1 Implementación

En cada ronda de disparos del PWM6, después de I_OUT, Q_OUT, V_REF, VCC_A y FVR, el ADC convierte **AVSS** (canal interno 0x3B, 0 V). La suma de sus 16 conversiones por trama es el offset del ADC con la misma temporización que los demás canales. `iq_proc.c` lo resta de todos los canales antes de la normalización (`escalar()`).

- Corrige el offset (AD04: hasta ±6 LSB) y su deriva con la temperatura y con VDD, porque se mide continuamente (16 veces en cada trama de 10 ms).
- La orden `?` muestra el offset medido en LSB. La orden `A` (o la casilla *Autocero ADC* del programa del PC) lo activa y desactiva para comparar. En la simulación, con un offset de +3 LSB, VCC_A sale 3.651 V con autocero y 3.644 V sin él.
- **Limitación:** un ADC unipolar no puede dar códigos negativos. Si el offset es negativo, AVSS da 0 y no se corrige: el autocero solo ve offsets positivos. Lo demás (ganancia, VDD) ya se cancela con los cocientes.

### 7.2 Otra lectura del requisito

«Autocero con conversor y PWM» también puede referirse a **compensar en analógico el desequilibrio del puente**. Un PWM filtrado (un DAC) restaría una tensión a I y Q antes de la etapa de ganancia, para que con ×101 los 62 mV de desequilibrio no saturen el ADC. Eso necesita hardware que la tarjeta P2 no tiene: un nodo sumador antes de la ganancia y un filtro RC del PWM. Si se añade, dos salidas PWM libres (el PWM7 y un CCP en modo PWM, con el TMR4 para no tocar el disparo) pueden generar las dos tensiones, y el lazo «Fijar cero» las ajustaría con las medidas del ADC.

---

## 8. Cambios en el firmware y verificación

- `adcc.c`: modo básico, ADACT = PWM6_OUT, interrupción de fin de conversión. `tmr2.c`: 100 µs sin interrupción. `pwm6.c`: nuevo. `interrupt_manager.c`: atiende el ADC primero.
- `app/adquisicion.c`: secuencia de canales desde la interrupción, tramas de 10 ms, cola de tramas, captura `m` y salida del disparo por RA1.
- `app/iq_proc.c`: etapa de autocero (bit 0x10 de `etapas`). `app/comunicacion.c`: órdenes `A` y `m`, líneas `M,`, offset y muestras perdidas en `?`.
- `main.c`: el ritmo lo marcan las tramas del ADC (sin tick de TMR2).
- Pruebas: `tests/test_iq_proc.c` (autocero con offset conocido). `tests/sim`: el ADC simulado convierte cada 100 µs lo que la interrupción dejó en ADPCH, con +3 LSB de offset, paradas del lazo de 30 y 150 ms y la captura `m`. `tests/test_medir_jitter.py`: SINAD de un ADC ideal (74 dB) y σ simulados.

Pendiente en hardware: compilar con XC8, medir el jitter real del HFINTOSC (sección 6) y el SNR del ADC con la tarjeta.
