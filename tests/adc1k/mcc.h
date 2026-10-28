/*
 * mcc.h FALSO - solo para simular en el PC el firmware del avance "ADC a
 * 1000 muestras/s" (ADC_1kSPS_PIC16F18426.X, tests/adc1k, "make adc1k").
 *
 * Declara lo que usa main.c (registros, drivers de MCC y macros de pines) y
 * adc1k/sim.c lo implementa con un "hardware" simulado. No se usa en el PIC.
 */
#ifndef MCC_H
#define MCC_H

#include <stdbool.h>
#include <stdint.h>

/* Canales del ADC (adcc.h) */
typedef enum {
    SENAL = 0x10,
    channel_AVSS = 0x3B,
    channel_FVR_buffer1 = 0x3E
} adcc_channel_t;

/* Registros (solo los campos que usa main.c) */
typedef struct { unsigned ADGO : 1; } sim_adcon0_t;
typedef struct { unsigned ADIF : 1; } sim_pir1_t;
typedef struct { unsigned ADIE : 1; } sim_pie1_t;
typedef struct { unsigned ANSA1 : 1; } sim_ansela_t;
typedef struct { unsigned TRISA1 : 1; } sim_trisa_t;
typedef struct { unsigned SLRA1 : 1; } sim_slrcona_t;

extern volatile sim_adcon0_t ADCON0bits;
extern volatile sim_pir1_t PIR1bits;
extern sim_pie1_t PIE1bits;
extern sim_ansela_t ANSELAbits;
extern sim_trisa_t TRISAbits;
extern sim_slrcona_t SLRCONAbits;
extern uint8_t ADPCH, ADACT, RA1PPS;

/* Drivers de MCC */
void SYSTEM_Initialize(void);
uint16_t ADCC_GetConversionResult(void);
uint16_t ADCC_GetSingleConversion(adcc_channel_t canal);
void ADCC_SetADIInterruptHandler(void (*manejador)(void));
bool FVR_IsOutputReady(void);
void EUSART1_Write(uint8_t dato);
bool EUSART1_is_rx_ready(void);
uint8_t EUSART1_Read(void);

/* Interrupciones */
void sim_gie(int habilitar);
#define INTERRUPT_GlobalInterruptEnable()       sim_gie(1)
#define INTERRUPT_GlobalInterruptDisable()      sim_gie(0)
#define INTERRUPT_PeripheralInterruptEnable()   ((void)0)

/* Pines: sim_led cuenta los cambios del LED */
extern int sim_led;
#define LED_Toggle()            (sim_led++)

#endif /* MCC_H */
