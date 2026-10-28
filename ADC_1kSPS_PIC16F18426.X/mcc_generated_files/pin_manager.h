/**
  @File Name
    pin_manager.h

  @Summary
    Macros de los pines (Pin Module de MCC Classic).

  @Description
    Mapa de pines del avance "ADC a 1000 muestras/s" (PIC16F18426, 14 pines):

      Pin  Puerto  Nombre en MCC  Funcion
      10   RC0     SENAL          ADC ANC0 <- generador de senales
       6   RC4     -              EUSART1 TX (PPS) -> RX del adaptador USB-serie
       5   RC5     -              EUSART1 RX (PPS) <- TX del adaptador USB-serie
       2   RA5     LED            LED de actividad (cambia cada 500 muestras)
      12   RA1     -              ICSPCLK; en produccion, salida del disparo (PWM6)
      13   RA0     -              ICSPDAT
       4   RA3     -              MCLR (pull-up externo de 10 kohm)
      RA2, RA4, RC1, RC2, RC3     sin uso (analogicos)

    Equivalente a lo que genera MCC Classic con docs/avance_adc_1ksps.md.
*/

#ifndef PIN_MANAGER_H
#define PIN_MANAGER_H

/**
  Section: Included Files
*/

#include <xc.h>

#define INPUT   1
#define OUTPUT  0

#define HIGH    1
#define LOW     0

#define ANALOG      1
#define DIGITAL     0

#define PULL_UP_ENABLED      1
#define PULL_UP_DISABLED     0

// get/set LED aliases
#define LED_TRIS                 TRISAbits.TRISA5
#define LED_LAT                  LATAbits.LATA5
#define LED_PORT                 PORTAbits.RA5
#define LED_WPU                  WPUAbits.WPUA5
#define LED_OD                   ODCONAbits.ODCA5
#define LED_ANS                  ANSELAbits.ANSA5
#define LED_SetHigh()            do { LATAbits.LATA5 = 1; } while(0)
#define LED_SetLow()             do { LATAbits.LATA5 = 0; } while(0)
#define LED_Toggle()             do { LATAbits.LATA5 = ~LATAbits.LATA5; } while(0)
#define LED_GetValue()           PORTAbits.RA5
#define LED_SetDigitalInput()    do { TRISAbits.TRISA5 = 1; } while(0)
#define LED_SetDigitalOutput()   do { TRISAbits.TRISA5 = 0; } while(0)
#define LED_SetPullup()          do { WPUAbits.WPUA5 = 1; } while(0)
#define LED_ResetPullup()        do { WPUAbits.WPUA5 = 0; } while(0)
#define LED_SetPushPull()        do { ODCONAbits.ODCA5 = 0; } while(0)
#define LED_SetOpenDrain()       do { ODCONAbits.ODCA5 = 1; } while(0)
#define LED_SetAnalogMode()      do { ANSELAbits.ANSA5 = 1; } while(0)
#define LED_SetDigitalMode()     do { ANSELAbits.ANSA5 = 0; } while(0)

// get/set SENAL aliases
#define SENAL_TRIS                 TRISCbits.TRISC0
#define SENAL_LAT                  LATCbits.LATC0
#define SENAL_PORT                 PORTCbits.RC0
#define SENAL_WPU                  WPUCbits.WPUC0
#define SENAL_OD                   ODCONCbits.ODCC0
#define SENAL_ANS                  ANSELCbits.ANSC0
#define SENAL_SetHigh()            do { LATCbits.LATC0 = 1; } while(0)
#define SENAL_SetLow()             do { LATCbits.LATC0 = 0; } while(0)
#define SENAL_Toggle()             do { LATCbits.LATC0 = ~LATCbits.LATC0; } while(0)
#define SENAL_GetValue()           PORTCbits.RC0
#define SENAL_SetDigitalInput()    do { TRISCbits.TRISC0 = 1; } while(0)
#define SENAL_SetDigitalOutput()   do { TRISCbits.TRISC0 = 0; } while(0)
#define SENAL_SetPullup()          do { WPUCbits.WPUC0 = 1; } while(0)
#define SENAL_ResetPullup()        do { WPUCbits.WPUC0 = 0; } while(0)
#define SENAL_SetPushPull()        do { ODCONCbits.ODCC0 = 0; } while(0)
#define SENAL_SetOpenDrain()       do { ODCONCbits.ODCC0 = 1; } while(0)
#define SENAL_SetAnalogMode()      do { ANSELCbits.ANSC0 = 1; } while(0)
#define SENAL_SetDigitalMode()     do { ANSELCbits.ANSC0 = 0; } while(0)

/**
   @Param
    none
   @Returns
    none
   @Description
    GPIO and peripheral I/O initialization
   @Example
    PIN_MANAGER_Initialize();
 */
void PIN_MANAGER_Initialize (void);

/**
 * @Param
    none
 * @Returns
    none
 * @Description
    Interrupt on Change Handling routine
 * @Example
    PIN_MANAGER_IOC();
 */
void PIN_MANAGER_IOC(void);

#endif // PIN_MANAGER_H
