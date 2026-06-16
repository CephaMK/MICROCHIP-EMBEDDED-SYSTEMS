/*
 * File:   SMART-PARKING-COUNTER-SYSTEM-2-DIGIT-DISPLAY.c
 * 
 * Counts cars entering and leaving
 * Shows available slots (00?99) on a 7-segment display
 * Uses sensors (simulated switches in Proteus)
 * Controls a gate (LED/relay)
 */

#include <xc.h>
#define _XTAL_FREQ 20000000

// CONFIG BITS 
#pragma config FOSC = HS
#pragma config WDTE = OFF
#pragma config PWRTE = OFF
#pragma config BOREN = ON
#pragma config LVP = OFF
#pragma config CPD = OFF
#pragma config WRT = OFF
#pragma config CP = OFF

// 7-SEGMENT LOOKUP TABLE 
// Common cathode display codes: 0 to 9
const unsigned char seg[10] = {
    0x3F,  // 0: 0011 1111
    0x06,  // 1: 0000 0110
    0x5B,  // 2: 0101 1011
    0x4F,  // 3: 0100 1111
    0x66,  // 4: 0110 0110
    0x6D,  // 5: 0110 1101
    0x7D,  // 6: 0111 1101
    0x07,  // 7: 0000 0111
    0x7F,  // 8: 0111 1111
    0x6F   // 9: 0110 1111
};

//  GLOBAL VARIABLES 
volatile unsigned char count = 0;
volatile unsigned char tens = 0;
volatile unsigned char units = 0;

volatile unsigned char display_flag = 0;
volatile unsigned char gate_timer = 0;

volatile unsigned char entry_debounce = 0;
volatile unsigned char exit_debounce = 0;
volatile unsigned char entry_request = 0;

// FUNCTION TO SPLIT COUNT INTO DIGITS
void update_digits(void)
{
    tens = count / 10;
    units = count % 10;
}

//  TIMER0 + EXTERNAL INTERRUPT ISR 
void __interrupt() isr(void)
{
    //  TIMER0 INTERRUPT
    if (INTCONbits.TMR0IF == 1)
    {
        TMR0 = 100;
        INTCONbits.TMR0IF = 0;

        // Gate pulse timing
        if (gate_timer > 0)
        {
            gate_timer--;
            if (gate_timer == 0)
            {
                PORTCbits.RC2 = 0;
            }
        }

        // Entry debounce timing
        if (entry_debounce > 0) entry_debounce--;

        // Exit debounce timing
        if (exit_debounce > 0) exit_debounce--;

        // 7-SEGMENT MULTIPLEXING 
        // Turn both digits OFF first (common cathode: HIGH = OFF)
        PORTCbits.RC0 = 1;
        PORTCbits.RC1 = 1;

        if (display_flag == 0)
        {
            PORTD = seg[tens];
            PORTCbits.RC0 = 0;  // ? Enable tens: LOW = ON for common cathode
            display_flag = 1;
        }
        else
        {
            PORTD = seg[units];
            PORTCbits.RC1 = 0;  // ? Enable units: LOW = ON for common cathode
            display_flag = 0;
        }
    }

    // EXTERNAL INTERRUPT ON RB0/INT 
    if (INTCONbits.INTF == 1) 
    {
        if (entry_debounce == 0)
        {
            entry_request = 1;
            entry_debounce = 25;
        }
        INTCONbits.INTF = 0;
    }
}

//  MAIN PROGRAM 
void main(void)
{
    // I/O SETUP 
    TRISD = 0x00;                   // PORTD output for segments
    TRISCbits.TRISC0 = 0;           // RC0 output for tens digit
    TRISCbits.TRISC1 = 0;           // RC1 output for units digit
    TRISCbits.TRISC2 = 0;           // RC2 output for gate

    TRISBbits.TRISB0 = 1;           // RB0 input for entry sensor
    TRISBbits.TRISB1 = 1;           // RB1 input for exit sensor

    PORTD = 0x00;
    // Initialize digit controls to DISABLED (HIGH for common cathode)
    PORTC = 0x00;
    PORTCbits.RC0 = 1;  // Tens digit OFF
    PORTCbits.RC1 = 1;  // Units digit OFF
    PORTCbits.RC2 = 0;  // Gate OFF

    count = 0;
    update_digits();

    // OPTION REGISTER 
    OPTION_REG = 0x05;  // Pull-ups ON, falling edge INT, Timer0 1:64

    //  TIMER0 SETUP 
    TMR0 = 100;
    INTCONbits.TMR0IF = 0;
    INTCONbits.TMR0IE = 1;

    // EXTERNAL INTERRUPT SETUP 
    INTCONbits.INTF = 0;
    INTCONbits.INTE = 1;

    // GLOBAL INTERRUPTS ON 
    INTCONbits.GIE = 1;

    while (1)
    {
        //  ENTRY SENSOR HANDLING 
        if (entry_request == 1)
        {
            entry_request = 0;

            if (count < 99)
            {
                count++;
                update_digits();
                PORTCbits.RC2 = 1;
                gate_timer = 250;
            }
        }

        //  EXIT SENSOR HANDLING 
        if (PORTBbits.RB1 == 0)
        {
            if (exit_debounce == 0)
            {
                exit_debounce = 25;

                if (count > 0)
                {
                    count--;
                    update_digits();
                    PORTCbits.RC2 = 1;
                    gate_timer = 250;
                }
            }
            while (PORTBbits.RB1 == 0);  // Wait for release
        }
    }
}