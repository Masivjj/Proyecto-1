/*
 * test_iq_proc.c - Comprueba en el PC el procesamiento del firmware con los
 * vectores de prueba de la seccion 9.1 de la especificacion.
 *
 * Compila exactamente los mismos iq_proc.c y formato.c que el PIC (ver
 * Makefile). Las entradas se dan en voltios, como en LabVIEW, y se
 * convierten en los codigos que leeria el ADC del PIC:
 *   I_OUT = V_REF + Iraw,  Q_OUT = V_REF + Qraw,  V_REF = 1.85 V
 * Con VDD = 4.095 V una cuenta vale 1 mV, asi que los vectores dan codigos
 * exactos; otra VDD comprueba que VDD se cancela.
 */
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "iq_proc.h"
#include "formato.h"

#define PI 3.14159265358979

static double vdd = 4.095;
static int fallos = 0;
static int total = 0;

static void comprobar(const char *nombre, double obtenido, double esperado, double tol)
{
    bool ok = fabs(obtenido - esperado) <= tol;

    total++;
    if (!ok) {
        fallos++;
    }
    printf("%s  %-58s %11.4f  (esperado %11.4f)\n", ok ? "OK   " : "FALLO", nombre, obtenido, esperado);
}

static void comprobar_cond(const char *nombre, bool condicion)
{
    total++;
    if (!condicion) {
        fallos++;
    }
    printf("%s  %s\n", condicion ? "OK   " : "FALLO", nombre);
}

/* Suma de N_SOBREMUESTREO conversiones de la tension v (ADC de 12 bits contra VDD) */
static uint16_t codigo(double v)
{
    double c = floor(v / vdd * (double)ADC_CUENTAS + 0.5);

    if (c < 0.0) {
        c = 0.0;
    }
    if (c > (double)ADC_CUENTAS) {
        c = (double)ADC_CUENTAS;
    }
    return (uint16_t)(c * N_SOBREMUESTREO);
}

static iq_codigos_t codigos(double iraw, double qraw, double vcc)
{
    const double vref = 1.85;
    iq_codigos_t c;

    c.cI = codigo(vref + iraw);
    c.cQ = codigo(vref + qraw);
    c.cR = codigo(vref);
    c.cV = codigo(vcc / DIVISOR_VCCA);
    c.cF = codigo(FVR_V);
    return c;
}

/* Procesa un bloque completo con la misma entrada. Los eventos se dan antes
 * de la primera muestra, como un boton pulsado durante el bloque. */
static void bloque(double iraw, double qraw, double vcc, uint8_t eventos,
                   iq_muestra_t *m, iq_bloque_t *b)
{
    iq_codigos_t c = codigos(iraw, qraw, vcc);
    unsigned i;
    bool cierre_ok = true;

    if (eventos != 0u) {
        iq_evento(eventos);
    }
    for (i = 0; i < N_BLOQUE; i++) {
        bool fin = iq_procesar(&c, m, b);
        if (fin != (i == N_BLOQUE - 1u)) {
            cierre_ok = false;
        }
    }
    if (!cierre_ok) {
        comprobar_cond("el bloque se cierra en la muestra N_BLOQUE", false);
    }
}

static void pruebas_1_a_3(void)
{
    iq_muestra_t m;
    iq_bloque_t b;

    printf("\n-- #1 a #3: escala y ganancia DC, entrada (1.000, -0.500) V, Vcc = 3.7 V\n");
    iq_inicializar();
    bloque(1.0, -0.5, 3.7, 0u, &m, &b);
    comprobar("#1 G=2:   Ip (mV)", m.Ip, -500.0, 0.001);
    comprobar("#1 G=2:   Qp (mV)", m.Qp, -250.0, 0.001);
    comprobar("#1 G=2:   I' = Ip (sin cero, theta = 0)", m.I1, -500.0, 0.001);
    comprobar("#1 G=2:   Q' = Qp", m.Q1, -250.0, 0.001);
    comprobar("#1 G=2:   VCC_A medido (V)", b.vcc, 3.7, 0.001);
    comprobar_cond("#1 G=2:   sin banderas", b.banderas == 0u);

    iq_fijar_ganancia(11u);
    bloque(1.0, -0.5, 3.7, 0u, &m, &b);
    comprobar("#2 G=11:  Ip (mV)", m.Ip, -1000.0 / 11.0, 0.001);
    comprobar("#2 G=11:  Qp (mV)", m.Qp, -500.0 / 11.0, 0.001);

    iq_fijar_ganancia(101u);
    bloque(1.0, -0.5, 3.7, 0u, &m, &b);
    comprobar("#3 G=101: Ip (mV)", m.Ip, -1000.0 / 101.0, 0.0001);
    comprobar("#3 G=101: Qp (mV)", m.Qp, -500.0 / 101.0, 0.0001);

    comprobar_cond("ganancia no valida (5) rechazada", !iq_fijar_ganancia(5u) && iq_ganancia() == 101u);
}

static void pruebas_4_a_8(void)
{
    iq_muestra_t m;
    iq_bloque_t b;
    const double r = 150.0 * sqrt(2.0);     /* 212.132 mV */

    printf("\n-- #4 a #8: cero, theta y rotacion (G = 2)\n");
    iq_inicializar();

    /* #4 */
    bloque(1.0, -0.5, 3.7, IQ_EV_FIJAR_CERO, &m, &b);
    comprobar("#4 Fijar cero: I0 (mV)", b.I0, -500.0, 0.001);
    comprobar("#4 Fijar cero: Q0 (mV)", b.Q0, -250.0, 0.001);
    comprobar("#4 |V| del mismo bloque (cero ya aplicado, como LabVIEW)", b.V, 0.0, 0.001);
    bloque(1.0, -0.5, 3.7, 0u, &m, &b);
    comprobar("#4 I' (bloque siguiente)", m.I1, 0.0, 0.001);
    comprobar("#4 Q' (bloque siguiente)", m.Q1, 0.0, 0.001);

    /* #5 */
    bloque(1.2, -0.5, 3.7, 0u, &m, &b);
    comprobar("#5 entrada (1.200, -0.500): I' (mV)", m.I1, -100.0, 0.001);
    comprobar("#5 Q' (mV)", m.Q1, 0.0, 0.001);

    /* #6 */
    bloque(1.3, -0.2, 3.7, IQ_EV_CALIBRAR, &m, &b);
    comprobar("#6 Calibrar theta: theta (grados)", b.theta, 135.0, 0.001);
    comprobar("#6 |V| del mismo bloque (mV)", b.V, r, 0.001);
    comprobar("#6 Fase del mismo bloque (grados)", b.fase, 0.0, 0.001);
    comprobar("#6 d estimada = |V| / 4.03", b.d, r / 4.03, 0.001);
    bloque(1.3, -0.2, 3.7, 0u, &m, &b);
    comprobar("#6 I' (bloque siguiente)", m.I1, r, 0.001);
    comprobar("#6 Q' (bloque siguiente)", m.Q1, 0.0, 0.001);

    /* #7 */
    bloque(1.6, 0.1, 3.7, 0u, &m, &b);
    comprobar("#7 entrada (1.600, 0.100): I' (mV)", m.I1, 2.0 * r, 0.001);
    comprobar("#7 Q' (mV)", m.Q1, 0.0, 0.001);

    /* #8 */
    bloque(1.3, -0.8, 3.7, 0u, &m, &b);
    comprobar("#8 entrada (1.300, -0.800): I' (mV)", m.I1, 0.0, 0.001);
    comprobar("#8 Q' (mV)", m.Q1, r, 0.001);
    comprobar("#8 Fase (grados)", b.fase, 90.0, 0.001);
    comprobar("#8 |V| (mV)", b.V, r, 0.001);
}

static void pruebas_9_a_11(void)
{
    iq_muestra_t m;
    iq_bloque_t b;

    printf("\n-- #9 a #11: signo de I y VCC_A\n");
    iq_inicializar();
    iq_fijar_corr_signo(false);
    bloque(1.0, -0.5, 3.7, 0u, &m, &b);
    comprobar("#9 sin correccion de signo: Ip (mV)", m.Ip, 500.0, 0.001);

    iq_inicializar();
    bloque(1.0, -0.5, 3.0, 0u, &m, &b);
    comprobar("#10 Vcc = 3.0 V: Ip (mV) = -500 * 3.7/3.0", m.Ip, -500.0 * 3.7 / 3.0, 0.001);
    comprobar("#10 Vcc = 3.0 V: Qp (mV)", m.Qp, -250.0 * 3.7 / 3.0, 0.001);
    comprobar_cond("#10 LED VCC_A apagado", (b.banderas & IQ_FL_VCC_FUERA) == 0u);

    iq_inicializar();
    bloque(1.0, -0.5, 2.0, 0u, &m, &b);
    comprobar("#11 Vcc = 2.0 V: se usa 3.7 V -> Ip (mV)", m.Ip, -500.0, 0.001);
    comprobar("#11 VCC_A medido (V)", b.vcc, 2.0, 0.001);
    comprobar_cond("#11 LED \"VCC_A fuera de rango\" encendido", (b.banderas & IQ_FL_VCC_FUERA) != 0u);
    bloque(1.0, -0.5, 3.7, 0u, &m, &b);
    comprobar_cond("#11 VCC_A vuelve a 3.7 V: LED apagado", (b.banderas & IQ_FL_VCC_FUERA) == 0u);
    bloque(1.0, -0.5, 3.0, 0u, &m, &b);
    comprobar("#11 ... y la escala vuelve a ser ratiometrica", m.Ip, -500.0 * 3.7 / 3.0, 0.001);

    bloque(1.0, -0.5, 0.3, 0u, &m, &b);      /* VCC_A cae de golpe (cable suelto) */
    comprobar("caida brusca de VCC_A: escala de respaldo con el FVR (Ip)", m.Ip, -500.0, 0.001);
    comprobar_cond("caida brusca de VCC_A: LED encendido", (b.banderas & IQ_FL_VCC_FUERA) != 0u);
}

static void prueba_12(void)
{
    iq_muestra_t m;
    iq_bloque_t b;
    double radio_min = 1e9, radio_max = 0.0, giro = 0.0, ang_ant = 0.0;
    unsigned n;

    printf("\n-- #12: batido 0.5 cos / 0.5 sin a 4 Hz, G = 2\n");
    iq_inicializar();
    for (n = 0; n < 100u; n++) {
        double t = n * (PERIODO_MS / 1000.0);
        iq_codigos_t c = codigos(0.5 * cos(2 * PI * 4 * t), 0.5 * sin(2 * PI * 4 * t), 3.7);
        double r, ang, dang;

        (void)iq_procesar(&c, &m, &b);
        r = sqrt((double)m.I1 * m.I1 + (double)m.Q1 * m.Q1);
        ang = atan2(m.Q1, m.I1);
        radio_min = fmin(radio_min, r);
        radio_max = fmax(radio_max, r);
        if (n > 0u) {
            dang = ang - ang_ant;
            while (dang > PI) {
                dang -= 2 * PI;
            }
            while (dang < -PI) {
                dang += 2 * PI;
            }
            giro += dang;
        }
        ang_ant = ang;
    }
    comprobar("#12 radio minimo (mV)", radio_min, 250.0, 0.5);
    comprobar("#12 radio maximo (mV)", radio_max, 250.0, 0.5);
    comprobar("#12 giro en 0.99 s (grados; negativo = horario)", giro * 180.0 / PI, -4.0 * 360.0 * 0.99, 0.5);
}

static void pruebas_extra(void)
{
    iq_muestra_t m;
    iq_bloque_t b;
    double vdd_original = vdd;
    iq_codigos_t c;

    printf("\n-- Pruebas adicionales\n");

    vdd = 3.3;
    iq_inicializar();
    bloque(1.0, -0.5, 3.0, 0u, &m, &b);
    comprobar("VDD = 3.3 V: Ip igual que con 4.095 V (VDD se cancela)", m.Ip, -500.0 * 3.7 / 3.0, 0.6);
    comprobar("VDD = 3.3 V: VCC_A medido con el FVR (V)", b.vcc, 3.0, 0.002);
    vdd = vdd_original;

    iq_inicializar();
    bloque(1.0, -0.5, 3.7, IQ_EV_FIJAR_CERO | IQ_EV_BORRAR_CERO, &m, &b);
    comprobar_cond("\"Borrar 0\" manda sobre \"Fijar cero\" en el mismo bloque", b.I0 == 0.0f && b.Q0 == 0.0f);

    bloque(1.3, -0.2, 3.7, IQ_EV_FIJAR_CERO | IQ_EV_CALIBRAR, &m, &b);
    comprobar("Fijar cero + Calibrar a la vez: theta = atan2(0, 0)", b.theta, 0.0, 0.001);

    bloque(1.3, -0.2, 3.7, IQ_EV_BORRAR_CERO, &m, &b);
    bloque(1.0, -0.5, 3.7, IQ_EV_CALIBRAR, &m, &b);
    comprobar("Calibrar sin cero: theta = atan2(-250, -500)", b.theta, atan2(-250.0, -500.0) * 180.0 / PI, 0.001);
    bloque(1.0, -0.5, 3.7, IQ_EV_THETA_0, &m, &b);
    comprobar("\"theta = 0\"", b.theta, 0.0, 0.0);
    bloque(1.0, -0.5, 3.7, IQ_EV_BORRAR_TRAZA, &m, &b);
    comprobar_cond("evento \"Borrar traza\" devuelto en el bloque", (b.eventos & IQ_EV_BORRAR_TRAZA) != 0u);

    iq_inicializar();
    bloque(1.84, -0.5, 3.7, 0u, &m, &b);      /* I_OUT = 3.69 V con VCC_A = 3.7 V */
    comprobar_cond("saturacion de I detectada (I_OUT a 10 mV del riel)",
                   (b.banderas & IQ_FL_SAT_I) != 0u && (b.banderas & IQ_FL_SAT_Q) == 0u);
    bloque(1.0, -1.83, 3.7, 0u, &m, &b);      /* Q_OUT = 0.02 V */
    comprobar_cond("saturacion de Q detectada (Q_OUT a 20 mV de GND)", (b.banderas & IQ_FL_SAT_Q) != 0u);

    vdd = 3.6;                                 /* el pin VCC_A por encima de VDD: el ADC satura */
    bloque(1.0, -0.5, 3.7 * DIVISOR_VCCA, 0u, &m, &b);
    comprobar_cond("VCC_A >= VDD detectado", (b.banderas & IQ_FL_SAT_VCC) != 0u);
    vdd = vdd_original;

    iq_inicializar();
    c = codigos(1.0, -0.5, 3.7);
    c.cF = 0u;                                 /* sin lectura del FVR */
    {
        unsigned i;
        for (i = 0; i < N_BLOQUE; i++) {
            (void)iq_procesar(&c, &m, &b);
        }
    }
    comprobar("sin FVR: escala ratiometrica igual (Ip)", m.Ip, -500.0, 0.001);
    comprobar_cond("sin FVR: bandera y VCC_A = 0", (b.banderas & IQ_FL_SIN_FVR) != 0u && b.vcc == 0.0f);

    iq_inicializar();
    iq_fijar_calibracion(62.3276f, 21.1352f, 0.0f);   /* cero de la medida real (9.2) */
    bloque(1.0, -0.5, 3.7, 0u, &m, &b);
    comprobar("iq_fijar_calibracion: I' = Ip - I0", m.I1, -500.0 - 62.3276, 0.001);
}

static void pruebas_formato(void)
{
    char buf[16];
    uint8_t n;

    printf("\n-- Formato de numeros\n");
#define FORMATO(func, valor, texto)                                         \
    n = func(buf, valor);                                                   \
    buf[n] = '\0';                                                          \
    comprobar_cond(#func "(" #valor ") = \"" texto "\"", strcmp(buf, texto) == 0)
    FORMATO(fmt_u32, 0u, "0");
    FORMATO(fmt_u32, 7u, "7");
    FORMATO(fmt_u32, 1000u, "1000");
    FORMATO(fmt_u32, 4294967295u, "4294967295");
    FORMATO(fmt_i32, -7, "-7");
    FORMATO(fmt_i32, -500000, "-500000");
    FORMATO(fmt_i32, 2147483647, "2147483647");
    FORMATO(fmt_i32, (-2147483647 - 1), "-2147483648");
#undef FORMATO
    comprobar("fmt_a_entero(-500 mV, 1000) = -500000 uV", fmt_a_entero(-500.0f, 1000.0f), -500000.0, 0.0);
    comprobar("fmt_a_entero(212.132 mV, 1000)", fmt_a_entero(212.132f, 1000.0f), 212132.0, 0.0);
    comprobar("fmt_a_entero(0.0006 mV, 1000) redondea a 1 uV", fmt_a_entero(0.0006f, 1000.0f), 1.0, 0.0);
    comprobar("fmt_a_entero(-0.0004 mV, 1000) redondea a 0 uV", fmt_a_entero(-0.0004f, 1000.0f), 0.0, 0.0);
    comprobar("fmt_a_entero(1e10) satura", fmt_a_entero(1e10f, 1.0f), 2e9, 0.0);
    comprobar("fmt_a_entero(-1e10) satura", fmt_a_entero(-1e10f, 1.0f), -2e9, 0.0);
}

int main(void)
{
    printf("Pruebas del procesamiento I/Q (seccion 9.1) con iq_proc.c del firmware\n");
    pruebas_1_a_3();
    pruebas_4_a_8();
    pruebas_9_a_11();
    prueba_12();
    pruebas_extra();
    pruebas_formato();
    printf("\n%d pruebas, %d fallos\n", total, fallos);
    return (fallos == 0) ? 0 : 1;
}
