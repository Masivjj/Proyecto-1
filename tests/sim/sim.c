/*
 * sim.c - Simula el firmware del PIC en el PC.
 *
 * Compila main.c y los .c de app/ SIN CAMBIOS contra un mcc.h falso
 * (sim/mcc.h) e implementa aqui un "hardware" simulado:
 *   - PWM6 + ADC: cada 100 us (un disparo) se convierte el canal que la
 *     interrupcion dejo en ADPCH y se llama a esa interrupcion, como en el PIC.
 *     El ADC tiene un offset de +3 LSB (lo mide el autocero en AVSS).
 *   - tarjeta P2: senal sintetica (cero del puente, una fisura a 60 grados,
 *     un lift-off a 20 grados y algo de ruido) con VDD, VCC_A, V_REF, FVR y
 *     la inversion de signo del canal I
 *   - UART: lo que envia el PIC sale por stdout; las ordenes siguen un guion
 *   - EEPROM: 256 bytes que se cargan y guardan en un archivo
 *   - lazo principal: cada vez que deshabilita las interrupciones pasan 2.5 ms;
 *     en t = 3 s se para 30 ms (la cola de tramas lo absorbe) y en t = 6.5 s,
 *     150 ms (la cola se llena y se pierden tramas)
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
#define OFFSET_ADC          3           /* LSB, en todas las conversiones */
#define DISPAROS_POR_VUELTA 25          /* 2.5 ms cada vez que main deshabilita GIE */

/* ---- Registros simulados ---- */
sim_adcon0_t ADCON0bits;
sim_adcon2_t ADCON2bits;
volatile sim_pir1_t PIR1bits;
sim_pie1_t PIE1bits;
sim_ansela_t ANSELAbits;
sim_trisa_t TRISAbits;
sim_slrcona_t SLRCONAbits;
uint8_t ADPCH, ADACT, RA1PPS;
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
    {7.50, "m"},    /* captura para medir el jitter         */
    {9.00, "b"},    /* borrar traza                         */
    {9.30, "Z"},    /* desactivar la etapa de cero ...      */
    {9.45, "z"},    /* ... y "Fijar cero" se ignora         */
    {9.60, "T"},    /* desactivar la etapa de theta         */
    {9.70, "A"},    /* desactivar el autocero               */
    {9.80, "?"},    /* estado: muestras perdidas y offset   */
};

static void (*manejador_adc)(void);
static long n_disparo, max_disparos;
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

/* Una conversion de 12 bits de la tension v, con el offset del ADC */
static uint16_t convertir(double v)
{
    double c = floor(v / VDD * ADC_CUENTAS + 0.5) + OFFSET_ADC;

    if (c < 0) {
        c = 0;
    }
    if (c > ADC_CUENTAS) {
        c = ADC_CUENTAS;
    }
    return (uint16_t)c;
}

static void fallo(const char *motivo)
{
    fprintf(stderr, "sim: %s (disparo %ld)\n", motivo, n_disparo);
    exit(2);
}

/* ---- Drivers de MCC simulados ---- */

void SYSTEM_Initialize(void)
{
}

uint16_t ADCC_GetConversionResult(void)
{
    return resultado_adc;
}

void ADCC_SetADIInterruptHandler(void (*manejador)(void))
{
    manejador_adc = manejador;
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

unsigned sim_boton(int n)
{
    double t = n_disparo * (PERIODO_DISPARO_US / 1e6);

    if (n == 0) {
        return !(t >= 7.00 && t < 7.15);    /* "Fijar cero": 150 ms pulsado */
    }
    return !(t >= 8.00 && t < 8.02);        /* "Calibrar theta": rebote de 20 ms */
}

/* ---- Paso del tiempo ---- */

/* Un flanco del PWM6: conversion del canal en ADPCH e interrupcion ADIF */
static void un_disparo(void)
{
    double t = n_disparo * (PERIODO_DISPARO_US / 1e6), ip, qp, v;
    long trama;
    size_t k;

    if (ADACT != 0x0E || ADCON2bits.ADMD != 0 || ADCON0bits.ADCS != 0 || ADCON0bits.ADCONT != 0) {
        fallo("ADC sin modo basico / disparo PWM6_out / reloj FOSC");
    }
    if (!PIE1bits.ADIE || manejador_adc == NULL) {
        fallo("interrupcion del ADC sin habilitar");
    }
    senal(t, &ip, &qp);
    switch (ADPCH) {
    /* La tarjeta invierte I; la etapa DC multiplica por G; todo escala con VCC_A */
    case 0x10: v = V_REF - ip * G_TARJETA / 1000.0 * (VCC_A / 3.7); break;
    case 0x11: v = V_REF + qp * G_TARJETA / 1000.0 * (VCC_A / 3.7); break;
    case 0x12: v = V_REF; break;
    case 0x13: v = VCC_A / DIVISOR_VCCA; break;
    case 0x3E: v = 1.024; break;
    case 0x3B: v = 0.0; break;
    default: fallo("canal del ADC desconocido"); v = 0.0;
    }
    resultado_adc = convertir(v);
    PIR1bits.ADIF = 0;                      /* lo hace ADCC_ISR() */
    manejador_adc();
    n_disparo++;

    if (n_disparo % DISPAROS_POR_TRAMA == 0) {
        trama = n_disparo / DISPAROS_POR_TRAMA;
        for (k = 0; k < sizeof guion / sizeof guion[0]; k++) {
            if ((long)(guion[k].t * 1000 / PERIODO_MS + 0.5) == trama) {
                size_t l = strlen(guion[k].orden);
                memcpy(rx + rx_n, guion[k].orden, l);
                rx_n += (int)l;
            }
        }
    }
}

void sim_gie(int habilitar)
{
    static int parado_3s, parado_6s;
    long disparos = DISPAROS_POR_VUELTA, d;
    FILE *f;

    INTCONbits.GIE = (unsigned)habilitar;
    if (habilitar || manejador_adc == NULL) {
        return;
    }
    /* La escritura de EEPROM en curso termina antes de la vuelta siguiente */
    if (NVMCON1bits.WR) {
        if (!NVMCON1bits.WREN || !NVMCON1bits.NVMREGS || NVMCON2 != 0xAA) {
            fallo("secuencia de escritura de la EEPROM incorrecta");
        }
        eeprom[((NVMADRH << 8) | NVMADRL) & 0xFF] = NVMDATL;
        NVMCON1bits.WR = 0;
    }
    if (n_disparo >= max_disparos) {
        f = fopen(archivo_eeprom, "wb");
        if (f != NULL) {
            fwrite(eeprom, 1, sizeof eeprom, f);
            fclose(f);
        }
        exit(0);
    }
    /* Lazo lento: el muestreo sigue mientras main esta parado */
    if (!parado_3s && n_disparo >= 30000) {
        parado_3s = 1;
        disparos += 3 * DISPAROS_POR_TRAMA;
    }
    if (!parado_6s && n_disparo >= 65000) {
        parado_6s = 1;
        disparos += 15 * DISPAROS_POR_TRAMA;
    }
    for (d = 0; d < disparos; d++) {
        un_disparo();
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
    max_disparos = (long)(atof(argv[1]) * 1e6 / PERIODO_DISPARO_US);
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
