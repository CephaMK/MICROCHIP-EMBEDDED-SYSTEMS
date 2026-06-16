/*
 * File:   DIGITAL-PASSWORD-DOOR-LOCK-SYSTEM.c
 * Optimized with Internal EEPROM for persistent password storage.
 */

#include <xc.h>
#define _XTAL_FREQ 20000000

#define RS PORTDbits.RD0
#define RW PORTDbits.RD1
#define EN PORTDbits.RD2

#pragma config FOSC = HS, WDTE = OFF, PWRTE = OFF, BOREN = ON, LVP = OFF
#pragma config CPD = OFF, WRT = OFF, CP = OFF

// ==========================================
// PIC16F877A INTERNAL EEPROM DRIVER
// ==========================================
void EEPROM_Write(unsigned char address, unsigned char data) {
    while(EECON1bits.WR); EEADR = address; EEDATA = data;
    EECON1bits.EEPGD = 0;  EECON1bits.WREN = 1;
    INTCONbits.GIE = 0; EECON2 = 0x55; EECON2 = 0xAA; EECON1bits.WR = 1; INTCONbits.GIE = 1;
    EECON1bits.WREN = 0;
}
unsigned char EEPROM_Read(unsigned char address) {
    EEADR = address;  EECON1bits.EEPGD = 0; EECON1bits.RD = 1; return EEDATA;
}

// EEPROM Addresses
#define EEPROM_FLAG 10  // Address to check if initialized (Stores 0xA5)
#define EEPROM_PASS_1 0
#define EEPROM_PASS_2 1
#define EEPROM_PASS_3 2
#define EEPROM_PASS_4 3

// --- LCD & KEYPAD FUNCTIONS ---
void lcd_pulse() { EN = 1; __delay_us(2); EN = 0; __delay_us(50); }
void lcd_nibble(unsigned char data) { PORTD = (PORTD & 0x0F) | (data & 0xF0); lcd_pulse(); }
void lcd_cmd(unsigned char cmd) { RS = 0; RW = 0; lcd_nibble(cmd & 0xF0); lcd_nibble((cmd << 4) & 0xF0); __delay_ms(2); }
void lcd_data(unsigned char data) { RS = 1; RW = 0; lcd_nibble(data & 0xF0); lcd_nibble((data << 4) & 0xF0); }
void lcd_str(const char *s) { while(*s) lcd_data(*s++); }
void lcd_init() {
    __delay_ms(20); lcd_nibble(0x30); __delay_ms(5); lcd_nibble(0x30); __delay_us(200);
    lcd_nibble(0x30); __delay_us(200); lcd_nibble(0x20);
    lcd_cmd(0x28); lcd_cmd(0x0C); lcd_cmd(0x06); lcd_cmd(0x01);
}
char keypad() {
    char keymap[4][3] = {{'1','2','3'},{'4','5','6'},{'7','8','9'},{'*','0','#'}};
    for(int col = 0; col < 3; col++) {
        PORTBbits.RB0 = 1; PORTBbits.RB1 = 1; PORTBbits.RB2 = 1;
        if(col == 0) PORTBbits.RB0 = 0; if(col == 1) PORTBbits.RB1 = 0; if(col == 2) PORTBbits.RB2 = 0;
        __delay_ms(10);
        if(!PORTBbits.RB4){ while(!PORTBbits.RB4); return keymap[0][col]; }
        if(!PORTBbits.RB5){ while(!PORTBbits.RB5); return keymap[1][col]; }
        if(!PORTBbits.RB6){ while(!PORTBbits.RB6); return keymap[2][col]; }
        if(!PORTBbits.RB7){ while(!PORTBbits.RB7); return keymap[3][col]; }
    } return 0;
}

void main() {
    char password[4];
    char entered[4];
    char i = 0, key, attempts = 0;

    TRISD = 0x00; PORTD = 0x00; TRISB = 0xF8; OPTION_REGbits.nRBPU = 0;
    TRISCbits.TRISC2 = 0; PORTCbits.RC2 = 0; ADCON1 = 0x07;
    lcd_init();

    // --- EEPROM OPTIMIZATION: First-time setup & Password Loading ---
    if(EEPROM_Read(EEPROM_FLAG) != 0xA5) {
        // First time running! Save default password '1234'
        EEPROM_Write(EEPROM_PASS_1, '1');
        EEPROM_Write(EEPROM_PASS_2, '2');
        EEPROM_Write(EEPROM_PASS_3, '3');
        EEPROM_Write(EEPROM_PASS_4, '4');
        EEPROM_Write(EEPROM_FLAG, 0xA5); // Set flag so we don't overwrite next time
    }
    
    // Load password from EEPROM into RAM
    password[0] = EEPROM_Read(EEPROM_PASS_1);
    password[1] = EEPROM_Read(EEPROM_PASS_2);
    password[2] = EEPROM_Read(EEPROM_PASS_3);
    password[3] = EEPROM_Read(EEPROM_PASS_4);

    while(1) {
        lcd_cmd(0x01); lcd_str("ENTER PIN:"); __delay_ms(200); lcd_cmd(0xC0);
        i = 0;
        while(i < 4) {
            key = keypad();
            if(key >= '0' && key <= '9') { entered[i] = key; lcd_data('*'); i++; }
        }
        __delay_ms(300);

        for(i = 0; i < 4; i++) { if(entered[i] != password[i]) break; }

        if(i == 4) {
            lcd_cmd(0x01); lcd_str("ACCESS GRANTED");
            PORTCbits.RC2 = 1; __delay_ms(3000); PORTCbits.RC2 = 0;
            attempts = 0;
            
            // --- NEW FEATURE: Allow user to change PIN ---
            lcd_cmd(0x01); lcd_str("Press # to chng"); lcd_cmd(0xC0); lcd_str("PIN or wait...");
            
            char change_mode = 0;
            for(int t = 0; t < 30; t++) { // Wait 3 seconds
                if(keypad() == '#') { change_mode = 1; break; }
                __delay_ms(100);
            }
            
            if(change_mode) {
                lcd_cmd(0x01); lcd_str("ENTER NEW PIN:"); lcd_cmd(0xC0);
                char new_pass[4]; char j = 0;
                while(j < 4) {
                    char k = keypad();
                    if(k >= '0' && k <= '9') { new_pass[j] = k; lcd_data('*'); j++; }
                }
                
                // Save new password to EEPROM
                EEPROM_Write(EEPROM_PASS_1, new_pass[0]);
                EEPROM_Write(EEPROM_PASS_2, new_pass[1]);
                EEPROM_Write(EEPROM_PASS_3, new_pass[2]);
                EEPROM_Write(EEPROM_PASS_4, new_pass[3]);
                
                // Update RAM variable immediately
                for(j = 0; j < 4; j++) password[j] = new_pass[j];
                
                lcd_cmd(0x01); lcd_str("PIN CHANGED!"); __delay_ms(2000);
            }
        } else {
            lcd_cmd(0x01); lcd_str("WRONG PIN"); attempts++;
            if(attempts >= 3) {
                lcd_cmd(0xC0); lcd_str("LOCKED!"); __delay_ms(5000); attempts = 0;
            } else { __delay_ms(2000); }
        }
    }
}