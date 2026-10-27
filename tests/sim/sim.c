/*
 * sim.c - Simula el firmware del PIC en el PC.
 *
 * Compila main.c y los .c de app/ SIN CAMBIOS contra un mcc.h falso (sim/mcc.h) e
 * implementa aqui un "hardware" simulado:
 *   - ADC: la tarjeta P2 con una senal sintetica (cero del puente, una
 *     fisura a 60 grados, un lift-off a 20 grados y algo de ruido), con VDD,
 *     VCC_A, V_REF y FVR, y la inversion de signo del canal I
 *   - UART: lo que envia el PIC sale por stdout; las ordenes siguen un guion
 *   - EEPROM: 256 bytes que se cargan y guardan en un archivo
 *   - TMR2: un tick de 10 ms cada dos vueltas del lazo principal, con dos
 *     rafagas que simulan un lazo lento (3 ticks a la vez y 15 a la vez)
 *   - pulsadores: una pulsacion de 150 ms y un rebote de 20 ms
 *
 * Uso: sim <segundos> <archivo_eeprom>     (ver tests/Makefile, "make sim")
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mcc_generated_files/mcc.h"
#include "app/app_config.h"

#define PI 3.14159265358979

/* ---- Registros simulados ---- */
sim_adcon2_t ADCON2bits;
sim_adcon3_t ADCON3bits;
volatile sim_pir1_t PIR1bits;
sim_intcon_t INTCONbits;
volatile sim_nvmcon1_t NVMCON1bits;
uint8_t NVMADRH, NVMADRL, NVMDATL, NVMCON2;
int sim_led;

/* ---- Tarjeta P2 simulada ---- */
static const double VDD = 3.70;         /* alimentacion del PIC                  */
static const double VCC_A = 3.65;       /* como en la medida real (seccion 9.2)  */
static const double V_REF = 3.65 / 2;
static const double G_TARJETA = 2.0;    /* jumpers JP701/JP702                   */

/* Guion de ordenes por UART: instante (s) y caracteres */
static const struct {
    double t;
    const char *orden;
} guion[] = {
    {0.50, "z"},    /* fijar cero sobre metal sano          */
    {1.00, "?"},    /* estado                               */
    {4.00, "c"},    /* calibrar theta durante el lift-off   */
    {5.00, "p"},    /* volcar la traza                      */
    {5.80, "1"},    /* ganancia x11 ...                     */
    {5.90, "2"},    /* ... y de vuelta a x2                 */
    {6.20, "q"},    /* orden desconocida                    */
    {9.00, "b"},    /* borrar traza                         */
};

static void (*manejador_tmr2)(void);
static long n_tick, max_ticks;
static uint8_t eeprom[256];
static const char *archivo_eeprom;
static uint16_t resultado_adc;
static char rx[64];
static int rx_n, rx_i;
static unsigned semilla = 12345u;

static double ruido(void)
{
    semilla = semilla * 1103515245u + 12345u;
    return (double)((semilla >> 16) & 0x7FFFu) / 32768.0 - 0.5;
}

/* Senal a la salida del mezclador, ya normalizada (mV), en el instante t (s) */
static void senal(double t, double *ip, double *qp)
{
    double i = 62.3276, q = 21.1352;            /* desequilibrio del puente */

    if (t >= 2.0 && t < 3.0) {                  /* fisura: dos lobulos a 60 grados */
        double a = 65.0 * sin(2 * PI * (t - 2.0));
        i += a * cos(60 * PI / 180);
        q += a * sin(60 * PI / 180);
    }
    if (t >= 3.5 && t < 4.5) {                  /* lift-off (papel) a 20 grados */
        i += 30.0 * cos(20 * PI / 180);
        q += 30.0 * sin(20 * PI / 180);
    }
    *ip = i + 0.2 * ruido();
    *qp = q + 0.2 * ruido();
}

/* Suma de N_SOBREMUESTREO conversiones de la tension v */
static uint16_t codigo(double v)
{
    double c = floor(v / VDD * ADC_CUENTAS + 0.5);

    if (c < 0) {
        c = 0;
    }
    if (c > ADC_CUENTAS) {
        c = ADC_CUENTAS;
    }
    return (uint16_t)(c * N_SOBREMUESTREO);
}

static void fallo(const char *motivo)
{
    fprintf(stderr, "sim: %s (tick %ld)\n", motivo, n_tick);
    exit(2);
}

/* ---- Drivers de MCC simulados ---- */

void SYSTEM_Initialize(void)
{
}

void ADCC_StartConversion(adcc_channel_t canal)
{
    double ip, qp, iraw, qraw;

    if (ADCON2bits.ADMD != 3 || ADCON2bits.ADCRS != 0 || ADCON3bits.ADTMD != 7) {
        fallo("ADCC sin Burst Average / ADCRS = 0 / ADTMD = siempre");
    }
    senal(n_tick * (PERIODO_MS / 1000.0), &ip, &qp);
    /* La tarjeta invierte I; la etapa DC multiplica por G; todo escala con VCC_A */
    iraw = -ip * G_TARJETA / 1000.0 * (VCC_A / 3.7);
    qraw = qp * G_TARJETA / 1000.0 * (VCC_A / 3.7);
    switch (canal) {
    case 0x10: resultado_adc = codigo(V_REF + iraw); break;
    case 0x11: resultado_adc = codigo(V_REF + qraw); break;
    case 0x12: resultado_adc = codigo(V_REF); break;
    case 0x13: resultado_adc = codigo(VCC_A / DIVISOR_VCCA); break;
    case 0x3E: resultado_adc = codigo(1.024); break;
    default: fallo("canal del ADC desconocido");
    }
    PIR1bits.ADTIF = 1;
}

uint16_t ADCC_GetFilterValue(void)
{
    return resultado_adc;
}

void ADCC_SetRepeatCount(uint8_t repeticiones)
{
    if (repeticiones != N_SOBREMUESTREO) {
        fallo("ADRPT distinto de N_SOBREMUESTREO");
    }
}

bool FVR_IsOutputReady(void)
{
    return true;
}

uint8_t DATAEE_ReadByte(uint16_t direccion)
{
    if (NVMCON1bits.WR) {
        fallo("lectura de la EEPROM con una escritura en curso");
    }
    if (direccion < 0xF000u || direccion > 0xF0FFu) {
        fallo("direccion de EEPROM fuera de rango");
    }
    return eeprom[direccion - 0xF000u];
}

bool EUSART1_is_rx_ready(void)
{
    return rx_i < rx_n;
}

uint8_t EUSART1_Read(void)
{
    return (uint8_t)rx[rx_i++];
}

void EUSART1_Write(uint8_t dato)
{
    putchar(dato);
}

void TMR2_SetInterruptHandler(void (*manejador)(void))
{
    manejador_tmr2 = manejador;
}

unsigned sim_boton(int n)
{
    double t = n_tick * (PERIODO_MS / 1000.0);

    if (n == 0) {
        return !(t >= 7.00 && t < 7.15);    /* "Fijar cero": 150 ms pulsado */
    }
    return !(t >= 8.00 && t < 8.02);        /* "Calibrar theta": rebote de 20 ms */
}

/* ---- Paso del tiempo ---- */

static void un_tick(void)
{
    size_t k;

    n_tick++;
    for (k = 0; k < sizeof guion / sizeof guion[0]; k++) {
        if ((long)(guion[k].t * 100 + 0.5) == n_tick) {
            size_t l = strlen(guion[k].orden);
            memcpy(rx + rx_n, guion[k].orden, l);
            rx_n += (int)l;
        }
    }
    manejador_tmr2();
}

void sim_gie(int habilitar)
{
    static int vuelta;
    int rafaga, r;
    FILE *f;

    INTCONbits.GIE = (unsigned)habilitar;
    if (habilitar || manejador_tmr2 == NULL) {
        return;
    }
    if ((++vuelta & 1) == 0) {
        return;                             /* dos vueltas del lazo por tick */
    }
    /* La escritura de EEPROM en curso termina antes del siguiente tick */
    if (NVMCON1bits.WR) {
        if (!NVMCON1bits.WREN || !NVMCON1bits.NVMREGS || NVMCON2 != 0xAA) {
            fallo("secuencia de escritura de la EEPROM incorrecta");
        }
        eeprom[((NVMADRH << 8) | NVMADRL) & 0xFF] = NVMDATL;
        NVMCON1bits.WR = 0;
    }
    if (n_tick >= max_ticks) {
        f = fopen(archivo_eeprom, "wb");
        if (f != NULL) {
            fwrite(eeprom, 1, sizeof eeprom, f);
            fclose(f);
        }
        exit(0);
    }
    /* Lazo lento: en t = 3.00 s llegan 3 ticks juntos y en t = 6.50 s, 15 */
    rafaga = (n_tick == 300) ? 3 : (n_tick == 650) ? 15 : 1;
    for (r = 0; r < rafaga; r++) {
        un_tick();
    }
}

void firmware_main(void);

int main(int argc, char **argv)
{
    FILE *f;

    if (argc != 3) {
        fprintf(stderr, "uso: %s <segundos> <archivo_eeprom>\n", argv[0]);
        return 1;
    }
    max_ticks = (long)(atof(argv[1]) * 100);
    archivo_eeprom = argv[2];
    memset(eeprom, 0xFF, sizeof eeprom);    /* EEPROM borrada */
    f = fopen(archivo_eeprom, "rb");
    if (f != NULL) {
        if (fread(eeprom, 1, sizeof eeprom, f) != sizeof eeprom) {
            memset(eeprom, 0xFF, sizeof eeprom);
        }
        fclose(f);
    }
    firmware_main();                        /* main() del firmware: no vuelve */
    return 1;
}
