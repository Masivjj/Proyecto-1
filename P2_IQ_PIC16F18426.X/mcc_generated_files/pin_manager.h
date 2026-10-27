/**
  @File Name
    pin_manager.h

  @Summary
    Macros de los pines (Pin Module de MCC Classic).

  @Description
    Mapa de pines de la seccion 10.3 de la especificacion (PIC16F18426, 14 pines):

      Pin  Puerto  Nombre en MCC  Funcion
      10   RC0     I_OUT          ADC ANC0 <- J805.1
       9   RC1     Q_OUT          ADC ANC1 <- J805.3
       8   RC2     V_REF          ADC ANC2 <- J805.2
       7   RC3     VCC_A          ADC ANC3 <- J805.5
       6   RC4     -              EUSART1 TX (PPS) -> RX del adaptador USB-serie
       5   RC5     -              EUSART1 RX (PPS) <- TX del adaptador USB-serie
      11   RA2     BTN_CERO       pulsador "Fijar cero" (pull-up interno, activo en bajo)
       3   RA4     BTN_THETA      pulsador "Calibrar theta" (pull-up interno, activo en bajo)
       2   RA5     LED            LED de estado / alarma
      13   RA0     -              ICSPDAT (sin uso: entrada analogica)
      12   RA1     -              ICSPCLK (sin uso: entrada analogica)
       4   RA3     -              MCLR (pull-up externo de 10 kohm)

    Equivalente a lo que genera MCC Classic con docs/configuracion_MCC_Classic.md.
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

// get/set BTN_CERO aliases
#define BTN_CERO_TRIS                 TRISAbits.TRISA2
#define BTN_CERO_LAT                  LATAbits.LATA2
#define BTN_CERO_PORT                 PORTAbits.RA2
#define BTN_CERO_WPU                  WPUAbits.WPUA2
#define BTN_CERO_OD                   ODCONAbits.ODCA2
#define BTN_CERO_ANS                  ANSELAbits.ANSA2
#define BTN_CERO_SetHigh()            do { LATAbits.LATA2 = 1; } while(0)
#define BTN_CERO_SetLow()             do { LATAbits.LATA2 = 0; } while(0)
#define BTN_CERO_Toggle()             do { LATAbits.LATA2 = ~LATAbits.LATA2; } while(0)
#define BTN_CERO_GetValue()           PORTAbits.RA2
#define BTN_CERO_SetDigitalInput()    do { TRISAbits.TRISA2 = 1; } while(0)
#define BTN_CERO_SetDigitalOutput()   do { TRISAbits.TRISA2 = 0; } while(0)
#define BTN_CERO_SetPullup()          do { WPUAbits.WPUA2 = 1; } while(0)
#define BTN_CERO_ResetPullup()        do { WPUAbits.WPUA2 = 0; } while(0)
#define BTN_CERO_SetPushPull()        do { ODCONAbits.ODCA2 = 0; } while(0)
#define BTN_CERO_SetOpenDrain()       do { ODCONAbits.ODCA2 = 1; } while(0)
#define BTN_CERO_SetAnalogMode()      do { ANSELAbits.ANSA2 = 1; } while(0)
#define BTN_CERO_SetDigitalMode()     do { ANSELAbits.ANSA2 = 0; } while(0)

// get/set BTN_THETA aliases
#define BTN_THETA_TRIS                 TRISAbits.TRISA4
#define BTN_THETA_LAT                  LATAbits.LATA4
#define BTN_THETA_PORT                 PORTAbits.RA4
#define BTN_THETA_WPU                  WPUAbits.WPUA4
#define BTN_THETA_OD                   ODCONAbits.ODCA4
#define BTN_THETA_ANS                  ANSELAbits.ANSA4
#define BTN_THETA_SetHigh()            do { LATAbits.LATA4 = 1; } while(0)
#define BTN_THETA_SetLow()             do { LATAbits.LATA4 = 0; } while(0)
#define BTN_THETA_Toggle()             do { LATAbits.LATA4 = ~LATAbits.LATA4; } while(0)
#define BTN_THETA_GetValue()           PORTAbits.RA4
#define BTN_THETA_SetDigitalInput()    do { TRISAbits.TRISA4 = 1; } while(0)
#define BTN_THETA_SetDigitalOutput()   do { TRISAbits.TRISA4 = 0; } while(0)
#define BTN_THETA_SetPullup()          do { WPUAbits.WPUA4 = 1; } while(0)
#define BTN_THETA_ResetPullup()        do { WPUAbits.WPUA4 = 0; } while(0)
#define BTN_THETA_SetPushPull()        do { ODCONAbits.ODCA4 = 0; } while(0)
#define BTN_THETA_SetOpenDrain()       do { ODCONAbits.ODCA4 = 1; } while(0)
#define BTN_THETA_SetAnalogMode()      do { ANSELAbits.ANSA4 = 1; } while(0)
#define BTN_THETA_SetDigitalMode()     do { ANSELAbits.ANSA4 = 0; } while(0)

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

// get/set I_OUT aliases
#define I_OUT_TRIS                 TRISCbits.TRISC0
#define I_OUT_LAT                  LATCbits.LATC0
#define I_OUT_PORT                 PORTCbits.RC0
#define I_OUT_WPU                  WPUCbits.WPUC0
#define I_OUT_OD                   ODCONCbits.ODCC0
#define I_OUT_ANS                  ANSELCbits.ANSC0
#define I_OUT_SetHigh()            do { LATCbits.LATC0 = 1; } while(0)
#define I_OUT_SetLow()             do { LATCbits.LATC0 = 0; } while(0)
#define I_OUT_Toggle()             do { LATCbits.LATC0 = ~LATCbits.LATC0; } while(0)
#define I_OUT_GetValue()           PORTCbits.RC0
#define I_OUT_SetDigitalInput()    do { TRISCbits.TRISC0 = 1; } while(0)
#define I_OUT_SetDigitalOutput()   do { TRISCbits.TRISC0 = 0; } while(0)
#define I_OUT_SetPullup()          do { WPUCbits.WPUC0 = 1; } while(0)
#define I_OUT_ResetPullup()        do { WPUCbits.WPUC0 = 0; } while(0)
#define I_OUT_SetPushPull()        do { ODCONCbits.ODCC0 = 0; } while(0)
#define I_OUT_SetOpenDrain()       do { ODCONCbits.ODCC0 = 1; } while(0)
#define I_OUT_SetAnalogMode()      do { ANSELCbits.ANSC0 = 1; } while(0)
#define I_OUT_SetDigitalMode()     do { ANSELCbits.ANSC0 = 0; } while(0)

// get/set Q_OUT aliases
#define Q_OUT_TRIS                 TRISCbits.TRISC1
#define Q_OUT_LAT                  LATCbits.LATC1
#define Q_OUT_PORT                 PORTCbits.RC1
#define Q_OUT_WPU                  WPUCbits.WPUC1
#define Q_OUT_OD                   ODCONCbits.ODCC1
#define Q_OUT_ANS                  ANSELCbits.ANSC1
#define Q_OUT_SetHigh()            do { LATCbits.LATC1 = 1; } while(0)
#define Q_OUT_SetLow()             do { LATCbits.LATC1 = 0; } while(0)
#define Q_OUT_Toggle()             do { LATCbits.LATC1 = ~LATCbits.LATC1; } while(0)
#define Q_OUT_GetValue()           PORTCbits.RC1
#define Q_OUT_SetDigitalInput()    do { TRISCbits.TRISC1 = 1; } while(0)
#define Q_OUT_SetDigitalOutput()   do { TRISCbits.TRISC1 = 0; } while(0)
#define Q_OUT_SetPullup()          do { WPUCbits.WPUC1 = 1; } while(0)
#define Q_OUT_ResetPullup()        do { WPUCbits.WPUC1 = 0; } while(0)
#define Q_OUT_SetPushPull()        do { ODCONCbits.ODCC1 = 0; } while(0)
#define Q_OUT_SetOpenDrain()       do { ODCONCbits.ODCC1 = 1; } while(0)
#define Q_OUT_SetAnalogMode()      do { ANSELCbits.ANSC1 = 1; } while(0)
#define Q_OUT_SetDigitalMode()     do { ANSELCbits.ANSC1 = 0; } while(0)

// get/set V_REF aliases
#define V_REF_TRIS                 TRISCbits.TRISC2
#define V_REF_LAT                  LATCbits.LATC2
#define V_REF_PORT                 PORTCbits.RC2
#define V_REF_WPU                  WPUCbits.WPUC2
#define V_REF_OD                   ODCONCbits.ODCC2
#define V_REF_ANS                  ANSELCbits.ANSC2
#define V_REF_SetHigh()            do { LATCbits.LATC2 = 1; } while(0)
#define V_REF_SetLow()             do { LATCbits.LATC2 = 0; } while(0)
#define V_REF_Toggle()             do { LATCbits.LATC2 = ~LATCbits.LATC2; } while(0)
#define V_REF_GetValue()           PORTCbits.RC2
#define V_REF_SetDigitalInput()    do { TRISCbits.TRISC2 = 1; } while(0)
#define V_REF_SetDigitalOutput()   do { TRISCbits.TRISC2 = 0; } while(0)
#define V_REF_SetPullup()          do { WPUCbits.WPUC2 = 1; } while(0)
#define V_REF_ResetPullup()        do { WPUCbits.WPUC2 = 0; } while(0)
#define V_REF_SetPushPull()        do { ODCONCbits.ODCC2 = 0; } while(0)
#define V_REF_SetOpenDrain()       do { ODCONCbits.ODCC2 = 1; } while(0)
#define V_REF_SetAnalogMode()      do { ANSELCbits.ANSC2 = 1; } while(0)
#define V_REF_SetDigitalMode()     do { ANSELCbits.ANSC2 = 0; } while(0)

// get/set VCC_A aliases
#define VCC_A_TRIS                 TRISCbits.TRISC3
#define VCC_A_LAT                  LATCbits.LATC3
#define VCC_A_PORT                 PORTCbits.RC3
#define VCC_A_WPU                  WPUCbits.WPUC3
#define VCC_A_OD                   ODCONCbits.ODCC3
#define VCC_A_ANS                  ANSELCbits.ANSC3
#define VCC_A_SetHigh()            do { LATCbits.LATC3 = 1; } while(0)
#define VCC_A_SetLow()             do { LATCbits.LATC3 = 0; } while(0)
#define VCC_A_Toggle()             do { LATCbits.LATC3 = ~LATCbits.LATC3; } while(0)
#define VCC_A_GetValue()           PORTCbits.RC3
#define VCC_A_SetDigitalInput()    do { TRISCbits.TRISC3 = 1; } while(0)
#define VCC_A_SetDigitalOutput()   do { TRISCbits.TRISC3 = 0; } while(0)
#define VCC_A_SetPullup()          do { WPUCbits.WPUC3 = 1; } while(0)
#define VCC_A_ResetPullup()        do { WPUCbits.WPUC3 = 0; } while(0)
#define VCC_A_SetPushPull()        do { ODCONCbits.ODCC3 = 0; } while(0)
#define VCC_A_SetOpenDrain()       do { ODCONCbits.ODCC3 = 1; } while(0)
#define VCC_A_SetAnalogMode()      do { ANSELCbits.ANSC3 = 1; } while(0)
#define VCC_A_SetDigitalMode()     do { ANSELCbits.ANSC3 = 0; } while(0)

// get/set RC4 procedures
#define RC4_SetHigh()            do { LATCbits.LATC4 = 1; } while(0)
#define RC4_SetLow()             do { LATCbits.LATC4 = 0; } while(0)
#define RC4_Toggle()             do { LATCbits.LATC4 = ~LATCbits.LATC4; } while(0)
#define RC4_GetValue()              PORTCbits.RC4
#define RC4_SetDigitalInput()    do { TRISCbits.TRISC4 = 1; } while(0)
#define RC4_SetDigitalOutput()   do { TRISCbits.TRISC4 = 0; } while(0)
#define RC4_SetPullup()             do { WPUCbits.WPUC4 = 1; } while(0)
#define RC4_ResetPullup()           do { WPUCbits.WPUC4 = 0; } while(0)
#define RC4_SetAnalogMode()         do { ANSELCbits.ANSC4 = 1; } while(0)
#define RC4_SetDigitalMode()        do { ANSELCbits.ANSC4 = 0; } while(0)

// get/set RC5 procedures
#define RC5_SetHigh()            do { LATCbits.LATC5 = 1; } while(0)
#define RC5_SetLow()             do { LATCbits.LATC5 = 0; } while(0)
#define RC5_Toggle()             do { LATCbits.LATC5 = ~LATCbits.LATC5; } while(0)
#define RC5_GetValue()              PORTCbits.RC5
#define RC5_SetDigitalInput()    do { TRISCbits.TRISC5 = 1; } while(0)
#define RC5_SetDigitalOutput()   do { TRISCbits.TRISC5 = 0; } while(0)
#define RC5_SetPullup()             do { WPUCbits.WPUC5 = 1; } while(0)
#define RC5_ResetPullup()           do { WPUCbits.WPUC5 = 0; } while(0)
#define RC5_SetAnalogMode()         do { ANSELCbits.ANSC5 = 1; } while(0)
#define RC5_SetDigitalMode()        do { ANSELCbits.ANSC5 = 0; } while(0)

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
