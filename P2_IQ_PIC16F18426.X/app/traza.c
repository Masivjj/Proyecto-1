/*
 * traza.c - Traza del plano polar (P9) en un buffer circular.
 *
 * Dibujar (r, phi) en un plano polar es lo mismo que dibujar (I', Q') en un
 * plano cartesiano con la misma escala en los dos ejes (seccion 4.10), asi
 * que se guardan I' y Q' y no hace falta calcular r ni phi por muestra.
 * Con la salida por UART (opcion A) el PC guarda y dibuja la traza larga;
 * esta copia corta se vuelca con la orden 'p' y sirve de base para una
 * pantalla local (opcion B).
 */
#include "traza.h"

#if N_TRAZA > 0u

typedef char traza_indice_de_8_bits[(N_TRAZA <= 127u) ? 1 : -1];

static float   traza_i[N_TRAZA];
static float   traza_q[N_TRAZA];
static uint8_t siguiente;       /* posicion del proximo punto */
static uint8_t cuenta;          /* puntos validos */

void traza_agregar(float i1, float q1)
{
    traza_i[siguiente] = i1;
    traza_q[siguiente] = q1;
    if (++siguiente >= N_TRAZA) {
        siguiente = 0u;
    }
    if (cuenta < N_TRAZA) {
        cuenta++;
    }
}

void traza_borrar(void)
{
    siguiente = 0u;
    cuenta = 0u;
}

uint8_t traza_puntos(void)
{
    return cuenta;
}

bool traza_leer(uint8_t n, float *i1, float *q1)
{
    uint8_t k;

    if (n >= cuenta) {
        return false;
    }
    /* El punto mas antiguo esta 'cuenta' posiciones por detras de 'siguiente' */
    k = (uint8_t)(siguiente + N_TRAZA - cuenta + n);
    if (k >= N_TRAZA) {
        k = (uint8_t)(k - N_TRAZA);
    }
    *i1 = traza_i[k];
    *q1 = traza_q[k];
    return true;
}

#else /* N_TRAZA == 0: sin traza en el PIC */

void traza_agregar(float i1, float q1)
{
    (void)i1;
    (void)q1;
}

void traza_borrar(void)
{
}

uint8_t traza_puntos(void)
{
    return 0u;
}

bool traza_leer(uint8_t n, float *i1, float *q1)
{
    (void)n;
    (void)i1;
    (void)q1;
    return false;
}

#endif
