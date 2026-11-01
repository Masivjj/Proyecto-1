/**
  @File Name
    interrupt_manager.c

  @Summary
    Rutina de servicio de interrupcion (Interrupt Module de MCC Classic).

  @Description
    Atiende el ADC (fin de cada conversion disparada por el PWM6, cada
    100 us) y la EUSART1 (transmision y recepcion con buffer). Con reloj
    externo (opcion RELOJ), primero el fallo del oscilador (OSFIF): el FSCM
    deja el PIC a 1 MHz y app/reloj.c lo devuelve a 16 MHz.
    Equivalente a lo que genera MCC Classic con
    docs/configuracion_MCC_Classic.md.
*/

#include "interrupt_manager.h"
#include "mcc.h"
#if RELOJ_EXTERNO
#include "../app/reloj.h"
#endif

void __interrupt() INTERRUPT_InterruptManager (void)
{
    // interrupt handler
    if(INTCONbits.PEIE == 1)
    {
#if RELOJ_EXTERNO
        if(PIE1bits.OSFIE == 1 && PIR1bits.OSFIF == 1)
        {
            reloj_fallo_isr();
        }
        else
#endif
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
