/*
 * main.c - Detector de fisuras en aluminio por corrientes de Eddy:
 * procesamiento de las componentes DC I y Q de la tarjeta P2 con un
 * PIC16F18426 (MPLAB X + XC8 + MCC Classic).
 *
 * Reproduce el procesamiento del VI de LabVIEW P2_IQ_SINAD.vi
 * (docs/P2_IQ_procesamiento_para_PIC16F18426.md).
 *
 * El muestreo no lo hace este lazo: el PWM6 dispara cada conversion del ADC
 * por hardware cada 100 us y la interrupcion del ADC acumula las 16
 * conversiones de cada canal (adquisicion.c, docs/analisis_adc_jitter.md).
 * Cada 10 ms queda una trama en la cola, y este lazo:
 *   1. calcula Ip, Qp (procesadas) e I', Q' (finales)          (iq_proc.c)
 *   2. guarda (I', Q') en la traza del plano polar                (traza.c)
 *      y envia la muestra por UART                         (comunicacion.c)
 *   3. cada 10 muestras (0.1 s) cierra el bloque: cero, theta, |V|, Fase, d
 *   4. atiende pulsadores, ordenes por UART, EEPROM y LED
 * Si el lazo se retrasa (p. ej. el cierre de bloque con "Calibrar theta"
 * calcula atan2, sin y cos), las tramas esperan en la cola: las muestras se
 * tomaron a su hora y no se pierden mientras quepan (N_COLA_TRAMAS).
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

#define LED_PERIODO_TRAMAS      100u    /* destello de vida cada 1 s */
#define LED_ENCENDIDO_TRAMAS    5u      /* durante 50 ms             */

static uint8_t  banderas_lazo;          /* IQ_FL_RETRASO del bloque     */
static bool     alarma;                 /* ultimo bloque con IQ_FL_ALARMA */

static void led_tarea(void)
{
    static uint8_t fase;

    if (++fase >= LED_PERIODO_TRAMAS) {
        fase = 0u;
    }
    if (alarma || (fase < LED_ENCENDIDO_TRAMAS)) {
        LED_SetHigh();
    } else {
        LED_SetLow();
    }
}

static void atender_muestra(const iq_codigos_t *c, uint32_t t_ms, uint32_t muestras_perdidas)
{
    iq_muestra_t m;
    iq_bloque_t  b;
    bool fin_bloque;
    uint8_t ev;

    fin_bloque = iq_procesar(c, &m, &b);

    /* La traza se congela mientras se vuelca o mientras su memoria esta
     * prestada a una captura */
    if (!com_volcando_traza() && !adq_capturando()) {
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
    iq_codigos_t c;
    uint32_t trama;
    uint32_t perdidas = 0u;
    uint32_t perdidas_inicio;
    uint32_t perdidas_ahora;

    // initialize the device
    SYSTEM_Initialize();

    iq_inicializar();
    cargados = ajustes_cargar();
    adq_inicializar();

    // Enable the Global Interrupts
    INTERRUPT_GlobalInterruptEnable();

    // Enable the Peripheral Interrupts
    INTERRUPT_PeripheralInterruptEnable();

    com_bienvenida(cargados);
    /* El mensaje de bienvenida tarda unos 50 ms: lo que se pierda mientras
     * tanto no cuenta como retraso */
    perdidas_inicio = adq_tramas_perdidas();

    while (1)
    {
        if (adq_leer(&c, &trama)) {
            perdidas_ahora = adq_tramas_perdidas() - perdidas_inicio;
            if (perdidas_ahora != perdidas) {
                /* La cola se lleno: se descartaron muestras */
                perdidas = perdidas_ahora;
                banderas_lazo |= IQ_FL_RETRASO;
            }
            atender_muestra(&c, trama * PERIODO_MS, perdidas);
        }
    }
}
