/*
 * adquisicion.c - Lectura de los canales con el ADCC en modo Burst Average.
 *
 * Al poner ADGO, el hardware repite la conversion del canal hasta que ADCNT
 * llega a ADRPT (16). Entonces ADFLTR = ADACC >> ADCRS = suma de las 16
 * conversiones (ADCRS = 0) y, con ADTMD = "siempre", se activa ADTIF.
 * Todas las conversiones son contra VDD, que se cancela en la normalizacion
 * ratiometrica (seccion 10.4).
 *
 * Tiempo por canal: 16 x (8 us de adquisicion + 14 TAD + 2 TCY) ~ 0.36 ms.
 * Cinco canales: ~1.8 ms de cada periodo de 10 ms.
 * Entre I y Q pasan ~0.36 ms: con senales de 10 Hz el desfase es despreciable.
 */
#include "../mcc_generated_files/mcc.h"
#include "adquisicion.h"

/* Canales del ADC (tabla 32-4 de la hoja de datos). Se usan los valores y no
 * los nombres que genera MCC para no depender de como se llamen los pines. */
#define CANAL_I_OUT     ((adcc_channel_t)0x10)  /* RC0 / ANC0 */
#define CANAL_Q_OUT     ((adcc_channel_t)0x11)  /* RC1 / ANC1 */
#define CANAL_V_REF     ((adcc_channel_t)0x12)  /* RC2 / ANC2 */
#define CANAL_VCC_A     ((adcc_channel_t)0x13)  /* RC3 / ANC3 */
#define CANAL_FVR       ((adcc_channel_t)0x3E)  /* buffer 1 del FVR */

#define ADMD_BURST_AVERAGE  3u
#define ADTMD_SIEMPRE       7u

/* Proteccion contra un bloqueo si el ADC no responde: unas 20 000 vueltas
 * (decenas de ms). Una rafaga normal tarda ~0.36 ms. */
#define ESPERA_MAX      20000u

static uint16_t rafaga(adcc_channel_t canal)
{
    uint16_t espera = 0u;

    PIR1bits.ADTIF = 0;
    ADCC_StartConversion(canal);
    while ((PIR1bits.ADTIF == 0u) && (++espera < ESPERA_MAX)) {
    }
    return ADCC_GetFilterValue();
}

void adq_inicializar(void)
{
    uint16_t espera = 0u;

    /* MCC ya deja el ADCC asi (docs/configuracion_MCC_Classic.md); se fija
     * aqui tambien porque iq_proc.c depende de que el resultado sea la SUMA
     * de N_SOBREMUESTREO conversiones. */
    ADCC_SetRepeatCount(N_SOBREMUESTREO);
    ADCON2bits.ADCRS = 0;
    ADCON2bits.ADMD = ADMD_BURST_AVERAGE;
    ADCON3bits.ADTMD = ADTMD_SIEMPRE;

#if USAR_FVR
    while (!FVR_IsOutputReady() && (++espera < ESPERA_MAX)) {
    }
#else
    (void)espera;
#endif
}

void adq_leer(iq_codigos_t *c)
{
    c->cI = rafaga(CANAL_I_OUT);
    c->cQ = rafaga(CANAL_Q_OUT);
    c->cR = rafaga(CANAL_V_REF);
    c->cV = rafaga(CANAL_VCC_A);
#if USAR_FVR
    c->cF = rafaga(CANAL_FVR);
#else
    c->cF = 0u;
#endif
}
