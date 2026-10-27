/*
 * mcc.h FALSO - solo para simular el firmware en el PC (tests/sim).
 *
 * Sustituye a P2_IQ_PIC16F18426.X/mcc_generated_files/mcc.h: declara lo que
 * usan main.c y los .c de app/ (registros, drivers de MCC y macros de pines) y
 * sim.c lo implementa con un "hardware" simulado. No se usa en el PIC.
 */
#ifndef MCC_H
#define MCC_H

#include <stdbool.h>
#include <stdint.h>

/* Registros (solo los campos que usa la aplicacion) */
typedef uint8_t adcc_channel_t;
typedef struct { unsigned ADCRS : 3; unsigned ADMD : 3; } sim_adcon2_t;
typedef struct { unsigned ADTMD : 3; } sim_adcon3_t;
typedef struct { unsigned ADTIF : 1; } sim_pir1_t;
typedef struct { unsigned GIE : 1; unsigned PEIE : 1; } sim_intcon_t;
typedef struct { unsigned NVMREGS : 1; unsigned WREN : 1; unsigned WR : 1; unsigned RD : 1; } sim_nvmcon1_t;

extern sim_adcon2_t ADCON2bits;
extern sim_adcon3_t ADCON3bits;
extern volatile sim_pir1_t PIR1bits;
extern sim_intcon_t INTCONbits;
extern volatile sim_nvmcon1_t NVMCON1bits;
extern uint8_t NVMADRH, NVMADRL, NVMDATL, NVMCON2;

/* Drivers de MCC */
void SYSTEM_Initialize(void);
void ADCC_StartConversion(adcc_channel_t canal);
uint16_t ADCC_GetFilterValue(void);
void ADCC_SetRepeatCount(uint8_t repeticiones);
bool FVR_IsOutputReady(void);
uint8_t DATAEE_ReadByte(uint16_t direccion);
void EUSART1_Write(uint8_t dato);
bool EUSART1_is_rx_ready(void);
uint8_t EUSART1_Read(void);
void TMR2_SetInterruptHandler(void (*manejador)(void));

/* Interrupciones: cada vez que main() las deshabilita pasa el tiempo */
void sim_gie(int habilitar);
#define INTERRUPT_GlobalInterruptEnable()       sim_gie(1)
#define INTERRUPT_GlobalInterruptDisable()      sim_gie(0)
#define INTERRUPT_PeripheralInterruptEnable()   ((void)0)

/* Pines */
extern int sim_led;
unsigned sim_boton(int n);
#define LED_SetHigh()           (sim_led = 1)
#define LED_SetLow()            (sim_led = 0)
#define BTN_CERO_GetValue()     sim_boton(0)
#define BTN_THETA_GetValue()    sim_boton(1)

#endif /* MCC_H */
