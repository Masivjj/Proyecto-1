/**
  @File Name
    fvr.h

  @Summary
    Referencia fija de tension (FVR).

  @Description
    Buffer 1 (hacia el ADC) con ganancia 1x = 1.024 V; buffer 2 apagado.
    El firmware lo convierte al arrancar para conocer VDD (la referencia
    del ADC): VDD = 1.024 * 4095 / cFVR.
    Equivalente a lo que genera MCC Classic con docs/avance_adc_1ksps.md.
*/

#ifndef FVR_H
#define FVR_H

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
  Section: FVR APIs
*/

void FVR_Initialize(void);
bool FVR_IsOutputReady(void);

#ifdef __cplusplus  // Provide C++ Compatibility

    }

#endif

#endif // FVR_H
