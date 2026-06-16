/*
 * File:   DIGITAL-PASSWORD-DOOR-LOCK-SYSTEM.c
 *
 * User enters a password using keypad
 * Password is displayed on LCD
 * If correct ? unlock (LED/relay ON)
 * If wrong ? error message
 * lockout after 3 attempts
 */


#include <xc.h>
#define _XTAL_FREQ 20000000

// LCD PINS (UNCHANGED)
#define RS PORTDbits.RD0
#define RW PORTDbits.RD1
#define EN PORTDbits.RD2

// CONFIG (UNCHANGED)
#pragma config FOSC = HS
#pragma config WDTE = OFF
#pragma config PWRTE = OFF
#pragma config BOREN = ON
#pragma config LVP = OFF
#pragma config CPD = OFF
#pragma config WRT = OFF
#pragma config CP = OFF

// LCD FUNCTIONS 

void lcd_pulse()
{
    EN = 1;
    __delay_us(2);
    EN = 0;
    __delay_us(50);
}

// ? FIX #2: Removed __delay_ms(5) from start - timing handled elsewhere
void lcd_nibble(unsigned char data)
{
    PORTD = (PORTD & 0x0F) | (data & 0xF0);     // Keep lower bits, send upper nibble
    lcd_pulse();
}

void lcd_cmd(unsigned char cmd)
{
    RS = 0; RW = 0;
    lcd_nibble(cmd & 0xF0);
    lcd_nibble((cmd << 4) & 0xF0);
    __delay_ms(2);
}

void lcd_data(unsigned char data)
{
    RS = 1; RW = 0;
    lcd_nibble(data & 0xF0);
    lcd_nibble((data << 4) & 0xF0);
}

void lcd_str(const char *s)
{
    while(*s) 
    {
        lcd_data(*s++);
    }
}

void lcd_init()
{
    // Initialization sequence (IMPORTANT for 4-bit mode)
    __delay_ms(20);
    lcd_nibble(0x30);
    __delay_ms(5);
    lcd_nibble(0x30);
    __delay_us(200);
    lcd_nibble(0x30);
    __delay_us(200);
    lcd_nibble(0x20);       // Switch to 4-bit mode

    lcd_cmd(0x28);
    lcd_cmd(0x0C);
    lcd_cmd(0x06);
    lcd_cmd(0x01);
}

// KEYPAD 

char keypad()
{
    char keymap[4][3] = {
        {'1','2','3'},
        {'4','5','6'},
        {'7','8','9'},
        {'*','0','#'}
    };

    int col;

    for(col = 0; col < 3; col++)
    {
        // ? FIX #3: Set only column outputs (RB0-RB2) - don't write to input pins
        PORTBbits.RB0 = 1;
        PORTBbits.RB1 = 1;
        PORTBbits.RB2 = 1;

        // Activate one column at a time (LOW)
        if(col == 0) PORTBbits.RB0 = 0;
        if(col == 1) PORTBbits.RB1 = 0;
        if(col == 2) PORTBbits.RB2 = 0;

        // ? FIX #1: ADD DEBOUNCE DELAY - wait for mechanical contacts to settle
        __delay_ms(10);

        // Check rows with proper waiting for key release
        if(!PORTBbits.RB4){ while(!PORTBbits.RB4); return keymap[0][col]; }
        if(!PORTBbits.RB5){ while(!PORTBbits.RB5); return keymap[1][col]; }
        if(!PORTBbits.RB6){ while(!PORTBbits.RB6); return keymap[2][col]; }
        if(!PORTBbits.RB7){ while(!PORTBbits.RB7); return keymap[3][col]; }
    }

    return 0;
}

// MAIN FUNCTION

void main()
{
    char password[4] = {'1','2','3','4'}; // correct PIN
    char entered[4];
    char i = 0;
    char key;
    char attempts = 0;

    // PIN CONFIGURATION (UNCHANGED - exactly as you wrote)
    TRISD = 0x00;           // LCD output
    PORTD = 0x00;
    TRISB = 0xF8;           // keypad (RB0-RB2 outputs, RB4-RB7 inputs)
    PORTCbits.RC2 = 0;
    OPTION_REGbits.nRBPU = 0;  // Enable internal pull-ups on PORTB inputs (RB4-RB7)
    TRISCbits.TRISC2 = 0;   // output (lock control)
    
    // ? FIX #4: Configure PORTA pins as digital I/O (good practice)
    ADCON1 = 0x07;

    lcd_init();

    while(1)
    {
        lcd_cmd(0x01);
        lcd_str("ENTER PIN:");
        __delay_ms(200);
        lcd_cmd(0xC0);

        i = 0;

        // == INPUT 4 DIGITS ==
        while(i < 4)
        {
            key = keypad();
            if(key)
            {
                entered[i] = key;
                lcd_data('*'); // hide password
                i++;
            }
        }

        __delay_ms(300);

        //  CHECK PASSWORD 
        for(i = 0; i < 4; i++)
        {
            if(entered[i] != password[i])
                break;
        }

        if(i == 4)
        {
            // CORRECT PASSWORD
            lcd_cmd(0x01);
            lcd_str("ACCESS GRANTED");
            PORTCbits.RC2 = 1; // unlock
            __delay_ms(3000);
            PORTCbits.RC2 = 0;
            attempts = 0;
        }
        else
        {
            // WRONG PASSWORD
            lcd_cmd(0x01);
            lcd_str("WRONG PIN");
            attempts++;

            if(attempts >= 3)
            {
                lcd_cmd(0xC0);
                lcd_str("LOCKED!");
                __delay_ms(5000);
                attempts = 0;
            }
            else
            {
                __delay_ms(2000);
            }
        }
    }
}
