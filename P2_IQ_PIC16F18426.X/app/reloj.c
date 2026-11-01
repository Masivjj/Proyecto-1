/*
 * reloj.c - Paso al reloj externo de 4 MHz (cristal u oscilador) con el PLL
 * x4, y reserva con el HFINTOSC (docs/reloj_externo_4MHz.md).
 *
 * Los bits de configuracion arrancan siempre con el HFINTOSC (RSTOSC =
 * HFINT32) y FEXTOSC prepara el oscilador externo (HS/XT con cristal, ECM
 * con oscilador). OSCILLATOR_Initialize() de MCC deja el HFINTOSC a 16 MHz
 * (OSCFRQ) y los perifericos (TMR2, ADC, EUSART) configurados para 16 MHz.
 * Aqui se pide el cambio al externo (seccion "Clock Switching" de la hoja de
 * datos): el HFINTOSC sigue funcionando hasta que el externo ha superado el
 * temporizador de arranque (OST, 1024 ciclos) y el PLL esta enganchado;
 * entonces el cambio es automatico (CSWHOLD = 0) y COSC lo refleja.
 *
 * Como el externo y el HFINTOSC de reserva dan los mismos 16 MHz, los
 * perifericos no se tocan: si el externo no esta, el firmware funciona igual
 * con la exactitud del HFINTOSC (+-2 %).
 */
#include "../mcc_generated_files/mcc.h"
#include "app_config.h"
#include "reloj.h"

#define OSCCON1_HFINTOSC    0x60u   /* NOSC = 110: HFINTOSC a OSCFRQ; NDIV = 1:1 */
#define OSCCON1_EXT_PLL4X   0x20u   /* NOSC = 010: EXTOSC con PLL x4; NDIV = 1:1 */
#define COSC_EXT_PLL4X      0x02u
#define ESPERA_PASOS        1000u   /* x 100 us: hasta 100 ms para que arranque */

#if RELOJ_EXTERNO
typedef char reloj_16_mhz[(F_EXTERNO_HZ * 4u == FOSC_HZ) ? 1 : -1];
#endif

static uint8_t estado = RELOJ_EST_INTERNO;

void reloj_inicializar(void)
{
#if RELOJ_EXTERNO
    uint16_t pasos = 0u;

    PIR1bits.OSFIF = 0;
    OSCCON1 = OSCCON1_EXT_PLL4X;
    while ((OSCCON2bits.COSC != COSC_EXT_PLL4X) && (++pasos < ESPERA_PASOS)) {
        __delay_us(100);
    }
    if (OSCCON2bits.COSC == COSC_EXT_PLL4X) {
        estado = RELOJ_EST_EXTERNO;
    } else {
        /* Sin reloj externo: se abandona el cambio y sigue el HFINTOSC */
        OSCCON1 = OSCCON1_HFINTOSC;
        estado = RELOJ_EST_RESERVA;
    }
#else
    estado = RELOJ_EST_INTERNO;
#endif
}

bool reloj_tarea(void)
{
#if RELOJ_EXTERNO
    if (PIR1bits.OSFIF) {
        /* El FSCM ha pasado a un reloj interno: se fija el HFINTOSC a la
         * frecuencia de OSCFRQ (16 MHz) y no se vuelve al externo. */
        PIR1bits.OSFIF = 0;
        OSCCON1 = OSCCON1_HFINTOSC;
        if (estado == RELOJ_EST_EXTERNO) {
            estado = RELOJ_EST_RESERVA;
            return true;
        }
    }
#endif
    return false;
}

uint8_t reloj_estado(void)
{
    return estado;
}
