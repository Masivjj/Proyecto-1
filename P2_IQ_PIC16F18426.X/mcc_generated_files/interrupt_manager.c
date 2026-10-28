/**
  @File Name
    interrupt_manager.c

  @Summary
    Rutina de servicio de interrupcion (Interrupt Module de MCC Classic).

  @Description
    Atiende el ADC (fin de cada conversion disparada por el PWM6, cada
    100 us) y la EUSART1 (transmision y recepcion con buffer).
    Equivalente a lo que genera MCC Classic con
    docs/configuracion_MCC_Classic.md.
*/

#include "interrupt_manager.h"
#include "mcc.h"

void __interrupt() INTERRUPT_InterruptManager (void)
{
    // interrupt handler
    if(INTCONbits.PEIE == 1)
    {
        if(PIE1bits.ADIE == 1 && PIR1bits.ADIF == 1)
        {
            ADCC_ISR();
        }
        else if(PIE3bits.TX1IE == 1 && PIR3bits.TX1IF == 1)
        {
            EUSART1_TxDefaultInterruptHandler();
        }
        else if(PIE3bits.RC1IE == 1 && PIR3bits.RC1IF == 1)
        {
            EUSART1_RxDefaultInterruptHandler();
        }
        else
        {
            //Unhandled Interrupt
        }
    }
    else
    {
        //Unhandled Interrupt
    }
}
