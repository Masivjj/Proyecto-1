/**
  @File Name
    memory.c

  @Summary
    Acceso a la EEPROM de datos (Memory Module de MCC Classic).

  @Description
    Ver memory.h. Equivalente a lo que genera MCC Classic con
    docs/configuracion_MCC_Classic.md.
*/

/**
  Section: Included Files
*/

#include <xc.h>
#include "memory.h"

/**
  Section: Data EEPROM Module APIs
*/

void DATAEE_WriteByte(uint16_t bAdd, uint8_t bData)
{
    uint8_t GIEBitValue = INTCONbits.GIE;

    NVMADRH = (uint8_t)((bAdd >> 8) & 0xFF);
    NVMADRL = (uint8_t)(bAdd & 0xFF);
    NVMDATL = bData;
    NVMCON1bits.NVMREGS = 1;
    NVMCON1bits.WREN = 1;
    INTCONbits.GIE = 0;     // Disable interrupts
    NVMCON2 = 0x55;
    NVMCON2 = 0xAA;
    NVMCON1bits.WR = 1;
    // Wait for write to complete
    while (NVMCON1bits.WR)
    {
    }

    NVMCON1bits.WREN = 0;
    INTCONbits.GIE = GIEBitValue;   // restore interrupt enable
}

uint8_t DATAEE_ReadByte(uint16_t bAdd)
{
    NVMADRH = (uint8_t)((bAdd >> 8) & 0xFF);
    NVMADRL = (uint8_t)(bAdd & 0xFF);
    NVMCON1bits.NVMREGS = 1;
    NVMCON1bits.RD = 1;
    NOP();  // NOPs may be required for latency at high frequencies
    NOP();

    return (NVMDATL);
}
