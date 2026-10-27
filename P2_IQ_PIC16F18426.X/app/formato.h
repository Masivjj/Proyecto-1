/*
 * formato.h - Conversion de numeros a texto para la salida por UART.
 *
 * Se evita printf("%f"), que en XC8 ocupa mucha memoria y es lento: los
 * valores se envian como enteros (uV, centesimas de grado, etc.).
 * No toca el hardware: se prueba en el PC (tests/test_iq_proc.c).
 */
#ifndef FORMATO_H
#define FORMATO_H

#include <stdint.h>

#define FMT_MAX_CARACTERES  11u     /* "-2147483648" */

/* Redondea x * escala al entero mas cercano, saturando a +-2 000 000 000. */
int32_t fmt_a_entero(float x, float escala);

/* Escriben el numero en decimal en dst (sin terminador '\0') y devuelven el
 * numero de caracteres (como mucho FMT_MAX_CARACTERES). */
uint8_t fmt_u32(char *dst, uint32_t v);
uint8_t fmt_i32(char *dst, int32_t v);

#endif /* FORMATO_H */
