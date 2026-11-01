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
    /* RC5 (RX1) con umbral TTL: VIH = 0.25 VDD + 0.8 V (D321), 1.85 V con
     * VDD = 4.2 V. Con Schmitt (0.8 VDD, D322) un adaptador USB-serie de
     * 3.3 V no llegaria a VIH si VDD pasa de 4.1 V (celda Li cargada). Se
     * fija antes de habilitar la EUSART (hoja de datos, 14.4.5). */
    INLVLC = 0x1F;


    RX1PPS = 0x15;   //RC5->EUSART1:RX1;
    RC4PPS = 0x0F;   //RC4->EUSART1:TX1;
}

void PIN_MANAGER_IOC(void)
{
}
