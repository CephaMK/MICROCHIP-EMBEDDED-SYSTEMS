/*
 * File:   SMART-DIGITAL-THERMOSTAT.c
 * 
 * Measures temperature from an LM35, shows it on an LCD, 
 * lets you set a target with a keypad, and switches a relay/fan output on or off.
 */

#include <xc.h>
#define _XTAL_FREQ 20000000   // 20MHz crystal frequency

// LCD CONTROL PINS 
#define RS PORTDbits.RD0      // Register Select
#define RW PORTDbits.RD1      // Read/Write
#define EN PORTDbits.RD2      // Enable

//  CONFIG BITS 
#pragma config FOSC = HS
#pragma config WDTE = OFF
#pragma config PWRTE = OFF
#pragma config BOREN = ON
#pragma config LVP = OFF
#pragma config CPD = OFF
#pragma config WRT = OFF
#pragma config CP = OFF

//  GLOBAL VARIABLES
unsigned int adc;             // Raw ADC value
unsigned int temperature;     // Temperature in scaled form (xx.xx)
unsigned int set_temp = 30;   // Default set temperature (30°C)
unsigned int current_temp;    // Integer temperature for comparison

unsigned char key;            // Keypad input
unsigned char input_stage = 0; // Tracks digit entry stage
unsigned char first_digit = 0; // Stores first digit of input

//  LCD LOW LEVEL 

// Generates enable pulse
void lcd_pulse(void)
{
    EN = 1;
    __delay_us(2);
    EN = 0;
    __delay_us(50);
}

// Sends 4-bit data to LCD (upper nibble)
void lcd_write_nibble(unsigned char nibble)
{
    PORTD = (PORTD & 0x0F) | (nibble & 0xF0); // Keep lower bits, send upper nibble
    lcd_pulse();
}

// Sends command to LCD
void lcd_command(unsigned char cmd)
{
    RS = 0;   // Instruction mode
    RW = 0;   // Write mode

    lcd_write_nibble(cmd & 0xF0);   // Send upper nibble
    lcd_write_nibble(cmd << 4);     // Send lower nibble

    __delay_ms(2);
}

// Sends data (character) to LCD
void lcd_data(unsigned char data)
{
    RS = 1;   // Data mode
    RW = 0;   // Write mode

    lcd_write_nibble(data & 0xF0);
    lcd_write_nibble((data << 4) & 0xF0);
}

// Prints string to LCD
void lcd_str(const char *s)
{
    while (*s)
    {
        lcd_data(*s++);   // Send each character
    }
}

//LCD INITIALIZATION
void lcd_initialize(void)
{
    __delay_ms(20);   // Wait for LCD to power up

    RS = 0;
    RW = 0;
    EN = 0;

    // Initialization sequence (IMPORTANT for 4-bit mode)
    lcd_write_nibble(0x30);
    __delay_ms(5);

    lcd_write_nibble(0x30);
    __delay_us(200);

    lcd_write_nibble(0x30);
    __delay_us(200);

    lcd_write_nibble(0x20);   // Switch to 4-bit mode
    __delay_us(200);

    lcd_command(0x28); // 4-bit, 2 lines
    lcd_command(0x0C); // Display ON, cursor OFF
    lcd_command(0x06); // Auto increment cursor
    lcd_command(0x01); // Clear display
}

//          PRINT NUMBER FUNCTIONS

// Prints integer number
void lcd_put_uint(unsigned int value)
{
    unsigned int div = 10000;
    unsigned char started = 0;
    unsigned char digit;

    while (div > 0)
    {
        digit = value / div;

        // Skip leading zeros
        if (digit != 0 || started || div == 1)
        {
            lcd_data(digit + '0');
            started = 1;
        }

        value = value % div;
        div /= 10;
    }
}

// Prints temperature in format XX.XX
void lcd_put_temp(unsigned int value)
{
    unsigned int whole = value / 100; // Integer part
    unsigned char frac = value % 100; // Decimal part

    lcd_put_uint(whole);

    lcd_data('.');

    lcd_data((frac / 10) + '0');
    lcd_data((frac % 10) + '0');
}

//           ADC FUNCTION 

// Reads analog value from AN0
unsigned int adc_read(void)
{
    __delay_us(20);                 // Acquisition time
    ADCON0bits.GO_nDONE = 1;        // Start conversion

    while (ADCON0bits.GO_nDONE);    // Wait until done

    return ((unsigned int)ADRESH << 8) | ADRESL; // Combine result
}

//  KEYPAD FUNCTION 

// Scans keypad and returns pressed key
unsigned char keypad_getkey(void)
{
    const unsigned char keymap[4][3] = {
        {'1','2','3'},
        {'4','5','6'},
        {'7','8','9'},
        {'*','0','#'}
    };

    unsigned char col;      //COLUMN

    for (col = 0; col < 3; col++)
    {
        // Set all columns HIGH
        PORTBbits.RB0 = 1;
        PORTBbits.RB1 = 1;
        PORTBbits.RB2 = 1;

        // Activate one column at a time (LOW)
        if (col == 0) PORTBbits.RB0 = 0;
        else if (col == 1) PORTBbits.RB1 = 0;
        else PORTBbits.RB2 = 0;

        __delay_ms(2); // Debounce delay

        // Check rows
        if (PORTBbits.RB4 == 0) { while (PORTBbits.RB4 == 0); return keymap[0][col]; }
        if (PORTBbits.RB5 == 0) { while (PORTBbits.RB5 == 0); return keymap[1][col]; }
        if (PORTBbits.RB6 == 0) { while (PORTBbits.RB6 == 0); return keymap[2][col]; }
        if (PORTBbits.RB7 == 0) { while (PORTBbits.RB7 == 0); return keymap[3][col]; }
    }

    return 0; // No key pressed
}

//  MAIN FUNCTION 
void main(void)
{
    // PORT CONFIGURATION 
    TRISD = 0x00;           // LCD output
    TRISCbits.TRISC2 = 0;   // Output for relay/fan
    TRISB = 0xF8;           // Keypad (RB0?RB2 outputs, RB4?RB7 inputs)
    
    OPTION_REGbits.nRBPU = 0;  // Enable internal pull-ups on PORTB inputs (RB4-RB7)
    TRISAbits.TRISA0 = 1;   // Analog input (LM35)

    PORTD = 0x00;
    PORTC = 0x00;
    PORTB = 0x07;

    // ADC SETUP 
    ADCON1 = 0xBE;   // Configure AN0 as analog
    ADCON0 = 0x41;   // Turn ON ADC

    //  LCD INIT 
    lcd_initialize();

    lcd_command(0x80);
    lcd_str("THERMOSTAT");

    lcd_command(0xC0);
    lcd_str("SET:30C");

    __delay_ms(1000);
    lcd_command(0x01);

    // MAIN LOOP 
    while (1)
    {
        // KEYPAD INPUT 
        key = keypad_getkey();

        if (key >= '0' && key <= '9')
        {
            if (input_stage == 0)
            {
                // First digit
                first_digit = key - '0';
                set_temp = first_digit * 10;
                input_stage = 1;
            }
            else
            {
                // Second digit
                set_temp = (first_digit * 10) + (key - '0');
                input_stage = 0;
            }
        }
        else if (key == '*')
        {
            // Reset temperature
            set_temp = 30;
            input_stage = 0;
        }

        //TEMPERATURE READING
        adc = adc_read();

        // Convert ADC to temperature (LM35: 10mV per °C)
        temperature = (unsigned int)(((unsigned long)adc * 50000UL) / 1023UL);

        current_temp = temperature / 100;

        // CONTROL LOGIC 
        
        if (current_temp >= set_temp)
        {
            PORTCbits.RC2 = 1;   // Turn ON fan/relay
        }
        else
        {
            PORTCbits.RC2 = 0;   // Turn OFF
        }
        

        // LCD DISPLAY 
        lcd_command(0x80);
        lcd_str("TEMP:");
        lcd_put_temp(temperature);
        lcd_data(0xDF);   // Degree symbol
        lcd_data('C');

        lcd_command(0xC0);
        lcd_str("SET:");
        lcd_put_uint(set_temp);
        lcd_data(0xDF);
        lcd_data('C');

        if (PORTCbits.RC2)
            lcd_str(" ON ");
        else
            lcd_str(" OFF");

        __delay_ms(200);
    }
}