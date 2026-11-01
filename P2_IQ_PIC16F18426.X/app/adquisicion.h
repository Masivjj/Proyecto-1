/*
 * adquisicion.h - Conversiones del ADC disparadas por hardware (PWM6), con
 * jitter de muestreo limitado al del oscilador (docs/analisis_adc_jitter.md).
 * Cada entrada se muestrea a 1000 muestras/s (una conversion por ronda de 1 ms).
 */
#ifndef ADQUISICION_H
#define ADQUISICION_H

#include <stdbool.h>
#include <stdint.h>
#include "iq_proc.h"

/* Prepara el ADC (modo basico, disparo PWM6_out), la secuencia de canales y,
 * si SALIDA_DISPARO, la salida del PWM6 por RA1. Llamar despues de
 * SYSTEM_Initialize() y antes de habilitar las interrupciones. */
void adq_inicializar(void);

/* Saca de la cola la trama mas antigua: los codigos (suma de N_SOBREMUESTREO
 * conversiones por canal) y su numero (1, 2, 3... una cada PERIODO_MS desde
 * el arranque, tambien durante las capturas). Devuelve false si no hay. */
bool adq_leer(iq_codigos_t *c, uint32_t *numero);

/* Tramas descartadas porque la cola estaba llena (main iba mas de
 * N_COLA_TRAMAS tramas por detras). */
uint32_t adq_tramas_perdidas(void);

/* Captura de n conversiones seguidas de I_OUT, una por ronda (cada
 * PERIODO_MUESTREO_US = 1 ms), para medir el SNR y el jitter (orden 'm').
 * Empieza en la trama siguiente y dura n ms; las tramas normales siguen.
 * adq_pedir_captura devuelve false si ya hay una captura sin liberar. */
bool adq_pedir_captura(uint16_t *buffer, uint16_t n);
bool adq_captura_lista(void);       /* el buffer ya esta lleno              */
void adq_liberar_captura(void);     /* despues de enviar el buffer          */
bool adq_capturando(void);          /* pedida, en curso o lista sin liberar */

#endif /* ADQUISICION_H */
