/*
 * File:   TIMER INTERRUPT.c
 */

#include <xc.h>
#define _XTAL_FREQ 20000000
//CONFIG

#pragma config FOSC = HS        // Oscillator Selection bits (HS oscillator)
#pragma config WDTE = OFF       // Watchdog Timer Enable bit (WDT disabled)
#pragma config PWRTE = OFF      // Power-up Timer Enable bit (PWRT disabled)
#pragma config BOREN = ON       // Brown-out Reset Enable bit (BOR enabled)
#pragma config LVP = ON         // Low-Voltage (Single-Supply) In-Circuit Serial Programming Enable bit (RB3/PGM pin has PGM function; low-voltage programming enabled)
#pragma config CPD = OFF        // Data EEPROM Memory Code Protection bit (Data EEPROM code protection off)
#pragma config WRT = OFF        // Flash Program Memory Write Enable bits (Write protection off; all program memory may be written to by EECON control)
#pragma config CP = OFF         // Flash Program Memory Code Protection bit (Code protection off)


unsigned char timer_count = 0;

void __interrupt() timer_0(void)
{
    if (INTCONbits.TMR0IF == 1)
    {
        TMR0 = 59;      //10.09MS delay 
        timer_count++;
        INTCONbits.TMR0IF = 0;
    }
}

void main(void) 
{
    INTCONbits.GIE = 1;
    INTCONbits.PEIE = 1;
    INTCONbits.TMR0IE = 1;
    
    OPTION_REG = 0x07;
        
    TRISB = 0;
    while(1)
    {
        if (timer_count == 0)
        {
            PORTBbits.RB0 = 1;
        }
        if (timer_count == 100)
        {
            PORTBbits.RB0 = 0;
        }
        if (timer_count  == 200)
        {
            timer_count = 0;
        }
        
    }
    return;
}
