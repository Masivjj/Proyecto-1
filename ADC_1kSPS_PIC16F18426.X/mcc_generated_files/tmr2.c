/**
  @File Name
    tmr2.c

  @Summary
    Temporizador TMR2: base de tiempo del PWM6 que dispara el ADC (1 ms).

  @Description
    Ver tmr2.h. Equivalente a lo que genera MCC Classic con
    docs/avance_adc_1ksps.md.
*/

/**
  Section: Included Files
*/

#include <xc.h>
#include "tmr2.h"

/**
  Section: Global Variables Definitions
*/

/**
  Section: TMR2 APIs
*/

void TMR2_Initialize(void)
{
    // Set TMR2 to the options selected in the User Interface

    // T2CS FOSC/4;
    T2CLKCON = 0x01;

    // T2PSYNC Not Synchronized; T2MODE Software control; T2CKPOL Rising Edge; T2CKSYNC Not Synchronized;
    T2HLT = 0x00;

    // T2RSEL T2INPPS pin;
    T2RST = 0x00;

    // T2PR 249;
    T2PR = 0xF9;

    // TMR2 0;
    T2TMR = 0x00;

    // Clearing IF flag.
    PIR4bits.TMR2IF = 0;

    // T2CKPS 1:32; T2OUTPS 1:1; TMR2ON on;
    T2CON = 0xD0;
}

void TMR2_ModeSet(TMR2_HLT_MODE mode)
{
   // Configure different types HLT mode
    T2HLTbits.MODE = mode;
}

void TMR2_Start(void)
{
    // Start the Timer by writing to TMRxON bit
    T2CONbits.TMR2ON = 1;
}

void TMR2_StartTimer(void)
{
    TMR2_Start();
}

void TMR2_Stop(void)
{
    // Stop the Timer by writing to TMRxON bit
    T2CONbits.TMR2ON = 0;
}

void TMR2_StopTimer(void)
{
    TMR2_Stop();
}

uint8_t TMR2_Counter8BitGet(void)
{
    uint8_t readVal;

    readVal = TMR2;

    return readVal;
}

uint8_t TMR2_ReadTimer(void)
{
    return TMR2_Counter8BitGet();
}

void TMR2_Counter8BitSet(uint8_t timerVal)
{
    // Write to the Timer2 register
    TMR2 = timerVal;
}

void TMR2_WriteTimer(uint8_t timerVal)
{
    TMR2_Counter8BitSet(timerVal);
}

void TMR2_Period8BitSet(uint8_t periodVal)
{
   PR2 = periodVal;
}

void TMR2_LoadPeriodRegister(uint8_t periodVal)
{
   TMR2_Period8BitSet(periodVal);
}

bool TMR2_HasOverflowOccured(void)
{
    // check if  overflow has occurred by checking the TMRIF bit
    bool status = PIR4bits.TMR2IF;
    if(status)
    {
        // Clearing IF flag.
        PIR4bits.TMR2IF = 0;
    }
    return status;
}
