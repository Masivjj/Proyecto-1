/*
 * comunicacion.c - Protocolo por UART con el PC (ver comunicacion.h).
 *
 * Todo se envia como enteros (uV, centesimas de grado...) para no usar
 * printf("%f"). Ancho de banda: una linea de muestra (~45 caracteres) cada
 * 10 ms y una de bloque (~65) cada 100 ms ocupan la mitad de 115200 baudios.
 * EUSART1_Write() solo espera si el buffer de 128 bytes esta lleno; por eso
 * los textos largos (estado y volcado de la traza) salen a linea por tick.
 */
#include "../mcc_generated_files/mcc.h"
#include "comunicacion.h"
#include "adquisicion.h"
#include "ajustes.h"
#include "formato.h"
#include "traza.h"

#define UV_POR_MV       1000.0f     /* mV -> uV                      */
#define MV_POR_V        1000.0f     /* V -> mV                       */
#define CENTESIMAS      100.0f      /* grados -> centesimas de grado */
#define MILESIMAS       1000.0f     /* d (x1e-4) -> milesimas        */

#define ESTADO_LINEAS   8u
#define CODIGOS_POR_LINEA 10u       /* captura: codigos en cada linea M,...   */

static uint8_t estado_paso;         /* proxima linea del estado (0 = ninguna) */
static uint8_t volcado_total;       /* puntos a volcar (0 = sin volcado)      */
static uint8_t volcado_n;           /* puntos ya enviados                     */
static uint16_t *captura;           /* buffer de la captura en curso          */
static uint16_t captura_enviados;

/* ---- Salida ---- */

static void tx_car(char ch)
{
    EUSART1_Write((uint8_t)ch);
}

static void tx_texto(const char *s)
{
    while (*s != '\0') {
        tx_car(*s++);
    }
}

static void tx_buffer(const char *p, uint8_t n)
{
    while (n-- != 0u) {
        tx_car(*p++);
    }
}

static void tx_u32(uint32_t v)
{
    char buf[FMT_MAX_CARACTERES];

    tx_buffer(buf, fmt_u32(buf, v));
}

static void tx_i32(int32_t v)
{
    char buf[FMT_MAX_CARACTERES];

    tx_buffer(buf, fmt_i32(buf, v));
}

/* ",<valor redondeado de x * escala>" */
static void tx_campo(float x, float escala)
{
    tx_car(',');
    tx_i32(fmt_a_entero(x, escala));
}

static void tx_fin(void)
{
    tx_car('\r');
    tx_car('\n');
}

/* v / 100 con dos decimales (v >= 0) */
static void tx_centesimas(uint32_t v)
{
    tx_u32(v / 100u);
    tx_car('.');
    tx_car((char)('0' + (uint8_t)((v / 10u) % 10u)));
    tx_car((char)('0' + (uint8_t)(v % 10u)));
}

/* " nombre=ON" o " nombre=OFF" segun la etapa este activa */
static void tx_etapa(const char *nombre, uint8_t etapa)
{
    tx_car(' ');
    tx_texto(nombre);
    tx_texto(((iq_etapas() & etapa) != 0u) ? "=ON" : "=OFF");
}

static void tx_configuracion(void)
{
    float i0, q0, th;

    iq_leer_calibracion(&i0, &q0, &th);
    tx_texto("# G=");
    tx_u32(iq_ganancia());
    tx_etapa("cero", IQ_ETAPA_CERO);
    tx_etapa("theta", IQ_ETAPA_THETA);
    tx_etapa("ganancia", IQ_ETAPA_GANANCIA);
    tx_etapa("norm", IQ_ETAPA_NORMALIZACION);
    tx_etapa("autocero", IQ_ETAPA_AUTOCERO);
    tx_texto(" I0_uV=");
    tx_i32(fmt_a_entero(i0, UV_POR_MV));
    tx_texto(" Q0_uV=");
    tx_i32(fmt_a_entero(q0, UV_POR_MV));
    tx_texto(" theta_cdeg=");
    tx_i32(fmt_a_entero(th, CENTESIMAS));
    tx_fin();
}

/* Lineas del mensaje de estado y ayuda (paso = 1 .. ESTADO_LINEAS) */
static void tx_linea_estado(uint8_t paso, uint32_t muestras_perdidas)
{
    switch (paso) {
    case 1u:
        tx_texto("# P2 IQ - PIC16F18426 - firmware " VERSION_FIRMWARE "\r\n");
        break;
    case 2u:
        tx_configuracion();
        break;
    case 3u:
        tx_texto("# puntos en traza: ");
        tx_u32(traza_puntos());
        tx_texto("  muestras perdidas: ");
        tx_u32(muestras_perdidas);
        tx_texto("  offset ADC (AVSS): ");
        tx_centesimas((uint32_t)fmt_a_entero(iq_offset_lsb(), 100.0f));
        tx_texto(" LSB");
        tx_fin();
        break;
    case 4u:
        tx_texto("# muestra: t_ms,Ip_uV,Qp_uV,I1_uV,Q1_uV\r\n");
        break;
    case 5u:
        tx_texto("# bloque:  B,V_uV,fase_cdeg,d_milesimas,I0_uV,Q0_uV,theta_cdeg,vcc_mV,G,banderas,etapas\r\n");
        break;
    case 6u:
        tx_texto("# ordenes: z=fijar cero x=borrar cero c=calibrar theta t=theta 0 b=borrar traza\r\n");
        break;
    case 7u:
        tx_texto("#          2/1/0=ganancia x2/x11/x101 p=volcar traza m=captura (jitter) ?=estado\r\n");
        break;
    default:
        tx_texto("# etapas (activar/desactivar): Z=cero T=theta G=ganancia N=normalizacion A=autocero\r\n");
        break;
    }
}

void com_bienvenida(bool ajustes_cargados)
{
    uint8_t paso;

    tx_texto(ajustes_cargados ? "# EEPROM: ajustes cargados\r\n"
                              : "# EEPROM: sin datos, valores por defecto\r\n");
    for (paso = 1u; paso <= ESTADO_LINEAS; paso++) {
        tx_linea_estado(paso, 0u);
    }
}

void com_muestra(uint32_t t_ms, const iq_muestra_t *m)
{
    tx_u32(t_ms);
    tx_campo(m->Ip, UV_POR_MV);
    tx_campo(m->Qp, UV_POR_MV);
    tx_campo(m->I1, UV_POR_MV);
    tx_campo(m->Q1, UV_POR_MV);
    tx_fin();
}

void com_bloque(const iq_bloque_t *b)
{
    tx_car('B');
    tx_campo(b->V, UV_POR_MV);
    tx_campo(b->fase, CENTESIMAS);
    tx_campo(b->d, MILESIMAS);
    tx_campo(b->I0, UV_POR_MV);
    tx_campo(b->Q0, UV_POR_MV);
    tx_campo(b->theta, CENTESIMAS);
    tx_campo(b->vcc, MV_POR_V);
    tx_car(',');
    tx_u32(iq_ganancia());
    tx_car(',');
    tx_u32(b->banderas);
    tx_car(',');
    tx_u32(b->etapas);
    tx_fin();
}

/* Final de la respuesta a una orden de la etapa indicada */
static void tx_fin_orden(uint8_t etapa)
{
    if ((iq_etapas() & etapa) == 0u) {
        tx_texto(" -> ignorada (etapa desactivada)");
    }
    tx_fin();
}

void com_eventos_boton(uint8_t eventos)
{
    if ((eventos & IQ_EV_FIJAR_CERO) != 0u) {
        tx_texto("# boton: fijar cero");
        tx_fin_orden(IQ_ETAPA_CERO);
    }
    if ((eventos & IQ_EV_CALIBRAR) != 0u) {
        tx_texto("# boton: calibrar theta");
        tx_fin_orden(IQ_ETAPA_THETA);
    }
}

/* ---- Ordenes ---- */

/* Orden de una etapa ("Fijar cero", "Calibrar theta"...). Si la etapa esta
 * desactivada, iq_proc la ignora al cerrar el bloque. */
static void orden_de_etapa(uint8_t etapa, uint8_t evento, const char *texto)
{
    iq_evento(evento);
    tx_texto(texto);
    tx_fin_orden(etapa);
}

static void alternar_etapa(uint8_t etapa)
{
    uint8_t antes = iq_etapas();

    if ((iq_fijar_etapas((uint8_t)(antes ^ etapa)) & etapa) == (antes & etapa)) {
        tx_texto("# sin FVR (USAR_FVR = 0) la normalizacion no se puede desactivar\r\n");
    }
    ajustes_guardar();
    tx_configuracion();
}

static void cambiar_ganancia(uint8_t g)
{
    (void)iq_fijar_ganancia(g);
    ajustes_guardar();
    tx_configuracion();
}

static void iniciar_volcado(void)
{
    volcado_total = traza_puntos();
    volcado_n = 0u;
    tx_texto("# traza: ");
    tx_u32(volcado_total);
    tx_texto(" puntos P,I1_uV,Q1_uV del mas antiguo al mas reciente\r\n");
    if (volcado_total == 0u) {
        tx_texto("# fin traza\r\n");
    }
}

static void volcar_punto(void)
{
    float i1, q1;

    if (traza_leer(volcado_n, &i1, &q1)) {
        tx_car('P');
        tx_campo(i1, UV_POR_MV);
        tx_campo(q1, UV_POR_MV);
        tx_fin();
        volcado_n++;
    } else {
        volcado_n = volcado_total;      /* se borro la traza durante el volcado */
    }
    if (volcado_n >= volcado_total) {
        volcado_total = 0u;
        tx_texto("# fin traza\r\n");
    }
}

/* Orden 'm': captura N_CAPTURA conversiones seguidas de I_OUT (una por
 * disparo del PWM6) para medir el jitter con pc/medir_jitter.py */
static void iniciar_captura(void)
{
    if (adq_capturando() || (volcado_total != 0u)) {
        tx_texto("# m: espere a que termine la captura o el volcado en curso\r\n");
        return;
    }
    captura = traza_prestar_memoria();      /* vacia la traza */
    captura_enviados = 0u;
    (void)adq_pedir_captura(captura, N_CAPTURA);
    tx_texto("# m: captura de ");
    tx_u32(N_CAPTURA);
    tx_texto(" conversiones de I_OUT, una cada ");
    tx_u32(PERIODO_DISPARO_US);
    tx_texto(" us\r\n");
}

/* Envia la captura terminada, CODIGOS_POR_LINEA codigos por llamada */
static void enviar_captura(void)
{
    uint8_t k;

    tx_car('M');
    for (k = 0u; (k < CODIGOS_POR_LINEA) && (captura_enviados < N_CAPTURA); k++) {
        tx_car(',');
        tx_u32(captura[captura_enviados++]);
    }
    tx_fin();
    if (captura_enviados >= N_CAPTURA) {
        adq_liberar_captura();
        traza_borrar();
        tx_texto("# fin captura\r\n");
    }
}

static void atender_orden(char ch)
{
    switch (ch) {
    case 'z':
        orden_de_etapa(IQ_ETAPA_CERO, IQ_EV_FIJAR_CERO, "# z: fijar cero");
        break;
    case 'x':
        orden_de_etapa(IQ_ETAPA_CERO, IQ_EV_BORRAR_CERO, "# x: borrar cero");
        break;
    case 'c':
        orden_de_etapa(IQ_ETAPA_THETA, IQ_EV_CALIBRAR, "# c: calibrar theta");
        break;
    case 't':
        orden_de_etapa(IQ_ETAPA_THETA, IQ_EV_THETA_0, "# t: theta = 0");
        break;
    case 'b':
        iq_evento(IQ_EV_BORRAR_TRAZA);
        tx_texto("# b: borrar traza\r\n");
        break;
    case '2':
        cambiar_ganancia(2u);
        break;
    case '1':
        cambiar_ganancia(11u);
        break;
    case '0':
        cambiar_ganancia(101u);
        break;
    case 'Z':
        alternar_etapa(IQ_ETAPA_CERO);
        break;
    case 'T':
        alternar_etapa(IQ_ETAPA_THETA);
        break;
    case 'G':
        alternar_etapa(IQ_ETAPA_GANANCIA);
        break;
    case 'N':
        alternar_etapa(IQ_ETAPA_NORMALIZACION);
        break;
    case 'A':
        alternar_etapa(IQ_ETAPA_AUTOCERO);
        break;
    case 'p':
        if (adq_capturando()) {
            tx_texto("# p: espere a que termine la captura\r\n");
        } else if (volcado_total == 0u) {
            iniciar_volcado();
        }
        break;
    case 'm':
        iniciar_captura();
        break;
    case '?':
    case 'h':
        estado_paso = 1u;
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

void com_tarea(uint32_t muestras_perdidas)
{
    while (EUSART1_is_rx_ready()) {
        atender_orden((char)EUSART1_Read());
    }
    if (estado_paso != 0u) {
        tx_linea_estado(estado_paso, muestras_perdidas);
        estado_paso = (estado_paso < ESTADO_LINEAS) ? (uint8_t)(estado_paso + 1u) : 0u;
    } else if (volcado_total != 0u) {
        volcar_punto();
    } else if (adq_captura_lista()) {
        enviar_captura();
    }
}

bool com_volcando_traza(void)
{
    return (volcado_total != 0u);
}
