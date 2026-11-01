/**
  @File Name
    device_config.h

  @Summary
    Frecuencia del reloj para las macros __delay_ms() / __delay_us().

  @Description
    Equivalente al archivo que genera MCC Classic para el PIC16F18426 con la
    configuracion de docs/configuracion_MCC_Classic.md. Si se vuelve a generar
    el codigo con MCC, este archivo se sustituye: hay que volver a dejar
    _XTAL_FREQ = FOSC_HZ, que depende de la opcion RELOJ (app/app_config.h).
*/

#ifndef DEVICE_CONFIG_H
#define DEVICE_CONFIG_H

#include "../app/app_config.h"

/* 32 MHz con el HFINTOSC; 16 MHz con el reloj externo de 4 MHz y el PLL x4 */
#define _XTAL_FREQ FOSC_HZ

#endif  /* DEVICE_CONFIG_H */
