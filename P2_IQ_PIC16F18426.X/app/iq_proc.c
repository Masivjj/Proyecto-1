/*
 * iq_proc.c - Procesamiento I/Q (ver iq_proc.h).
 *
 * Formulas, constantes y orden de las operaciones de la especificacion
 * (secciones 4 y 10). Diferencias con LabVIEW, todas de un bloque (0.1 s)
 * como mucho:
 *  - El cero y theta capturados en un bloque se aplican a las MUESTRAS desde
 *    el bloque siguiente (diferencia aceptada en 10.6). Los INDICADORES del
 *    bloque (|V|, Fase, d) si usan el cero y theta de ese mismo bloque, como
 *    el VI: la resta del cero y la rotacion son lineales, asi que la media de
 *    I', Q' sale exacta de las medias de Ip, Qp.
 *  - La decision de P1 (VCC_A > 2.5 V) se toma con la media del bloque
 *    anterior; en el primer bloque, con la primera muestra.
 *  - La normalizacion ratiometrica usa el VCC_A de cada muestra (10.4).
 *
 * Las etapas cero, theta, ganancia y normalizacion se pueden desactivar en
 * marcha (iq_fijar_etapas). Desactivar una etapa no borra lo que tenia
 * guardado (I0, Q0, theta, G): al volver a activarla se aplica de nuevo.
 */
#include <math.h>
#include "iq_proc.h"

/* El calculo necesita float de 32 bits. En MPLAB X: Project Properties >
 * XC8 Global Options > "Size of float" = 32 bits (y "Size of double" = 32). */
typedef char iq_float_de_32_bits[(sizeof(float) == 4u) ? 1 : -1];

#define RAD_A_GRADOS    57.29578f       /* 180 / pi */
#define GRADOS_A_RAD    0.01745329f     /* pi / 180 */

/* Limites sobre las sumas de N_SOBREMUESTREO conversiones */
#define FONDO_SUMA      ((uint32_t)N_SOBREMUESTREO * ADC_CUENTAS)
#define CV_SATURADO     ((uint16_t)(N_SOBREMUESTREO * (ADC_CUENTAS - 1u))) /* VCC_A >= VDD */
#define CV_MINIMO       (FONDO_SUMA / 8u)   /* por debajo de VDD/8 no se divide por VCC_A */
/* Lectura plausible del FVR: 1.024 V +-4 % con VDD entre 2.3 y 5.5 V */
#define CF_MINIMO       ((uint16_t)(N_SOBREMUESTREO * 600u))
#define CF_MAXIMO       ((uint16_t)(N_SOBREMUESTREO * 2100u))

/* ---- Controles ---- */
static uint8_t ganancia;            /* G: divisor de la etapa DC          */
static uint8_t etapas;              /* IQ_ETAPA_x activas                 */

/* ---- Estado persistente (shift registers del VI) ---- */
static float   I0, Q0;              /* cero (mV)                          */
static float   theta_deg;           /* angulo de calibracion (grados)     */
static float   c_th, s_th;          /* cos y sin de theta: solo cambian con theta */

/* ---- P1 ---- */
static bool    vcc_valida;          /* decision vigente: VCC_A > 2.5 V    */
static bool    primera_muestra;

/* ---- Acumuladores del bloque ---- */
static float    suma_Ip, suma_Qp;
static uint32_t suma_cV, suma_cF;   /* para la media de VCC_A en voltios  */
static uint8_t  n_blk;
static uint8_t  banderas_blk;
static uint8_t  eventos_pend;

static void fijar_theta(float grados)
{
    float r;

    theta_deg = grados;
    if (grados == 0.0f) {
        c_th = 1.0f;
        s_th = 0.0f;
    } else {
        r = grados * GRADOS_A_RAD;
        c_th = cosf(r);
        s_th = sinf(r);
    }
}

static void reiniciar_bloque(void)
{
    suma_Ip = 0.0f;
    suma_Qp = 0.0f;
    suma_cV = 0u;
    suma_cF = 0u;
    n_blk = 0u;
    banderas_blk = 0u;
}

/* P6 y P8 con las etapas activas: (Ip, Qp) -> (I', Q') */
static void quitar_cero_y_rotar(float ip, float qp, float *i1, float *q1)
{
    if ((etapas & IQ_ETAPA_CERO) != 0u) {
        ip -= I0;
        qp -= Q0;
    }
    if ((etapas & IQ_ETAPA_THETA) != 0u) {
        *i1 = ip * c_th + qp * s_th;
        *q1 = qp * c_th - ip * s_th;
    } else {
        *i1 = ip;
        *q1 = qp;
    }
}

void iq_inicializar(void)
{
    ganancia = GANANCIA_INICIAL;
    iq_fijar_etapas(ETAPAS_INICIALES);
    I0 = 0.0f;
    Q0 = 0.0f;
    fijar_theta(0.0f);
    vcc_valida = true;
    primera_muestra = true;
    eventos_pend = 0u;
    reiniciar_bloque();
}

bool iq_fijar_ganancia(uint8_t g)
{
    if ((g != 2u) && (g != 11u) && (g != 101u)) {
        return false;
    }
    ganancia = g;
    return true;
}

uint8_t iq_ganancia(void)
{
    return ganancia;
}

uint8_t iq_fijar_etapas(uint8_t activas)
{
    etapas = (uint8_t)(activas & IQ_ETAPAS_TODAS);
#if !USAR_FVR
    /* Sin el FVR no se conocen los voltios absolutos: la normalizacion es
     * la unica escala posible y no se puede desactivar. */
    etapas |= IQ_ETAPA_NORMALIZACION;
#endif
    return etapas;
}

uint8_t iq_etapas(void)
{
    return etapas;
}

void iq_fijar_calibracion(float i0, float q0, float theta_grados)
{
    I0 = i0;
    Q0 = q0;
    fijar_theta(theta_grados);
}

void iq_leer_calibracion(float *i0, float *q0, float *theta_grados)
{
    *i0 = I0;
    *q0 = Q0;
    *theta_grados = theta_deg;
}

void iq_evento(uint8_t eventos)
{
    eventos_pend |= eventos;
}

/* Salida saturada si cx < 0.015 cv o cx > 0.985 cv (seccion 10.4).
 * 0.015 = 3/200 y 0.985 = 197/200, en enteros. */
static bool saturada(uint16_t cx, uint32_t cv)
{
    uint32_t x = (uint32_t)cx * 200u;

    return (x < cv * 3u) || (x > cv * 197u);
}

/* P1-P3: de los codigos del ADC a las senales procesadas (mV). */
static void escalar(const iq_codigos_t *c, iq_muestra_t *m)
{
    uint32_t cv = (uint32_t)c->cV * DIVISOR_VCCA;  /* VCC_A en cuentas de I_OUT */
    bool fvr_ok = (c->cF >= CF_MINIMO) && (c->cF <= CF_MAXIMO);
    bool normalizar = ((etapas & IQ_ETAPA_NORMALIZACION) != 0u);
    float divisor = ((etapas & IQ_ETAPA_GANANCIA) != 0u) ? (float)ganancia : 1.0f;
    float k;

    if (saturada(c->cI, cv)) {
        banderas_blk |= IQ_FL_SAT_I;
    }
    if (saturada(c->cQ, cv)) {
        banderas_blk |= IQ_FL_SAT_Q;
    }
    if (c->cV >= CV_SATURADO) {
        banderas_blk |= IQ_FL_SAT_VCC;
    }

    /* P1: VCC_A [V] = 1.024 cV / cF (VDD se cancela) */
    if (fvr_ok) {
        suma_cV += cv;
        suma_cF += c->cF;
        if (primera_muestra) {
            vcc_valida = (FVR_V * (float)cv > VCC_UMBRAL_V * (float)c->cF);
        }
    } else {
#if USAR_FVR
        banderas_blk |= IQ_FL_SIN_FVR;
#endif
    }
    primera_muestra = false;

    /* P2: k en mV por cuenta de (cX - cR) */
    if (normalizar && vcc_valida && (cv >= CV_MINIMO)) {
        /* Ratiometrica (10.4): Ip = (cI - cR) / cV * 3.7 V * 1000 / G.
         * Es lo mismo que (3.7 / VCC_A) / G * 1000 aplicado a los voltios
         * crudos: VDD se cancela. */
        k = (VCC_NOM_V * 1000.0f) / ((float)cv * divisor);
    } else {
        if (normalizar && vcc_valida) {
            /* VCC_A se ha caido dentro del bloque. Si no, el LED lo decide la
             * media del bloque (cerrar_bloque), igual que en el VI. */
            banderas_blk |= IQ_FL_VCC_FUERA;
        }
        if (fvr_ok) {
            /* Sin normalizar (etapa desactivada o VCC_A fuera de rango, donde
             * LabVIEW usa Vused = 3.7 V): k = 1000 / G sobre los voltios
             * crudos, que valen (cX - cR) * 1.024 / cF. */
            k = (FVR_V * 1000.0f) / ((float)c->cF * divisor);
        } else {
            k = 0.0f;   /* sin VCC_A ni FVR no hay escala posible */
        }
    }

    /* P3. La tarjeta fabricada entrega I con el signo invertido (pines 12/13
     * del 74HC4053 cruzados, seccion 1.3): se corrige siempre con kI = -k.
     * Si se arregla la tarjeta, quitar el signo menos de la linea de Ip. */
    m->Ip = -k * (float)((int32_t)c->cI - (int32_t)c->cR);
    m->Qp = k * (float)((int32_t)c->cQ - (int32_t)c->cR);
}

/* Cierre del bloque: P1 (media de VCC_A), P4, P5, P7 y P10. */
static void cerrar_bloque(iq_bloque_t *b)
{
    float mI, mQ, mIc, mQc, mI1, mQ1;
    uint8_t ev = eventos_pend;

    eventos_pend = 0u;

    /* Las ordenes de una etapa desactivada no hacen nada */
    if ((etapas & IQ_ETAPA_CERO) == 0u) {
        ev &= (uint8_t)~(IQ_EV_FIJAR_CERO | IQ_EV_BORRAR_CERO);
    }
    if ((etapas & IQ_ETAPA_THETA) == 0u) {
        ev &= (uint8_t)~(IQ_EV_CALIBRAR | IQ_EV_THETA_0);
    }

    /* P4: medias de las senales procesadas */
    mI = suma_Ip / (float)N_BLOQUE;
    mQ = suma_Qp / (float)N_BLOQUE;

    /* P1: VCC_A medio; decide la escala del bloque siguiente */
    if (suma_cF != 0u) {
        b->vcc = FVR_V * (float)suma_cV / (float)suma_cF;
        vcc_valida = (b->vcc > VCC_UMBRAL_V);
        if (!vcc_valida) {
            banderas_blk |= IQ_FL_VCC_FUERA;
        }
    } else {
        b->vcc = 0.0f;          /* desconocido: se sigue con la ratiometrica */
        vcc_valida = true;
    }

    /* P5: cero. "Borrar 0" manda sobre "Fijar cero", como en el VI. */
    if ((ev & IQ_EV_FIJAR_CERO) != 0u) {
        I0 = mI;
        Q0 = mQ;
    }
    if ((ev & IQ_EV_BORRAR_CERO) != 0u) {
        I0 = 0.0f;
        Q0 = 0.0f;
    }

    /* P7: theta = atan2(mQc, mIc) en grados, con el cero ya actualizado
     * (si la etapa de cero esta desactivada, sin restarlo). "theta = 0" manda. */
    if ((ev & IQ_EV_CALIBRAR) != 0u) {
        mIc = mI;
        mQc = mQ;
        if ((etapas & IQ_ETAPA_CERO) != 0u) {
            mIc -= I0;
            mQc -= Q0;
        }
        fijar_theta(atan2f(mQc, mIc) * RAD_A_GRADOS);
    }
    if ((ev & IQ_EV_THETA_0) != 0u) {
        fijar_theta(0.0f);
    }

    /* P6, P8 y P10: medias de I', Q' con el cero y theta de este bloque */
    quitar_cero_y_rotar(mI, mQ, &mI1, &mQ1);
    b->V = sqrtf(mI1 * mI1 + mQ1 * mQ1);
    b->fase = atan2f(mQ1, mI1) * RAD_A_GRADOS;
    b->d = b->V / MV_POR_1E4;
    b->I0 = I0;
    b->Q0 = Q0;
    b->theta = theta_deg;
    b->banderas = banderas_blk;
    b->eventos = ev;
    b->etapas = etapas;

    reiniciar_bloque();
}

bool iq_procesar(const iq_codigos_t *c, iq_muestra_t *m, iq_bloque_t *b)
{
    escalar(c, m);                              /* P1-P3 */
    quitar_cero_y_rotar(m->Ip, m->Qp, &m->I1, &m->Q1);  /* P6, P8 */

    suma_Ip += m->Ip;                           /* P4 */
    suma_Qp += m->Qp;
    if (++n_blk < N_BLOQUE) {
        return false;
    }
    cerrar_bloque(b);
    return true;
}
