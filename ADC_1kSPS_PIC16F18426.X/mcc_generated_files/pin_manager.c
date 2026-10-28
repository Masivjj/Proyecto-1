/**
  @File Name
    pin_manager.c

  @Summary
    Configuracion de los pines (Pin Module de MCC Classic).

  @Description
    RC0 analogico (SENAL, entrada del generador); RC4 = TX1 y RC5 = RX1 por
    PPS; RA5 salida (LED); los demas pines sin uso, en analogico. Equivalente a lo que genera MCC
    Classic con docs/avance_adc_1ksps.md.
*/

#include "pin_manager.h"


void PIN_MANAGER_Initialize(void)
{
    /**
    LATx registers
    */
    LATA = 0x00;
    LATC = 0x00;

    /**
    TRISx registers
    */
    TRISA = 0x1F;
    TRISC = 0x2F;

    /**
    ANSELx registers
    */
    ANSELC = 0x0F;
    ANSELA = 0x17;

    /**
    WPUx registers
    */
    WPUA = 0x00;
    WPUC = 0x00;

    /**
    ODx registers
    */
    ODCONA = 0x00;
    ODCONC = 0x00;

    /**
    SLRCONx registers
    */
    SLRCONA = 0x37;
    SLRCONC = 0x3F;

    /**
    INLVLx registers
    */
    INLVLA = 0x3F;
    INLVLC = 0x3F;


    RX1PPS = 0x15;   //RC5->EUSART1:RX1;
    RC4PPS = 0x0F;   //RC4->EUSART1:TX1;
}

void PIN_MANAGER_IOC(void)
{
}
