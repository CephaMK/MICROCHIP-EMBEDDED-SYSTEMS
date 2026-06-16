//LIQUID CRYSTAL DISPLAY 8-BIT MODE

#include <xc.h>
#define _XTAL_FREQ 20000000
#define RS PORTDbits.RD0
#define RW PORTDbits.RD1
#define EN PORTDbits.RD2


//CONFIG
#pragma config FOSC = HS        // Oscillator Selection bits (HS oscillator)
#pragma config WDTE = OFF       // Watchdog Timer Enable bit (WDT disabled)
#pragma config PWRTE = OFF      // Power-up Timer Enable bit (PWRT disabled)
#pragma config BOREN = ON       // Brown-out Reset Enable bit (BOR enabled)
#pragma config LVP = ON         // Low-Voltage (Single-Supply) In-Circuit Serial Programming Enable bit (RB3/PGM pin has PGM function; low-voltage programming enabled)
#pragma config CPD = OFF        // Data EEPROM Memory Code Protection bit (Data EEPROM code protection off)
#pragma config WRT = OFF        // Flash Program Memory Write Enable bits (Write protection off; all program memory may be written to by EECON control)
#pragma config CP = OFF         // Flash Program Memory Code Protection bit (Code protection off)

//Passing DATA to the LCD
void lcd_data(unsigned char data)
{
    PORTC = data;
    RS = 1;
    RW = 0;
    EN = 1;
    __delay_ms(5);
    EN = 0;
    
}

//Passing INSTRUCTIONS to the LCD
void lcd_command(unsigned char cmd)
{
    PORTC = cmd;
    RS = 0;
    RW = 0;
    EN = 1;
    __delay_ms(5);
    EN = 0;
    
}

//letters and digits to be printed
void lcd_string(const unsigned char *str,unsigned char num)
{
    unsigned char i;
    for (i = 0; i < num; i++)
    {
        lcd_data(str[i]);
    }
}

void lcd_initialize()
{
    lcd_command(0x38);       //sets LCD to 16x2 model
    lcd_command(0x06);      //auto increments LCD to next element when the current element is printed
    lcd_command(0x0C);      //turns LCD on and cursor off
    lcd_command(0x01);      //Clear screen
}


void main(void) 
{
    TRISC = 0X00;
    TRISD = 0X00;
    lcd_initialize();
    
    while(1)
    {
        lcd_command(0x83);
        lcd_string("EMBEDDED", 8);
        lcd_command(0XC3);
        lcd_string("SYSTEMS", 7);
    }
    
    return;
}
