/**
  @File Name
    mcc.h

  @Summary
    Cabecera principal de los controladores de MCC Classic.

  @Description
    Equivalente al archivo que genera MCC Classic para el PIC16F18426 con la
    configuracion de docs/configuracion_MCC_Classic.md. Si se vuelve a generar
    el codigo con MCC, este archivo se sustituye.
*/

#ifndef MCC_H
#define MCC_H
#include <xc.h>
#include "device_config.h"
#include "pin_manager.h"
#include <stdint.h>
#include <stdbool.h>
#include "interrupt_manager.h"
#include "fvr.h"
#include "memory.h"
#include "tmr2.h"
#include "pwm6.h"
#include "adcc.h"
#include "eusart1.h"

/**
 * @Param
    none
 * @Returns
    none
 * @Description
    Initializes the device to the default states configured in the
 *                  MCC GUI
 * @Example
    SYSTEM_Initialize(void);
 */
void SYSTEM_Initialize(void);

/**
 * @Param
    none
 * @Returns
    none
 * @Description
    Initializes the oscillator to the default states configured in the
 *                  MCC GUI
 * @Example
    OSCILLATOR_Initialize(void);
 */
void OSCILLATOR_Initialize(void);

/**
 * @Param
    none
 * @Returns
    none
 * @Description
    Initializes the PMD module to the default states configured in the
 *                  MCC GUI
 * @Example
    PMD_Initialize(void);
 */
void PMD_Initialize(void);

#endif  /* MCC_H */
