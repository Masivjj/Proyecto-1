/**
  @File Name
    device_config.h

  @Summary
    Frecuencia del reloj para las macros __delay_ms() / __delay_us().

  @Description
    Equivalente al archivo que genera MCC Classic para el PIC16F18426 con la
    configuracion de docs/avance_adc_1ksps.md. Si se vuelve a generar
    el codigo con MCC, este archivo se sustituye.
*/

#ifndef DEVICE_CONFIG_H
#define DEVICE_CONFIG_H

#define _XTAL_FREQ 32000000

#endif  /* DEVICE_CONFIG_H */
