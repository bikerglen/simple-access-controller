//-----------------------------------------------------------------------------------------------
// notes
//
// 04 90 D3 1A 6F 66 80
//
// data from scope: 0011 0001 1100 1010 0011 1100 0000 1001
//                     3    5    c    a    3    c    0    9        
// data from rfid reader app: 09:3c:ca:35


//-----------------------------------------------------------------------------------------------
// includes
//

#include <xc.h>
#include <stdint.h>
#include "system.h"


//-----------------------------------------------------------------------------------------------
// defines
//

#define N_CARDS 2


//-----------------------------------------------------------------------------------------------
// typedefs
//


//-----------------------------------------------------------------------------------------------
// prototypes
//

void ResetUID (void);
void ShiftUID (void);


//-----------------------------------------------------------------------------------------------
// globals
//

// flag from timer isr to main to execute 250 Hz / 4 ms tick
volatile uint8_t flag250 = 0;  

// led timer counter
uint16_t ledTimer = 0;

// work variables for constructing card UID from card reader
uint8_t cbits = 0;
uint8_t newUidBit = 0;
uint8_t uidBits = 0;
uint8_t uidData[8];

uint8_t mainUidBits = 0;
uint8_t mainUidData[8];
uint8_t relayTimer1 = 0;
uint8_t badUidTimer = 0;
uint8_t goodUidTimer = 0;

const uint8_t authedUids[N_CARDS][8] = {
    { 0x12, 0x34, 0x56, 0x78, 0x00, 0x00, 0x00, 0x00 }, // bike helmet
    { 0x87, 0x65, 0x43, 0x21, 0x00, 0x00, 0x00, 0x00 }  // skate helmet
};


//-----------------------------------------------------------------------------------------------
// main
//

void main(void)
{
    uint8_t card;
    uint8_t i;
    uint8_t uidAuthorized;
    uint8_t notAuthorized;
    
    SYSTEM_Initialize ();

    // no analog I/O
    ANSELA = 0;
    ANSELB = 0;
    ANSELC = 0;
    ANSELD = 0;
    ANSELE = 0;
   
    // initialize relays to off
    LATAbits.LATA2 = RELAY_OFF;
    LATAbits.LATA3 = RELAY_OFF;
    TRISAbits.TRISA2 = 0;
    TRISAbits.TRISA3 = 0;
    
    // initialize LEDs to off
    LATBbits.LATB2 = LED_OFF;
    LATBbits.LATB4 = LED_OFF;
    TRISBbits.TRISB2 = 0;
    TRISBbits.TRISB4 = 0;
    
    // initialize W0, W1, SW1, SW2 as inputs
    TRISCbits.TRISC0 = 1;
    TRISCbits.TRISC1 = 1;
    TRISBbits.TRISB3 = 1;
    TRISBbits.TRISB5 = 1;
    
    // clear uid from card
    uidBits = 0;
    for (i = 0; i < 8; i++) {
        uidData[i] = 0;
    }

    // enable priority interrupts
    RCONbits.IPEN = 1;

    // configure TMR2
    TMR2_Initialize ();
    
    // enable iocc0 and iocc1
    IOCCbits.IOCC0 = 1;    // enable IOCC0
    IOCCbits.IOCC1 = 1;    // enable IOCC1
    cbits = PORTC;         // latch state
    INTCON2bits.IOCIP = 0; // low priority
    INTCONbits.IOCIF = 0;  // clear flag
    INTCONbits.IOCIE = 1;  // enable interrupt

    // enable low priority interrupts
    INTCONbits.GIEH = 1;
    INTCONbits.GIEL = 1;

    while(1)
    {
        // run 200 Hz tasks
        if (flag250) {
            // clear flag
            flag250 = 0;
            
            // blink led or turn on for a while if not authorized
            if (badUidTimer != 0) {
                LED1 = LED_ON;
                badUidTimer--;
            } else if (ledTimer == 0) {
                LED1 = LED_ON;
            } else if (ledTimer == 25) {
                LED1 = LED_OFF;
            }
            
            // increment led timer counter, 1.5 second period
            if (++ledTimer >= 250) {
                ledTimer = 0;
            }

            // turn on green led for a while if authorized
            if (goodUidTimer != 0) {
                LED2 = LED_ON;
                goodUidTimer--;
            } else {
                LED2 = LED_OFF;
            }
           
            // decrement relay 1 timer until it hits zero then turn off the relay
            if (relayTimer1 == 0) {
                RELAY1 = RELAY_OFF;
            } else {
                relayTimer1--;
            }
            
            if (mainUidBits != 0) {
                // check card against valid cards
                notAuthorized = 1;
                for (card = 0; card < N_CARDS; card++) {
                    uidAuthorized = 1;
                    for (i = 0; i < 8; i++) {
                        if (mainUidData[i] != authedUids[card][i]) {
                            uidAuthorized = 0;
                        }
                    }
                    if (uidAuthorized) {
                        notAuthorized = 0;
                        RELAY1 = RELAY_ON;
                        LED2 = LED_ON;
                        relayTimer1 = 125;
                        goodUidTimer = 250;
                    }
                }
                mainUidBits = 0;
                if (notAuthorized) {
                    badUidTimer = 250;
                }
            }
        }
    }//end while
}//end main


void TMR2_Initialize (void)
{
    PR2 = TMR2_PERIOD;
    TMR2 = 0x00;
    IPR1bits.TMR2IP = 0;
    PIR1bits.TMR2IF = 0;
    PIE1bits.TMR2IE = 1;
    T2CON = 0x5E;
}


void TMR2_InterruptHandler (void)
{
    PIR1bits.TMR2IF = 0;
    flag250 = 1;
}


void IOC_InterruptHandler (void)
{
    cbits = PORTC;

    switch (cbits & 3) {
        case 0:
            // both low is an error, reset the UID and start over
            ResetUID ();
            break;
        case 1: 
            // W1 is low and W0 is high, that's a '1'
            newUidBit = 1;
            ShiftUID ();
            break;
        case 2:
            // W1 is high and W0 is low, that's a '0'
            newUidBit = 0;
            ShiftUID ();
            break;
        case 3:
            // both high is a no operation
            break;
    }

    INTCONbits.IOCIF = 0;  // clear flag
}


void ResetUID (void)
{
    uint8_t i;
    
    uidBits = 0;
    for (i = 0; i < 8; i++) {
        uidData[i] = 0;
    }
}


void ShiftUID (void)
{
    if (uidBits >= 64) {
        ResetUID ();
    } else {
        uidBits++;
        
        uidData[7] = uidData[7] << 1;
        uidData[7] |= (uidData[6] & 0x80) ? 1 : 0;

        uidData[6] = uidData[6] << 1;
        uidData[6] |= (uidData[5] & 0x80) ? 1 : 0;

        uidData[5] = uidData[5] << 1;
        uidData[5] |= (uidData[4] & 0x80) ? 1 : 0;

        uidData[4] = uidData[4] << 1;
        uidData[4] |= (uidData[3] & 0x80) ? 1 : 0;

        uidData[3] = uidData[3] << 1;
        uidData[3] |= (uidData[2] & 0x80) ? 1 : 0;

        uidData[2] = uidData[2] << 1;
        uidData[2] |= (uidData[1] & 0x80) ? 1 : 0;

        uidData[1] = uidData[1] << 1;
        uidData[1] |= (uidData[0] & 0x80) ? 1 : 0;

        uidData[0] = uidData[0] << 1;
        uidData[0] |= newUidBit ? 1 : 0;
    }
    
    // set data timeout timer to 4 milliseconds
    TMR0_Initialize ();
}


void TMR0_Initialize (void)
{
    T0CONbits.T08BIT = 0;
    TMR0H = TMR0_RELOAD_VALUE >> 8;
    TMR0L = TMR0_RELOAD_VALUE & 0xFF;
    INTCON2bits.TMR0IP = 0;
    INTCONbits.TMR0IF = 0;
    INTCONbits.TMR0IE = 1;
    T0CON = 0x93;
}


void TMR0_InterruptHandler (void)
{
    uint8_t i;
    
    // clear flag, disable interrupt, and stop timer
    INTCONbits.TMR0IF = 0;
    INTCONbits.TMR0IE = 0;
    T0CONbits.TMR0ON = 0;

    if (uidBits >= 26) {
        // forward UID to main loop for checking
        mainUidBits = uidBits;
        for (i = 0; i < 8; i++) {
            mainUidData[i] = uidData[i];
        }
    }
    
    // get ready for next card scan
    ResetUID ();
}


        
