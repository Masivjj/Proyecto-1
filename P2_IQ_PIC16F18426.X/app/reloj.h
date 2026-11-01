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

/* Llamar en el lazo principal. Devuelve true una vez si el monitor de fallos
 * (FSCM) detecta que el reloj externo se ha parado: el PIC sigue con el
 * HFINTOSC a 16 MHz (mismas frecuencias de muestreo, con su +-2 %). */
bool reloj_tarea(void);

uint8_t reloj_estado(void);         /* RELOJ_EST_x */

#endif /* RELOJ_H */
