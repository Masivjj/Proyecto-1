# Procesamiento I/Q y plano polar del VI `P2_IQ_SINAD.vi` — especificación para portarlo a un PIC16F18426 (MPLAB X + XC8)

Este documento describe, con el detalle necesario para reprogramarlo en C, qué hace el VI de LabVIEW `P2_IQ_SINAD.vi` desde que lee las salidas de la tarjeta P2 hasta que dibuja el plano polar. Al final propone cómo trasladarlo a un PIC16F18426 y trae un prompt para arrancar la implementación en un chat nuevo.

Todas las fórmulas, constantes y el orden de las operaciones están tomados del VI tal como quedó el 23/10/2026 (impresión del diagrama de bloques y del panel frontal de esa fecha).

---

## 0. Cómo usar este documento

1. Las secciones 1 a 8 describen **lo que hace LabVIEW**: son la especificación funcional. El código del PIC debe reproducir esas fórmulas.
2. La sección 9 trae **vectores de prueba** con resultados conocidos, para verificar el código del PIC.
3. Las secciones 10 y 11 proponen **cómo implementarlo en el PIC16F18426**. Son una propuesta de partida: las decisiones abiertas están en la sección 12.
4. La sección 13 trae el **prompt sugerido** para el chat nuevo.

---

## 1. Contexto mínimo del sistema

La tarjeta P2 es un demodulador I/Q analógico para detectar fisuras en un tubo de aluminio con una sonda de corrientes de Eddy diferencial (dos bobinas en un puente, excitación de 20.000 kHz). La tarjeta entrega dos tensiones DC, I y Q, ya filtradas a 10 Hz. El software solo trabaja con esas tensiones lentas.

### 1.1 Señales que entrega la tarjeta (bornera J805)

| Pin J805 | Señal | Rango / valor típico |
|---|---|---|
| 1 | I_OUT | V_REF ± 1.8 V como máximo (satura más allá) |
| 2 | V_REF (por 100 Ω) | VCC_A / 2 ≈ 1.85 V |
| 3 | Q_OUT | V_REF ± 1.8 V como máximo |
| 4 | V_REF (por 100 Ω) | Igual que el pin 2 |
| 5 | VCC_A (por 100 Ω) | 3.0 a 4.2 V (celda Li); 3.65 V en las medidas reales |
| 6 | GND | — |

- Las señales útiles son **I_OUT − V_REF** y **Q_OUT − V_REF**. V_REF es la "tierra" de la señal, porque la tarjeta usa una sola alimentación.
- El ancho de banda de I_OUT/Q_OUT es de unos 10 Hz (filtro Sallen‑Key de 9.85 Hz). En ×101 hay además un polo de 159 Hz en la etapa de ganancia.

### 1.2 Ganancia DC de la tarjeta

La etapa final tiene ganancia ×2, ×11 o ×101, seleccionada por los jumpers JP701 (canal I) y JP702 (canal Q). **El software no puede leer los jumpers**: la ganancia se le indica a mano y debe coincidir siempre con ellos.

### 1.3 Particularidades de la tarjeta fabricada

- **El canal I sale con el signo invertido** (pines 12/13 del 74HC4053 cruzados). El software lo corrige multiplicando I por −1.
- Las salidas son **ratiométricas**: escalan con VCC_A. El software las normaliza a VCC_A = 3.7 V.

### 1.4 Constantes del sistema

| Constante | Valor | Uso |
|---|---|---|
| VCC nominal | 3.7 V | Normalización ratiométrica |
| Umbral de VCC_A válido | 2.5 V | Si VCC_A medido ≤ 2.5 V, se usa 3.7 V y se enciende el LED "VCC_A fuera de rango" |
| Ganancias DC | 2, 11, 101 | Divisor de la etapa DC |
| mV por 10⁻⁴ | 4.03 mV | Señal que produce una grieta de d = 10⁻⁴ a la salida del mezclador |
| Saturación de la salida | V_REF ± 1.8 V | En ×101 equivale a ±17.8 mV a la salida del filtro |
| Ruido de salida en ×101 | 0.70 mV rms | Referencia para verificar |

De dónde sale 4.03 mV: `(2/π) · 253 · (2.0 Vpp / 8) · 10⁻⁴ = 4.03 mV`, es decir la diferencia del puente (exc/4·d), por la ganancia de la cadena (253) y por el factor 2/π del mezclador conmutado.

---

## 2. Visión general del VI

El VI tiene dos lazos que corren en paralelo:

1. **Productor (adquisición):** lee del NI USB‑6211 bloques de 100 muestras por canal a 1000 muestras/s (es decir, un bloque cada 0.1 s) y los mete en una cola.
2. **Consumidor (procesamiento):** saca cada bloque de la cola y aplica, en este orden:
   1. Normalización de VCC_A y protección.
   2. Corrección de signo de I, división por la ganancia DC, normalización ×3.7/VCC_A y paso a mV → señales **procesadas** (Ip, Qp).
   3. Promedios del bloque.
   4. Cero (I0, Q0) → señales **sin cero** (Ic, Qc).
   5. Ángulo de calibración θ.
   6. Rotación por θ → señales **finales** (I′, Q′).
   7. Conversión de cada muestra a polar (r, φ) y traza del plano polar.
   8. Indicadores del bloque: |V|, Fase y d estimada.
   9. Además: SINAD, THD y espectro de las crudas (no se portan al PIC; ver sección 8) y grabación en archivo.

En el PIC no hacen falta ni la cola ni los dos lazos: basta un temporizador, el ADC y un lazo principal (sección 10).

---

## 3. Adquisición en LabVIEW (lazo productor)

| Parámetro | Valor |
|---|---|
| Dispositivo | NI USB‑6211 (ADC de 16 bits) |
| Canal 0 (I_OUT) | `ai0`, diferencial: AI0 (+) = I_OUT, AI8 (−) = V_REF |
| Canal 1 (Q_OUT) | `ai1`, diferencial: AI1 (+) = Q_OUT, AI9 (−) = V_REF |
| Canal 2 (VCC_A) | `ai2`, diferencial: AI2 (+) = VCC_A, AI10 (−) = GND |
| Rango | ±5 V en los tres canales (LSB ≈ 153 µV) |
| Modo | Continuo |
| Frecuencia de muestreo | 1000 muestras/s por canal |
| Tamaño de bloque | 100 muestras por canal (0.1 s) |

Por ser diferenciales, el NI entrega directamente **Iraw = I_OUT − V_REF** y **Qraw = Q_OUT − V_REF**, en voltios.

La arquitectura productor/consumidor usa una cola de "arreglo de waveforms". Al detener el VI, el productor envía un arreglo vacío como señal de fin, el consumidor se detiene al recibirlo y la cola se libera al final. Nada de esto se necesita en el PIC.

---

## 4. Procesamiento paso a paso (lazo consumidor)

### 4.1 Variables y unidades

| Símbolo | Significado | Unidad | Alcance |
|---|---|---|---|
| `Iraw[n]`, `Qraw[n]` | I_OUT − V_REF, Q_OUT − V_REF | V | Por muestra (100 por bloque) |
| `Vcc[n]` | VCC_A medido | V | Por muestra |
| `G` | Ganancia DC seleccionada (2, 11, 101) | — | Control |
| `corr` | Casilla "Corrección signo I" (normalmente verdadera) | booleano | Control |
| `Ip[n]`, `Qp[n]` | Señales procesadas: nivel a la salida del mezclador, normalizadas a 3.7 V | mV | Por muestra |
| `I0`, `Q0` | Cero (punto de metal sano) | mV | Estado persistente |
| `Ic[n]`, `Qc[n]` | Señales sin cero | mV | Por muestra |
| `θ` | Ángulo de calibración (dirección del lift‑off) | grados | Estado persistente |
| `I1[n]`, `Q1[n]` | Señales finales, I′ y Q′ | mV | Por muestra |
| `r[n]`, `φ[n]` | Polar de cada muestra final | mV, grados | Por muestra |
| `|V|`, `Fase`, `d` | Indicadores del bloque | mV, grados, ×10⁻⁴ | Por bloque |

### 4.2 P1 — VCC_A y protección

```
Vcc_blk  = media(Vcc[0..99])
valido   = (Vcc_blk > 2.5)
Vused    = valido ? Vcc_blk : 3.7
LED_vcc  = NOT valido          // "VCC_A fuera de rango"
```

### 4.3 P2 — Factor de escala

```
k   = (3.7 / Vused) / G * 1000          // de V crudos a mV normalizados
kI  = corr ? (-1 * k) : (+1 * k)        // signo de I
```

En el VI: `Divide(3.7, Vused)` → `Divide(·, G)` → `Multiply(·, 1000)` = k; `Select(corr, −1, 1)` → `Multiply(k, ·)` = kI.

### 4.4 P3 — Señales procesadas (gráfica "I y Q procesadas (mV)")

```
Ip[n] = kI * Iraw[n]
Qp[n] = k  * Qraw[n]
```

`Ip`/`Qp` son la salida del mezclador en mV, independientes de la ganancia DC y de la tensión de la celda.

### 4.5 P4 — Promedios del bloque

```
mI = media(Ip[0..99])
mQ = media(Qp[0..99])
```

### 4.6 P5 — Cero (botones "Fijar cero" y "Borrar 0")

Los botones son de tipo *latch*: cada pulsación vale **un solo bloque**. `bloque_valido` es falso solo en el bloque vacío de fin (en el PIC siempre es verdadero).

```
fijar = Fijar_cero AND bloque_valido
I0_tmp = fijar ? mI : I0_anterior
Q0_tmp = fijar ? mQ : Q0_anterior
I0 = Borrar_0 ? 0 : I0_tmp
Q0 = Borrar_0 ? 0 : Q0_tmp
// I0, Q0 se guardan (shift register) para el siguiente bloque y se muestran en "I0 (mV)", "Q0 (mV)"
```

Importante: el cero capturado en un bloque **se aplica a ese mismo bloque**.

### 4.7 P6 — Señales sin cero

```
Ic[n] = Ip[n] - I0
Qc[n] = Qp[n] - Q0
mIc = mI - I0
mQc = mQ - Q0
```

### 4.8 P7 — Ángulo θ (botones "Calibrar theta" y "theta = 0")

```
theta_med = atan2(mQc, mIc) * 180 / π       // atan2(y, x): y = mQc, x = mIc; resultado en (−180°, 180°]
cal = Calibrar_theta AND bloque_valido
th_tmp = cal ? theta_med : theta_anterior
θ = theta_0 ? 0 : th_tmp
// θ se guarda (shift register) y se muestra en "theta (°)"
```

Uso: con la sonda quieta sobre papel (lift‑off conocido), se pulsa "Calibrar theta". Así θ es la dirección del lift‑off y, después de rotar, el lift‑off cae sobre el eje de 0°. El θ capturado también se aplica al mismo bloque.

### 4.9 P8 — Rotación (gráfica "I y Q (mV)")

```
θr = θ * π / 180
c = cos(θr);  s = sin(θr)
I1[n] = Ic[n]*c + Qc[n]*s       // I′
Q1[n] = Qc[n]*c - Ic[n]*s       // Q′
```

En el VI: multiplicadores M1 = c·Ic y M2 = s·Qc van al sumador (I′); M3 = s·Ic y M4 = c·Qc van al restador, con **M4 arriba y M3 abajo** (Q′ = M4 − M3). Equivale a rotar cada punto −θ: un punto con ángulo α pasa a tener ángulo α − θ, y su distancia al origen no cambia.

### 4.10 P9 — Plano polar (traza)

Por cada muestra final:

```
r[n] = sqrt(I1[n]^2 + Q1[n]^2)                  // mV
φ[n] = atan2(Q1[n], I1[n]) * 180 / π            // grados
```

Implementación en el VI:

1. `Re/Im To Polar` (x = I1, y = Q1) → r y theta (rad) → theta × 180/π.
2. `Index & Bundle Cluster Array` → arreglo de pares (r, φ).
3. Traza con memoria (shift register):
   ```
   T = concatenar(T_anterior, pares_del_bloque)
   ini = max(tamaño(T) - Npuntos, 0)            // Npuntos = "Puntos en traza" (2000 a 12000)
   T = subarreglo(T, ini, Npuntos)              // se quedan los últimos Npuntos
   T = Borrar_traza ? vacío : T
   ```
4. `Polar Plot.vi` dibuja T en un indicador 2D Picture de 400 × 400 px.
   - Ángulo 0° a la derecha, 90° arriba (antihorario positivo).
   - **Autoescala:** el anillo exterior vale el máximo r de la traza (en la medida real, 73 mV). Hay anillos intermedios y rayos radiales.
   - Los puntos se unen con líneas en el orden de llegada.
5. `Array Size(T)` → indicador "puntos actuales".

Clave para el PIC: dibujar (r, φ) en un plano polar es lo mismo que dibujar el punto **(x, y) = (I1, Q1)** en un plano cartesiano con la misma escala en ambos ejes. Para dibujar no hace falta calcular r ni φ por muestra.

### 4.11 P10 — Indicadores del bloque

```
mI1 = media(I1[0..99]);  mQ1 = media(Q1[0..99])
|V|  = sqrt(mI1^2 + mQ1^2)                  // "|V| (mV)"
Fase = atan2(mQ1, mI1) * 180 / π            // "Fase (°)"
d    = |V| / mV_por_1e4                     // "d estimada (x10^-4)", mV_por_1e4 = 4.03
```

- La Fase se mide desde el eje del lift‑off (porque ya se rotó θ): ≈ 0° es lift‑off; lejos de 0°/180° es un defecto.
- Con |V| ≈ 0, la Fase salta al azar; es normal.
- d incluye el lift‑off. Para la grieta sola: `|Q1|/4.03`.

### 4.12 Otros indicadores

| Indicador | Cálculo |
|---|---|
| VCC_A (V) | `Vcc_blk` |
| elementos en cola | Tamaño de la cola (solo LabVIEW) |
| puntos actuales | Tamaño de la traza |

---

## 5. Botones y controles: comportamiento exacto

| Control | Tipo | Efecto |
|---|---|---|
| Ganancia DC | Selector 2/11/101 | Divisor G. Debe coincidir con JP701/JP702 |
| Corrección signo I | Casilla (por defecto ON) | kI = −k |
| VCC nominal | Numérico (3.7) | Normalización |
| mV por 10⁻⁴ | Numérico (4.03) | Divisor de d |
| Puntos en traza | Entero (2000; 12000 en la medida real) | Longitud de la traza |
| Fijar cero | Pulsador (latch) | I0 = mI, Q0 = mQ del bloque actual |
| Borrar 0 | Pulsador | I0 = Q0 = 0 |
| Calibrar theta | Pulsador | θ = atan2(mQc, mIc) del bloque actual |
| theta = 0 | Pulsador | θ = 0 |
| Borrar traza | Pulsador | Vacía la traza |
| Grabar | Interruptor | Habilita *Write To Measurement File* (guarda I′, Q′ y un comentario) |

El estado persistente es: **I0, Q0, θ y la traza**. Se reinician a 0 / vacío al arrancar el VI.

---

## 6. Rangos y resolución en LabVIEW

| Ganancia | 1 LSB del NI (153 µV) en mV procesados | Ruido típico en mV procesados |
|---|---|---|
| ×2 | 0.077 mV | ≈ 0.06 mV rms (dominado por el NI) |
| ×11 | 0.014 mV | — |
| ×101 | 0.0015 mV | ≈ 0.007 mV rms (0.70 mV rms crudos / 101) |

Una grieta de 10⁻⁴ produce 4.03 mV procesados (8 mV crudos en ×2, 0.41 V en ×101).

---

## 7. Lo que muestran las tres gráficas del VI

| Gráfica | Señal | Etapa |
|---|---|---|
| I y Q crudas (V) | Iraw, Qraw | Sin procesar |
| I y Q procesadas (mV) | Ip, Qp | Después de P3 |
| I y Q (mV) | I1, Q1 (I′, Q′) | Después de P8 |

---

## 8. Funciones del VI que **no** se portan al PIC

| Función en LabVIEW | Por qué no se porta | Alternativa en el PIC |
|---|---|---|
| SINAD y THD (*SINAD Analyzer*, *Harmonic Distortion Analyzer*) sobre buffers móviles de 10 000 muestras por canal | Necesitan una FFT de 10 000 puntos: decenas de KB de RAM. El PIC tiene 2 KB | Calcular la **SNR DC**, 20·log(|media|/σ), con sumas acumuladas (Welford); o enviar los datos al PC |
| Espectro (*FFT Power Spectrum*) | Igual | En el PC |
| *Write To Measurement File* | No hay sistema de archivos | Enviar por UART y guardar en el PC |
| Cola productor/consumidor y señal de fin | Propio de LabVIEW | Interrupción de temporizador + lazo principal |

Además, el SINAD de la salida solo tiene sentido con un tono de prueba: con la excitación propia la salida es DC (en la medida real daba −1.57 dB y THD 105 %, valores sin significado).

---

## 9. Vectores de prueba

### 9.1 Pruebas con valores conocidos (validadas en LabVIEW)

Todas con Vcc = 3.7 V (sin efecto de normalización) y Corrección signo I = ON.

| # | Entrada (Iraw, Qraw) en V | Configuración | Resultado esperado |
|---|---|---|---|
| 1 | (1.000, −0.500) | G = 2, sin cero, θ = 0 | Ip = −500 mV, Qp = −250 mV |
| 2 | (1.000, −0.500) | G = 11 | Ip = −90.9 mV, Qp = −45.5 mV |
| 3 | (1.000, −0.500) | G = 101 | Ip = −9.90 mV, Qp = −4.95 mV |
| 4 | (1.000, −0.500), luego "Fijar cero" | G = 2 | I0 = −500, Q0 = −250; I1 = Q1 ≈ 0 |
| 5 | Cero de #4; entrada (1.200, −0.500) | θ = 0 | I1 = −100 mV, Q1 ≈ 0 |
| 6 | Cero de #4; entrada (1.300, −0.200) y "Calibrar theta" | G = 2 | Ic = −150, Qc = +150 → θ = 135°; I1 = +212.1 mV, Q1 ≈ 0; |V| = 212.1, Fase ≈ 0° |
| 7 | Cero y θ de #6; entrada (1.600, 0.100) | — | I1 = +424.3 mV, Q1 ≈ 0 |
| 8 | Cero y θ de #6; entrada (1.300, −0.800) | — | I1 ≈ 0, Q1 = +212.1 mV; Fase = 90° |
| 9 | Desactivar la corrección con la entrada de #1 | G = 2 | Ip = +500 mV |
| 10 | Vcc = 3.0 V, entrada (1.000, −0.500) | G = 2 | Ip = −616.7 mV (factor 3.7/3.0) |
| 11 | Vcc < 2.5 V | — | Se usa 3.7 V; LED encendido |
| 12 | Batido: Iraw = 0.5·cos(2π·4t), Qraw = 0.5·sin(2π·4t) | G = 2, sin cero | Círculo de radio 250 mV; con la corrección de signo gira en sentido **horario** |

Prueba de ruido en LabVIEW: en ×2, tras el cero, I1 y Q1 oscilan ±0.25 mV (ruido del NI).

### 9.2 Medida real con la tarjeta (23/10/2026, 2:41 a. m.)

Comentario guardado: *"dc ganancia x2, primero sección sana luego defecto luego sano"*.

| Magnitud | Valor |
|---|---|
| Ganancia | ×2, corrección de signo ON |
| VCC_A | 3.65321 V |
| I0 / Q0 | 62.3276 mV / 21.1352 mV |
| θ | 0 (no calibrado) |
| Puntos en traza | 12 000 (12 s) |
| |V| (bloque al imprimir) | 24.3293 mV |
| Fase | 59.3775° |
| d estimada | 6.03704 (×10⁻⁴) = 24.3293 / 4.03 |
| Traza polar | Línea recta que pasa por el origen a ≈ 60° / 240°, con extremos de unos 60–70 mV (escala del anillo exterior: 73 mV) |

Interpretación: es la firma de la sonda diferencial, con los dos lóbulos (bobina A y bobina B) alineados sobre la misma recta, a unos 60° del eje I (θ no estaba calibrado). Sirve para comparar el resultado del PIC con el de LabVIEW si se repite la pasada.

---

## 10. Propuesta de implementación en el PIC16F18426

### 10.1 Recursos del microcontrolador (verificar en la hoja de datos)

| Recurso | Valor esperado |
|---|---|
| Núcleo | PIC16 de 8 bits, sin multiplicador por hardware |
| Reloj | Oscilador interno HFINTOSC de hasta 32 MHz → 8 MIPS |
| Memoria | 28 KB de Flash (16 K palabras), 2 KB de RAM, 256 B de EEPROM |
| ADC | ADC² / ADCC de 12 bits, de una sola entrada (no diferencial), con modos de promedio y acumulación por hardware |
| Referencias del ADC | VDD, pin externo o FVR (1.024 / 2.048 / 4.096 V) |
| Comunicación | EUSART (UART), MSSP (I2C/SPI), con PPS para asignar pines |
| Temporizadores | TMR0, TMR1, TMR2 |
| Encapsulado | 14 pines |

### 10.2 Diferencias de diseño frente a LabVIEW

| Aspecto | LabVIEW (NI USB‑6211) | PIC16F18426 |
|---|---|---|
| Entradas | Diferenciales | De una sola entrada: hay que medir I_OUT, Q_OUT y V_REF por separado y restar |
| Resolución | 16 bits en ±5 V (153 µV) | 12 bits en 0–VDD (≈ 0.9 mV con VDD = 3.7 V) |
| Muestreo | 1000 muestras/s, bloques de 100 | Propuesta: 100 muestras/s de salida (cada una promedio de 16 conversiones por canal) y bloques de 10 muestras (0.1 s, igual que LabVIEW) |
| Normalización | Divide por VCC_A medido | **Ratiométrica:** dividir por la lectura de VCC_A; VDD se cancela (sección 10.4) |
| Plano polar | 2D Picture, 400 × 400 | UART → PC, u OLED SSD1306 de 128 × 64 (sección 10.7) |
| Traza | Hasta 12 000 puntos | Buffer circular de 64–128 puntos (por la RAM) |
| SINAD/THD | Sí | No (sección 8) |

Resolución en el PIC (VDD = 3.7 V, 1 cuenta = 0.904 mV crudos):

| Ganancia | 1 cuenta en mV procesados | d mínima apreciable |
|---|---|---|
| ×2 | 0.452 mV | ≈ 0.11·10⁻⁴ |
| ×11 | 0.082 mV | ≈ 0.02·10⁻⁴ |
| ×101 | 0.009 mV | ≈ 0.002·10⁻⁴ |

En ×2 el ruido de la tarjeta (≈ 13 µV crudos) es mucho menor que una cuenta, así que promediar no mejora la resolución (no hay *dither*). Para defectos pequeños conviene ×11 o ×101.

### 10.3 Mapa de pines propuesto (14 pines; confirmar con la hoja de datos)

| Pin | Puerto | Función propuesta | Conexión |
|---|---|---|---|
| 1 | VDD | Alimentación | VCC de la tarjeta (ver 10.4) |
| 14 | VSS | Tierra | J805.6 (GND) |
| 10 | RC0 / ANC0 | ADC: I_OUT | J805.1 |
| 9 | RC1 / ANC1 | ADC: Q_OUT | J805.3 |
| 8 | RC2 / ANC2 | ADC: V_REF | J805.2 |
| 7 | RC3 / ANC3 | ADC: VCC_A | J805.5 |
| 6 | RC4 | UART TX (PPS) | Adaptador USB‑serie (RX) |
| 5 | RC5 | UART RX (PPS) | Adaptador USB‑serie (TX) |
| 11 | RA2 | Pulsador "Fijar cero" (pull‑up interno, activo en bajo) | — |
| 3 | RA4 | Pulsador "Calibrar θ" | — |
| 2 | RA5 | LED de estado / saturación | — |
| 13 | RA0 / ICSPDAT | Programación (opcional: SDA del OLED) | — |
| 12 | RA1 / ICSPCLK | Programación (opcional: SCL del OLED) | — |
| 4 | RA3 / MCLR | Reset (pull‑up de 10 kΩ) | — |

El resto de las órdenes (borrar cero, θ = 0, ganancia, borrar traza) se pueden dar por UART.

### 10.4 ADC y normalización ratiométrica

**Referencia del ADC: VDD.** Conversiones sucesivas de cuatro canales, todas contra el mismo VDD:

```
cI = código de I_OUT     cQ = código de Q_OUT
cR = código de V_REF     cV = código de VCC_A
```

Como `tensión = código · VDD / 4095`, la normalización de LabVIEW queda:

```
Ip [mV] = kI_signo · (cI − cR) / cV · 3.7 V · 1000 / G
        = signo · (cI − cR) · 3700 / (cV · G)
Qp [mV] = (cQ − cR) · 3700 / (cV · G)
```

VDD **se cancela**: no hace falta conocerlo ni que sea estable, y el resultado ya está normalizado a VCC_A = 3.7 V, exactamente como en LabVIEW.

- **Alimentación del PIC:** desde VCC de la tarjeta, no desde VCC_A, para no meter ruido digital en la parte analógica. La normalización anterior funciona igual. Los rangos quedan bien: I_OUT, Q_OUT, V_REF y VCC_A están entre 0 y VDD.
- **VCC_A en voltios** (para el LED de batería baja o de fuera de rango): convertir también el canal interno FVR de 1.024 V y usar `VCC_A = 1.024 · cV / cFVR`. Es opcional.
- **Sobremuestreo:** modo *Burst Average* del ADCC con 16 repeticiones por canal (ADRPT = 16): el registro acumulador da la suma de 16 conversiones. A 100 muestras/s × 4 canales × 16 conversiones son unas 6400 conversiones/s; con un TAD de 1 µs (≈ 15 µs por conversión) ocupa menos del 10 % del tiempo.
- **Saturación:** las salidas saturan a unos 50 mV de los rieles de VCC_A. Señalarla si `cI < 0.015·cV` o `cI > 0.985·cV` (igual para cQ).
- **Configuración:** MCC Classic tiene el módulo ADCC y genera la inicialización (reloj del ADC, referencia, canal, modo y tiempo de adquisición). Los nombres de los registros (ADCON0/2, ADPCH, ADREF, ADRPT, ADACC, ADFLTR, ADACQ…) deben verificarse en la hoja de datos.

### 10.5 Formato numérico

**Versión 1 (recomendada para empezar): `float` de 32 bits.**

- En XC8, configurar float/double de 32 bits en las opciones globales del proyecto.
- Presupuesto: a 32 MHz hay 80 000 ciclos de instrucción cada 10 ms. Las operaciones en coma flotante por software cuestan del orden de cientos a miles de ciclos, y sin/cos/atan2 del orden de 10⁴. El camino por muestra (4 multiplicaciones, 2 restas, 2 sumas) cabe holgado.
- `cos θ` y `sin θ` se calculan **solo** al cambiar θ, no por muestra.
- `atan2` y `sqrt` solo se necesitan para |V|, Fase y d (una vez por bloque) y, si se dibuja en polar, no por punto (sección 4.10).

**Versión 2 (optimización): punto fijo.**

- Trabajar en cuentas sobremuestreadas (`int32_t`), con `cos θ` y `sin θ` en formato Q15 (`int16_t`), y rotar con `(int32_t)a·c + (int32_t)b·s >> 15`.
- Convertir a mV solo al enviar los datos.
- `sqrt`: raíz cuadrada entera de un `uint32_t`.
- `atan2` aproximado (error máximo ≈ 0.1°), con `z = y/x` cuando |y| ≤ |x|:
  ```
  atan(z) ≈ (π/4)·z − z·(|z| − 1)·(0.2447 + 0.0663·|z|)
  ```
  y luego ajustar por octante.

### 10.6 Pseudocódigo del lazo (versión 1, en C)

```c
// ---------- Constantes ----------
#define VCC_NOM_MV    3700.0f
#define MV_POR_1E4    4.03f
#define N_BLOQUE      10        // 10 muestras de salida a 100 Hz = 0.1 s (igual que el bloque de LabVIEW)
#define N_TRAZA       128       // puntos guardados para el plano polar

// ---------- Estado persistente (equivale a los shift registers del VI) ----------
static float I0 = 0.0f, Q0 = 0.0f;           // cero, en mV
static float theta_deg = 0.0f;               // ángulo de calibración
static float c_th = 1.0f, s_th = 0.0f;       // cos y sin de theta
static uint8_t G = 2;                        // debe coincidir con JP701/JP702
static bool corr_signo = true;               // la placa fabricada invierte I

// ---------- Acumuladores del bloque ----------
static float sum_Ip, sum_Qp, sum_I1, sum_Q1;
static uint8_t n_blk = 0;

// ---------- Eventos (pulsadores u órdenes por UART); cada uno vale un bloque ----------
volatile bool ev_fijar, ev_borrar0, ev_calibrar, ev_theta0, ev_borrar_traza;

// Se llama cada 10 ms con los códigos sobremuestreados de los cuatro canales
void procesar_muestra(int32_t cI, int32_t cQ, int32_t cR, int32_t cV)
{
    // P1-P3: escala ratiométrica (VDD se cancela) y signo de I
    float k  = VCC_NOM_MV / ((float)cV * (float)G);   // mV por unidad de (cX − cR)
    float kI = corr_signo ? -k : k;
    float Ip = kI * (float)(cI - cR);
    float Qp = k  * (float)(cQ - cR);

    // P6: sin cero (se usa el cero vigente)
    float Ic = Ip - I0;
    float Qc = Qp - Q0;

    // P8: rotación
    float I1 = Ic * c_th + Qc * s_th;
    float Q1 = Qc * c_th - Ic * s_th;

    // P9: traza (para dibujar basta el par cartesiano)
    traza_agregar(I1, Q1);
    uart_enviar_muestra(Ip, Qp, I1, Q1);     // enteros en µV o 0.01 mV; evitar printf("%f")

    // Acumular para el bloque
    sum_Ip += Ip;  sum_Qp += Qp;  sum_I1 += I1;  sum_Q1 += Q1;
    if (++n_blk < N_BLOQUE) return;

    // ---------- Fin de bloque (cada 0.1 s) ----------
    float mI  = sum_Ip / N_BLOQUE,  mQ  = sum_Qp / N_BLOQUE;
    float mI1 = sum_I1 / N_BLOQUE,  mQ1 = sum_Q1 / N_BLOQUE;
    sum_Ip = sum_Qp = sum_I1 = sum_Q1 = 0.0f;  n_blk = 0;

    // P5: cero
    if (ev_fijar)   { I0 = mI; Q0 = mQ; }
    if (ev_borrar0) { I0 = 0;  Q0 = 0;  }

    // P7: theta (con el cero ya actualizado)
    if (ev_calibrar) {
        float mIc = mI - I0, mQc = mQ - Q0;
        theta_deg = atan2f(mQc, mIc) * 57.29578f;
        c_th = cosf(theta_deg * 0.01745329f);
        s_th = sinf(theta_deg * 0.01745329f);
    }
    if (ev_theta0) { theta_deg = 0; c_th = 1; s_th = 0; }
    if (ev_borrar_traza) traza_borrar();
    ev_fijar = ev_borrar0 = ev_calibrar = ev_theta0 = ev_borrar_traza = false;

    // P10: indicadores del bloque
    float V    = sqrtf(mI1 * mI1 + mQ1 * mQ1);    // |V| en mV
    float fase = atan2f(mQ1, mI1) * 57.29578f;    // grados
    float d    = V / MV_POR_1E4;                  // en unidades de 1e-4
    uart_enviar_bloque(V, fase, d, I0, Q0, theta_deg);
}
```

**Diferencia aceptada respecto a LabVIEW:** en el VI, el cero y θ capturados en un bloque se aplican a ese mismo bloque; en este esquema se aplican desde la muestra siguiente. La diferencia dura como mucho 0.1 s.

**Temporización:** TMR2 (o TMR0) a 10 ms activa una bandera; el lazo principal hace las cuatro conversiones (I, Q, V_REF, VCC_A) y llama a `procesar_muestra`. Los pulsadores se leen con antirrebote y por flanco, para imitar el comportamiento *latch* de LabVIEW.

### 10.7 Salida de datos y plano polar

**Opción A (recomendada para empezar): UART → PC.**

- EUSART a 115 200 baudios (a 32 MHz, con BRG16 = 1 y BRGH = 1, el error es de ≈ 0.6 %).
- Una línea por muestra (100 por segundo), con números enteros, por ejemplo en µV:
  ```
  t_ms,Ip_uV,Qp_uV,I1_uV,Q1_uV
  ```
- Una línea por bloque (10 por segundo):
  ```
  B,V_uV,fase_cdeg,d_x1e4_milesimas,I0_uV,Q0_uV,theta_cdeg
  ```
- Órdenes de un carácter: `z` fijar cero, `x` borrar cero, `c` calibrar θ, `t` θ = 0, `b` borrar traza, `2`/`1`/`0` ganancia ×2/×11/×101, `s` alternar la corrección de signo.
- En el PC, un script de Python (pyserial + matplotlib) dibuja el plano polar como un gráfico XY de (I1, Q1) con escalas iguales, y puede guardar CSV para el informe.

**Opción B (sistema autónomo): OLED SSD1306 de 128 × 64 por I2C (MSSP).**

- Plano en un área de 64 × 64 px: centro (32, 32), radio de 30 px.
- Punto en pantalla: `px = 32 + I1·k`, `py = 32 − Q1·k`, con `k = 30 / r_max`.
- `r_max` es el máximo de la traza (autoescala como LabVIEW) o una escala fija elegida con un botón.
- Dibujar los ejes, un círculo en r_max y otro en r_max/2, y los puntos de la traza unidos con líneas (Bresenham).
- El resto de la pantalla, con texto: |V|, Fase, d, G.
- El framebuffer ocupa 1 KB de los 2 KB de RAM: cabe, pero ajustado. Alternativa: dibujar por páginas sin framebuffer completo.

### 10.8 Memoria (estimación)

| Uso | RAM |
|---|---|
| Traza de 128 puntos (2 × `int8_t` en coordenadas de pantalla, o 2 × `int16_t`) | 256–512 B |
| Estado y acumuladores (`float`) | < 100 B |
| Buffer de UART | 64–128 B |
| Framebuffer OLED (solo opción B) | 1024 B |

Opcional: guardar I0, Q0, θ, G y la corrección de signo en la EEPROM para conservarlos al apagar.

---

## 11. Tabla de equivalencias LabVIEW → PIC

| LabVIEW | PIC16F18426 |
|---|---|
| DAQ Assistant (diferencial, 1 kS/s, bloques de 100) | ADCC: 4 canales de una sola entrada contra VDD, promedio de 16 por canal, 100 muestras/s |
| Cola productor/consumidor | Interrupción de temporizador + lazo principal |
| Mean de VCC_A, protección de 2.5 V y LED | `cV` en la fórmula ratiométrica; FVR opcional para conocer VCC_A en V |
| Factor k (3.7/Vcc/G·1000) y signo de I | `3700/(cV·G)` y `corr_signo` |
| Shift registers I0, Q0, θ | Variables `static` (y EEPROM opcional) |
| Botones *latch* | Pulsadores con antirrebote por flanco u órdenes UART |
| Rotación (4 multiplicadores, suma y resta) | Dos líneas de C con `c_th`, `s_th` precalculados |
| Re/Im To Polar + Polar Plot (400 × 400) | Gráfico XY (I1, Q1) con escalas iguales: en el PC o en el OLED |
| Traza de hasta 12 000 puntos | Buffer circular de 64–128 puntos |
| \|V\|, Fase, d estimada | Una vez por bloque: `sqrtf`, `atan2f`, división por 4.03 |
| SINAD, THD, espectro | No se portan; SNR DC o análisis en el PC |
| Write To Measurement File | Envío por UART y registro en el PC |

---

## 12. Decisiones abiertas para el chat nuevo

1. **Salida:** solo UART al PC (opción A), OLED autónomo (opción B), o ambas.
2. **Entradas de usuario:** cuántos pulsadores físicos y qué se hace por UART.
3. **Ganancia:** ¿se fija por orden UART o con interruptores (2 pines)?
4. **Alimentación del PIC:** desde VCC de la tarjeta (recomendado), y cómo llevar la tierra.
5. **Formato numérico:** `float` (versión 1) o punto fijo (versión 2).
6. **Programador:** si hay un programador físico (PICkit, MPLAB Snap) o se trabajará primero en el simulador de MPLAB X, alimentando el procesamiento con los vectores de la sección 9.
7. **Frecuencia de salida:** 100 muestras/s (propuesta) u otra; con 50 o 25 la traza cubre más tiempo con la misma RAM.

---

## 13. Prompt sugerido para el chat nuevo

> Adjunto un documento .md con la especificación del procesamiento I/Q y el plano polar de un VI de LabVIEW (`P2_IQ_SINAD.vi`) para un detector de fisuras por corrientes de Eddy. Quiero recrear ese procesamiento en un **PIC16F18426** usando **MPLAB X, el compilador XC8 y MCC Classic**.
>
> Trabajemos paso a paso: dame un paso, espera a que lo implemente y te confirme, y solo entonces pasa al siguiente. Empecemos por:
> 1. Crear el proyecto y configurar con MCC Classic el reloj (HFINTOSC a 32 MHz), los pines, el ADCC (4 canales, referencia VDD, modo Burst Average con 16 repeticiones), un temporizador de 10 ms y la EUSART a 115 200 baudios.
> 2. Leer los cuatro canales y enviar los códigos crudos por UART para verificarlos.
> 3. Implementar el procesamiento de las secciones 4 y 10.6 y comprobarlo con los vectores de prueba de la sección 9.
> 4. Implementar la salida del plano polar (empezando por la opción A: UART y script de Python en el PC).
>
> Respeta las fórmulas, constantes y el orden de operaciones del documento. Las decisiones abiertas están en la sección 12: pregúntame por ellas cuando lleguemos a cada punto.
