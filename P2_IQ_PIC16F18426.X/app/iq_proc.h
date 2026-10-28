/*
 * iq_proc.h - Procesamiento I/Q del VI P2_IQ_SINAD.vi portado al PIC16F18426.
 *
 * Reproduce, en el mismo orden, las etapas de la especificacion (seccion 4):
 *   P1  VCC_A y proteccion de 2.5 V
 *   P2  factor de escala k (ratiometrico) y signo de I (corregido siempre)
 *   P3  senales procesadas Ip, Qp (mV a la salida del mezclador, a 3.7 V)
 *   P4  medias del bloque
 *   P5  cero I0, Q0 ("Fijar cero" / "Borrar 0")
 *   P6  senales sin cero Ic, Qc
 *   P7  angulo de calibracion theta ("Calibrar theta" / "theta = 0")
 *   P8  rotacion por -theta: senales finales I', Q'
 *   P10 indicadores del bloque: |V|, Fase y d estimada
 * La traza del plano polar (P9) esta en traza.c.
 *
 * Las etapas de cero, theta, ganancia, normalizacion a VCC_A y autocero
 * del ADC se activan y desactivan en marcha con iq_fijar_etapas() (ordenes
 * Z, T, G, N y A por UART).
 *
 * El modulo no toca el hardware: compila igual con XC8 y con gcc, y se
 * comprueba en el PC con los vectores de prueba de la seccion 9
 * (tests/test_iq_proc.c).
 */
#ifndef IQ_PROC_H
#define IQ_PROC_H

#include <stdbool.h>
#include <stdint.h>
#include "app_config.h"

/* Codigos sobremuestreados de una muestra. Cada uno es la SUMA de
 * N_SOBREMUESTREO conversiones de 12 bits (0..65520), todas contra VDD. */
typedef struct {
    uint16_t cI;        /* I_OUT  (J805.1) */
    uint16_t cQ;        /* Q_OUT  (J805.3) */
    uint16_t cR;        /* V_REF  (J805.2) */
    uint16_t cV;        /* VCC_A  (J805.5) */
    uint16_t cF;        /* FVR de 1.024 V (canal interno); 0 si no se mide */
    uint16_t cZ;        /* AVSS (0 V, canal interno): offset del ADC para el autocero */
} iq_codigos_t;

/* Resultado de cada muestra (100 por segundo) */
typedef struct {
    float Ip, Qp;       /* senales procesadas, mV  (grafica "I y Q procesadas")  */
    float I1, Q1;       /* senales finales I', Q', mV (grafica "I y Q" y polar)  */
} iq_muestra_t;

/* Resultado de cada bloque (N_BLOQUE muestras = 0.1 s) */
typedef struct {
    float   V;          /* |V| (mV)                                              */
    float   fase;       /* Fase (grados), medida desde el eje del lift-off       */
    float   d;          /* d estimada (x 1e-4)                                   */
    float   I0, Q0;     /* cero vigente (mV)                                     */
    float   theta;      /* theta vigente (grados)                                */
    float   vcc;        /* VCC_A medio del bloque (V); 0 si no se pudo medir     */
    uint8_t banderas;   /* IQ_FL_x                                               */
    uint8_t eventos;    /* IQ_EV_x aplicados al cerrar este bloque               */
    uint8_t etapas;     /* IQ_ETAPA_x activas al cerrar este bloque              */
} iq_bloque_t;

/* Etapas que se pueden desactivar. Desactivada, la etapa no actua sobre la
 * senal y sus ordenes se ignoran, pero lo guardado (I0, Q0, theta, G) se
 * conserva y vuelve a aplicarse al activarla. */
#define IQ_ETAPA_CERO           0x01u   /* P5-P6: restar I0, Q0; Fijar cero / Borrar 0      */
#define IQ_ETAPA_THETA          0x02u   /* P7-P8: rotar por -theta; Calibrar theta / theta=0 */
#define IQ_ETAPA_GANANCIA       0x04u   /* P2: dividir por G (desactivada: G = 1)           */
#define IQ_ETAPA_NORMALIZACION  0x08u   /* P1-P2: normalizar a VCC_A = 3.7 V (desactivada:
                                         * voltios absolutos medidos con el FVR)            */
#define IQ_ETAPA_AUTOCERO       0x10u   /* restar a todos los canales el offset del ADC
                                         * medido en AVSS (cZ)                              */
#define IQ_ETAPAS_TODAS         0x1Fu

/* Eventos: equivalen a los botones "latch" del VI. Cada uno vale un bloque y
 * se aplica al cerrar el bloque en curso. */
#define IQ_EV_FIJAR_CERO    0x01u
#define IQ_EV_BORRAR_CERO   0x02u
#define IQ_EV_CALIBRAR      0x04u
#define IQ_EV_THETA_0       0x08u
#define IQ_EV_BORRAR_TRAZA  0x10u
#define IQ_EV_CALIBRACION   (IQ_EV_FIJAR_CERO | IQ_EV_BORRAR_CERO | IQ_EV_CALIBRAR | IQ_EV_THETA_0)

/* Banderas del bloque */
#define IQ_FL_VCC_FUERA     0x01u   /* VCC_A <= 2.5 V: se usa 3.7 V (LED del VI)        */
#define IQ_FL_SAT_I         0x02u   /* I_OUT a menos de ~50 mV de un riel de VCC_A      */
#define IQ_FL_SAT_Q         0x04u   /* Q_OUT a menos de ~50 mV de un riel de VCC_A      */
#define IQ_FL_SAT_VCC       0x08u   /* VCC_A >= VDD: la normalizacion no es fiable      */
#define IQ_FL_SIN_FVR       0x10u   /* sin lectura valida del FVR: VCC_A en V desconocido */
#define IQ_FL_RETRASO       0x20u   /* (main.c) se descartaron tramas: cola llena       */
#define IQ_FL_ALARMA        (IQ_FL_VCC_FUERA | IQ_FL_SAT_I | IQ_FL_SAT_Q | IQ_FL_SAT_VCC)

/* Pone el estado como al arrancar el VI: I0 = Q0 = 0, theta = 0,
 * G = GANANCIA_INICIAL y etapas = ETAPAS_INICIALES. */
void    iq_inicializar(void);

bool    iq_fijar_ganancia(uint8_t g);           /* 2, 11 o 101; false si no vale */
uint8_t iq_ganancia(void);

/* Fija las etapas activas (mascara IQ_ETAPA_x) y devuelve las que quedan
 * activas: sin FVR (USAR_FVR = 0) la normalizacion no se puede desactivar.
 * Para alternar una: iq_fijar_etapas(iq_etapas() ^ IQ_ETAPA_CERO). */
uint8_t iq_fijar_etapas(uint8_t activas);
uint8_t iq_etapas(void);

/* Offset del ADC (LSB) medido en AVSS en la ultima muestra. Un offset
 * negativo se lee como 0: el ADC no da codigos por debajo de 0. */
float   iq_offset_lsb(void);

void    iq_fijar_calibracion(float i0, float q0, float theta_grados);
void    iq_leer_calibracion(float *i0, float *q0, float *theta_grados);

void    iq_evento(uint8_t eventos);

/* Procesa una muestra. Devuelve true si con ella se cierra un bloque; en ese
 * caso *b recibe los indicadores del bloque. */
bool    iq_procesar(const iq_codigos_t *c, iq_muestra_t *m, iq_bloque_t *b);

#endif /* IQ_PROC_H */
