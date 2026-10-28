/**
  @File Name
    pwm6.c

  @Summary
    PWM6: disparo por hardware de las conversiones del ADC.

  @Description
    Ver pwm6.h. Periodo = (T2PR + 1) * 4 * TOSC * prescaler = 200 * 4 *
    31.25 ns * 4 = 100 us. Ciclo de trabajo = PWM6DC / (4 * (T2PR + 1)) =
    400 / 800 = 50 %. Equivalente a lo que genera MCC Classic con
    docs/configuracion_MCC_Classic.md.
*/

/**
  Section: Included Files
*/

#include <xc.h>
#include <stdbool.h>
#include "pwm6.h"

/**
  Section: PWM Module APIs
*/

void PWM6_Initialize(void)
{
    // Set the PWM6 to the options selected in the User Interface

    // PWM6POL active_hi; PWM6EN enabled;
    PWM6CON = 0x80;

    // DC 100;
    PWM6DCH = 0x64;

    // DC 0;
    PWM6DCL = 0x00;

    // Select timer
    CCPTMRS1bits.P6TSEL = 1;
}

void PWM6_LoadDutyValue(uint16_t dutyValue)
{
    // Writing to 8 MSBs of PWM duty cycle in PWMDCH register
    PWM6DCH = (uint8_t)((dutyValue & 0x03FC) >> 2);

    // Writing to 2 LSBs of PWM duty cycle in PWMDCL register
    PWM6DCL = (uint8_t)((dutyValue & 0x0003) << 6);
}

bool PWM6_OutputStatusGet(void)
{
    // Returns the output status
    return(PWM6CONbits.PWM6OUT);
}
