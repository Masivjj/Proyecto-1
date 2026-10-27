/**
  @File Name
    tmr2.c

  @Summary
    Temporizador TMR2: tick de 10 ms.

  @Description
    Ver tmr2.h. Equivalente a lo que genera MCC Classic con
    docs/configuracion_MCC_Classic.md.
*/

/**
  Section: Included Files
*/

#include <xc.h>
#include "tmr2.h"

void (*TMR2_InterruptHandler)(void);

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

    // T2PR 124;
    T2PR = 0x7C;

    // TMR2 0;
    T2TMR = 0x00;

    // Clearing IF flag before enabling the interrupt.
    PIR4bits.TMR2IF = 0;

    // Enabling TMR2 interrupt.
    PIE4bits.TMR2IE = 1;

    // Set Default Interrupt Handler
    TMR2_SetInterruptHandler(TMR2_DefaultInterruptHandler);

    // T2CKPS 1:128; T2OUTPS 1:5; TMR2ON on;
    T2CON = 0xF4;
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

void TMR2_ISR(void)
{

    // clear the TMR2 interrupt flag
     PIR4bits.TMR2IF = 0;

    // ticker function call;
    // ticker is 1 -> Callback function gets called every time this ISR executes
    TMR2_CallBack();
}

void TMR2_CallBack(void)
{
    // Add your custom callback code here
    // this code executes every TMR2_INTERRUPT_TICKER_FACTOR periods of TMR2
    if(TMR2_InterruptHandler)
    {
        TMR2_InterruptHandler();
    }
}

void TMR2_SetInterruptHandler(void (* InterruptHandler)(void)){
    TMR2_InterruptHandler = InterruptHandler;
}

void TMR2_DefaultInterruptHandler(void){
    // add your TMR2 interrupt custom code
    // or set custom function using TMR2_SetInterruptHandler()
}
