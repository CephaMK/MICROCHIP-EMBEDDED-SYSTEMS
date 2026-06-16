#include <xc.h>
#define _XTAL_FREQ 20000000

// CONFIG
#pragma config FOSC = HS
#pragma config WDTE = OFF
#pragma config PWRTE = OFF
#pragma config BOREN = ON
#pragma config LVP = ON
#pragma config CPD = OFF
#pragma config WRT = OFF
#pragma config CP = OFF

void main(void) 
{
    unsigned char segment[10] = {
        0xC0, // 0
        0xF9, // 1
        0xA4, // 2
        0xB0, // 3
        0x99, // 4
        0x92, // 5
        0x82, // 6
        0xF8, // 7
        0x80, // 8
        0x90  // 9
    };

    int i;

    TRISB = 0x00;

    while(1)
    {
        for(i = 0; i < 10; i++)
        {
            PORTB = segment[i];
            __delay_ms(1000);
        }
    }
}