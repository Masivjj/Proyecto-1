/*
 * main.c - Avance del proyecto: el ADC del PIC16F18426 muestrea una senal
 * (de un generador) a 1000 muestras/s y envia cada muestra por la UART.
 * MPLAB X + XC8 + MCC Classic (docs/avance_adc_1ksps.md).
 *
 * El muestreo no lo hace este lazo: el flanco de subida del PWM6 (TMR2, cada
 * 1 ms) arranca cada conversion del ADC por hardware (modo basico, disparo
 * ADACT = PWM6_out), asi que el instante de muestreo no depende del software
 * y su jitter es el del oscilador (docs/analisis_adc_jitter.md). La
 * interrupcion de fin de conversion deja el codigo en una cola (FIFO) y este
 * lazo lo envia como texto, una linea por muestra:
 *
 *     2047\r\n            codigo del ADC, 0..4095 (VDD = 4095)
 *
 * Las lineas que empiezan por '#' son mensajes (cabecera, respuestas, avisos).
 * Ordenes de un caracter: d = detener el envio, i = reanudarlo, ? = estado.
 *
 * A 115200 baudios caben 11520 caracteres/s; 1000 lineas de hasta 6
 * caracteres ocupan el 52 %. Si el lazo se retrasa (p. ej. mientras envia un
 * mensaje), las muestras esperan en la FIFO (N_FIFO - 1 = 127 ms) y no se
 * pierden; si se llena, se cuentan y se avisa con "# perdidas: N".
 */

#include "mcc_generated_files/mcc.h"
#include "config.h"

#define ADACT_PWM6_OUT      0x0Eu   /* tabla 32-6 */
#define PPS_PWM6OUT         0x0Du   /* tabla 16-3 */
#define FVR_PROMEDIO        16u
#define FIFO_MASCARA        (N_FIFO - 1u)

typedef char fifo_potencia_de_2[((N_FIFO & FIFO_MASCARA) == 0u) && (N_FIFO <= 256u) ? 1 : -1];

/* Cola de muestras: escribe la interrupcion, lee el lazo principal */
static volatile uint16_t fifo[N_FIFO];
static volatile uint8_t  fifo_escr;
static volatile uint8_t  fifo_lect;
static volatile bool     enviando = true;
static volatile uint32_t convertidas;
static volatile uint32_t perdidas;
static volatile uint16_t cuenta_led;

static uint16_t vdd_mv;

/* Contexto de interrupcion: ADIF, al terminar cada conversion */
static void al_terminar_conversion(void)
{
    uint16_t codigo = ADCC_GetConversionResult();
    uint8_t siguiente;

    convertidas++;
    if (++cuenta_led >= MUESTRAS_LED) {
        cuenta_led = 0u;
        LED_Toggle();
    }
    if (!enviando) {
        return;
    }
    siguiente = (uint8_t)((fifo_escr + 1u) & FIFO_MASCARA);
    if (siguiente == fifo_lect) {
        perdidas++;                     /* la UART no da abasto */
        return;
    }
    fifo[fifo_escr] = codigo;
    fifo_escr = siguiente;
}

static void tx_texto(const char *s)
{
    while (*s != '\0') {
        EUSART1_Write((uint8_t)*s++);
    }
}

/* Sin division: se restan potencias de 10 (el PIC16 no divide por hardware) */
static void tx_u32(uint32_t v)
{
    static const uint32_t potencias[9] = {
        1000000000UL, 100000000UL, 10000000UL, 1000000UL,
        100000UL, 10000UL, 1000UL, 100UL, 10UL
    };
    bool empezado = false;
    uint8_t i;
    char digito;

    for (i = 0u; i < 9u; i++) {
        digito = '0';
        while (v >= potencias[i]) {
            v -= potencias[i];
            digito++;
        }
        if (empezado || (digito != '0')) {
            EUSART1_Write((uint8_t)digito);
            empezado = true;
        }
    }
    EUSART1_Write((uint8_t)('0' + (uint8_t)v));
}

static void tx_fin(void)
{
    EUSART1_Write('\r');
    EUSART1_Write('\n');
}

/* Lee un contador de 32 bits que cambia en la interrupcion */
static uint32_t leer_contador(volatile uint32_t *c)
{
    uint32_t v;

    INTERRUPT_GlobalInterruptDisable();
    v = *c;
    INTERRUPT_GlobalInterruptEnable();
    return v;
}

/* VDD (la referencia del ADC) a partir del FVR de 1.024 V. Se mide al
 * arrancar, antes de habilitar el disparo, con conversiones por software. */
static uint16_t medir_vdd_mv(void)
{
    uint32_t suma = 0u;
    uint16_t espera = 0u;
    uint8_t k;

    ADACT = 0x00u;                      /* sin disparo del PWM6 mientras tanto */
    while (ADCON0bits.ADGO) {
    }
    while (!FVR_IsOutputReady() && (++espera < 20000u)) {
    }
    (void)ADCC_GetSingleConversion(channel_FVR_buffer1);   /* carga CHOLD */
    for (k = 0u; k < FVR_PROMEDIO; k++) {
        suma += ADCC_GetSingleConversion(channel_FVR_buffer1);
    }
    if (suma == 0u) {
        return 0u;
    }
    return (uint16_t)(((uint32_t)FVR_MV * 4095UL * FVR_PROMEDIO + suma / 2u) / suma);
}

static void iniciar_muestreo(void)
{
    fifo_escr = 0u;
    fifo_lect = 0u;
    ADPCH = CANAL_SENAL;
    ADCC_SetADIInterruptHandler(al_terminar_conversion);
    ADACT = ADACT_PWM6_OUT;
    PIR1bits.ADIF = 0;
    PIE1bits.ADIE = 1;

#if SALIDA_DISPARO && !defined(__DEBUG)
    /* El PWM6 por RA1 (ICSPCLK): cada flanco de subida es el disparo de una
     * conversion. Sin limitar la pendiente, para medir su jitter con un
     * osciloscopio. */
    RA1PPS = PPS_PWM6OUT;
    ANSELAbits.ANSA1 = 0;
    SLRCONAbits.SLRA1 = 0;
    TRISAbits.TRISA1 = 0;
#endif
}

static void enviar_cabecera(void)
{
    tx_texto("# ADC 1 kS/s - PIC16F18426 - firmware " VERSION_FIRMWARE "\r\n");
    tx_texto("# entrada RC0 (ANC0), 12 bits, referencia VDD = ");
    tx_u32(vdd_mv);
    tx_texto(" mV (medida con el FVR, +-4 %)\r\n");
    tx_texto("# muestreo: 1 conversion cada ");
    tx_u32(PERIODO_US);
    tx_texto(" us, disparada por el PWM6\r\n");
    tx_texto("# formato: una linea por muestra con el codigo del ADC (0-4095)\r\n");
    tx_texto("# ordenes: d=detener envio  i=reanudar  ?=estado\r\n");
}

static void enviar_estado(void)
{
    enviar_cabecera();
    tx_texto("# estado: envio ");
    tx_texto(enviando ? "en marcha" : "detenido");
    tx_texto(", convertidas ");
    tx_u32(leer_contador(&convertidas));
    tx_texto(", perdidas ");
    tx_u32(leer_contador(&perdidas));
    tx_fin();
}

static void atender_orden(char orden)
{
    switch (orden) {
    case 'd':
        enviando = false;
        fifo_lect = fifo_escr;          /* lo que quedaba en la cola tampoco sale */
        tx_texto("# d: envio detenido\r\n");
        break;
    case 'i':
        tx_texto("# i: envio en marcha\r\n");
        enviando = true;
        break;
    case '?':
    case 'h':
        enviar_estado();
        break;
    case '\r':
    case '\n':
    case ' ':
        break;
    default:
        tx_texto("# orden desconocida (? = ayuda)\r\n");
        break;
    }
}

/*
                         Main application
 */
void main(void)
{
    uint32_t perdidas_avisadas = 0u;
    uint32_t perdidas_ahora;
    uint16_t codigo;

    // initialize the device
    SYSTEM_Initialize();

    vdd_mv = medir_vdd_mv();
    iniciar_muestreo();

    // Enable the Global Interrupts
    INTERRUPT_GlobalInterruptEnable();

    // Enable the Peripheral Interrupts
    INTERRUPT_PeripheralInterruptEnable();

    enviar_cabecera();

    while (1)
    {
        while (EUSART1_is_rx_ready()) {
            atender_orden((char)EUSART1_Read());
        }

        if (fifo_lect != fifo_escr) {
            /* fifo[fifo_lect] no lo toca la interrupcion hasta que avance fifo_lect */
            codigo = fifo[fifo_lect];
            fifo_lect = (uint8_t)((fifo_lect + 1u) & FIFO_MASCARA);
            tx_u32(codigo);
            tx_fin();
        } else {
            /* Al dia: se avisa si se perdieron muestras */
            perdidas_ahora = leer_contador(&perdidas);
            if (perdidas_ahora != perdidas_avisadas) {
                perdidas_avisadas = perdidas_ahora;
                tx_texto("# perdidas: ");
                tx_u32(perdidas_ahora);
                tx_fin();
            }
        }
    }
}
