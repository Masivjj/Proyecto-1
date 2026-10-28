/*
 * traza.h - Traza del plano polar (P9): ultimos N_TRAZA puntos (I', Q').
 */
#ifndef TRAZA_H
#define TRAZA_H

#include <stdbool.h>
#include <stdint.h>
#include "app_config.h"

void    traza_agregar(float i1, float q1);
void    traza_borrar(void);
uint8_t traza_puntos(void);                             /* "puntos actuales" */
bool    traza_leer(uint8_t n, float *i1, float *q1);    /* n = 0: el mas antiguo */

/* La captura para medir el jitter (orden 'm') usa la memoria de la traza:
 * esta funcion vacia la traza y devuelve un buffer de N_CAPTURA valores de
 * 16 bits. Mientras se use, no hay que llamar a traza_agregar(). */
uint16_t *traza_prestar_memoria(void);

#endif /* TRAZA_H */
