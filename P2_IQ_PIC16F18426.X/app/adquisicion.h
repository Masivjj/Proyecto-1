/*
 * adquisicion.h - Lectura de I_OUT, Q_OUT, V_REF, VCC_A y el FVR con el ADCC.
 */
#ifndef ADQUISICION_H
#define ADQUISICION_H

#include "iq_proc.h"

/* Asegura la configuracion del ADCC que necesita iq_proc.c y espera al FVR.
 * Llamar despues de SYSTEM_Initialize(). */
void adq_inicializar(void);

/* Convierte los cinco canales; cada codigo es la suma de N_SOBREMUESTREO
 * conversiones. Tarda unos 1.8 ms. */
void adq_leer(iq_codigos_t *c);

#endif /* ADQUISICION_H */
