/*
 * reloj.h - Reloj del sistema: HFINTOSC interno o reloj externo de 4 MHz
 * con PLL x4, segun la opcion RELOJ (app_config.h, docs/reloj_externo_4MHz.md).
 */
#ifndef RELOJ_H
#define RELOJ_H

#include <stdbool.h>
#include <stdint.h>

enum {
    RELOJ_EST_INTERNO,      /* RELOJ_INTERNO: HFINTOSC a 32 MHz                  */
    RELOJ_EST_EXTERNO,      /* reloj externo de 4 MHz x PLL 4 = 16 MHz           */
    RELOJ_EST_RESERVA       /* el externo no arranco o fallo: HFINTOSC a 16 MHz  */
};

/* Con reloj externo, pasa del HFINTOSC (16 MHz) al externo con el PLL x4 y
 * espera hasta 100 ms a que arranque; si no, se queda con el HFINTOSC.
 * Llamar justo despues de SYSTEM_Initialize(), con las interrupciones
 * deshabilitadas. Con reloj interno no hace nada. */
void reloj_inicializar(void);

/* Interrupcion OSFIF (interrupt_manager.c): el monitor de fallos (FSCM) ha
 * detectado que el reloj externo se ha parado y ha pasado al HFINTOSC a
 * 1 MHz. Vuelve a 16 MHz con el HFINTOSC (mismas frecuencias de muestreo,
 * con su +-2 %). Solo con reloj externo. */
void reloj_fallo_isr(void);

/* Llamar en el lazo principal. Devuelve true una vez despues de un fallo
 * del reloj externo en marcha, para avisar por la UART. */
bool reloj_tarea(void);

uint8_t reloj_estado(void);         /* RELOJ_EST_x */

#endif /* RELOJ_H */
