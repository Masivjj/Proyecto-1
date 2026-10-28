/**
  @File Name
    pwm6.h

  @Summary
    PWM6: disparo por hardware de las conversiones del ADC.

  @Description
    Base de tiempo TMR2 (periodo 100 us), ciclo de trabajo 50 %. El flanco de
    subida (al reiniciarse el TMR2) arranca una conversion del ADC
    (ADACT = PWM6_out). La aplicacion puede sacarlo por RA1 para medir el
    jitter con un osciloscopio (SALIDA_DISPARO en app/app_config.h).
    Equivalente a lo que genera MCC Classic con docs/configuracion_MCC_Classic.md.
*/

#ifndef PWM6_H
#define PWM6_H

/**
  Section: Included Files
*/

#include <xc.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus  // Provide C++ Compatibility

    extern "C" {

#endif

/**
  Section: PWM Module APIs
*/

void PWM6_Initialize(void);
void PWM6_LoadDutyValue(uint16_t dutyValue);
bool PWM6_OutputStatusGet(void);

#ifdef __cplusplus  // Provide C++ Compatibility

    }

#endif

#endif  //PWM6_H
