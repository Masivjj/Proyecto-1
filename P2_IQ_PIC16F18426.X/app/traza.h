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

#endif /* TRAZA_H */
