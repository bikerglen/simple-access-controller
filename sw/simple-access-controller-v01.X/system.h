#ifndef SYSTEM_H
#define SYSTEM_H

#include <xc.h>
#include <stdbool.h>

// 250 Hz timer 2 period value
// dec2hex(12e6/16/12/250-1)
#define TMR2_PERIOD 0xF9

// 250 Hz timer 0 load / reload value
// dec2hex(65535-12e6/16/250) = 0xF447
#define TMR0_RELOAD_VALUE 0xF447

// GPIO input pins
#define W0     (PORTCbits.RC0 ? 1 : 0)
#define W1     (PORTCbits.RC1 ? 1 : 0)
#define SW1    (PORTBbits.RB3 ? 0 : 1)
#define SW2    (PORTBbits.RB5 ? 0 : 1)

// GPIO output pints
#define RELAY1 LATAbits.LATA2
#define RELAY2 LATAbits.LATA3
#define LED1   LATBbits.LATB2
#define LED2   LATBbits.LATB4

// led states
#define LED_ON   0
#define LED_OFF  1

// relay states
#define RELAY_ON  1
#define RELAY_OFF 0

void SYSTEM_Initialize (void);
void TMR2_Initialize (void);
void TMR2_InterruptHandler (void);
void IOC_InterruptHandler (void);
void TMR0_Initialize (void);
void TMR0_Halt (void);
void TMR0_InterruptHandler (void);

#endif //SYSTEM_H
