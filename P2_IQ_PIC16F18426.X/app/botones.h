/*
 * botones.h - Pulsadores "Fijar cero" (RA2) y "Calibrar theta" (RA4).
 */
#ifndef BOTONES_H
#define BOTONES_H

#include <stdint.h>

/* Llamar cada 10 ms. Devuelve los eventos IQ_EV_x de los pulsadores que se
 * acaban de pulsar (0 si ninguno). */
uint8_t botones_tarea(void);

#endif /* BOTONES_H */
