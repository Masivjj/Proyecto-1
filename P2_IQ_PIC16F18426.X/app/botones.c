/*
 * botones.c - Pulsadores "Fijar cero" (RA2) y "Calibrar theta" (RA4).
 *
 * Activos en bajo, con pull-up interno. Antirrebote por muestreo cada 10 ms:
 * un cambio cuenta cuando el nivel lleva ANTIRREBOTE_TICKS lecturas seguidas
 * estable. Cada pulsacion genera un unico evento, como los botones "latch"
 * del VI, que valen un bloque. El resto de ordenes (borrar cero, theta = 0,
 * ganancia, borrar traza) llegan por UART.
 */
#include "../mcc_generated_files/mcc.h"
#include "botones.h"
#include "iq_proc.h"

#define ANTIRREBOTE_TICKS   3u      /* 30 ms */

typedef struct {
    bool    pulsado;    /* estado estable */
    uint8_t cuenta;     /* lecturas seguidas distintas del estado estable */
} boton_t;

static boton_t boton_cero;
static boton_t boton_theta;

/* Devuelve true en el momento en que se confirma una pulsacion. */
static bool antirrebote(boton_t *b, bool leido)
{
    if (leido == b->pulsado) {
        b->cuenta = 0u;
        return false;
    }
    if (++b->cuenta < ANTIRREBOTE_TICKS) {
        return false;
    }
    b->cuenta = 0u;
    b->pulsado = leido;
    return leido;
}

uint8_t botones_tarea(void)
{
    uint8_t ev = 0u;

    if (antirrebote(&boton_cero, BTN_CERO_GetValue() == 0u)) {
        ev |= IQ_EV_FIJAR_CERO;
    }
    if (antirrebote(&boton_theta, BTN_THETA_GetValue() == 0u)) {
        ev |= IQ_EV_CALIBRAR;
    }
    return ev;
}
