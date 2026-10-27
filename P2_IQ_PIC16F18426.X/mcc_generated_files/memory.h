/**
  @File Name
    memory.h

  @Summary
    Acceso a la EEPROM de datos (Memory Module de MCC Classic).

  @Description
    La EEPROM de 256 bytes esta en 0xF000-0xF0FF (con NVMREGS = 1).
    MCC Classic genera tambien las funciones FLASH_x de la memoria de
    programa; este proyecto solo necesita las de la EEPROM.
    Equivalente a lo que genera MCC Classic con docs/configuracion_MCC_Classic.md.
*/

#ifndef MEMORY_H
#define MEMORY_H

/**
  Section: Included Files
*/

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus  // Provide C++ Compatibility

    extern "C" {

#endif

/**
  Section: Data EEPROM Module APIs
*/

/**
  @Summary
    Writes a data byte to Data EEPROM

  @Description
    This routine writes a data byte to given Data EEPROM location

  @Preconditions
    None

  @Param
    bAdd  - Data EEPROM location to which data to be written (0xF000-0xF0FF)
    bData - Data to be written to Data EEPROM location

  @Returns
    None
*/
void DATAEE_WriteByte(uint16_t bAdd, uint8_t bData);

/**
  @Summary
    Reads a data byte from Data EEPROM

  @Description
    This routine reads a data byte from given Data EEPROM location

  @Preconditions
    None

  @Param
    bAdd  - Data EEPROM location from which data has to be read (0xF000-0xF0FF)

  @Returns
    Data byte read from given Data EEPROM location
*/
uint8_t DATAEE_ReadByte(uint16_t bAdd);

#ifdef __cplusplus  // Provide C++ Compatibility

    }

#endif

#endif // MEMORY_H
