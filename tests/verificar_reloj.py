#!/usr/bin/env python3
"""Comprueba la opcion de reloj (RELOJ en app/app_config.h, "make reloj").

Uso: verificar_reloj.py <carpeta_firmware> <carpeta_build_reloj> [salida_sim_interno]

1. Registros: preprocesa los archivos de MCC con RELOJ = 0, 1 y 2 (gcc -E,
   con un xc.h vacio) y, con los valores que escriben, calcula lo que de
   verdad resulta en cada modo: FOSC, periodo del TMR2 (disparo del ADC),
   ciclo de trabajo del PWM6, TAD del ADC, baudios de la EUSART, bits de
   configuracion del oscilador y la secuencia de app/reloj.c.
2. Pines: compila y ejecuta un programa que imprime la configuracion del
   puerto A de pin_manager.h en cada modo (produccion, depuracion y
   SALIDA_DISPARO = 1): RA5/RA4 nunca como salida con reloj externo, LED y
   pulsador donde deben.
3. Firmware simulado (tests/sim con -DRELOJ=1 y 2): reloj externo bien,
   ausente y fallando a los 0.7 s.
"""
import os
import re
import subprocess
import sys
import tempfile

AQUI = os.path.dirname(os.path.abspath(__file__))
STUB = os.path.join(AQUI, "reloj")
fallos = 0


def comprobar(nombre, ok, detalle=""):
    global fallos
    if not ok:
        fallos += 1
    print(("OK     " if ok else "FALLO  ") + nombre + (f"  ({detalle})" if detalle else ""))


def preprocesar(ruta, defs):
    r = subprocess.run(["gcc", "-E", "-P", "-I", STUB, "-I", os.path.dirname(ruta)] + defs + [ruta],
                       capture_output=True, text=True)
    if r.returncode:
        raise SystemExit(f"gcc -E {ruta}: {r.stderr}")
    return r.stdout


def registros(fw, defs):
    """{registro: [valores literales escritos]} y {bit de configuracion: valor}."""
    escritos, config = {}, {}
    mcc = os.path.join(fw, "mcc_generated_files")
    for f in ("mcc.c", "tmr2.c", "pwm6.c", "adcc.c", "eusart1.c", "device_config.c"):
        texto = preprocesar(os.path.join(mcc, f), defs)
        for reg, val in re.findall(r"\b([A-Z][A-Z0-9]+)\s*=\s*(0x[0-9A-Fa-f]+|\d+)[uU]?\s*;", texto):
            escritos.setdefault(reg, []).append(int(val, 0))
        config.update(re.findall(r"#pragma config (\w+) = (\w+)", texto))
    texto = preprocesar(os.path.join(fw, "app", "reloj.c"), defs + ["-I", mcc])
    reloj = {r: sorted({int(v, 0) for v in re.findall(r"\b%s\s*=\s*(0x[0-9A-Fa-f]+)[uU]?\s*;" % r, texto)})
             for r in ("OSCCON1", "OSCFRQ")}
    reloj["OSFIE"] = "PIE1bits.OSFIE = 1" in texto
    texto = preprocesar(os.path.join(mcc, "interrupt_manager.c"), defs)
    reloj["isr"] = bool(re.search(r"PIE1bits\.OSFIE == 1 && PIR1bits\.OSFIF == 1\)\s*\{\s*reloj_fallo_isr\(\);", texto))
    xtal = preprocesar_texto('#include "device_config.h"\nXTAL=_XTAL_FREQ\n', mcc, defs)
    m = re.search(r"XTAL=\s*(\d+)", xtal)
    return escritos, config, reloj, int(m.group(1)) if m else None


def preprocesar_texto(texto, incluir, defs):
    with tempfile.NamedTemporaryFile("w", suffix=".c", delete=False) as t:
        t.write(texto)
    try:
        r = subprocess.run(["gcc", "-E", "-P", "-I", STUB, "-I", incluir] + defs + [t.name],
                           capture_output=True, text=True)
    finally:
        os.unlink(t.name)
    return r.stdout


HFFRQ_MHZ = {0: 1, 1: 2, 2: 4, 3: 8, 4: 12, 5: 16, 6: 32}


def parte_registros(fw):
    print("-- Registros de MCC en cada modo de reloj")
    casos = [("interno", 0, [], "OFF"), ("cristal (XT)", 1, [], "XT"), ("cristal (HS)", 1, ["-DCRISTAL_HS=1"], "HS"),
             ("oscilador", 2, [], "ECM")]
    for nombre, r, extra, fext in casos:
        w, cfg, osccon1_reloj, xtal = registros(fw, [f"-DRELOJ={r}"] + extra)
        uno = lambda reg: w.get(reg, [None])[0]
        f_int = HFFRQ_MHZ[uno("OSCFRQ")] * 1e6
        fosc = 16e6 if r else f_int
        comprobar(f"{nombre}: FEXTOSC = {fext}, RSTOSC = HFINT32, FCMEN y CSWEN = ON",
                  cfg.get("FEXTOSC") == fext and cfg.get("RSTOSC") == "HFINT32" and cfg.get("FCMEN") == "ON"
                  and cfg.get("CSWEN") == "ON", f"{cfg.get('FEXTOSC')}, {cfg.get('RSTOSC')}")
        if r:
            comprobar(f"{nombre}: 4 MHz x PLL 4 = 16 MHz = HFINTOSC de reserva (OSCFRQ)", f_int == fosc,
                      f"OSCFRQ = {uno('OSCFRQ')} -> {f_int / 1e6:.0f} MHz")
            comprobar(f"{nombre}: reloj.c pide EXTOSC x4 (0x20) y, si falla, HFINTOSC (0x60) a 16 MHz (0x05)",
                      osccon1_reloj["OSCCON1"] == [0x20, 0x60] and osccon1_reloj["OSCFRQ"] == [0x05],
                      f"{osccon1_reloj}")
            comprobar(f"{nombre}: el fallo (FSCM a 1 MHz) se atiende en la interrupcion OSFIF",
                      osccon1_reloj["OSFIE"] and osccon1_reloj["isr"])
        else:
            comprobar("interno: HFINTOSC a 32 MHz; reloj.c y la interrupcion no tocan el oscilador",
                      f_int == 32e6 and not osccon1_reloj["OSCCON1"] and not osccon1_reloj["OSCFRQ"]
                      and not osccon1_reloj["isr"])
        comprobar(f"{nombre}: _XTAL_FREQ = FOSC ({fosc / 1e6:.0f} MHz)", xtal == fosc, f"{xtal}")
        # TMR2 y PWM6: disparo del ADC
        presc = 2 ** ((uno("T2CON") >> 4) & 7)
        periodo = 4 / fosc * presc * (uno("T2PR") + 1)
        comprobar(f"{nombre}: TMR2 a FOSC/4, 1:{presc}, T2PR {uno('T2PR')} -> 100 us",
                  uno("T2CLKCON") == 0x01 and uno("T2CON") & 0x80 and abs(periodo - 100e-6) < 1e-12,
                  f"{periodo * 1e6:.3f} us")
        dc = (uno("PWM6DCH") << 2 | uno("PWM6DCL") >> 6) / (4 * (uno("T2PR") + 1))
        comprobar(f"{nombre}: PWM6 al 50 %", abs(dc - 0.5) < 1e-9, f"{dc:.3f}")
        # ADC
        tad = 2 * (uno("ADCLK") + 1) / fosc
        comprobar(f"{nombre}: TAD = 1 us y periodo = 100 TAD exactos", abs(tad - 1e-6) < 1e-12
                  and abs(periodo / tad - round(periodo / tad)) < 1e-9, f"ADCLK = {uno('ADCLK')}, {tad * 1e6:.2f} us")
        # EUSART
        brg16, brgh = bool(uno("BAUD1CON") & 0x08), bool(uno("TX1STA") & 0x04)
        n = uno("SP1BRGH") << 8 | uno("SP1BRGL")
        baud = fosc / (4 * (n + 1)) if brg16 and brgh else 0
        comprobar(f"{nombre}: EUSART a 115200 baudios +-1 %", abs(baud / 115200 - 1) < 0.01,
                  f"SP1BRG = {n}: {baud:.0f} ({baud / 115200 - 1:+.2%})")


def parte_pines(fw):
    print("\n-- Puerto A en cada modo (pin_manager.h)")
    programa = ('#include <stdio.h>\n#include "pin_manager.h"\n'
                'int main(void){printf("%d %d %d %d %d %d\\n",(int)(PINES_TRISA),(int)(PINES_ANSELA),'
                '(int)(PINES_WPUA),(int)(LED_EN_RA0),(int)(BTN_THETA_EN_RA4),(int)(BTN_THETA_EN_RA1));return 0;}\n')
    mcc = os.path.join(fw, "mcc_generated_files")
    with tempfile.TemporaryDirectory() as tmp:
        fuente = os.path.join(tmp, "pines.c")
        with open(fuente, "w") as f:
            f.write(programa)
        for r in (0, 1, 2):
            for nombre, extra in (("produccion", []), ("depuracion", ["-D__DEBUG"]),
                                  ("SALIDA_DISPARO=1", ["-DSALIDA_DISPARO=1"])):
                exe = os.path.join(tmp, "pines")
                subprocess.run(["gcc", "-I", STUB, "-I", mcc, f"-DRELOJ={r}"] + extra + [fuente, "-o", exe], check=True)
                tris, ansel, wpu, led0, btn4, btn1 = map(int, subprocess.run([exe], capture_output=True,
                                                                             text=True).stdout.split())
                bit = lambda v, b: (v >> b) & 1
                caso = f"RELOJ={r}, {nombre}"
                if r == 0:
                    comprobar(f"{caso}: como hasta ahora (0x1F, 0x03, 0x14)", (tris, ansel, wpu) == (0x1F, 0x03, 0x14))
                    continue
                ok = bit(tris, 5) and not bit(wpu, 5) and not bit(ansel, 5)          # RA5 = OSC1/CLKIN
                if r == 1:
                    ok = ok and bit(tris, 4) and not bit(wpu, 4) and not bit(ansel, 4) and not btn4  # RA4 = OSC2
                    quiere_btn1 = nombre == "produccion"
                    ok = ok and btn1 == quiere_btn1 and (bit(wpu, 1) and bit(tris, 1) and not bit(ansel, 1)
                                                        if quiere_btn1 else not bit(wpu, 1))
                else:
                    ok = ok and btn4 and bit(wpu, 4) and bit(tris, 4) and not btn1     # pulsador en RA4
                quiere_led = nombre != "depuracion"
                ok = ok and led0 == quiere_led and (not bit(tris, 0) and not bit(ansel, 0) if quiere_led
                                                   else bit(tris, 0))
                ok = ok and bit(wpu, 2) and bit(tris, 2)                               # "Fijar cero" en RA2
                comprobar(f"{caso}: oscilador en RA5{'/RA4' if r == 1 else ''}, LED "
                          f"{'RA0' if quiere_led else 'no'}, theta en {'RA4' if btn4 else 'RA1' if btn1 else 'ninguno'}",
                          ok, f"TRISA {tris:#04x} ANSELA {ansel:#04x} WPUA {wpu:#04x}")


def leer(ruta):
    with open(ruta, encoding="ascii") as f:
        lineas = f.read().splitlines()
    t, bloques = [], []
    for linea in lineas:
        if linea[:1].isdigit():
            t.append(int(linea.split(",")[0]))
        elif linea.startswith("B,"):
            bloques.append((t[-1], int(linea[2:].split(",")[8])))
    return lineas, t, bloques


def continua(t):
    return all(b - a == 10 for a, b in zip(t, t[1:])) and t[-1] >= 1480


def parte_sim(build, interno):
    print("\n-- Firmware simulado con reloj externo")
    if interno and os.path.exists(interno):
        lineas, t, b = leer(interno)
        comprobar("interno: '# reloj: interno (HFINTOSC) a 32 MHz' y sin bandera 64",
                  "# reloj: interno (HFINTOSC) a 32 MHz" in lineas and not any(f & 64 for _, f in b))
    for r, texto in ((1, "cristal de 4 MHz (RA5/RA4)"), (2, "oscilador de 4 MHz (RA5)")):
        lineas, t, b = leer(os.path.join(build, f"r{r}", "bien.txt"))
        comprobar(f"{texto}, bien: '# reloj: ... x PLL 4 = 16 MHz', sin bandera 64 y sin huecos",
                  f"# reloj: {texto} x PLL 4 = 16 MHz" in lineas and not any(f & 64 for _, f in b) and continua(t))
        lineas, t, b = leer(os.path.join(build, f"r{r}", "ausente.txt"))
        aviso = f"# reloj: FALLO del {texto}: HFINTOSC de reserva a 16 MHz (+-2 %)"
        comprobar(f"{texto}, ausente: aviso al arrancar, bandera 64 en todos los bloques y sin huecos",
                  aviso in lineas and b and all(f & 64 for _, f in b) and continua(t))
        lineas, t, b = leer(os.path.join(build, f"r{r}", "falla.txt"))
        parado = f"# reloj: el {texto} se ha parado; sigue el HFINTOSC a 16 MHz (+-2 %)"
        antes = [f for tb, f in b if tb < 700]
        despues = [f for tb, f in b if tb >= 800]
        comprobar(f"{texto}, se para a los 0.7 s: un aviso, bandera 64 desde entonces, '?' lo dice y sin huecos",
                  lineas.count(parado) == 1 and antes and not any(f & 64 for f in antes)
                  and despues and all(f & 64 for f in despues) and aviso in lineas and continua(t),
                  f"{lineas.count(parado)} avisos")


def main():
    fw, build = sys.argv[1], sys.argv[2]
    interno = sys.argv[3] if len(sys.argv) > 3 else None
    parte_registros(fw)
    parte_pines(fw)
    parte_sim(build, interno)
    print(f"\n{fallos} fallos")
    return 1 if fallos else 0


if __name__ == "__main__":
    sys.exit(main())
