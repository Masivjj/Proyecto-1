/**
  @File Name
    mcc.c

  @Summary
    Inicializacion del sistema (System Module de MCC Classic).

  @Description
    HFINTOSC a 32 MHz (FOSC = 32 MHz, 8 MIPS) o, con la opcion RELOJ de reloj
    externo, a 16 MHz hasta que app/reloj.c pasa al externo con el PLL x4
    (FOSC = 16 MHz, 4 MIPS). Todos los modulos habilitados
    en PMD. El ADCC se inicializa despues del TMR2 y el PWM6 que lo disparan.
    Equivalente a lo que genera MCC Classic con
    docs/configuracion_MCC_Classic.md.
*/

#include "mcc.h"


void SYSTEM_Initialize(void)
{

    PMD_Initialize();
    PIN_MANAGER_Initialize();
    OSCILLATOR_Initialize();
    FVR_Initialize();
    TMR2_Initialize();
    PWM6_Initialize();
    ADCC_Initialize();
    EUSART1_Initialize();
}

void OSCILLATOR_Initialize(void)
{
    // NOSC HFINTOSC; NDIV 1;
    OSCCON1 = 0x60;
    // CSWHOLD may proceed; SOSCPWR Low power;
    OSCCON3 = 0x00;
    // MFOEN disabled; LFOEN disabled; ADOEN disabled; SOSCEN disabled; EXTOEN disabled; HFOEN disabled;
    OSCEN = 0x00;
#if RELOJ_EXTERNO
    // HFFRQ 16_MHz; (app/reloj.c pasa despues al reloj externo con PLL x4;
    // si no arranca, el sistema sigue con el HFINTOSC a la misma frecuencia)
    OSCFRQ = 0x05;
#else
    // HFFRQ 32_MHz;
    OSCFRQ = 0x06;
#endif
    // HFTUN 0;
    OSCTUNE = 0x00;
}

void PMD_Initialize(void)
{
    // CLKRMD CLKR enabled; SYSCMD SYSCLK enabled; FVRMD FVR enabled; IOCMD IOC enabled; NVMMD NVM enabled;
    PMD0 = 0x00;
    // TMR0MD TMR0 enabled; TMR1MD TMR1 enabled; TMR4MD TMR4 enabled; TMR5MD TMR5 enabled; TMR2MD TMR2 enabled; TMR3MD TMR3 enabled; TMR6MD TMR6 enabled;
    PMD1 = 0x00;
    // NCO1MD NCO1 enabled;
    PMD2 = 0x00;
    // ZCDMD ZCD enabled; DAC1MD DAC1 enabled; CMP1MD CMP1 enabled; ADCMD ADC enabled; CMP2MD CMP2 enabled;
    PMD3 = 0x00;
    // CCP2MD CCP2 enabled; CCP1MD CCP1 enabled; CCP4MD CCP4 enabled; CCP3MD CCP3 enabled; PWM6MD PWM6 enabled; PWM7MD PWM7 enabled;
    PMD4 = 0x00;
    // CWG2MD CWG2 enabled; CWG1MD CWG1 enabled;
    PMD5 = 0x00;
    // U1MD EUSART1 enabled; MSSP1MD MSSP1 enabled; MSSP2MD MSSP2 enabled;
    PMD6 = 0x00;
    // CLC3MD CLC3 enabled; CLC4MD CLC4 enabled; DSM1MD DSM enabled; SMT1MD SMT1 enabled; CLC1MD CLC1 enabled; CLC2MD CLC2 enabled;
    PMD7 = 0x00;
}
