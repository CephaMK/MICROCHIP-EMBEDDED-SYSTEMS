//COMMON CATHODE DISPLAY

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

void main(void) 
{
    TRISB = 0x00;
    while(1)
    {
        PORTB =  0x3F;
        __delay_ms(1000);
        PORTB = 0X06;
        __delay_ms(1000);
        PORTB = 0X5B;
        __delay_ms(1000);
        PORTB = 0X4F;
        __delay_ms(1000);
        PORTB = 0X66;
        __delay_ms(1000);
        PORTB = 0X6D;
        __delay_ms(1000);
        PORTB = 0X7D;
        __delay_ms(1000);
        PORTB = 0X07;
        __delay_ms(1000);
        PORTB = 0X7F;
        __delay_ms(1000);
        PORTB = 0X6F;
        __delay_ms(1000);
    }
    return;
}
