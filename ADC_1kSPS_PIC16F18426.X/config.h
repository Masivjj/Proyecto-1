/*
 * config.h - Opciones del avance "ADC a 1000 muestras/s con salida serie".
 *
 * La frecuencia de muestreo la fijan el TMR2 y el PWM6 de MCC
 * (mcc_generated_files/tmr2.c y pwm6.c): si se cambian alli, hay que
 * cambiar tambien PERIODO_US para que la cabecera informe bien.
 */
#ifndef CONFIG_H
#define CONFIG_H

#define VERSION_FIRMWARE    "1.0"

#define PERIODO_US          1000u   /* un disparo del PWM6 = una muestra (1000 S/s)     */
#define CANAL_SENAL         SENAL   /* RC0 / ANC0 (adcc.h)                              */
#define FVR_MV              1024u   /* buffer 1 del FVR, ganancia 1x (+-4 %, FVR01)     */
#define N_FIFO              128u    /* muestras en espera de la UART (potencia de 2)    */
#define MUESTRAS_LED        500u    /* el LED cambia cada 0.5 s                         */

/* 1: el PWM6 (instante de cada conversion) sale por RA1 para medir su jitter
 * con un osciloscopio. RA1 es ICSPCLK: en las compilaciones de depuracion
 * (__DEBUG) no se usa, para no molestar al depurador. */
#ifndef SALIDA_DISPARO
#define SALIDA_DISPARO      1
#endif

#endif /* CONFIG_H */
