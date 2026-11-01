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
 *
 * Si el externo se para en marcha, el monitor de fallos (FSCM) pasa el
 * sistema al HFINTOSC a 1 MHz y activa OSFIF (seccion 8.4.2 de la hoja de
 * datos DS40001985C). A 1 MHz todo iria 16 veces mas lento (disparos del
 * ADC cada 1.6 ms, UART a ~7200 baudios), asi que el fallo se atiende en la
 * interrupcion (OSFIE): reloj_fallo_isr() vuelve a 16 MHz en unos us.
 * Cambiar NOSC/NDIV despeja la condicion de fallo (8.4.3).
 */
#include "../mcc_generated_files/mcc.h"
#include "app_config.h"
#include "reloj.h"

#define OSCCON1_HFINTOSC    0x60u   /* NOSC = 110: HFINTOSC a OSCFRQ; NDIV = 1:1 */
#define OSCFRQ_16MHZ        0x05u   /* HFFRQ = 101: 16 MHz (tabla de OSCFRQ)       */
#define OSCCON1_EXT_PLL4X   0x20u   /* NOSC = 010: EXTOSC con PLL x4; NDIV = 1:1 */
#define COSC_EXT_PLL4X      0x02u
#define ESPERA_PASOS        1000u   /* x 100 us: hasta 100 ms para que arranque */

#if RELOJ_EXTERNO
typedef char reloj_16_mhz[(F_EXTERNO_HZ * 4u == FOSC_HZ) ? 1 : -1];
#endif

static volatile uint8_t estado = RELOJ_EST_INTERNO;
static volatile bool    aviso_pendiente;        /* fallo en marcha sin avisar */

#if RELOJ_EXTERNO
/* Pasa (o vuelve) al HFINTOSC a 16 MHz: misma FOSC que el externo x4 */
static void usar_hfintosc_16mhz(void)
{
    OSCFRQ = OSCFRQ_16MHZ;
    OSCCON1 = OSCCON1_HFINTOSC;
}
#endif

void reloj_inicializar(void)
{
#if RELOJ_EXTERNO
    uint16_t pasos = 0u;

    PIR1bits.OSFIF = 0;                 /* hay que borrarlo antes del cambio (8.4.3) */
    OSCCON1 = OSCCON1_EXT_PLL4X;
    while ((OSCCON2bits.COSC != COSC_EXT_PLL4X) && !PIR1bits.OSFIF && (++pasos < ESPERA_PASOS)) {
        __delay_us(100);
    }
    if ((OSCCON2bits.COSC == COSC_EXT_PLL4X) && !PIR1bits.OSFIF) {
        estado = RELOJ_EST_EXTERNO;
        PIE1bits.OSFIE = 1;             /* un fallo posterior se atiende al momento */
    } else {
        /* Sin reloj externo (o el PLL no engancha): se abandona el cambio */
        usar_hfintosc_16mhz();
        PIR1bits.OSFIF = 0;
        estado = RELOJ_EST_RESERVA;
    }
#else
    estado = RELOJ_EST_INTERNO;
#endif
}

#if RELOJ_EXTERNO
void reloj_fallo_isr(void)
{
    /* El FSCM ha pasado al HFINTOSC a 1 MHz: se vuelve a 16 MHz y ya no se
     * usa el externo hasta el siguiente reset. */
    PIR1bits.OSFIF = 0;
    usar_hfintosc_16mhz();
    PIE1bits.OSFIE = 0;
    if (estado == RELOJ_EST_EXTERNO) {
        estado = RELOJ_EST_RESERVA;
        aviso_pendiente = true;
    }
}
#endif

bool reloj_tarea(void)
{
    bool aviso = aviso_pendiente;

    if (aviso) {
        aviso_pendiente = false;
    }
    return aviso;
}

uint8_t reloj_estado(void)
{
    return estado;
}
