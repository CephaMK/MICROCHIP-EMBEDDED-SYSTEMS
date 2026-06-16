/*
 * File:   SMART-HOME-AUTOMATION-SYSTEM.c
 * Description: Fully integrated smart home with 6 subsystems
 * Hardware: PIC16F877A @ 20MHz, 20x4 LCD, 4x3 Keypad
 * 
 * SUBSYSTEMS INCLUDED:
 * 1. Smart Lighting (LDR on RA0, Relay on RC0)
 * 2. Smart Garage (Sensors on RB3/RE0, Gate on RC1, Count on LCD)
 * 3. Smart Thermostat (LM35 on RA1, Fan on RC2)
 * 4. Security System (PIR on RA2, Buzzer on RC7)
 * 5. Digital Clock (DS1307 I2C on RC3/RC4)
 * 6. Smart Door Lock (Lock on RC5, LED on RC6, Password in EEPROM)
 */

#define _XTAL_FREQ 20000000
#include <xc.h>

// CONFIG BITS
#pragma config FOSC = HS, WDTE = OFF, PWRTE = OFF, BOREN = ON, LVP = OFF
#pragma config CPD = OFF, WRT = OFF, CP = OFF

// ============================================================================
// HARDWARE DEFINITIONS
// ============================================================================
// LCD (4-bit mode on PORTD)
#define LCD_RS_PIN   PORTDbits.RD0
#define LCD_RW_PIN   PORTDbits.RD1
#define LCD_EN_PIN   PORTDbits.RD2

// Outputs
#define LIGHT_RELAY     PORTCbits.RC0  // 1. Smart Lighting
#define GARAGE_GATE     PORTCbits.RC1  // 2. Smart Garage
#define THERMO_FAN      PORTCbits.RC2  // 3. Smart Thermostat
#define ALARM_BUZZER    PORTCbits.RC7  // 4. Security System (Moved from RC3 to avoid I2C conflict)
#define DOOR_LOCK       PORTCbits.RC5  // 6. Smart Door Lock
#define DOOR_LED        PORTCbits.RC6  // 6. Smart Door Lock

// Inputs
#define ENTRY_SENSOR    PORTBbits.RB3  // 2. Garage Entry (External Interrupt)
#define EXIT_SENSOR     PORTEbits.RE0  // 2. Garage Exit
#define PIR_SENSOR      PORTAbits.RA2  // 4. Security PIR

// ============================================================================
// GLOBAL VARIABLES
// ============================================================================
// 5. Digital Clock
unsigned char rtc_sec=0, rtc_min=0, rtc_hr=0, rtc_date=0, rtc_month=0, rtc_yr=0;

// 3. Thermostat
unsigned int current_temp = 2500, set_temp = 28;

// 2. Garage
unsigned char garage_count = 0, garage_entry_req = 0;
unsigned char entry_debounce = 0, exit_debounce = 0;

// 4. Security
unsigned char pir_debounce = 0, security_armed = 0, alarm_active = 0;

// 6. Door Lock
unsigned char door_locked = 1, lock_timer = 0;
unsigned char password[4];
#define EEPROM_FLAG  10
#define EEPROM_PASS1 0

// 1. Lighting
unsigned char light_override = 0;
unsigned int ldr_value = 0;

// Menu & Display
unsigned char lcd_update_counter = 0, current_menu = 0;

// ============================================================================
// FUNCTION PROTOTYPES
// ============================================================================
void lcd_init(void); void lcd_cmd(unsigned char cmd); void lcd_data(unsigned char data);
void lcd_str(const char *s); void lcd_goto(unsigned char pos); void lcd_clear(void);
void lcd_put_uint(unsigned int value); void lcd_put_2digit(unsigned char value);
unsigned char keypad_getkey(void); unsigned char keypad_wait(void);
unsigned int adc_read(unsigned char channel);
void rtc_write_time(unsigned char h, unsigned char m, unsigned char s, unsigned char d, unsigned char mo, unsigned char y);
void rtc_read_time(void);
unsigned char menu_get_number(unsigned char max_val); // Added to fix C90 compilation error

// ============================================================================
// INTERNAL EEPROM DRIVER
// ============================================================================
void EEPROM_Write(unsigned char address, unsigned char data) {
    while(EECON1bits.WR); EEADR = address; EEDATA = data;
    EECON1bits.EEPGD = 0; EECON1bits.WREN = 1;
    INTCONbits.GIE = 0; EECON2 = 0x55; EECON2 = 0xAA; EECON1bits.WR = 1; INTCONbits.GIE = 1;
    EECON1bits.WREN = 0;
}
unsigned char EEPROM_Read(unsigned char address) {
    EEADR = address; EECON1bits.EEPGD = 0; EECON1bits.RD = 1; return EEDATA;
}

// ============================================================================
// LCD DRIVER (4-bit mode)
// ============================================================================
void lcd_pulse(void) { LCD_EN_PIN = 1; __delay_us(2); LCD_EN_PIN = 0; __delay_us(50); }

void lcd_write_nibble(unsigned char nibble) {
    // PORTD is now completely safe because the Timer0 ISR no longer touches it!
    PORTD = (PORTD & 0x0F) | (nibble & 0xF0);
    lcd_pulse();
}

void lcd_cmd(unsigned char cmd) {
    LCD_RS_PIN = 0; LCD_RW_PIN = 0;
    lcd_write_nibble(cmd & 0xF0);
    lcd_write_nibble((unsigned char)(cmd << 4));
    if(cmd == 0x01 || cmd == 0x02) __delay_ms(2); else __delay_us(50);
}

void lcd_data(unsigned char data) {
    LCD_RS_PIN = 1; LCD_RW_PIN = 0;
    lcd_write_nibble(data & 0xF0);
    lcd_write_nibble((unsigned char)(data << 4));
    __delay_us(50);
}

void lcd_str(const char *s) { while(*s) lcd_data(*s++); }
void lcd_goto(unsigned char pos) { lcd_cmd(0x80 | pos); }
void lcd_clear(void) { lcd_cmd(0x01); __delay_ms(2); }

void lcd_init(void) {
    __delay_ms(50); LCD_RS_PIN = 0; LCD_RW_PIN = 0; LCD_EN_PIN = 0;
    lcd_write_nibble(0x30); __delay_ms(5); lcd_write_nibble(0x30); __delay_us(200);
    lcd_write_nibble(0x30); __delay_us(200); lcd_write_nibble(0x20); __delay_us(200);
    lcd_cmd(0x28); lcd_cmd(0x0C); lcd_cmd(0x06); lcd_cmd(0x01);
}

void lcd_put_uint(unsigned int value) {
    unsigned int div = 10000; unsigned char started = 0, digit;
    while(div > 0) {
        digit = (unsigned char)(value / div);
        if(digit != 0 || started || div == 1) { lcd_data(digit + '0'); started = 1; }
        value = value % div; div /= 10;
    }
}
void lcd_put_2digit(unsigned char value) { lcd_data((value / 10) + '0'); lcd_data((value % 10) + '0'); }

// ============================================================================
// KEYPAD & ADC DRIVERS
// ============================================================================
unsigned char keypad_getkey(void) {
    const unsigned char keymap[4][3] = {{'1','2','3'}, {'4','5','6'}, {'7','8','9'}, {'*','0','#'}};
    for(unsigned char col = 0; col < 3; col++) {
        PORTBbits.RB0 = 1; PORTBbits.RB1 = 1; PORTBbits.RB2 = 1;
        if(col == 0) PORTBbits.RB0 = 0; else if(col == 1) PORTBbits.RB1 = 0; else PORTBbits.RB2 = 0;
        __delay_ms(2);
        if(PORTBbits.RB4 == 0) { while(PORTBbits.RB4 == 0){} return keymap[0][col]; }
        if(PORTBbits.RB5 == 0) { while(PORTBbits.RB5 == 0){} return keymap[1][col]; }
        if(PORTBbits.RB6 == 0) { while(PORTBbits.RB6 == 0){} return keymap[2][col]; }
        if(PORTBbits.RB7 == 0) { while(PORTBbits.RB7 == 0){} return keymap[3][col]; }
    } return 0;
}
unsigned char keypad_wait(void) { unsigned char k; while(1) { k = keypad_getkey(); if(k) return k; } }

unsigned int adc_read(unsigned char channel) {
    ADCON0 = (ADCON0 & 0xC7) | (unsigned char)(channel << 3);
    __delay_us(20); ADCON0bits.GO_nDONE = 1; while(ADCON0bits.GO_nDONE){}
    return ((unsigned int)ADRESH << 8) | ADRESL;
}

// ============================================================================
// DS1307 RTC DRIVER (I2C)
// ============================================================================
void i2c_wait(void) { while((SSPCON2 & 0x1F) || (SSPSTATbits.R_W)); }
void i2c_start(void) { i2c_wait(); SSPCON2bits.SEN = 1; while(SSPCON2bits.SEN); }
void i2c_restart(void) { i2c_wait(); SSPCON2bits.RSEN = 1; while(SSPCON2bits.RSEN); }
void i2c_stop(void) { i2c_wait(); SSPCON2bits.PEN = 1; while(SSPCON2bits.PEN); }
unsigned char i2c_write(unsigned char data) { i2c_wait(); SSPBUF = data; i2c_wait(); return !SSPCON2bits.ACKSTAT; }
unsigned char i2c_read_ack(void) {
    unsigned char data; i2c_wait(); SSPCON2bits.RCEN = 1; while(!SSPSTATbits.BF); data = SSPBUF;
    i2c_wait(); SSPCON2bits.ACKDT = 0; SSPCON2bits.ACKEN = 1; while(SSPCON2bits.ACKEN); return data;
}
unsigned char i2c_read_nack(void) {
    unsigned char data; i2c_wait(); SSPCON2bits.RCEN = 1; while(!SSPSTATbits.BF); data = SSPBUF;
    i2c_wait(); SSPCON2bits.ACKDT = 1; SSPCON2bits.ACKEN = 1; while(SSPCON2bits.ACKEN); return data;
}
unsigned char dec_to_bcd(unsigned char val) { return (unsigned char)(((val / 10) << 4) | (val % 10)); }
unsigned char bcd_to_dec(unsigned char val) { return (unsigned char)(((val >> 4) * 10) + (val & 0x0F)); }

void rtc_write_time(unsigned char h, unsigned char m, unsigned char s, unsigned char d, unsigned char mo, unsigned char y) {
    i2c_start(); i2c_write(0xD0); i2c_write(0x00);
    i2c_write(dec_to_bcd(s) & 0x7F); i2c_write(dec_to_bcd(m)); i2c_write(dec_to_bcd(h));
    i2c_write(dec_to_bcd(1)); i2c_write(dec_to_bcd(d)); i2c_write(dec_to_bcd(mo)); i2c_write(dec_to_bcd(y));
    i2c_stop();
}

void rtc_read_time(void) {
    i2c_start(); i2c_write(0xD0); i2c_write(0x00); i2c_restart(); i2c_write(0xD1);
    rtc_sec   = i2c_read_ack(); rtc_min   = i2c_read_ack(); rtc_hr    = i2c_read_ack();
    i2c_read_ack(); rtc_date  = i2c_read_ack(); rtc_month = i2c_read_ack(); rtc_yr    = i2c_read_nack();
    i2c_stop();
    rtc_sec   = bcd_to_dec(rtc_sec & 0x7F);
    rtc_min   = bcd_to_dec(rtc_min);
    rtc_hr    = bcd_to_dec(rtc_hr & 0x3F); // Mask out 12/24h mode bit
    rtc_date  = bcd_to_dec(rtc_date); rtc_month = bcd_to_dec(rtc_month); rtc_yr    = bcd_to_dec(rtc_yr);
}

// ============================================================================
// TIMER0 ISR (Handles Timers, Debounce, and Garage Entry Interrupt)
// NOTE: PORTD is NO LONGER touched here, guaranteeing LCD stability!
// ============================================================================
void __interrupt() isr(void) {
    if(INTCONbits.TMR0IF) {
        TMR0 = 100; 
        INTCONbits.TMR0IF = 0;
        
        if(garage_entry_req > 0) garage_entry_req--;
        if(entry_debounce > 0) entry_debounce--;
        if(exit_debounce > 0) exit_debounce--;
        if(pir_debounce > 0) pir_debounce--;
        
        if(lock_timer > 0) { 
            lock_timer--; 
            if(lock_timer == 0) { DOOR_LOCK = 1; DOOR_LED = 0; door_locked = 1; } 
        }
    }
    
    // External Interrupt for Garage Entry (RB3)
    if(INTCONbits.INTF) {
        if(entry_debounce == 0 && garage_count < 3) {
            entry_debounce = 50; 
            garage_count++;
            GARAGE_GATE = 1; 
            garage_entry_req = 250; 
        }
        INTCONbits.INTF = 0;
    }
}

// ============================================================================
// SUBSYSTEM UPDATES
// ============================================================================
void update_lighting(void) {
    if(light_override == 0) { ldr_value = adc_read(0); LIGHT_RELAY = (ldr_value < 300) ? 1 : 0; }
    else if(light_override == 1) LIGHT_RELAY = 1;
    else LIGHT_RELAY = 0;
}

void update_thermostat(void) {
    unsigned int adc_val = adc_read(1);
    current_temp = (unsigned int)(((unsigned long)adc_val * 50000UL) / 1023UL);
    THERMO_FAN = ((current_temp / 100) >= set_temp) ? 1 : 0;
}

void update_garage(void) {
    if(EXIT_SENSOR == 0 && exit_debounce == 0) {
        exit_debounce = 50;
        if(garage_count > 0) { garage_count--; GARAGE_GATE = 1; garage_entry_req = 250; }
    }
    if(garage_entry_req == 0) GARAGE_GATE = 0;
}

void update_security(void) {
    if(security_armed) {
        if(PIR_SENSOR == 1 && pir_debounce == 0) { pir_debounce = 100; alarm_active = 1; }
        if(alarm_active) ALARM_BUZZER = !ALARM_BUZZER;
    } else { ALARM_BUZZER = 0; alarm_active = 0; }
}

// ============================================================================
// MENU SYSTEM
// ============================================================================
unsigned char menu_get_number(unsigned char max_val) {
    unsigned char val = 0, stage = 0, first = 0;
    lcd_goto(0x40); lcd_str("                "); lcd_goto(0x40);
    while(1) {
        unsigned char k = keypad_wait();
        if(k >= '0' && k <= '9') {
            if(stage == 0) { first = k - '0'; val = first * 10; lcd_data(k); stage = 1; }
            else { val = (first * 10) + (k - '0'); lcd_data(k); if(val > max_val) val = max_val; return val; }
        } else if(k == '*') { val = 0; stage = 0; lcd_goto(0x40); lcd_str("                "); lcd_goto(0x40); }
    }
}

void menu_home(void) {
    rtc_read_time(); lcd_goto(0x00); lcd_str("HOME AUTO v2.0  ");
    lcd_goto(0x40); lcd_str("TIME:"); lcd_put_2digit(rtc_hr); lcd_data(':'); lcd_put_2digit(rtc_min); lcd_data(':'); lcd_put_2digit(rtc_sec);
    lcd_goto(0x14); lcd_str("T:"); lcd_put_uint(current_temp / 100); lcd_data(0xDF); lcd_str("C  G:"); lcd_data(garage_count + '0'); lcd_str("/3");
    lcd_goto(0x54); if(security_armed) lcd_str("SEC:ARMED ALARM:"); else lcd_str("SEC:OFF   ALARM:");
    if(alarm_active) lcd_str("ON "); else lcd_str("OFF");
}

void menu_clock(void) {
    rtc_read_time(); lcd_goto(0x00); lcd_str("  DIGITAL CLOCK "); lcd_goto(0x40); lcd_str("  ");
    lcd_put_2digit(rtc_hr); lcd_data(':'); lcd_put_2digit(rtc_min); lcd_data(':'); lcd_put_2digit(rtc_sec); lcd_str("      ");
    lcd_goto(0x14); lcd_str(" DATE: "); lcd_put_2digit(rtc_date); lcd_data('/'); lcd_put_2digit(rtc_month); lcd_data('/'); lcd_str("20"); lcd_put_2digit(rtc_yr);
    lcd_goto(0x54); lcd_str("*=SET  #=BACK ");
    if(keypad_getkey() == '*') {
        lcd_clear(); lcd_goto(0x00); lcd_str("SET HR (0-23):"); unsigned char h = menu_get_number(23);
        lcd_clear(); lcd_goto(0x00); lcd_str("SET MIN (0-59):"); unsigned char m = menu_get_number(59);
        rtc_write_time(h, m, 0, rtc_date, rtc_month, rtc_yr);
    }
}

void menu_thermostat(void) {
    lcd_goto(0x00); lcd_str("THERMOSTAT      "); lcd_goto(0x40); lcd_str("NOW:"); lcd_put_uint(current_temp / 100); lcd_str("C FAN:"); lcd_str(THERMO_FAN ? "ON " : "OFF");
    lcd_goto(0x14); lcd_str("SET:"); lcd_put_uint(set_temp); lcd_data(0xDF); lcd_str("C             "); lcd_goto(0x54); lcd_str("*=SET  #=BACK ");
    if(keypad_getkey() == '*') {
        lcd_clear(); lcd_goto(0x00); lcd_str("SET TEMP (C):"); unsigned char new_temp = menu_get_number(50);
        if(new_temp >= 10 && new_temp <= 50) { set_temp = new_temp; EEPROM_Write(20, (unsigned char)set_temp); }
    }
}

void menu_garage(void) {
    lcd_goto(0x00); lcd_str("SMART GARAGE    "); lcd_goto(0x40); lcd_str("OCCUPIED: "); lcd_put_uint(garage_count); lcd_str("/3       ");
    lcd_goto(0x14); lcd_str("AVAILABLE: "); lcd_put_uint(3 - garage_count); lcd_str("       "); lcd_goto(0x54); lcd_str("GATE:"); lcd_str(GARAGE_GATE ? "OPEN " : "CLOSE"); lcd_str(" #=BACK");
}

void menu_security(void) {
    lcd_goto(0x00); lcd_str("SECURITY SYSTEM "); lcd_goto(0x40); lcd_str("STATUS: "); if(security_armed) lcd_str("ARMED   "); else lcd_str("DISARMED");
    lcd_goto(0x14); lcd_str("ALARM:  "); if(alarm_active) lcd_str("ACTIVE!   "); else lcd_str("QUIET     "); lcd_goto(0x54); lcd_str("*=TOGGLE #=BACK");
    if(keypad_getkey() == '*') { security_armed = !security_armed; if(!security_armed) { ALARM_BUZZER = 0; alarm_active = 0; } __delay_ms(300); }
}

void menu_door_lock(void) {
    lcd_goto(0x00); lcd_str("DOOR LOCK       "); lcd_goto(0x40); lcd_str("STATUS: "); if(door_locked) lcd_str("LOCKED   "); else lcd_str("UNLOCKED ");
    lcd_goto(0x14); lcd_str("                "); lcd_goto(0x54); lcd_str("*=OPEN #=BACK ");
    if(keypad_getkey() == '*') {
        lcd_clear(); lcd_goto(0x00); lcd_str("ENTER PIN:"); lcd_goto(0x40);
        char entered[4];
        unsigned char i; // Declared at block scope for C90 compatibility
        for(i = 0; i < 4; i++) { entered[i] = keypad_wait(); lcd_data('*'); }
        __delay_ms(300);
        unsigned char correct = 1;
        for(i = 0; i < 4; i++) { if(entered[i] != password[i]) { correct = 0; break; } }
        if(correct) {
            DOOR_LOCK = 0; DOOR_LED = 1; door_locked = 0; lock_timer = 100;
            lcd_clear(); lcd_goto(0x00); lcd_str(" ACCESS GRANTED"); lcd_goto(0x40); lcd_str("  UNLOCKED...  "); __delay_ms(1500);
        } else { lcd_clear(); lcd_goto(0x00); lcd_str(" WRONG PIN!   "); __delay_ms(1500); }
    }
}

void menu_settings(void) {
    lcd_goto(0x00); lcd_str("SETTINGS        "); lcd_goto(0x40); lcd_str("1:Light 2:PIN   "); lcd_goto(0x14); lcd_str("3:Reset  #=BACK "); lcd_goto(0x54); lcd_str("                ");
    unsigned char k = keypad_wait();
    if(k == '1') {
        lcd_clear(); lcd_goto(0x00); lcd_str("LIGHT MODE:"); lcd_goto(0x40);
        if(light_override == 0) lcd_str("AUTO (LDR)    "); else if(light_override == 1) lcd_str("MANUAL ON     "); else lcd_str("MANUAL OFF    ");
        lcd_goto(0x14); lcd_str("*=CYCLE #=BACK");
        while(1) { k = keypad_wait(); if(k == '*') { light_override++; if(light_override > 2) light_override = 0; lcd_goto(0x40); if(light_override == 0) lcd_str("AUTO (LDR)    "); else if(light_override == 1) lcd_str("MANUAL ON     "); else lcd_str("MANUAL OFF    "); } else if(k == '#') return; }
    } else if(k == '2') {
        lcd_clear(); lcd_goto(0x00); lcd_str("OLD PIN:"); lcd_goto(0x40);
        char old[4];
        unsigned char i; // Declared at block scope for C90 compatibility
        for(i = 0; i < 4; i++) { old[i] = keypad_wait(); lcd_data('*'); }
        unsigned char valid = 1;
        for(i = 0; i < 4; i++) { if(old[i] != password[i]) { valid = 0; break; } }
        if(!valid) { lcd_clear(); lcd_goto(0x00); lcd_str("WRONG PIN!    "); __delay_ms(1500); return; }
        lcd_clear(); lcd_goto(0x00); lcd_str("NEW PIN:"); lcd_goto(0x40);
        for(i = 0; i < 4; i++) { password[i] = keypad_wait(); lcd_data('*'); EEPROM_Write(EEPROM_PASS1 + i, password[i]); }
        lcd_clear(); lcd_goto(0x00); lcd_str("PIN CHANGED!  "); __delay_ms(1500);
    } else if(k == '3') {
        lcd_clear(); lcd_goto(0x00); lcd_str("RESET ALL? *=Y"); k = keypad_wait();
        if(k == '*') { set_temp = 28; EEPROM_Write(20, 28); security_armed = 0; light_override = 0; lcd_clear(); lcd_goto(0x00); lcd_str("RESET DONE!   "); __delay_ms(1500); }
    }
}

// ============================================================================
// MAIN
// ============================================================================
void main(void) {
    TRISA = 0xFF; TRISB = 0xF8; 
    TRISC = 0x00;           
    TRISCbits.TRISC3 = 1;   // RC3 (SCL) MUST be input for I2C
    TRISCbits.TRISC4 = 1;   // RC4 (SDA) MUST be input for I2C
    TRISD = 0x00; TRISE = 0xE8;
    
    PORTA = 0x00; PORTB = 0x07; PORTC = 0x00; PORTD = 0x00; PORTE = 0x06;
    OPTION_REG = 0x04; INTCONbits.INTF = 0; INTCONbits.INTE = 1; // Enable INT for Garage Entry
    ADCON1 = 0x80; ADCON0 = 0xC1;
    SSPSTAT = 0x80; SSPCON = 0x28; SSPADD = 49; // I2C 100kHz
    TMR0 = 100; INTCONbits.TMR0IF = 0; INTCONbits.TMR0IE = 1; INTCONbits.GIE = 1;
    
    lcd_init(); lcd_goto(0x00); lcd_str(" SMART HOME SYS "); lcd_goto(0x40); lcd_str("  LOADING...   "); __delay_ms(1500);
    
    unsigned char saved_temp = EEPROM_Read(20);
    if(saved_temp >= 10 && saved_temp <= 50) set_temp = saved_temp;
    else { set_temp = 28; EEPROM_Write(20, 28); }
    
    if(EEPROM_Read(EEPROM_FLAG) != 0xA5) {
        password[0] = '1'; password[1] = '2'; password[2] = '3'; password[3] = '4';
        EEPROM_Write(0, '1'); EEPROM_Write(1, '2'); EEPROM_Write(2, '3'); EEPROM_Write(3, '4'); EEPROM_Write(EEPROM_FLAG, 0xA5);
    } else { password[0] = EEPROM_Read(0); password[1] = EEPROM_Read(1); password[2] = EEPROM_Read(2); password[3] = EEPROM_Read(3); }
    
    rtc_read_time();
    if(rtc_yr > 99 || rtc_month > 12 || rtc_date > 31) rtc_write_time(12, 0, 0, 9, 6, 26); // Default: June 9, 2026
    
    lcd_clear(); lcd_goto(0x00); lcd_str("SYSTEM READY!   "); lcd_goto(0x40); lcd_str("Default PIN:1234"); __delay_ms(2000);
    
    while(1) {
        update_lighting(); update_thermostat(); update_garage(); update_security();
        if(lcd_update_counter++ > 50) {
            lcd_update_counter = 0;
            unsigned char k = keypad_getkey();
            if(k == '1') current_menu = 1; else if(k == '2') current_menu = 2; else if(k == '3') current_menu = 3;
            else if(k == '4') current_menu = 4; else if(k == '5') current_menu = 5; else if(k == '6') current_menu = 6; else if(k == '0') current_menu = 0;
            switch(current_menu) {
                case 0: menu_home(); break; case 1: menu_clock(); break; case 2: menu_thermostat(); break;
                case 3: menu_garage(); break; case 4: menu_security(); break; case 5: menu_door_lock(); break; case 6: menu_settings(); break;
            }
        }
        __delay_ms(4);
    }
}