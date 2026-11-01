/*
 * adquisicion.c - Conversiones del ADC disparadas por el PWM6.
 *
 * El ADC trabaja en modo basico (MD = 000, seccion 32.2.7 de la hoja de
 * datos) y cada conversion la arranca el flanco de subida del PWM6_out
 * (ADACT, disparo de autoconversion, seccion 32.2.6), sin intervencion del
 * software. El PWM6 usa el TMR2 (FOSC/4, prescaler 1:4, T2PR = 199):
 * un flanco cada 100 us. Con el reloj del ADC derivado de FOSC (ADCS = 0,
 * FOSC/32) el condensador de muestreo se desconecta un numero fijo de ciclos
 * de FOSC despues del flanco, asi que el jitter del instante de muestreo es
 * el del oscilador: no le afectan las interrupciones ni el lazo principal.
 * Con FRC (ADCS = 1), en cambio, el inicio no estaria sincronizado con FOSC.
 *
 * Los disparos se agrupan en rondas de 1 ms (10 disparos): I_OUT, Q_OUT,
 * V_REF, VCC_A, FVR y AVSS ocupan las posiciones 0 a 5 y las 4 restantes
 * son disparos libres (convierten AVSS y se descartan). Cada canal ocupa
 * siempre la misma posicion, asi que se muestrea exactamente cada 1 ms:
 * 1000 muestras/s por entrada, con el periodo fijado por el TMR2.
 * Una trama de 10 ms son 10 rondas. Al terminar cada conversion, la
 * interrupcion suma el resultado en su canal y elige el canal de la
 * siguiente, que tiene ~85 us para asentarse antes de su disparo. Al
 * terminar la trama, las sumas pasan a una cola que vacia main(): si main
 * se retrasa, las muestras se siguen tomando a su hora.
 *
 * La captura de la orden 'm' copia las conversiones de I_OUT de las rondas
 * (una por ms) sin parar las tramas: son las mismas muestras que se procesan.
 */
#include "../mcc_generated_files/mcc.h"
#include "adquisicion.h"

/* Canales del ADC (tabla 32-4 de la hoja de datos) */
#define CANAL_I_OUT     0x10u   /* RC0 / ANC0 */
#define CANAL_Q_OUT     0x11u   /* RC1 / ANC1 */
#define CANAL_V_REF     0x12u   /* RC2 / ANC2 */
#define CANAL_VCC_A     0x13u   /* RC3 / ANC3 */
#define CANAL_AVSS      0x3Bu   /* 0 V interno: autocero */
#define CANAL_FVR       0x3Eu   /* buffer 1 del FVR */

#define ADACT_PWM6_OUT  0x0Eu   /* tabla 32-6 */
#define ADMD_BASICO     0u
#define PPS_PWM6OUT     0x0Du   /* tabla 16-3 */

#define N_CANALES               6u

typedef char adq_ronda_cabe[(N_CANALES <= DISPAROS_POR_RONDA) ? 1 : -1];
typedef char adq_ronda_1ms[(PERIODO_MUESTREO_US == 1000u) ? 1 : -1];
typedef char adq_trama_10ms[(DISPAROS_POR_TRAMA * PERIODO_DISPARO_US == PERIODO_MS * 1000u) ? 1 : -1];
typedef char adq_suma_16_bits[((uint32_t)N_SOBREMUESTREO * ADC_CUENTAS <= 0xFFFFu) ? 1 : -1];

/* Orden de las conversiones en cada ronda; la posicion es el indice de suma[] */
enum { IDX_I, IDX_Q, IDX_R, IDX_V, IDX_F, IDX_Z };
static const uint8_t secuencia[N_CANALES] = {
    CANAL_I_OUT, CANAL_Q_OUT, CANAL_V_REF, CANAL_VCC_A, CANAL_FVR, CANAL_AVSS
};

typedef struct {
    uint16_t suma[N_CANALES];
    uint32_t numero;
} trama_t;

/* Estado de la interrupcion */
static volatile uint16_t suma[N_CANALES];
static volatile uint8_t  ranura;        /* posicion en la ronda de la conversion que termina */
static volatile uint8_t  ronda;         /* ronda de la trama (0 .. N_SOBREMUESTREO-1)        */
static volatile uint32_t num_trama;

/* Cola de tramas terminadas (escribe la interrupcion, lee main) */
static volatile trama_t  cola[N_COLA_TRAMAS];
static volatile uint8_t  cola_escr, cola_lect, cola_n;
static volatile uint32_t perdidas;

/* Captura de la orden 'm' */
enum { CAP_NADA, CAP_PEDIDA, CAP_EN_CURSO, CAP_LISTA };
static volatile uint8_t  cap_estado;
static uint16_t * volatile cap_buf;
static volatile uint16_t cap_n, cap_i;

static void fin_de_trama(void)
{
    uint8_t k;

    num_trama++;
    if (cola_n < N_COLA_TRAMAS) {
        for (k = 0u; k < N_CANALES; k++) {
            cola[cola_escr].suma[k] = suma[k];
        }
        cola[cola_escr].numero = num_trama;
        if (++cola_escr >= N_COLA_TRAMAS) {
            cola_escr = 0u;
        }
        cola_n++;
    } else {
        perdidas++;
    }
    for (k = 0u; k < N_CANALES; k++) {
        suma[k] = 0u;
    }
    if (cap_estado == CAP_PEDIDA) {     /* la captura empieza con la trama */
        cap_i = 0u;
        cap_estado = CAP_EN_CURSO;
    }
}

/* Contexto de interrupcion: ADIF, al terminar cada conversion */
static void al_terminar_conversion(void)
{
    uint16_t resultado = ADCC_GetConversionResult();

    if (ranura < N_CANALES) {
        suma[ranura] += resultado;
        if ((ranura == IDX_I) && (cap_estado == CAP_EN_CURSO)) {
            cap_buf[cap_i] = resultado;
            if (++cap_i >= cap_n) {
                cap_estado = CAP_LISTA;
            }
        }
    }                                   /* disparo libre: se descarta */

    if (++ranura >= DISPAROS_POR_RONDA) {
        ranura = 0u;
        if (++ronda >= N_SOBREMUESTREO) {
            ronda = 0u;
            fin_de_trama();
        }
    }

    /* Canal de la proxima conversion */
    ADPCH = (ranura < N_CANALES) ? secuencia[ranura] : CANAL_AVSS;
}

void adq_inicializar(void)
{
    uint8_t k;
    uint16_t espera = 0u;

    /* MCC ya deja el ADCC asi (docs/configuracion_MCC_Classic.md); se fija
     * aqui tambien porque la secuencia de canales depende de ello. */
    ADCON0bits.ADCONT = 0;              /* una conversion por disparo        */
    ADCON0bits.ADCS = 0;                /* reloj del ADC sincrono con FOSC   */
    ADCON2bits.ADMD = ADMD_BASICO;
    ADACT = ADACT_PWM6_OUT;

    for (k = 0u; k < N_CANALES; k++) {
        suma[k] = 0u;
    }
    ranura = 0u;
    ronda = 0u;
    ADPCH = secuencia[0];
    ADCC_SetADIInterruptHandler(al_terminar_conversion);
    PIR1bits.ADIF = 0;
    PIE1bits.ADIE = 1;

#if SALIDA_DISPARO && !defined(__DEBUG)
    /* El PWM6 por RA1 (ICSPCLK): cada flanco de subida es el disparo de una
     * conversion. Midiendo su jitter con un osciloscopio se mide el del
     * muestreo. Sin limitar la pendiente: un flanco lento convierte el
     * ruido de tension en jitter aparente en el osciloscopio. */
    RA1PPS = PPS_PWM6OUT;
    ANSELAbits.ANSA1 = 0;
    SLRCONAbits.SLRA1 = 0;
    TRISAbits.TRISA1 = 0;
#endif

    while (!FVR_IsOutputReady() && (++espera < 20000u)) {
    }
}

bool adq_leer(iq_codigos_t *c, uint32_t *numero)
{
    bool hay;

    INTERRUPT_GlobalInterruptDisable();
    hay = (cola_n != 0u);
    if (hay) {
        c->cI = cola[cola_lect].suma[IDX_I];
        c->cQ = cola[cola_lect].suma[IDX_Q];
        c->cR = cola[cola_lect].suma[IDX_R];
        c->cV = cola[cola_lect].suma[IDX_V];
#if USAR_FVR
        c->cF = cola[cola_lect].suma[IDX_F];
#else
        c->cF = 0u;
#endif
        c->cZ = cola[cola_lect].suma[IDX_Z];
        *numero = cola[cola_lect].numero;
        if (++cola_lect >= N_COLA_TRAMAS) {
            cola_lect = 0u;
        }
        cola_n--;
    }
    INTERRUPT_GlobalInterruptEnable();
    return hay;
}

uint32_t adq_tramas_perdidas(void)
{
    uint32_t n;

    INTERRUPT_GlobalInterruptDisable();
    n = perdidas;
    INTERRUPT_GlobalInterruptEnable();
    return n;
}

bool adq_pedir_captura(uint16_t *buffer, uint16_t n)
{
    if (cap_estado != CAP_NADA) {
        return false;
    }
    cap_buf = buffer;
    cap_n = n;
    cap_i = 0u;
    cap_estado = CAP_PEDIDA;            /* la interrupcion la arranca en la trama siguiente */
    return true;
}

bool adq_captura_lista(void)
{
    return (cap_estado == CAP_LISTA);
}

void adq_liberar_captura(void)
{
    if (cap_estado == CAP_LISTA) {
        cap_estado = CAP_NADA;
    }
}

bool adq_capturando(void)
{
    return (cap_estado != CAP_NADA);
}
