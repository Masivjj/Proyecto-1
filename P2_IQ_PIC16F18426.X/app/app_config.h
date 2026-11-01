/*
 * app_config.h - Constantes y opciones de compilacion de la aplicacion.
 *
 * Los valores salen de la especificacion "Procesamiento I/Q y plano polar del
 * VI P2_IQ_SINAD.vi" (docs/P2_IQ_procesamiento_para_PIC16F18426.md),
 * secciones 1.4 y 10. Este archivo lo usan tanto el firmware (XC8) como las
 * pruebas en el PC (tests/).
 *
 * Las opciones con #ifndef tambien se pueden fijar sin tocar este archivo:
 * Project Properties > XC8 Compiler > Preprocessing > Define macros
 * (por ejemplo GANANCIA_INICIAL=11).
 */
#ifndef APP_CONFIG_H
#define APP_CONFIG_H

/* ------------------------------------------------------------------------
 * Constantes del sistema (seccion 1.4)
 * ------------------------------------------------------------------------ */
#define VCC_NOM_V           3.7f    /* "VCC nominal": normalizacion ratiometrica       */
#define VCC_UMBRAL_V        2.5f    /* VCC_A <= 2.5 V: se usa 3.7 V y se enciende LED  */
#define MV_POR_1E4          4.03f   /* "mV por 10^-4": senal de una grieta d = 1e-4    */

/* Ganancia DC al arrancar sin datos en la EEPROM: 2, 11 o 101.
 * DEBE coincidir con los jumpers JP701 (canal I) y JP702 (canal Q). */
#ifndef GANANCIA_INICIAL
#define GANANCIA_INICIAL    2u
#endif

/* Etapas del procesamiento activas al arrancar sin datos en la EEPROM
 * (1 = activa, 0 = desactivada). En marcha se alternan con las ordenes
 * Z, T, G, N y A por UART o con las casillas del programa del PC, y el
 * estado se guarda en la EEPROM. */
#ifndef CERO_ACTIVO
#define CERO_ACTIVO             1   /* restar el cero I0, Q0 ("Fijar cero")      */
#endif
#ifndef THETA_ACTIVO
#define THETA_ACTIVO            1   /* rotar por theta ("Calibrar theta")         */
#endif
#ifndef GANANCIA_ACTIVA
#define GANANCIA_ACTIVA         1   /* dividir por la ganancia DC G               */
#endif
#ifndef NORMALIZACION_ACTIVA
#define NORMALIZACION_ACTIVA    1   /* normalizar a VCC_A = 3.7 V (calibracion)   */
#endif
#ifndef AUTOCERO_ACTIVO
#define AUTOCERO_ACTIVO         1   /* restar el offset del ADC medido en AVSS    */
#endif
#define ETAPAS_INICIALES    ((CERO_ACTIVO ? 0x01u : 0u) | (THETA_ACTIVO ? 0x02u : 0u) | \
                             (GANANCIA_ACTIVA ? 0x04u : 0u) | (NORMALIZACION_ACTIVA ? 0x08u : 0u) | \
                             (AUTOCERO_ACTIVO ? 0x10u : 0u))

/* La correccion del signo de I (la tarjeta fabricada lo entrega invertido,
 * seccion 1.3) ya no es una opcion: se aplica siempre (iq_proc.c, P3). */

/* ------------------------------------------------------------------------
 * Reloj del sistema (docs/reloj_externo_4MHz.md)
 *
 * RELOJ_INTERNO         HFINTOSC del PIC a 32 MHz (como hasta ahora). LED en
 *                       RA5, "Calibrar theta" en RA4, disparo del PWM6 en RA1.
 * RELOJ_CRISTAL_4MHZ    cristal de cuarzo de 4 MHz entre RA5 (OSC1, pin 2) y
 *                       RA4 (OSC2, pin 3), con sus dos condensadores. El PLL
 *                       x4 da FOSC = 16 MHz. El LED pasa a RA0 (pin 13) y
 *                       "Calibrar theta" a RA1 (pin 12), asi que el disparo
 *                       del PWM6 ya no sale por RA1 (salvo SALIDA_DISPARO=1,
 *                       que deja sin ese pulsador: queda la orden 'c').
 * RELOJ_OSCILADOR_4MHZ  oscilador de cuarzo de 4 MHz con salida CMOS (modulo
 *                       de 4 patas) a RA5 (CLKIN, pin 2). PLL x4 = 16 MHz.
 *                       Solo el LED cambia de pin: pasa a RA0 (pin 13).
 *
 * Con reloj externo el PIC arranca con el HFINTOSC a 16 MHz y pasa al
 * externo (app/reloj.c). Si el externo no arranca en 100 ms, o se para
 * despues (monitor de fallos FSCM), sigue con el HFINTOSC a 16 MHz: las
 * mismas frecuencias de muestreo, con su +-2 %. La orden '?' dice que reloj
 * esta en uso y los bloques llevan la bandera 64 mientras se usa el de
 * reserva. Los registros de MCC tienen los valores de cada caso (#if RELOJ).
 *
 * El modo se elige al compilar (aqui o en Project Properties > XC8 Compiler >
 * Preprocessing > Define macros, por ejemplo RELOJ=1): los bits de
 * configuracion del oscilador y el uso de RA4/RA5 no pueden cambiar en marcha.
 * ------------------------------------------------------------------------ */
#define RELOJ_INTERNO           0
#define RELOJ_CRISTAL_4MHZ      1
#define RELOJ_OSCILADOR_4MHZ    2
#ifndef RELOJ
#define RELOJ               RELOJ_INTERNO
#endif

#if RELOJ == RELOJ_INTERNO
#define FOSC_HZ             32000000UL  /* HFINTOSC, OSCFRQ = 32 MHz                 */
#elif (RELOJ == RELOJ_CRISTAL_4MHZ) || (RELOJ == RELOJ_OSCILADOR_4MHZ)
#define FOSC_HZ             16000000UL  /* 4 MHz x PLL 4 (o HFINTOSC a 16 MHz de reserva) */
#else
#error "RELOJ debe ser RELOJ_INTERNO (0), RELOJ_CRISTAL_4MHZ (1) o RELOJ_OSCILADOR_4MHZ (2)"
#endif
#define RELOJ_EXTERNO       (RELOJ != RELOJ_INTERNO)
#define F_EXTERNO_HZ        4000000UL

/* Cristal: 0 = modo HS (ganancia alta: arranque seguro a 4 MHz, el limite
 * entre XT y HS), 1 = modo XT (ganancia media, menos potencia en el cristal) */
#ifndef CRISTAL_XT
#define CRISTAL_XT          0
#endif

/* ------------------------------------------------------------------------
 * Adquisicion (seccion 10.4 y docs/analisis_adc_jitter.md)
 *
 * ADC en modo basico. Cada conversion la dispara por hardware el flanco de
 * subida del PWM6 (TMR2, periodo PERIODO_DISPARO_US = 100 us): el instante
 * de muestreo no depende del software y su jitter es el del oscilador.
 *
 * Los disparos se agrupan en rondas de 1 ms (DISPAROS_POR_RONDA = 10):
 * I_OUT, Q_OUT, V_REF, VCC_A, FVR, AVSS y 4 disparos libres. Cada entrada
 * se convierte una vez por ronda, siempre en la misma posicion, asi que
 * cada canal se muestrea a 1000 muestras/s con un periodo fijo de 1 ms,
 * como el DAQ Assistant del VI (1 kS/s por canal). Una trama de 10 ms son
 * N_SOBREMUESTREO = 10 rondas: la muestra de salida de cada canal es la
 * suma de sus 10 conversiones (100 S/s de salida, bloques de 10 = 0.1 s).
 * ------------------------------------------------------------------------ */
#define PERIODO_MS          10u     /* una muestra de salida (trama) cada 10 ms (100 S/s) */
#define N_BLOQUE            10u     /* 10 muestras = 0.1 s, como el bloque de LabVIEW     */
#define ADC_CUENTAS         4095u   /* fondo de escala del ADC de 12 bits                 */
#define PERIODO_DISPARO_US  100u    /* periodo del PWM6 = TMR2: FOSC/4, T2PR = 199 y      */
                                    /* prescaler 1:4 (32 MHz) o 1:2 (16 MHz)              */
#define DISPAROS_POR_RONDA  10u     /* 6 canales + 4 libres = 1 ms                        */
#define PERIODO_MUESTREO_US (PERIODO_DISPARO_US * DISPAROS_POR_RONDA)  /* 1000 us por canal */
#define MUESTRAS_POR_S      (1000000u / PERIODO_MUESTREO_US)           /* 1000 S/s por canal */
#define N_SOBREMUESTREO     10u     /* rondas (conversiones por canal) en cada trama      */
#define DISPAROS_POR_TRAMA  (DISPAROS_POR_RONDA * N_SOBREMUESTREO)     /* 100 = 10 ms     */
#define N_COLA_TRAMAS       4u      /* tramas que pueden esperar a main (40 ms de retraso) */
#define N_CAPTURA           200u    /* conversiones de I_OUT seguidas (1 por ms) en la orden 'm' */

/* 1: el PWM6 (instante de cada conversion) sale por RA1 para medir su
 * jitter con un osciloscopio. RA1 es ICSPCLK: en las compilaciones de
 * depuracion (__DEBUG) no se usa, para no molestar al depurador.
 * Con el cristal, RA1 es el pulsador "Calibrar theta" y vale 0 por defecto. */
#ifndef SALIDA_DISPARO
#if RELOJ == RELOJ_CRISTAL_4MHZ
#define SALIDA_DISPARO      0
#else
#define SALIDA_DISPARO      1
#endif
#endif

/* Pulsador "Calibrar theta": RA4, o RA1 si el cristal ocupa RA4. Sin el en
 * las compilaciones de depuracion con cristal (RA1 es del depurador) y con
 * cristal y SALIDA_DISPARO = 1: entonces solo queda la orden 'c'. */
#define BOTON_THETA_EN_RA1  ((RELOJ == RELOJ_CRISTAL_4MHZ) && !SALIDA_DISPARO)

/* 1: convertir tambien el FVR (1.024 V) para conocer VCC_A en voltios. Hace
 *    posible la proteccion de 2.5 V y el LED "VCC_A fuera de rango" (P1).
 * 0: normalizacion ratiometrica pura, sin proteccion (pseudocodigo 10.6). */
#ifndef USAR_FVR
#define USAR_FVR            1
#endif
#define FVR_V               1.024f  /* tension nominal del buffer 1 del FVR (ganancia 1x) */

/* Divisor resistivo en la entrada VCC_A. 1 = conexion directa (seccion 10.3).
 * Poner 2 si se monta un divisor 1:2 (2 x 10 kohm) porque VCC_A puede
 * superar la VDD del PIC. */
#ifndef DIVISOR_VCCA
#define DIVISOR_VCCA        1u
#endif

/* ------------------------------------------------------------------------
 * Plano polar, salida y memoria no volatil
 * ------------------------------------------------------------------------ */
#ifndef N_TRAZA
#define N_TRAZA             64u     /* puntos (I', Q') guardados en el PIC (0 = sin traza) */
#endif
#ifndef USAR_EEPROM
#define USAR_EEPROM         1       /* 1: guardar I0, Q0, theta, G y etapas en la EEPROM  */
#endif

#define VERSION_FIRMWARE    "1.4"

#endif /* APP_CONFIG_H */
