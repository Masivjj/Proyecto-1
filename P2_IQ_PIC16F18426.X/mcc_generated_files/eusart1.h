/**
  @File Name
    eusart1.h

  @Summary
    EUSART1 asincrona a 115200 baudios con buffers por interrupcion.

  @Description
    8N1, BRG16 = 1, BRGH = 1, SP1BRG = 68: 32 MHz / (4 * 69) = 115942 baudios
    (error +0.6 %). TX en RC4 y RX en RC5 (PPS). Buffer de transmision de 128
    bytes y de recepcion de 8 bytes.
    Equivalente a lo que genera MCC Classic con docs/configuracion_MCC_Classic.md.
*/

#ifndef EUSART1_H
#define EUSART1_H

/**
  Section: Included Files
*/

#include <xc.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus  // Provide C++ Compatibility

    extern "C" {

#endif


/**
  Section: Macro Declarations
*/

#define EUSART1_DataReady  (EUSART1_is_rx_ready())

/**
  Section: Data Type Definitions
*/

typedef union {
    struct {
        unsigned perr : 1;
        unsigned ferr : 1;
        unsigned oerr : 1;
        unsigned reserved : 5;
    };
    uint8_t status;
}eusart1_status_t;

/**
 Section: Global variables
 */
extern volatile uint8_t eusart1TxBufferRemaining;
extern volatile uint8_t eusart1RxCount;

/**
  Section: EUSART1 handler function pointers
*/

extern void (*EUSART1_TxDefaultInterruptHandler)(void);
extern void (*EUSART1_RxDefaultInterruptHandler)(void);

/**
  Section: EUSART1 APIs
*/

void EUSART1_Initialize(void);
bool EUSART1_is_tx_ready(void);
bool EUSART1_is_rx_ready(void);
bool EUSART1_is_tx_done(void);
eusart1_status_t EUSART1_get_last_status(void);
uint8_t EUSART1_Read(void);
void EUSART1_Write(uint8_t txData);
void EUSART1_Transmit_ISR(void);
void EUSART1_Receive_ISR(void);
void EUSART1_RxDataHandler(void);
void EUSART1_SetFramingErrorHandler(void (* interruptHandler)(void));
void EUSART1_SetOverrunErrorHandler(void (* interruptHandler)(void));
void EUSART1_SetErrorHandler(void (* interruptHandler)(void));
void EUSART1_SetTxInterruptHandler(void (* interruptHandler)(void));
void EUSART1_SetRxInterruptHandler(void (* interruptHandler)(void));

#ifdef __cplusplus  // Provide C++ Compatibility

    }

#endif

#endif  // EUSART1_H
