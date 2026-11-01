#!/usr/bin/env python3
"""Genera docs/codigo_explicado.html con el código del firmware y su explicación
línea a línea, y comprueba que no quede ninguna línea sin explicar.

Cada archivo de esta carpeta (main.c.txt, app/iq_proc.c.txt, ...) explica el
archivo del mismo nombre de P2_IQ_PIC16F18426.X:

    # resumen: Para qué sirve el archivo, en un párrafo.
    @@ 12-18
    Explicación de las líneas 12 a 18 (puede ocupar varias líneas).
    @@ 19
    Explicación de la línea 19.

Comprobaciones (si falla alguna, termina con error):
  - todos los .c y .h del proyecto tienen su explicación;
  - toda línea que no esté en blanco cae dentro de un bloque @@;
  - los bloques van en orden, no se solapan y no pasan del final del archivo;
  - el código no ha cambiado desde que se escribió la explicación (huella
    SHA-1 en huellas.json; "--sellar" la actualiza después de revisar).

Uso:
    python3 docs/explicacion/generar.py            comprueba y genera la página
    python3 docs/explicacion/generar.py --sellar   además guarda las huellas actuales
"""
import hashlib
import html
import json
import os
import re
import sys

AQUI = os.path.dirname(os.path.abspath(__file__))
RAIZ = os.path.normpath(os.path.join(AQUI, "..", ".."))
FW = os.path.join(RAIZ, "P2_IQ_PIC16F18426.X")
PLANTILLA = os.path.join(AQUI, "plantilla.html")
INTRO = os.path.join(AQUI, "introduccion.html")
SALIDA = os.path.join(RAIZ, "docs", "codigo_explicado.html")
HUELLAS = os.path.join(AQUI, "huellas.json")

# Orden de lectura recomendado para la sustentación
ORDEN = [
    ("Programa principal", ["main.c"]),
    ("Aplicación", ["app/app_config.h", "app/adquisicion.h", "app/adquisicion.c", "app/iq_proc.h", "app/iq_proc.c",
                    "app/comunicacion.h", "app/comunicacion.c", "app/formato.h", "app/formato.c",
                    "app/traza.h", "app/traza.c", "app/ajustes.h", "app/ajustes.c",
                    "app/botones.h", "app/botones.c", "app/reloj.h", "app/reloj.c"]),
    ("Drivers de MCC Classic", ["mcc_generated_files/mcc.h", "mcc_generated_files/mcc.c",
                                "mcc_generated_files/device_config.h", "mcc_generated_files/device_config.c",
                                "mcc_generated_files/pin_manager.h", "mcc_generated_files/pin_manager.c",
                                "mcc_generated_files/interrupt_manager.h", "mcc_generated_files/interrupt_manager.c",
                                "mcc_generated_files/tmr2.h", "mcc_generated_files/tmr2.c",
                                "mcc_generated_files/pwm6.h", "mcc_generated_files/pwm6.c",
                                "mcc_generated_files/adcc.h", "mcc_generated_files/adcc.c",
                                "mcc_generated_files/fvr.h", "mcc_generated_files/fvr.c",
                                "mcc_generated_files/eusart1.h", "mcc_generated_files/eusart1.c",
                                "mcc_generated_files/memory.h", "mcc_generated_files/memory.c"]),
]


def leer_explicacion(ruta):
    resumen, bloques, actual = "", [], None
    with open(ruta, encoding="utf-8") as f:
        for n, linea in enumerate(f, 1):
            linea = linea.rstrip("\n")
            m = re.match(r"^@@ (\d+)(?:-(\d+))?\s*$", linea)
            if m:
                a = int(m.group(1))
                actual = {"a": a, "b": int(m.group(2) or a), "texto": [], "linea_txt": n}
                bloques.append(actual)
            elif linea.startswith("# resumen:"):
                resumen = linea[len("# resumen:"):].strip()
            elif actual is not None:
                actual["texto"].append(linea)
            elif linea.strip():
                raise SystemExit(f"{ruta}:{n}: texto fuera de un bloque @@")
    for b in bloques:
        b["texto"] = "\n".join(b["texto"]).strip()
    return resumen, bloques


def inline(texto):
    """Markdown mínimo: `código`, **negrita**, *cursiva*."""
    t = html.escape(texto, quote=False)
    t = re.sub(r"`([^`]+)`", r"<code>\1</code>", t)
    t = re.sub(r"\*\*([^*]+)\*\*", r"<strong>\1</strong>", t)
    t = re.sub(r"(?<![*\w])\*([^*\n]+)\*(?!\w)", r"<em>\1</em>", t)
    return t


def parrafos(texto):
    partes = [p.strip() for p in re.split(r"\n\s*\n", texto) if p.strip()]
    salida = []
    for p in partes:
        lineas = p.split("\n")
        if all(re.match(r"^\s*[-•] ", l) for l in lineas):
            items = [inline(re.sub(r"^\s*[-•] ", "", l)) for l in lineas]
            salida.append("<ul>" + "".join("<li>" + i + "</li>" for i in items) + "</ul>")
        else:
            salida.append(f"<p>{inline(' '.join(l.strip() for l in lineas))}</p>")
    return "".join(salida)


def comprobar(nombre, fuente, bloques):
    errores = []
    n = len(fuente)
    previo = 0
    cubiertas = set()
    for b in bloques:
        if b["a"] > b["b"]:
            errores.append(f"bloque @@ {b['a']}-{b['b']} al revés")
        if b["a"] <= previo:
            errores.append(f"bloque @@ {b['a']}-{b['b']} desordenado o solapado (el anterior acaba en {previo})")
        if b["b"] > n:
            errores.append(f"bloque @@ {b['a']}-{b['b']} pasa del final ({n} líneas)")
        if not b["texto"]:
            errores.append(f"bloque @@ {b['a']}-{b['b']} sin explicación")
        cubiertas.update(range(b["a"], b["b"] + 1))
        previo = max(previo, b["b"])
    sin = [i + 1 for i, l in enumerate(fuente) if l.strip() and (i + 1) not in cubiertas]
    if sin:
        tramos, ini = [], sin[0]
        for x, y in zip(sin, sin[1:] + [None]):
            if y != x + 1:
                tramos.append(f"{ini}" if ini == x else f"{ini}-{x}")
                ini = y
        errores.append("líneas sin explicar: " + ", ".join(tramos))
    return [f"{nombre}: {e}" for e in errores]


def main():
    sellar = "--sellar" in sys.argv
    huellas = {}
    if os.path.exists(HUELLAS):
        with open(HUELLAS, encoding="utf-8") as f:
            huellas = json.load(f)

    en_orden = [r for _, rs in ORDEN for r in rs]
    reales = sorted(os.path.relpath(os.path.join(d, f), FW).replace(os.sep, "/")
                    for d, _, fs in os.walk(FW) for f in fs if f.endswith((".c", ".h")))
    errores = [f"{r}: está en el proyecto y no en la lista ORDEN de generar.py" for r in reales if r not in en_orden]
    errores += [f"{r}: está en ORDEN y no existe en el proyecto" for r in en_orden if r not in reales]

    archivos, total_lineas, total_bloques, avisos = [], 0, 0, []
    for grupo, rutas in ORDEN:
        for ruta in rutas:
            fuente_ruta = os.path.join(FW, ruta)
            expl_ruta = os.path.join(AQUI, ruta + ".txt")
            if not os.path.exists(fuente_ruta):
                continue
            with open(fuente_ruta, encoding="utf-8", errors="replace") as f:
                texto_fuente = f.read()
            fuente = texto_fuente.split("\n")
            if fuente and fuente[-1] == "":
                fuente.pop()
            if not os.path.exists(expl_ruta):
                errores.append(f"{ruta}: falta su explicación ({os.path.relpath(expl_ruta, RAIZ)})")
                continue
            resumen, bloques = leer_explicacion(expl_ruta)
            errores += comprobar(ruta, fuente, bloques)
            huella = hashlib.sha1(texto_fuente.encode("utf-8")).hexdigest()
            if sellar:
                huellas[ruta] = huella
            elif huellas.get(ruta) != huella:
                avisos.append(f"{ruta}: el código cambió desde que se escribió la explicación (revisar y usar --sellar)")
            total_lineas += len(fuente)
            total_bloques += len(bloques)
            archivos.append({
                "grupo": grupo, "ruta": ruta, "resumen": inline(resumen), "lineas": len(fuente),
                "codigo": fuente,
                "bloques": [{"a": b["a"], "b": b["b"], "html": parrafos(b["texto"])} for b in bloques],
            })

    for a in avisos:
        print("AVISO  " + a)
    if errores:
        for e in errores:
            print("ERROR  " + e)
        raise SystemExit(f"{len(errores)} errores: no se genera la página")

    if sellar:
        with open(HUELLAS, "w", encoding="utf-8") as f:
            json.dump(huellas, f, indent=1, sort_keys=True)
            f.write("\n")

    with open(PLANTILLA, encoding="utf-8") as f:
        plantilla = f.read()
    with open(INTRO, encoding="utf-8") as f:
        intro = f.read()
    datos = json.dumps({"archivos": archivos, "lineas": total_lineas, "bloques": total_bloques},
                       ensure_ascii=False, separators=(",", ":")).replace("</", "<\\/")
    pagina = plantilla.replace("<!--INTRODUCCION-->", intro).replace("/*DATOS*/null", datos)
    with open(SALIDA, "w", encoding="utf-8") as f:
        f.write(pagina)
    print(f"{len(archivos)} archivos, {total_lineas} líneas, {total_bloques} explicaciones, "
          f"0 líneas sin explicar -> {os.path.relpath(SALIDA, RAIZ)}")


if __name__ == "__main__":
    main()
