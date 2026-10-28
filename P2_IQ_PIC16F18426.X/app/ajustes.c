/*
 * ajustes.c - Guarda I0, Q0, theta, G y las etapas activas en la EEPROM
 * para conservarlos al apagar (seccion 10.8, opcional: USAR_EEPROM).
 *
 * Formato, 17 bytes desde 0xF000:
 *   0       'P' (marca)
 *   1       version del formato
 *   2..5    I0 (float, mV)
 *   6..9    Q0 (float, mV)
 *   10..13  theta (float, grados)
 *   14      G (2, 11 o 101)
 *   15      etapas activas (IQ_ETAPA_x: cero, theta, ganancia, normalizacion)
 *   16      complemento a 1 de la suma de los bytes 0..15
 *
 * La escritura no bloquea: cada byte tarda 4-5 ms, pero la CPU sigue
 * trabajando mientras tanto (hoja de datos, 6.4.3). ajustes_tarea() lanza
 * como mucho un byte por llamada, y solo los que han cambiado, asi que el
 * muestreo de 10 ms no se interrumpe. La suma de control se escribe la
 * ultima: si se corta la alimentacion a mitad, al arrancar se descartan
 * los datos y se usan los valores por defecto. Los datos de la version 1 del
 * formato (con la correccion de signo en el byte 15) tambien se descartan.
 */
#include <string.h>
#include "../mcc_generated_files/mcc.h"
#include "ajustes.h"
#include "iq_proc.h"

#define EE_BASE         0xF000u
#define EE_MARCA        0x50u       /* 'P' */
#define EE_VERSION      2u
#define EE_TAM          17u
#define EE_POS_SUMA     16u

#if USAR_EEPROM

static uint8_t imagen[EE_TAM];      /* lo que deberia contener la EEPROM */
static bool    pendiente;           /* hay bytes por escribir             */
static bool    escribiendo;         /* hay una escritura en curso         */

static uint8_t suma_control(const uint8_t *p, uint8_t n)
{
    uint8_t s = 0u;

    while (n-- != 0u) {
        s = (uint8_t)(s + *p++);
    }
    return (uint8_t)~s;
}

/* Descarta NaN, infinitos y valores absurdos */
static bool float_razonable(float x, float limite)
{
    return (x == x) && (x <= limite) && (x >= -limite);
}

/* Como DATAEE_WriteByte() de MCC, pero sin esperar a que termine */
static void iniciar_escritura(uint16_t dir, uint8_t dato)
{
    uint8_t gie = INTCONbits.GIE;

    NVMADRH = (uint8_t)(dir >> 8);
    NVMADRL = (uint8_t)dir;
    NVMDATL = dato;
    NVMCON1bits.NVMREGS = 1;
    NVMCON1bits.WREN = 1;
    INTCONbits.GIE = 0;         /* la secuencia de desbloqueo no puede interrumpirse */
    NVMCON2 = 0x55;
    NVMCON2 = 0xAA;
    NVMCON1bits.WR = 1;
    INTCONbits.GIE = gie;
    escribiendo = true;
}

bool ajustes_cargar(void)
{
    uint8_t i;
    float i0, q0, th;

    for (i = 0u; i < EE_TAM; i++) {
        imagen[i] = DATAEE_ReadByte((uint16_t)(EE_BASE + i));
    }
    if ((imagen[0] != EE_MARCA) || (imagen[1] != EE_VERSION) ||
        (imagen[EE_POS_SUMA] != suma_control(imagen, EE_POS_SUMA))) {
        return false;
    }
    memcpy(&i0, &imagen[2], sizeof(float));
    memcpy(&q0, &imagen[6], sizeof(float));
    memcpy(&th, &imagen[10], sizeof(float));
    if (!float_razonable(i0, 1.0e5f) || !float_razonable(q0, 1.0e5f) ||
        !float_razonable(th, 180.0f) || ((imagen[15] & (uint8_t)~IQ_ETAPAS_TODAS) != 0u)) {
        return false;
    }
    if (!iq_fijar_ganancia(imagen[14])) {
        return false;
    }
    (void)iq_fijar_etapas(imagen[15]);
    iq_fijar_calibracion(i0, q0, th);
    return true;
}

void ajustes_guardar(void)
{
    float i0, q0, th;

    iq_leer_calibracion(&i0, &q0, &th);
    imagen[0] = EE_MARCA;
    imagen[1] = EE_VERSION;
    memcpy(&imagen[2], &i0, sizeof(float));
    memcpy(&imagen[6], &q0, sizeof(float));
    memcpy(&imagen[10], &th, sizeof(float));
    imagen[14] = iq_ganancia();
    imagen[15] = iq_etapas();
    imagen[EE_POS_SUMA] = suma_control(imagen, EE_POS_SUMA);
    pendiente = true;
}

void ajustes_tarea(void)
{
    uint8_t i;

    if (NVMCON1bits.WR) {
        return;                     /* la escritura anterior no ha terminado */
    }
    if (escribiendo) {
        NVMCON1bits.WREN = 0;
        escribiendo = false;
    }
    if (!pendiente) {
        return;
    }
    for (i = 0u; i < EE_TAM; i++) {
        if (DATAEE_ReadByte((uint16_t)(EE_BASE + i)) != imagen[i]) {
            iniciar_escritura((uint16_t)(EE_BASE + i), imagen[i]);
            return;
        }
    }
    pendiente = false;              /* la EEPROM ya coincide con la imagen */
}

#else /* USAR_EEPROM == 0: siempre se arranca con los valores por defecto */

bool ajustes_cargar(void)
{
    return false;
}

void ajustes_guardar(void)
{
}

void ajustes_tarea(void)
{
}

#endif
