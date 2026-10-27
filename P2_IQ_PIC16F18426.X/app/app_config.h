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

/* "Correccion signo I": la tarjeta fabricada entrega I invertido (seccion 1.3). */
#ifndef CORR_SIGNO_INICIAL
#define CORR_SIGNO_INICIAL  1
#endif

/* ------------------------------------------------------------------------
 * Adquisicion (seccion 10.4)
 * ------------------------------------------------------------------------ */
#define PERIODO_MS          10u     /* TMR2: una muestra de salida cada 10 ms (100 S/s)  */
#define N_BLOQUE            10u     /* 10 muestras = 0.1 s, como el bloque de LabVIEW    */
#define N_SOBREMUESTREO     16u     /* conversiones por canal y muestra (ADRPT del ADCC) */
#define ADC_CUENTAS         4095u   /* fondo de escala del ADC de 12 bits                */

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
#define USAR_EEPROM         1       /* 1: guardar I0, Q0, theta, G y signo en la EEPROM   */
#endif

#define VERSION_FIRMWARE    "1.0"

#endif /* APP_CONFIG_H */
