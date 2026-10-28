/*
 * sim.c - Simula en el PC el firmware del avance "ADC a 1000 muestras/s"
 * (ADC_1kSPS_PIC16F18426.X/main.c, compilado SIN CAMBIOS contra adc1k/mcc.h).
 *
 * "Hardware" simulado:
 *   - TMR2 + PWM6: un disparo cada 1000 us desde el arranque; solo convierte
 *     si ADACT = PWM6_out. La conversion toma la senal 8 us despues del
 *     disparo (adquisicion) y pone ADIF; la interrupcion se atiende en cuanto
 *     GIE lo permite.
 *   - generador: 1.65 V + 1.5 V * sin(2 pi 117 Hz t) en RC0, VDD = 3.30 V,
 *     ruido del ADC de 0.3 LSB; el FVR da 1.024 V (conversiones por software)
 *   - UART a 115200 baudios con su velocidad real: 86.8 us por caracter y un
 *     buffer de 128; EUSART1_Write espera si esta lleno, como el driver de MCC
 *   - lazo principal: 5 us por vuelta y 4 us por caracter formateado; en
 *     t = 2 s se para 50 ms (la FIFO lo absorbe) y en t = 3 s, 200 ms (la FIFO
 *     se llena y se pierden muestras)
 *   - ordenes por la UART segun un guion
 *
 * Uso: sim <segundos> <archivo_conversiones>
 *   stdout: lo que envia el PIC; el archivo recibe "k,codigo" de cada
 *   conversion disparada, para comprobar que no falta ni sobra ninguna.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "mcc_generated_files/mcc.h"
#include "config.h"

#define PI              3.14159265358979
#define VDD             3.30
#define F_SENAL         117.0
#define T_CARACTER_US   (1e6 * 10.0 / 115200.0)
#define TX_BUFFER       128
#define US_POR_VUELTA   5.0
#define US_POR_CARACTER 4.0

/* ---- Registros simulados ---- */
volatile sim_adcon0_t ADCON0bits;
volatile sim_pir1_t PIR1bits;
sim_pie1_t PIE1bits;
sim_ansela_t ANSELAbits;
sim_trisa_t TRISAbits;
sim_slrcona_t SLRCONAbits;
uint8_t ADPCH, ADACT, RA1PPS;
int sim_led;

static const struct {
    double t;
    char orden;
} guion[] = {
    {1.00, '?'},        /* estado: 0 perdidas                       */
    {4.00, 'd'},        /* detener el envio ...                     */
    {4.50, 'i'},        /* ... y reanudarlo                         */
    {4.80, 'x'},        /* orden desconocida                        */
    {5.50, '?'},        /* estado: las perdidas de la parada de 3 s */
};

static double t_us, t_fin_us, proximo_disparo_us = PERIODO_US, tx_libre_us;
static int gie, n_guion;
static long disparos, conversiones;
static uint16_t resultado;
static void (*manejador_adc)(void);
static FILE *archivo_conversiones;
static char rx[16];
static int rx_n, rx_i;
static unsigned semilla = 2024u;

static double ruido_gauss(void)
{
    double u1, u2;

    semilla = semilla * 1103515245u + 12345u;
    u1 = ((semilla >> 8) & 0xFFFFFFu) / 16777216.0 + 1e-9;
    semilla = semilla * 1103515245u + 12345u;
    u2 = ((semilla >> 8) & 0xFFFFFFu) / 16777216.0;
    return sqrt(-2.0 * log(u1)) * cos(2 * PI * u2);
}

static uint16_t convertir(double v)
{
    double c = floor(v / VDD * 4095.0 + 0.3 * ruido_gauss() + 0.5);

    return (uint16_t)(c < 0 ? 0 : (c > 4095 ? 4095 : c));
}

static void fallo(const char *motivo)
{
    fflush(stdout);
    fprintf(stderr, "sim: %s (t = %.3f ms)\n", motivo, t_us / 1000);
    exit(2);
}

static void interrupcion(void)
{
    if (gie && PIE1bits.ADIE && PIR1bits.ADIF) {
        if (manejador_adc == NULL) {
            fallo("interrupcion del ADC sin manejador");
        }
        PIR1bits.ADIF = 0;              /* lo hace ADCC_ISR() */
        manejador_adc();
    }
}

static void terminar(void)
{
    fflush(stdout);
    fclose(archivo_conversiones);
    fprintf(stderr, "sim: %ld disparos, %ld conversiones, %d cambios del LED\n",
            disparos, conversiones, sim_led);
    exit(0);
}

/* El tiempo pasa: disparos del PWM6 (conversiones e interrupciones) */
static void avanzar(double dt_us)
{
    double objetivo = t_us + dt_us;

    while (proximo_disparo_us <= objetivo) {
        t_us = proximo_disparo_us;
        proximo_disparo_us += PERIODO_US;
        disparos++;
        if (ADACT == 0x0E) {
            double t = (t_us + 8.0) / 1e6;
            if (ADPCH != SENAL) {
                fallo("conversion disparada con un canal distinto de RC0");
            }
            resultado = convertir(1.65 + 1.5 * sin(2 * PI * F_SENAL * t));
            fprintf(archivo_conversiones, "%ld,%u\n", conversiones++, resultado);
            PIR1bits.ADIF = 1;
            interrupcion();
        }
        if (t_us >= t_fin_us) {
            terminar();
        }
    }
    t_us = objetivo;
}

/* ---- Drivers de MCC simulados ---- */

void SYSTEM_Initialize(void)
{
    ADACT = 0x0E;                       /* como ADCC_Initialize() */
    PIE1bits.ADIE = 1;
}

void sim_gie(int habilitar)
{
    gie = habilitar;
    interrupcion();                     /* una ADIF pendiente se atiende ahora */
}

uint16_t ADCC_GetConversionResult(void)
{
    return resultado;
}

uint16_t ADCC_GetSingleConversion(adcc_channel_t canal)
{
    if (ADACT != 0) {
        fallo("conversion por software con el disparo del PWM6 activo");
    }
    if (canal != channel_FVR_buffer1) {
        fallo("conversion por software de un canal inesperado");
    }
    avanzar(22.0);
    PIR1bits.ADIF = 1;                  /* ADIF tambien se pone (GIE esta apagado) */
    return convertir(1.024);
}

void ADCC_SetADIInterruptHandler(void (*manejador)(void))
{
    manejador_adc = manejador;
}

bool FVR_IsOutputReady(void)
{
    return true;
}

/* UART: cada caracter ocupa la linea T_CARACTER_US. Sin terminar de salir
 * caben TX_BUFFER caracteres en el buffer del driver, uno en TX1REG y otro en
 * el registro de desplazamiento: con 130 pendientes, EUSART1_Write espera. */
void EUSART1_Write(uint8_t dato)
{
    avanzar(US_POR_CARACTER);
    while (tx_libre_us - t_us > (TX_BUFFER + 1) * T_CARACTER_US - 1e-6) {
        avanzar(tx_libre_us - t_us - TX_BUFFER * T_CARACTER_US);
    }
    tx_libre_us = (tx_libre_us > t_us ? tx_libre_us : t_us) + T_CARACTER_US;
    putchar(dato);
}

bool EUSART1_is_rx_ready(void)
{
    static int parado_2s, parado_3s;

    avanzar(US_POR_VUELTA);
    if (!parado_2s && t_us >= 2.0e6) {
        parado_2s = 1;
        avanzar(50e3);                  /* el lazo se para 50 ms */
    }
    if (!parado_3s && t_us >= 3.0e6) {
        parado_3s = 1;
        avanzar(200e3);                 /* y luego 200 ms */
    }
    while (n_guion < (int)(sizeof guion / sizeof guion[0]) && t_us >= guion[n_guion].t * 1e6) {
        rx[rx_n++] = guion[n_guion++].orden;
    }
    return rx_i < rx_n;
}

uint8_t EUSART1_Read(void)
{
    return (uint8_t)rx[rx_i++];
}

void firmware_main(void);

int main(int argc, char **argv)
{
    if (argc != 3) {
        fprintf(stderr, "uso: %s <segundos> <archivo_conversiones>\n", argv[0]);
        return 1;
    }
    t_fin_us = atof(argv[1]) * 1e6;
    archivo_conversiones = fopen(argv[2], "w");
    if (archivo_conversiones == NULL) {
        perror(argv[2]);
        return 1;
    }
    firmware_main();                    /* main() del firmware: no vuelve */
    return 1;
}
