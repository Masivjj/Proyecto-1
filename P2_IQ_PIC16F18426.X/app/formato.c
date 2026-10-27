/*
 * formato.c - Conversion de numeros a texto (ver formato.h).
 *
 * El PIC16 no tiene divisor por hardware: en lugar de dividir por 10 se restan
 * potencias de 10, que es mucho mas rapido con numeros de 32 bits.
 */
#include <stdbool.h>
#include "formato.h"

#define LIMITE_ENTERO   2000000000L

static const uint32_t potencias_10[9] = {
    1000000000UL, 100000000UL, 10000000UL, 1000000UL,
    100000UL, 10000UL, 1000UL, 100UL, 10UL
};

int32_t fmt_a_entero(float x, float escala)
{
    float y = x * escala;

    if (y != y) {                       /* NaN */
        return 0;
    }
    if (y >= (float)LIMITE_ENTERO) {
        return LIMITE_ENTERO;
    }
    if (y <= -(float)LIMITE_ENTERO) {
        return -LIMITE_ENTERO;
    }
    return (int32_t)((y >= 0.0f) ? (y + 0.5f) : (y - 0.5f));
}

uint8_t fmt_u32(char *dst, uint32_t v)
{
    uint8_t n = 0u;
    uint8_t i;
    uint32_t p;
    char digito;

    for (i = 0u; i < 9u; i++) {
        p = potencias_10[i];
        digito = '0';
        while (v >= p) {
            v -= p;
            digito++;
        }
        if ((digito != '0') || (n != 0u)) {
            dst[n++] = digito;
        }
    }
    dst[n++] = (char)('0' + (uint8_t)v);
    return n;
}

uint8_t fmt_i32(char *dst, int32_t v)
{
    if (v < 0) {
        dst[0] = '-';
        /* -(v + 1) + 1 evita el desborde con INT32_MIN */
        return (uint8_t)(1u + fmt_u32(&dst[1], (uint32_t)(-(v + 1)) + 1u));
    }
    return fmt_u32(dst, (uint32_t)v);
}
