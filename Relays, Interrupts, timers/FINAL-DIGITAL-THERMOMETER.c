/*
 * File:   FINAL-DIGITAL-THERMOMETER.c
 * 
 * LM35 Temperature sensor in conjuction with an ADC to implement the
 * working of a digital thermometer
 */


#include <xc.h>
#define _XTAL_FREQ 20000000
#define RS PORTDbits.RD0
#define RW PORTDbits.RD1
#define EN PORTDbits.RD2

#pragma config FOSC = HS
#pragma config WDTE = OFF
#pragma config PWRTE = OFF
#pragma config BOREN = ON
#pragma config LVP = ON
#pragma config CPD = OFF
#pragma config WRT = OFF
#pragma config CP = OFF

unsigned int a, b, c, d, e, f;
unsigned int temp, adc;
float adc1, temperature; 

//Passing DATA to the LCD
void lcd_data(unsigned char b)
{
    PORTC = b;
    RS = 1;
    RW = 0;
    EN = 1;
    __delay_ms(5);
    EN = 0;
}

//Passing INSTRUCTIONS to the LCD
void lcd_command(unsigned char a)
{
    PORTC = a;
    RS = 0;
    RW = 0;
    EN = 1;
    __delay_ms(5);
    EN = 0;
}

// FIXED FUNCTION
void str(const unsigned char *d,unsigned char n)
{
    unsigned char i;
    for (i = 0; i < n; i++)
    {
        lcd_data(d[i]);   // ? FIXED
    }
}

void lcd_initialize()
{
    lcd_command(0x38);
    lcd_command(0x06);
    lcd_command(0x0C);
    lcd_command(0x01);
}

void __interrupt() adc_conv()
{
    if (PIR1bits.ADIF == 1)
    {
        adc = (ADRESH << 8);
        adc = adc + ADRESL;
        PIR1bits.ADIF = 0;
    }
}

void main(void)
{
    INTCONbits.GIE = 1;
    INTCONbits.PEIE = 1;
    PIE1bits.ADIE = 1;
    
    TRISD = 0X00;
    TRISC = 0X00;
    TRISAbits.TRISA0 = 1;   // ? ADC input

    PORTC = PORTD = 0X00;
    lcd_initialize();
    
    lcd_command(0x80);
    str("TEMP SENSOR:", 12);
    
    ADCON0 = 0X41;   // AN0 selected, ADC ON
    ADCON1 = 0X8E;   // AN0 analog, Vref = Vdd

    while(1)
    {
        ADCON0bits.GO = 1;          // FIXED start conversion
        while(ADCON0bits.GO);       //  wait for completion

        adc = (ADRESH << 8) + ADRESL;   //  ensure updated value

        adc1 = adc * 0.004887;     // FIXED conversion
        temperature = adc1 * 100;  // LM35: 10mV per °C

        lcd_command(0xC3);
        
        temp = (int)(temperature * 100);   // e.g. 32.45 ? 3245

        a = temp / 100;        // integer part ? 32
        b = temp % 100;        // decimal part ? 45

        c = a / 10;            // tens ? 3
        d = a % 10;            // units ? 2

        e = b / 10;            // first decimal ? 4
        f = b % 10;            // second decimal ? 5

        lcd_data(c + 0x30);
        lcd_data(d + 0x30);
        lcd_data('.');         // decimal point
        lcd_data(e + 0x30);
        lcd_data(f + 0x30);

        __delay_ms(500);
    }
}