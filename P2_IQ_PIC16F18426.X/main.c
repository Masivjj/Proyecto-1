/*
 * main.c - Detector de fisuras en aluminio por corrientes de Eddy:
 * procesamiento de las componentes DC I y Q de la tarjeta P2 con un
 * PIC16F18426 (MPLAB X + XC8 + MCC Classic).
 *
 * Reproduce el procesamiento del VI de LabVIEW P2_IQ_SINAD.vi
 * (docs/P2_IQ_procesamiento_para_PIC16F18426.md). Cada 10 ms (TMR2):
 *   1. convierte I_OUT, Q_OUT, V_REF, VCC_A y el FVR, 16 veces cada uno
 *      (adquisicion.c)
 *   2. calcula Ip, Qp (procesadas) e I', Q' (finales)          (iq_proc.c)
 *   3. guarda (I', Q') en la traza del plano polar                (traza.c)
 *      y envia la muestra por UART                         (comunicacion.c)
 *   4. cada 10 muestras (0.1 s) cierra el bloque: cero, theta, |V|, Fase, d
 *   5. atiende pulsadores, ordenes por UART, EEPROM y LED
 *
 * El LED (RA5) da un destello de 50 ms cada segundo mientras todo va bien y
 * se queda encendido si el ultimo bloque tuvo VCC_A fuera de rango o alguna
 * salida saturada.
 */

#include "mcc_generated_files/mcc.h"
#include "app/adquisicion.h"
#include "app/ajustes.h"
#include "app/botones.h"
#include "app/comunicacion.h"
#include "app/iq_proc.h"
#include "app/traza.h"

#define LED_PERIODO_TICKS   100u    /* destello de vida cada 1 s */
#define LED_ENCENDIDO_TICKS 5u      /* durante 50 ms             */

/* Un tick que no se atiende a tiempo (p. ej. el cierre de bloque con
 * "Calibrar theta" calcula atan2, sin y cos y puede pasar de 10 ms) se
 * atiende justo despues: la muestra sale con su t_ms y no se pierde. Solo
 * si el lazo va mas de MAX_ATRASO_TICKS por detras se descartan muestras. */
#define MAX_ATRASO_TICKS    10u     /* 100 ms */

static volatile uint8_t ticks_pendientes;   /* los suma el TMR2 cada 10 ms */

static uint32_t t_ms;                       /* instante de la muestra (ms)  */
static uint32_t muestras_perdidas;          /* descartadas por retraso      */
static uint8_t  banderas_lazo;              /* IQ_FL_RETRASO del bloque     */
static bool     alarma;                     /* ultimo bloque con IQ_FL_ALARMA */

/* Contexto de interrupcion (TMR2_ISR), cada 10 ms */
static void tick_10ms(void)
{
    if (ticks_pendientes != 0xFFu) {
        ticks_pendientes++;
    }
}

static void led_tarea(void)
{
    static uint8_t fase;

    if (++fase >= LED_PERIODO_TICKS) {
        fase = 0u;
    }
    if (alarma || (fase < LED_ENCENDIDO_TICKS)) {
        LED_SetHigh();
    } else {
        LED_SetLow();
    }
}

static void atender_muestra(void)
{
    iq_codigos_t c;
    iq_muestra_t m;
    iq_bloque_t  b;
    bool fin_bloque;
    uint8_t ev;

    adq_leer(&c);
    fin_bloque = iq_procesar(&c, &m, &b);

    if (!com_volcando_traza()) {        /* congelada mientras se vuelca */
        traza_agregar(m.I1, m.Q1);
    }
    com_muestra(t_ms, &m);

    if (fin_bloque) {
        if ((b.eventos & IQ_EV_BORRAR_TRAZA) != 0u) {
            traza_borrar();
        }
        if ((b.eventos & IQ_EV_CALIBRACION) != 0u) {
            ajustes_guardar();
        }
        b.banderas |= banderas_lazo;
        banderas_lazo = 0u;
        alarma = ((b.banderas & IQ_FL_ALARMA) != 0u);
        com_bloque(&b);
    }

    ev = botones_tarea();
    if (ev != 0u) {
        iq_evento(ev);
        com_eventos_boton(ev);
    }
    com_tarea(muestras_perdidas);
    ajustes_tarea();
    led_tarea();
}

/*
                         Main application
 */
void main(void)
{
    bool cargados;
    uint8_t n;
    uint16_t atrasados = 0u;    /* ticks por atender */

    // initialize the device
    SYSTEM_Initialize();

    iq_inicializar();
    cargados = ajustes_cargar();
    adq_inicializar();
    TMR2_SetInterruptHandler(tick_10ms);

    // Enable the Global Interrupts
    INTERRUPT_GlobalInterruptEnable();

    // Enable the Peripheral Interrupts
    INTERRUPT_PeripheralInterruptEnable();

    com_bienvenida(cargados);

    /* El mensaje de bienvenida tarda unos ms: no cuenta como retraso */
    INTERRUPT_GlobalInterruptDisable();
    ticks_pendientes = 0u;
    INTERRUPT_GlobalInterruptEnable();

    while (1)
    {
        INTERRUPT_GlobalInterruptDisable();
        n = ticks_pendientes;
        ticks_pendientes = 0u;
        INTERRUPT_GlobalInterruptEnable();

        atrasados += n;
        if (atrasados > MAX_ATRASO_TICKS) {
            /* Demasiado retraso: se descartan todas las muestras menos una */
            muestras_perdidas += (uint32_t)(atrasados - 1u);
            t_ms += (uint32_t)(atrasados - 1u) * PERIODO_MS;
            atrasados = 1u;
            banderas_lazo |= IQ_FL_RETRASO;
        }
        if (atrasados != 0u) {
            atrasados--;
            t_ms += PERIODO_MS;
            atender_muestra();
        }
    }
}
