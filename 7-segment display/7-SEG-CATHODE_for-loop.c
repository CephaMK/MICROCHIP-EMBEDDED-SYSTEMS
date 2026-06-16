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
        0x3F, // 0
        0x06, // 1
        0x5B, // 2
        0x4F, // 3
        0x66, // 4
        0x6D, // 5
        0x7D, // 6
        0x07, // 7
        0x7F, // 8
        0x6F  // 9
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

