//EXTERNAL INTERRUPT

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


void __interrupt() external_isr(void)       //interrupt function
{
    if(INTCONbits.INTF == 1)       //Checking if external interrupt bit (INTF) is enabled 
    {
        PORTCbits.RC0 = PORTCbits.RC0 ^ 1;     //Toggling of RC0 as per interrupt
        INTCONbits.INTF = 0;
    }
}

void main(void) 
{
    INTCONbits.GIE = 1;         //Enabling unmasked interrupts bit (global interrupt enable)
    INTCONbits.PEIE = 1;        //Enabling unmasked peripheral interrupt bit (peripheral interrupt enable bit)
    INTCONbits.INTE = 1;        //Enabling external interrupt enable pin
    
    TRISCbits.TRISC0 = 0;
    TRISBbits.TRISB0 = 1;
    OPTION_REGbits.INTEDG = 1;      //interrupt will occur on the rising edge of RBO/INT pin (FROM LOW TO HIGH)
    
    while(1)
    {
        ;
    }
    return;
}
