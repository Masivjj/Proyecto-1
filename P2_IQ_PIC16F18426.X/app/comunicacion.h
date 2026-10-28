/*
 * comunicacion.h - Protocolo por UART con el PC (opcion A, seccion 10.7).
 *
 * Lineas que envia el PIC (texto, terminadas en CR LF):
 *   t_ms,Ip_uV,Qp_uV,I1_uV,Q1_uV                       una por muestra (100/s)
 *   B,V_uV,fase_cdeg,d_milesimas,I0_uV,Q0_uV,theta_cdeg,vcc_mV,G,banderas,etapas
 *                                                      una por bloque (10/s)
 *   P,I1_uV,Q1_uV                                      volcado de la traza ('p')
 *   M,c1,c2,...,c10                                    captura ('m'): codigos de
 *                                                      12 bits de I_OUT
 *   # ...                                              comentarios y respuestas
 *
 * Ordenes de un caracter que recibe:
 *   z fijar cero        x borrar cero      c calibrar theta    t theta = 0
 *   b borrar traza      2 / 1 / 0 ganancia x2 / x11 / x101
 *   p volcar la traza   ? estado y ayuda
 *   m capturar N_CAPTURA conversiones seguidas de I_OUT (medida del jitter)
 *   Z / T / G / N / A activar o desactivar la etapa de cero / theta /
 *                 ganancia / normalizacion a VCC_A / autocero (las ordenes de
 *                 una etapa desactivada se contestan como ignoradas)
 */
#ifndef COMUNICACION_H
#define COMUNICACION_H

#include <stdbool.h>
#include <stdint.h>
#include "iq_proc.h"

void com_bienvenida(bool ajustes_cargados);
void com_muestra(uint32_t t_ms, const iq_muestra_t *m);
void com_bloque(const iq_bloque_t *b);
void com_eventos_boton(uint8_t eventos);

/* Llamar cada 10 ms: atiende las ordenes recibidas y envia el siguiente
 * punto del volcado de la traza, si lo hay. */
void com_tarea(uint32_t muestras_perdidas);

/* true mientras se vuelca la traza: entretanto no se le agregan puntos. */
bool com_volcando_traza(void);

#endif /* COMUNICACION_H */
