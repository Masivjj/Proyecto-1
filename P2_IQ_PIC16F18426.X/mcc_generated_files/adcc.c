/**
  @File Name
    adcc.c

  @Summary
    Controlador del ADC con calculo (ADCC / ADC2) del PIC16F18426.

  @Description
    Ver adcc.h. Equivalente a lo que genera MCC Classic (modulo ADCC) con
    docs/configuracion_MCC_Classic.md.
*/

/**
  Section: Included Files
*/

#include <xc.h>
#include "adcc.h"

/**
  Section: ADCC Module APIs
*/

void ADCC_Initialize(void)
{
    // set the ADCC to the options selected in the User Interface
    // ADLTH 0;
    ADLTHL = 0x00;
    // ADLTH 0;
    ADLTHH = 0x00;
    // ADUTH 0;
    ADUTHL = 0x00;
    // ADUTH 0;
    ADUTHH = 0x00;
    // ADSTPT 0;
    ADSTPTL = 0x00;
    // ADSTPT 0;
    ADSTPTH = 0x00;
    // ADACC 0;
    ADACCU = 0x00;
    // ADRPT 16;
    ADRPT = 0x10;
    // ADPCH ANA0;
    ADPCH = 0x00;
    // ADACQ 8;
    ADACQL = 0x08;
    // ADACQ 0;
    ADACQH = 0x00;
    // ADCAP Additional uC disabled;
    ADCAP = 0x00;
    // ADPRE 0;
    ADPREL = 0x00;
    // ADPRE 0;
    ADPREH = 0x00;
    // ADDSEN disabled; ADGPOL digital_low; ADIPEN disabled; ADPPOL Vss;
    ADCON1 = 0x00;
    // ADCRS 0; ADMD Burst_average_mode; ADACLR disabled; ADPSIS RES;
    ADCON2 = 0x03;
    // ADCALC First derivative of Single measurement; ADTMD enabled; ADSOI ADGO not cleared;
    ADCON3 = 0x07;
    // ADMATH registers not updated;
    ADSTAT = 0x00;
    // ADNREF VSS; ADPREF VDD;
    ADREF = 0x00;
    // ADACT disabled;
    ADACT = 0x00;
    // ADCS FOSC/32;
    ADCLK = 0x0F;
    // ADGO stop; ADFM right; ADON enabled; ADCS FOSC/ADCLK; ADCONT disabled;
    ADCON0 = 0x84;

}

void ADCC_StartConversion(adcc_channel_t channel)
{
    //Selects the A/D channel
    ADPCH = channel;

    //Turn on the ADC module
    ADCON0bits.ADON = 1;

    //Start the conversion
    ADCON0bits.ADGO = 1;
}

bool ADCC_IsConversionDone(void)
{
    //Start the conversion
    return ((unsigned char)(!ADCON0bits.ADGO));
}

adc_result_t ADCC_GetConversionResult(void)
{
    //Return the result
    return ((adc_result_t)((ADRESH << 8) + ADRESL));
}

adc_result_t ADCC_GetSingleConversion(adcc_channel_t channel)
{
    //Selects the A/D channel
    ADPCH = channel;

    //Turn on the ADC module
    ADCON0bits.ADON = 1;

    //Disable the continuous mode.
    ADCON0bits.ADCONT = 0;

    //Start the conversion
    ADCON0bits.ADGO = 1;


    //Wait for the conversion to finish
    while (ADCON0bits.ADGO)
    {
    }


    //Conversion finished, return the result
    return ((adc_result_t)((ADRESH << 8) + ADRESL));
}

void ADCC_StopConversion(void)
{
    //Reset the ADGO bit.
    ADCON0bits.ADGO = 0;
}

void ADCC_SetStopOnInterrupt(void)
{
    //Set the ADSOI bit.
    ADCON3bits.ADSOI = 1;
}

void ADCC_DischargeSampleCapacitor(void)
{
    //Set the ADC channel to AVss.
    ADPCH = 0x3B;
}

void ADCC_LoadAcquisitionRegister(uint16_t acquisitionValue)
{
    //Load the ADACQH and ADACQL registers.
    ADACQH = (uint8_t) (acquisitionValue >> 8);
    ADACQL = (uint8_t) acquisitionValue;
}

void ADCC_SetPrechargeTime(uint16_t prechargeTime)
{
    //Load the ADPREH and ADPREL registers.
    ADPREH = (uint8_t) (prechargeTime >> 8);
    ADPREL = (uint8_t) prechargeTime;
}

void ADCC_SetRepeatCount(uint8_t repeatCount)
{
    //Load the ADRPT register.
    ADRPT = repeatCount;
}

uint8_t ADCC_GetCurrentCountofConversions(void)
{
    //Return the contents of ADCNT register
    return ADCNT;
}

void ADCC_ClearAccumulator(void)
{
    //Reset the ADCON2bits.ADACLR bit.
    ADCON2bits.ADACLR = 1;
}

uint24_t ADCC_GetAccumulatorValue(void)
{
    //Return the contents of ADACCU, ADACCH and ADACCL registers
    return (((uint24_t)ADACCU << 16)+((uint24_t)ADACCH << 8) + ADACCL);
}

bool ADCC_HasAccumulatorOverflowed(void)
{
    //Return the status of ADSTATbits.ADAOV
    return ADSTATbits.ADAOV;
}

uint16_t ADCC_GetFilterValue(void)
{
    //Return the contents of ADFLTRH and ADFLTRL registers
    return ((uint16_t)((ADFLTRH << 8) + ADFLTRL));
}

uint16_t ADCC_GetPreviousResult(void)
{
    //Return the contents of ADPREVH and ADPREVL registers
    return ((uint16_t)((ADPREVH << 8) + ADPREVL));
}

void ADCC_DefineSetPoint(uint16_t setPoint)
{
    //Sets the ADSTPTH and ADSTPTL registers
    ADSTPTH = (uint8_t) (setPoint >> 8);
    ADSTPTL = (uint8_t) setPoint;
}

void ADCC_SetUpperThreshold(uint16_t upperThreshold)
{
    //Sets the ADUTHH and ADUTHL registers
    ADUTHH = (uint8_t) (upperThreshold >> 8);
    ADUTHL = (uint8_t) (upperThreshold);
}

void ADCC_SetLowerThreshold(uint16_t lowerThreshold)
{
    //Sets the ADLTHH and ADLTHL registers
    ADLTHH = (uint8_t) (lowerThreshold >> 8);
    ADLTHL = (uint8_t) lowerThreshold;
}

uint16_t ADCC_GetErrorCalculation(void)
{
    //Return the contents of ADERRH and ADERRL registers
    return ((uint16_t)((ADERRH << 8) + ADERRL));
}

void ADCC_EnableDoubleSampling(void)
{
    //Sets the ADCON1bits.ADDSEN
    ADCON1bits.ADDSEN = 1;
}

void ADCC_EnableContinuousConversion(void)
{
    //Sets the ADCON0bits.ADCONT
    ADCON0bits.ADCONT = 1;
}

void ADCC_DisableContinuousConversion(void)
{
    //Resets the ADCON0bits.ADCONT
    ADCON0bits.ADCONT = 0;
}

bool ADCC_HasErrorCrossedUpperThreshold(void)
{
    //Returns the value of ADSTATbits.ADUTHR bit.
    return ADSTATbits.ADUTHR;
}

bool ADCC_HasErrorCrossedLowerThreshold(void)
{
    //Returns the value of ADSTATbits.ADLTHR bit.
    return ADSTATbits.ADLTHR;
}

uint8_t ADCC_GetConversionStageStatus(void)
{
    //Returns the contents of ADSTATbits.ADSTAT field.
    return ADSTATbits.ADSTAT;
}
