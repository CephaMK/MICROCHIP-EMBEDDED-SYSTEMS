//MULTIPLEXING 7-SEGMENT COMMON CATHODE DISPLAYS USING TIMER INTERRUPT 

#include <xc.h>
#define _XTAL_FREQ 20000000

#pragma config FOSC = HS
#pragma config WDTE = OFF
#pragma config PWRTE = OFF
#pragma config BOREN = ON
#pragma config LVP = ON
#pragma config CPD = OFF
#pragma config WRT = OFF
#pragma config CP = OFF

unsigned char number = 00;
unsigned char i, j;

unsigned char seg(unsigned char digit)
{
    unsigned char segment[10] = {
        0x3F, 0x06, 0x5B, 0x4F, 0x66,
        0x6D, 0x7D, 0x07, 0x7F, 0x6F
    };
    return segment[digit];
}

unsigned char timer_count = 0;

void __interrupt() timer_0(void)
{
    if (INTCONbits.TMR0IF == 1)
    {
        if (timer_count >= 2)
        {
            timer_count = 0;
        }
        
        if (timer_count == 0)
        {
                  
            // Display tens
           PORTCbits.RC0 = 1;
           PORTCbits.RC1 = 0;
           PORTB = seg(j);
        
        }          
        else if (timer_count == 1)
        {
           // Display units
           PORTCbits.RC0 = 0;
           PORTCbits.RC1 = 1;
           PORTB = seg(i);
        }
        timer_count++;
        
        INTCONbits.TMR0IF = 0;
    }
}


void main(void) 
{
    TRISB = 0x00;
    TRISC = 0x00;
    
    INTCONbits.GIE = 1;
    INTCONbits.PEIE = 1;
    INTCONbits.TMR0IE = 1;
    
    OPTION_REG = 0x07;      //Pre-scalar = 256
    TMR0 = 59;      //10.09MS delay 
        
    
    while(1)
    {
        i = number / 10;   // tens
        j = number % 10;   // units

        number++;
        if (number == 100)
        {
            number = 0;
        }
        __delay_ms(500);
    }
}