/**
  @File Name
    interrupt_manager.c

  @Summary
    Rutina de servicio de interrupcion (Interrupt Module de MCC Classic).

  @Description
    Atiende la EUSART1 (transmision y recepcion con buffer) y el TMR2 (tick
    de 10 ms). Equivalente a lo que genera MCC Classic con
    docs/configuracion_MCC_Classic.md.
*/

#include "interrupt_manager.h"
#include "mcc.h"

void __interrupt() INTERRUPT_InterruptManager (void)
{
    // interrupt handler
    if(INTCONbits.PEIE == 1)
    {
        if(PIE3bits.TX1IE == 1 && PIR3bits.TX1IF == 1)
        {
            EUSART1_TxDefaultInterruptHandler();
        }
        else if(PIE3bits.RC1IE == 1 && PIR3bits.RC1IF == 1)
        {
            EUSART1_RxDefaultInterruptHandler();
        }
        else if(PIE4bits.TMR2IE == 1 && PIR4bits.TMR2IF == 1)
        {
            TMR2_ISR();
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
