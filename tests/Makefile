# Pruebas en el PC del firmware del PIC16F18426 (gcc y python3).
#
#   make        pruebas unitarias + simulacion del firmware + programa del PC
#   make test   solo las pruebas unitarias (vectores de la seccion 9.1)
#   make pc     pruebas del interprete de lineas de pc/plano_polar.py
#   make sim    simula el firmware completo (main.c y app/*.c sin cambios)
#               con un hardware falso y comprueba su salida por UART
#   make clean  borra lo compilado

FW      = ../P2_IQ_PIC16F18426.X
APP     = $(FW)/app
CFLAGS  = -std=c99 -Wall -Wextra -Wpedantic -Werror -O2
FUENTES = test_iq_proc.c $(APP)/iq_proc.c $(APP)/formato.c
CABECERAS = $(APP)/iq_proc.h $(APP)/formato.h $(APP)/app_config.h

SIM     = build/sim
SIM_APP = $(SIM)/app/iq_proc.c $(SIM)/app/formato.c $(SIM)/app/adquisicion.c \
          $(SIM)/app/botones.c $(SIM)/app/traza.c $(SIM)/app/ajustes.c \
          $(SIM)/app/comunicacion.c

.PHONY: all test sim pc clean

all: test sim pc

test_iq_proc: $(FUENTES) $(CABECERAS)
	$(CC) $(CFLAGS) -I$(APP) -o $@ $(FUENTES) -lm

test: test_iq_proc
	./test_iq_proc

# Los fuentes incluyen "../mcc_generated_files/mcc.h": se copian junto al
# mcc.h falso para compilarlos tal cual.
sim: sim/sim.c sim/mcc.h verificar_sim.py $(FW)/main.c $(wildcard $(APP)/*)
	rm -rf $(SIM)
	mkdir -p $(SIM)/app $(SIM)/mcc_generated_files
	cp $(FW)/main.c $(SIM)/
	cp $(APP)/*.c $(APP)/*.h $(SIM)/app/
	cp sim/mcc.h $(SIM)/mcc_generated_files/
	$(CC) -std=c99 -Wall -Wextra -O2 -Wno-main -Dmain=firmware_main -c $(SIM)/main.c -o $(SIM)/main.o
	$(CC) -std=c99 -Wall -Wextra -O2 -I$(SIM) -o $(SIM)/sim sim/sim.c $(SIM)/main.o $(SIM_APP) -lm
	./$(SIM)/sim 10 $(SIM)/eeprom.bin > $(SIM)/arranque1.txt
	./$(SIM)/sim 0.3 $(SIM)/eeprom.bin > $(SIM)/arranque2.txt
	python3 verificar_sim.py $(SIM)/arranque1.txt $(SIM)/arranque2.txt

pc:
	python3 test_plano_polar.py

clean:
	rm -rf test_iq_proc build
