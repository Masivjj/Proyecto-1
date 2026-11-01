/**
  @File Name
    device_config.c

  @Summary
    Bits de configuracion del PIC16F18426 (System Module de MCC Classic).

  @Description
    HFINTOSC a 32 MHz desde el arranque, MCLR activo, BOR a 2.45 V, WDT
    desactivado y programacion en baja tension (LVP). Equivalente a lo que
    genera MCC Classic con docs/configuracion_MCC_Classic.md.

    FEXTOSC depende de la opcion RELOJ (app/app_config.h): OFF con el reloj
    interno, XT (o HS con CRISTAL_HS=1) con el cristal de 4 MHz (la hoja de
    datos da XT hasta 4 MHz, OS8) y ECM (hasta 4 MHz, OS3) con el oscilador de
    4 MHz. Se arranca siempre con el HFINTOSC; app/reloj.c pasa al externo.
    Si se regenera con MCC, hay que volver a poner este #if.
*/

#include "device_config.h"

// Configuration bits: selected in the GUI

// CONFIG1
#if RELOJ == RELOJ_CRISTAL_4MHZ
#if CRISTAL_HS
#pragma config FEXTOSC = HS    // External Oscillator mode selection bits->HS (crystal oscillator) above 4MHz; PFM set to high power
#else
#pragma config FEXTOSC = XT    // External Oscillator mode selection bits->XT (crystal oscillator) above 500kHz, below 4MHz; PFM set to medium power
#endif
#elif RELOJ == RELOJ_OSCILADOR_4MHZ
#pragma config FEXTOSC = ECM    // External Oscillator mode selection bits->EC for 500kHz to 8MHz; PFM set to medium power
#else
#pragma config FEXTOSC = OFF    // External Oscillator mode selection bits->Oscillator not enabled
#endif
#pragma config RSTOSC = HFINT32    // Power-up default value for COSC bits->HFINTOSC with OSCFRQ= 32 MHz and CDIV = 1:1
#pragma config CLKOUTEN = OFF    // Clock Out Enable bit->CLKOUT function is disabled; i/o or oscillator function on OSC2
#pragma config CSWEN = ON    // Clock Switch Enable bit->Writing to NOSC and NDIV is allowed
#pragma config FCMEN = ON    // Fail-Safe Clock Monitor Enable bit->FSCM timer enabled

// CONFIG2
#pragma config MCLRE = ON    // Master Clear Enable bit->MCLR pin is Master Clear function
#pragma config PWRTS = OFF    // Power-up Timer Enable bit->PWRT disabled
#pragma config LPBOREN = OFF    // Low-Power BOR enable bit->ULPBOR disabled
#pragma config BOREN = ON    // Brown-out reset enable bits->Brown-out Reset Enabled, SBOREN bit is ignored
#pragma config BORV = LO    // Brown-out Reset Voltage Selection->Brown-out Reset Voltage (VBOR) set to 2.45V
#pragma config ZCD = OFF    // Zero-cross detect disable->Zero-cross detect circuit is disabled at POR.
#pragma config PPS1WAY = ON    // Peripheral Pin Select one-way control->The PPSLOCK bit can be cleared and set only once in software
#pragma config STVREN = ON    // Stack Overflow/Underflow Reset Enable bit->Stack Overflow or Underflow will cause a reset

// CONFIG3
#pragma config WDTCPS = WDTCPS_31    // WDT Period Select bits->Divider ratio 1:65536; software control of WDTPS
#pragma config WDTE = OFF    // WDT operating mode->WDT Disabled, SWDTEN is ignored
#pragma config WDTCWS = WDTCWS_7    // WDT Window Select bits->window always open (100%); software control; keyed access not required
#pragma config WDTCCS = SC    // WDT input clock selector->Software Control

// CONFIG4
#pragma config BBSIZE = BB512    // Boot Block Size Selection bits->512 words boot block size
#pragma config BBEN = OFF    // Boot Block Enable bit->Boot Block disabled
#pragma config SAFEN = OFF    // SAF Enable bit->SAF disabled
#pragma config WRTAPP = OFF    // Application Block Write Protection bit->Application Block not write protected
#pragma config WRTB = OFF    // Boot Block Write Protection bit->Boot Block not write protected
#pragma config WRTC = OFF    // Configuration Register Write Protection bit->Configuration Register not write protected
#pragma config WRTD = OFF    // Data EEPROM write protection bit->Data EEPROM NOT write protected
#pragma config WRTSAF = OFF    // Storage Area Flash Write Protection bit->SAF not write protected
#pragma config LVP = ON    // Low Voltage Programming Enable bit->Low Voltage programming enabled. MCLR/Vpp pin function is MCLR.

// CONFIG5
#pragma config CP = OFF    // UserNVM Program memory code protection bit->UserNVM code protection disabled
