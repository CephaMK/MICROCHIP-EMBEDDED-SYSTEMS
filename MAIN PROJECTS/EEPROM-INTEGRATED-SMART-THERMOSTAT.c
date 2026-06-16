/*
 * File:   EEPROM-INTEGRATED-SMART-THERMOSTAT.c
 * Optimized with Internal EEPROM to remember the Set Temperature.
 */

#include <xc.h>
#define _XTAL_FREQ 20000000

// LCD CONTROL PINS 
#define RS PORTDbits.RD0
#define RW PORTDbits.RD1
#define EN PORTDbits.RD2

// CONFIG BITS 
#pragma config FOSC = HS, WDTE = OFF, PWRTE = OFF, BOREN = ON, LVP = OFF
#pragma config CPD = OFF, WRT = OFF, CP = OFF

// ==========================================
// PIC16F877A INTERNAL EEPROM DRIVER
// ==========================================
void EEPROM_Write(unsigned char address, unsigned char data) {
    while(EECON1bits.WR);      // Wait for previous write to complete
    EEADR = address;           // Address to write to
    EEDATA = data;             // Data to write
    
    EECON1bits.EEPGD = 0;      // Point to Data EEPROM (not Flash)
    EECON1bits.WREN = 1;       // Enable write operations
    
    INTCONbits.GIE = 0;        // Disable interrupts (Required for unlock)
    EECON2 = 0x55;             // Unlock sequence byte 1
    EECON2 = 0xAA;             // Unlock sequence byte 2
    EECON1bits.WR = 1;         // Start the write
    INTCONbits.GIE = 1;        // Re-enable interrupts
    
    EECON1bits.WREN = 0;       // Disable writes for safety
}

unsigned char EEPROM_Read(unsigned char address) {
    EEADR = address;
    EECON1bits.EEPGD = 0;
    EECON1bits.RD = 1;
    return EEDATA;
}

// GLOBAL VARIABLES
unsigned int adc, temperature, current_temp;
unsigned int set_temp = 30;   
unsigned char key, input_stage = 0, first_digit = 0;

// --- LCD & KEYPAD FUNCTIONS (Identical to your original code) ---
void lcd_pulse(void) { EN = 1; __delay_us(2); EN = 0; __delay_us(50); }
void lcd_write_nibble(unsigned char nibble) { PORTD = (PORTD & 0x0F) | (nibble & 0xF0); lcd_pulse(); }
void lcd_command(unsigned char cmd) { RS = 0; RW = 0; lcd_write_nibble(cmd & 0xF0); lcd_write_nibble(cmd << 4); __delay_ms(2); }
void lcd_data(unsigned char data) { RS = 1; RW = 0; lcd_write_nibble(data & 0xF0); lcd_write_nibble((data << 4) & 0xF0); }
void lcd_str(const char *s) { while (*s) lcd_data(*s++); }
void lcd_initialize(void) {
    __delay_ms(20); RS = 0; RW = 0; EN = 0;
    lcd_write_nibble(0x30); __delay_ms(5); lcd_write_nibble(0x30); __delay_us(200);
    lcd_write_nibble(0x30); __delay_us(200); lcd_write_nibble(0x20); __delay_us(200);
    lcd_command(0x28); lcd_command(0x0C); lcd_command(0x06); lcd_command(0x01);
}
void lcd_put_uint(unsigned int value) {
    unsigned int div = 10000; unsigned char started = 0, digit;
    while (div > 0) { digit = value / div; if (digit != 0 || started || div == 1) { lcd_data(digit + '0'); started = 1; } value = value % div; div /= 10; }
}
void lcd_put_temp(unsigned int value) { lcd_put_uint(value / 100); lcd_data('.'); lcd_data(((value % 100) / 10) + '0'); lcd_data((value % 10) + '0'); }
unsigned int adc_read(void) { __delay_us(20); ADCON0bits.GO_nDONE = 1; while (ADCON0bits.GO_nDONE); return ((unsigned int)ADRESH << 8) | ADRESL; }
unsigned char keypad_getkey(void) {
    const unsigned char keymap[4][3] = {{'1','2','3'},{'4','5','6'},{'7','8','9'},{'*','0','#'}};
    for (unsigned char col = 0; col < 3; col++) {
        PORTBbits.RB0 = 1; PORTBbits.RB1 = 1; PORTBbits.RB2 = 1;
        if (col == 0) PORTBbits.RB0 = 0; else if (col == 1) PORTBbits.RB1 = 0; else PORTBbits.RB2 = 0;
        __delay_ms(2);
        if (PORTBbits.RB4 == 0) { while (PORTBbits.RB4 == 0); return keymap[0][col]; }
        if (PORTBbits.RB5 == 0) { while (PORTBbits.RB5 == 0); return keymap[1][col]; }
        if (PORTBbits.RB6 == 0) { while (PORTBbits.RB6 == 0); return keymap[2][col]; }
        if (PORTBbits.RB7 == 0) { while (PORTBbits.RB7 == 0); return keymap[3][col]; }
    } return 0;
}

void main(void) {
    TRISD = 0x00; TRISCbits.TRISC2 = 0; TRISB = 0xF8; OPTION_REGbits.nRBPU = 0; TRISAbits.TRISA0 = 1;
    PORTD = 0x00; PORTC = 0x00; PORTB = 0x07;
    ADCON1 = 0xBE; ADCON0 = 0x41;

    // --- EEPROM OPTIMIZATION: Load saved temperature on startup ---
    unsigned char saved_temp = EEPROM_Read(0);
    if(saved_temp >= 10 && saved_temp <= 99) {
        set_temp = saved_temp; // Valid temp found in EEPROM
    } else {
        set_temp = 30;         // EEPROM is blank (0xFF) or invalid, use default
    }

    lcd_initialize();
    lcd_command(0x80); lcd_str("THERMOSTAT");
    lcd_command(0xC0); lcd_str("SET:"); lcd_put_uint(set_temp); lcd_data(0xDF); lcd_data('C');
    __delay_ms(1500); lcd_command(0x01);

    while (1) {
        key = keypad_getkey();
        unsigned char old_set_temp = set_temp; // Track if temp changes

        if (key >= '0' && key <= '9') {
            if (input_stage == 0) { first_digit = key - '0'; set_temp = first_digit * 10; input_stage = 1; }
            else { set_temp = (first_digit * 10) + (key - '0'); input_stage = 0; }
        } else if (key == '*') { set_temp = 30; input_stage = 0; }

        // --- EEPROM OPTIMIZATION: Save ONLY if the temperature actually changed ---
        if(set_temp != old_set_temp && set_temp >= 10 && set_temp <= 99) {
            EEPROM_Write(0, set_temp); 
        }

        adc = adc_read();
        temperature = (unsigned int)(((unsigned long)adc * 50000UL) / 1023UL);
        current_temp = temperature / 100;

        if (current_temp >= set_temp) PORTCbits.RC2 = 1; else PORTCbits.RC2 = 0;

        lcd_command(0x80); lcd_str("TEMP:"); lcd_put_temp(temperature); lcd_data(0xDF); lcd_data('C');
        lcd_command(0xC0); lcd_str("SET:"); lcd_put_uint(set_temp); lcd_data(0xDF); lcd_data('C');
        if (PORTCbits.RC2) lcd_str(" ON "); else lcd_str(" OFF");

        __delay_ms(200);
    }
}
