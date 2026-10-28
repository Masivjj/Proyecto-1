/*
 * ajustes.h - I0, Q0, theta, G y etapas activas en la EEPROM.
 */
#ifndef AJUSTES_H
#define AJUSTES_H

#include <stdbool.h>

/* Lee la EEPROM y, si los datos son validos, los aplica a iq_proc.
 * Devuelve true si se cargaron. */
bool ajustes_cargar(void);

/* Toma la configuracion y la calibracion actuales de iq_proc y las deja
 * pendientes de guardar. No bloquea. */
void ajustes_guardar(void);

/* Llamar cada 10 ms: escribe en la EEPROM como mucho un byte pendiente. */
void ajustes_tarea(void);

#endif /* AJUSTES_H */
