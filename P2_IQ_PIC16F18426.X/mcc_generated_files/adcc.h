/**
  @File Name
    adcc.h

  @Summary
    Controlador del ADC con calculo (ADCC / ADC2) del PIC16F18426.

  @Description
    Equivalente a lo que genera MCC Classic (modulo ADCC) con
    docs/configuracion_MCC_Classic.md:
      - Referencia VDD / VSS, resultado justificado a la derecha
      - Reloj FOSC/32 -> TAD = 1.0 us a 32 MHz
      - Adquisicion: 8 TAD (8 us); sin precarga
      - Modo Burst Average, ADRPT = 16, ADCRS = 0: al terminar la rafaga,
        ADFLTR = ADACC = suma de las 16 conversiones
      - ADTMD = siempre: ADTIF se activa al terminar cada rafaga
      - Sin interrupciones (la aplicacion consulta ADTIF)
*/

#ifndef ADCC_H
#define ADCC_H

/**
  Section: Included Files
*/

#include <xc.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus  // Provide C++ Compatibility

    extern "C" {

#endif

/**
  Section: Data Types Definitions
*/

/**
 *  result size of an A/D conversion
 */

typedef uint16_t adc_result_t;
#ifndef uint24_t
typedef __uint24 uint24_t;
#endif

/**
 * Canales del ADC. Los cuatro primeros llevan el nombre dado al pin en el
 * Pin Module de MCC (tabla 32-4 de la hoja de datos).
 */

typedef enum
{
    I_OUT =  0x10,
    Q_OUT =  0x11,
    V_REF =  0x12,
    VCC_A =  0x13,
    channel_AVSS =  0x3B,
    channel_Temp =  0x3C,
    channel_DAC1 =  0x3D,
    channel_FVR_buffer1 =  0x3E,
    channel_FVR_buffer2 =  0x3F
} adcc_channel_t;

/**
  Section: ADCC Module APIs
*/

void ADCC_Initialize(void);
void ADCC_StartConversion(adcc_channel_t channel);
bool ADCC_IsConversionDone(void);
adc_result_t ADCC_GetConversionResult(void);
adc_result_t ADCC_GetSingleConversion(adcc_channel_t channel);
void ADCC_StopConversion(void);
void ADCC_SetStopOnInterrupt(void);
void ADCC_DischargeSampleCapacitor(void);
void ADCC_LoadAcquisitionRegister(uint16_t);
void ADCC_SetPrechargeTime(uint16_t);
void ADCC_SetRepeatCount(uint8_t);
uint8_t ADCC_GetCurrentCountofConversions(void);
void ADCC_ClearAccumulator(void);
uint24_t ADCC_GetAccumulatorValue(void);
bool ADCC_HasAccumulatorOverflowed(void);
uint16_t ADCC_GetFilterValue(void);
uint16_t ADCC_GetPreviousResult(void);
void ADCC_DefineSetPoint(uint16_t);
void ADCC_SetUpperThreshold(uint16_t);
void ADCC_SetLowerThreshold(uint16_t);
uint16_t ADCC_GetErrorCalculation(void);
void ADCC_EnableDoubleSampling(void);
void ADCC_EnableContinuousConversion(void);
void ADCC_DisableContinuousConversion(void);
bool ADCC_HasErrorCrossedUpperThreshold(void);
bool ADCC_HasErrorCrossedLowerThreshold(void);
uint8_t ADCC_GetConversionStageStatus(void);

#ifdef __cplusplus  // Provide C++ Compatibility

    }

#endif

#endif  //ADCC_H
