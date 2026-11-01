# Pruebas en el PC del firmware del PIC16F18426 (gcc y python3).
#
#   make        pruebas unitarias + simulacion del firmware + programa del PC
#   make test   solo las pruebas unitarias (vectores de la seccion 9.1)
#   make pc     pruebas de los programas del PC: interprete de lineas de
#               pc/plano_polar.py, analisis de pc/medir_jitter.py y
#               pc/adc_1ksps.py (numpy; despues de "make adc1k") y
#               pc/prueba_usb6211.py (sin la tarjeta)
#   make sim    simula el firmware completo (main.c y app/*.c sin cambios)
#               con un hardware falso y comprueba su salida por UART
#   make reloj  opcion de reloj externo de 4 MHz: registros de MCC en cada modo
#               y firmware simulado con el reloj bien, ausente y fallando
#   make adc1k  simula el avance ADC_1kSPS_PIC16F18426.X (1000 muestras/s
#               por la UART) y comprueba su salida
#   make clean  borra lo compilado

FW      = ../P2_IQ_PIC16F18426.X
APP     = $(FW)/app
CFLAGS  = -std=c99 -Wall -Wextra -Wpedantic -Werror -O2
FUENTES = test_iq_proc.c $(APP)/iq_proc.c $(APP)/formato.c
CABECERAS = $(APP)/iq_proc.h $(APP)/formato.h $(APP)/app_config.h

SIM     = build/sim
SIM_APP = $(SIM)/app/iq_proc.c $(SIM)/app/formato.c $(SIM)/app/adquisicion.c \
          $(SIM)/app/botones.c $(SIM)/app/traza.c $(SIM)/app/ajustes.c \
          $(SIM)/app/comunicacion.c $(SIM)/app/reloj.c

ADC1K   = ../ADC_1kSPS_PIC16F18426.X
B1K     = build/adc1k

.PHONY: all test sim reloj adc1k pc clean

all: test sim reloj adc1k pc

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
	$(CC) -std=c99 -Wall -Wextra -Werror -O2 -Wno-main -Dmain=firmware_main -c $(SIM)/main.c -o $(SIM)/main.o
	$(CC) -std=c99 -Wall -Wextra -Werror -O2 -I$(SIM) -o $(SIM)/sim sim/sim.c $(SIM)/main.o $(SIM_APP) -lm
	./$(SIM)/sim 10 $(SIM)/eeprom.bin > $(SIM)/arranque1.txt
	./$(SIM)/sim 0.3 $(SIM)/eeprom.bin > $(SIM)/arranque2.txt
	python3 verificar_sim.py $(SIM)/arranque1.txt $(SIM)/arranque2.txt

# Opcion RELOJ (reloj externo de 4 MHz): registros de MCC en cada modo y el
# firmware simulado con el cristal (RELOJ=1) y el oscilador (RELOJ=2), con el
# reloj bien, ausente y parandose a los 0.7 s. Usa la salida de "make sim"
# para el modo interno.
RLJ     = build/reloj
RLJ_APP = iq_proc formato adquisicion botones traza ajustes comunicacion reloj
reloj: sim/sim.c sim/mcc.h verificar_reloj.py reloj/xc.h $(FW)/main.c $(wildcard $(APP)/*) $(wildcard $(FW)/mcc_generated_files/*)
	rm -rf $(RLJ)
	set -e; for r in 1 2; do \
	  d=$(RLJ)/r$$r; mkdir -p $$d/app $$d/mcc_generated_files; \
	  cp $(FW)/main.c $$d/; cp $(APP)/*.c $(APP)/*.h $$d/app/; cp sim/mcc.h $$d/mcc_generated_files/; \
	  $(CC) -std=c99 -Wall -Wextra -Werror -Wundef -O2 -Wno-main -DRELOJ=$$r -Dmain=firmware_main -c $$d/main.c -o $$d/main.o; \
	  $(CC) -std=c99 -Wall -Wextra -Werror -Wundef -O2 -DRELOJ=$$r -I$$d -o $$d/sim sim/sim.c $$d/main.o \
	    $$(for f in $(RLJ_APP); do echo $$d/app/$$f.c; done) -lm; \
	  for e in bien ausente falla; do ./$$d/sim 1.5 $$d/eeprom_$$e.bin $$e > $$d/$$e.txt; done; \
	done
	python3 verificar_reloj.py $(FW) $(RLJ) $(SIM)/arranque1.txt

# Igual que "sim": main.c y config.h se copian junto al mcc.h falso
adc1k: adc1k/sim.c adc1k/mcc.h verificar_adc1k.py $(ADC1K)/main.c $(ADC1K)/config.h
	rm -rf $(B1K)
	mkdir -p $(B1K)/mcc_generated_files
	cp $(ADC1K)/main.c $(ADC1K)/config.h $(B1K)/
	cp adc1k/mcc.h $(B1K)/mcc_generated_files/
	$(CC) -std=c99 -Wall -Wextra -Werror -O2 -Wno-main -Dmain=firmware_main -c $(B1K)/main.c -o $(B1K)/main.o
	$(CC) -std=c99 -Wall -Wextra -Werror -O2 -I$(B1K) -o $(B1K)/sim adc1k/sim.c $(B1K)/main.o -lm
	./$(B1K)/sim 6 $(B1K)/conversiones.txt > $(B1K)/salida.txt
	python3 verificar_adc1k.py $(B1K)/salida.txt $(B1K)/conversiones.txt

pc:
	python3 test_plano_polar.py
	python3 test_medir_jitter.py
	python3 test_adc_1ksps.py
	python3 test_prueba_usb6211.py

clean:
	rm -rf test_iq_proc build
