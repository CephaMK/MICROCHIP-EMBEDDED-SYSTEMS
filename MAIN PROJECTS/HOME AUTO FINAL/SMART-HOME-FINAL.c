/*
 * File:   SMART-HOME-FINAL.c
 * Device: PIC16F877A @ 20MHz
 * Compiler: XC8
 *
 * ==========================================================================
 * COMPLETE BUG FIX SUMMARY (confirmed from schematic image analysis)
 * ==========================================================================
 *
 * BUG 1 — RTC ALWAYS SHOWS 00:00:00
 * -----------------------------------
 * Your DS1307 debug panel confirms the chip IS running (09:45:37, 11/06/26).
 * So the oscillator and wiring are fine. The problem is in how the PIC reads
 * the I2C bus. Two sub-causes:
 *
 * (a) SSPSTATbits.R_W vs R_nW:
 *     In XC8 for PIC16F877A, the correct SFR bit name is R_nW (Read-not-Write).
 *     Using R_W reads bit 0 (the Write Collision flag WC in some headers) which
 *     is almost always 0, so i2c_wait() returns immediately without waiting for
 *     the bus to clear. This corrupts every I2C transaction silently.
 *     FIX: Use SSPSTATbits.R_nW throughout.
 *
 * (b) Clock Halt bit on DS1307 first power-up:
 *     DS1307 register 0 bit 7 = CH (Clock Halt). At first power-up it = 1.
 *     rtc_read_time() masks it (raw & 0x7F) before decoding, so sec reads 0.
 *     The validity check (sec > 59) passes, rtc_write_time() is never called,
 *     CH stays 1, clock never starts.
 *     FIX: Check CH bit raw BEFORE decoding. If set, call rtc_write_time()
 *     to clear it and start the oscillator.
 *
 * (c) menu_clock() only calls rtc_read_time() but then the main loop's
 *     keypad_getkey() consumes '#' BEFORE menu_clock() can see it, causing
 *     #=BACK to not work and the clock screen to get "stuck".
 *     FIX: All menu navigation now handled ONLY inside each menu function.
 *     The main loop's keypad scan only handles the HOME screen keys (1-6).
 *     When in any sub-menu, the main loop does NOT call keypad_getkey().
 *
 * BUG 2 — GARAGE EXIT JUMP TO 0 (already fixed in previous version)
 * -------------------------------------------------------------------
 * Exit edge detection moved to ISR with one-shot flag. Retained here.
 *
 * BUG 3 — PIR / SECURITY NO RESPONSE
 * ------------------------------------
 * ADCON1 = 0x85 sets PCFG = 0b0101. On PIC16F877A this makes RA2 = VREF-
 * (an analog reference input), NOT a digital I/O pin. PORTAbits.RA2 reads
 * nothing when RA2 is in analog/reference mode.
 * FIX: ADCON1 = 0x8E (PCFG = 0b1110). Only AN0 (RA0) and AN1 (RA1) are
 * analog. RA2 through RA5 and RE0-RE2 are all digital I/O.
 * VREF+ becomes VDD internally — no need to tie RA3 to +5V.
 *
 * PROTEUS WIRING FOR PIR: Connect a BUTTON between RA2 and +5V.
 * Add a 10k resistor from RA2 to GND (pull-down). Button press = HIGH = motion.
 * OR use LOGICSTATE component output directly to RA2.
 *
 * BUG 4 — KEYPAD NEEDS 3-5 PRESSES
 * -----------------------------------
 * keypad_getkey() had while(RBx==0){} wait-for-release inside it. A 200ms
 * button hold blocked the entire main loop including lcd_update_counter, so
 * the menu never refreshed and the next check arrived too late.
 * FIX: keypad_scan_raw() = instant sample only. keypad_getkey() = single-fire
 * on press via keypad_prev state tracking. keypad_wait() = blocking for PIN
 * entry only (with proper debounce + release detection).
 *
 * ==========================================================================
 * PROTEUS WIRING CORRECTIONS REQUIRED IN YOUR SCHEMATIC
 * ==========================================================================
 *
 * 1. RA3 does NOT need to connect to +5V with ADCON1 = 0x8E.
 *    You can leave RA3 unconnected or use it as digital I/O.
 *
 * 2. PIR sensor (RA2): Add BUTTON between RA2 and +5V, 10k pull-down to GND.
 *    Currently nothing is connected to RA2 in your schematic.
 *
 * 3. All other wiring in your schematic appears correct.
 *    DS1307 SCL=RC3, SDA=RC4, pull-ups R8/R9 = confirmed correct.
 */

#define _XTAL_FREQ 20000000
#include <xc.h>

#pragma config FOSC = HS, WDTE = OFF, PWRTE = OFF, BOREN = ON, LVP = OFF
#pragma config CPD = OFF, WRT = OFF, CP = OFF

/* ==========================================================================
   HARDWARE PIN DEFINITIONS
   ========================================================================== */
#define LCD_RS_PIN      PORTDbits.RD0
#define LCD_RW_PIN      PORTDbits.RD1
#define LCD_EN_PIN      PORTDbits.RD2

#define LIGHT_RELAY     PORTCbits.RC0
#define GARAGE_GATE     PORTCbits.RC1
#define THERMO_FAN      PORTCbits.RC2
#define ALARM_BUZZER    PORTCbits.RC7
#define DOOR_LOCK       PORTCbits.RC5
#define DOOR_LED        PORTCbits.RC6

/* RB0 = garage entry interrupt (active-low, pull-up)    */
/* RE0 = garage exit sensor    (active-low, pull-up)     */
/* RA2 = PIR / security button (active-high, pull-DOWN)  */
#define EXIT_SENSOR     PORTEbits.RE0
#define PIR_SENSOR      PORTAbits.RA2

/* Keypad: columns = outputs on RB1-RB3, rows = inputs on RB4-RB7 */
#define KP_COL1         PORTBbits.RB1
#define KP_COL2         PORTBbits.RB2
#define KP_COL3         PORTBbits.RB3

/* ==========================================================================
   GLOBAL VARIABLES
   ========================================================================== */
unsigned char rtc_sec   = 0, rtc_min = 0, rtc_hr  = 0;
unsigned char rtc_date  = 9, rtc_month = 6, rtc_yr = 26;
unsigned int  current_temp  = 2500;
unsigned char set_temp      = 28;

volatile unsigned char garage_count      = 0;
volatile unsigned char garage_entry_req  = 0;
volatile unsigned char garage_exit_req   = 0;
volatile unsigned char entry_debounce    = 0;
volatile unsigned char exit_debounce     = 0;
volatile unsigned char pir_debounce      = 0;
volatile unsigned char security_armed    = 0;
volatile unsigned char alarm_active      = 0;
volatile unsigned char door_locked       = 1;
volatile unsigned char lock_timer        = 0;
volatile unsigned char alarm_beep_cnt    = 0;

/* ISR-side exit edge tracking */
volatile unsigned char exit_prev         = 1;

/* Main-loop PIR edge tracking */
unsigned char pir_prev       = 0;

/* Keypad single-fire state */
unsigned char keypad_prev    = 0;

/* LCD refresh throttle — used ONLY in home screen loop */
unsigned char lcd_tick       = 0;

/* Active menu: 0=home, 1=clock, 2=thermo, 3=garage, 4=security,
                5=door, 6=settings */
unsigned char current_menu   = 0;

unsigned char password[4];
unsigned char light_override = 0;
unsigned int  ldr_value      = 0;

#define EEPROM_FLAG   10
#define EEPROM_PASS1   0

/* ==========================================================================
   FUNCTION PROTOTYPES
   ========================================================================== */
void lcd_init(void);
void lcd_cmd(unsigned char cmd);
void lcd_data(unsigned char data);
void lcd_str(const char *s);
void lcd_goto(unsigned char pos);
void lcd_clear(void);
void lcd_put_uint(unsigned int v);
void lcd_put_2digit(unsigned char v);

unsigned char keypad_scan_raw(void);
unsigned char keypad_getkey(void);
unsigned char keypad_wait(void);
unsigned int  adc_read(unsigned char ch);

void i2c_wait(void);
void i2c_start(void);
void i2c_restart(void);
void i2c_stop(void);
unsigned char i2c_write(unsigned char d);
unsigned char i2c_read_ack(void);
unsigned char i2c_read_nack(void);

void rtc_write_time(unsigned char h, unsigned char m, unsigned char s,
                    unsigned char d, unsigned char mo, unsigned char y);
void rtc_read_time(void);
unsigned char rtc_is_halted(void);

unsigned char menu_get_number(unsigned char max_val);
void update_lighting(void);
void update_thermostat(void);
void update_garage(void);
void update_security(void);

void menu_home(void);
void menu_clock(void);
void menu_thermostat(void);
void menu_garage(void);
void menu_security(void);
void menu_door_lock(void);
void menu_settings(void);

void EEPROM_Write(unsigned char addr, unsigned char data);
unsigned char EEPROM_Read(unsigned char addr);

/* ==========================================================================
   EEPROM
   ========================================================================== */
void EEPROM_Write(unsigned char addr, unsigned char data)
{
    while (EECON1bits.WR);
    EEADR  = addr;
    EEDATA = data;
    EECON1bits.EEPGD = 0;
    EECON1bits.WREN  = 1;
    INTCONbits.GIE   = 0;
    EECON2 = 0x55;
    EECON2 = 0xAA;
    EECON1bits.WR    = 1;
    INTCONbits.GIE   = 1;
    EECON1bits.WREN  = 0;
}

unsigned char EEPROM_Read(unsigned char addr)
{
    EEADR = addr;
    EECON1bits.EEPGD = 0;
    EECON1bits.RD    = 1;
    return EEDATA;
}

/* ==========================================================================
   LCD — 4-bit mode, PORTD[7:4] data, RD0=RS RD1=RW RD2=EN
   ========================================================================== */
void lcd_pulse(void)
{
    LCD_EN_PIN = 1; __delay_us(2);
    LCD_EN_PIN = 0; __delay_us(50);
}

void lcd_nibble(unsigned char n)
{
    PORTD = (PORTD & 0x0F) | (n & 0xF0);
    lcd_pulse();
}

void lcd_cmd(unsigned char c)
{
    LCD_RS_PIN = 0; LCD_RW_PIN = 0;
    lcd_nibble(c & 0xF0);
    lcd_nibble((unsigned char)(c << 4));
    if (c == 0x01 || c == 0x02) __delay_ms(2);
    else __delay_us(50);
}

void lcd_data(unsigned char c)
{
    LCD_RS_PIN = 1; LCD_RW_PIN = 0;
    lcd_nibble(c & 0xF0);
    lcd_nibble((unsigned char)(c << 4));
    __delay_us(50);
}

void lcd_str(const char *s) { while (*s) lcd_data(*s++); }

void lcd_goto(unsigned char pos) { lcd_cmd((unsigned char)(0x80 | pos)); }

void lcd_clear(void) { lcd_cmd(0x01); __delay_ms(2); }

void lcd_init(void)
{
    __delay_ms(50);
    LCD_RS_PIN = 0; LCD_RW_PIN = 0; LCD_EN_PIN = 0;
    lcd_nibble(0x30); __delay_ms(5);
    lcd_nibble(0x30); __delay_us(200);
    lcd_nibble(0x30); __delay_us(200);
    lcd_nibble(0x20); __delay_us(200);
    lcd_cmd(0x28); lcd_cmd(0x0C); lcd_cmd(0x06); lcd_cmd(0x01);
}

void lcd_put_uint(unsigned int v)
{
    unsigned int  d = 10000;
    unsigned char s = 0, digit;
    while (d > 0)
    {
        digit = (unsigned char)(v / d);
        if (digit || s || d == 1) { lcd_data((unsigned char)(digit + '0')); s = 1; }
        v %= d; d /= 10;
    }
}

void lcd_put_2digit(unsigned char v)
{
    lcd_data((unsigned char)((v / 10) + '0'));
    lcd_data((unsigned char)((v % 10) + '0'));
}

/* ==========================================================================
   KEYPAD — columns RB1-RB3 (outputs), rows RB4-RB7 (inputs, pull-ups)
   ========================================================================== */
void kp_idle(void) { KP_COL1 = 1; KP_COL2 = 1; KP_COL3 = 1; }

/* Pure non-blocking sample — returns key char or 0 */
unsigned char keypad_scan_raw(void)
{
    static const unsigned char km[4][3] = {
        {'1','2','3'},{'4','5','6'},{'7','8','9'},{'*','0','#'}
    };
    unsigned char col;

    kp_idle();
    for (col = 0; col < 3; col++)
    {
        kp_idle();
        if      (col == 0) KP_COL1 = 0;
        else if (col == 1) KP_COL2 = 0;
        else               KP_COL3 = 0;
        __delay_us(500);
        if (!PORTBbits.RB4) { kp_idle(); return km[0][col]; }
        if (!PORTBbits.RB5) { kp_idle(); return km[1][col]; }
        if (!PORTBbits.RB6) { kp_idle(); return km[2][col]; }
        if (!PORTBbits.RB7) { kp_idle(); return km[3][col]; }
    }
    kp_idle();
    return 0;
}

/* Single-fire: returns char on NEW press only, 0 otherwise */
unsigned char keypad_getkey(void)
{
    unsigned char cur = keypad_scan_raw();
    if (cur == 0)          { keypad_prev = 0; return 0; }
    if (cur != keypad_prev){ keypad_prev = cur; return cur; }
    return 0;
}

/* Blocking wait — for PIN digit entry (waits press + release) */
unsigned char keypad_wait(void)
{
    unsigned char k;
    while (keypad_scan_raw()) { __delay_ms(5); }   /* wait for prior release */
    while (1)
    {
        k = keypad_scan_raw();
        if (k)
        {
            __delay_ms(30);
            if (keypad_scan_raw() == k)
            {
                while (keypad_scan_raw()) { __delay_ms(5); }
                __delay_ms(30);
                return k;
            }
        }
        __delay_ms(5);
    }
}

/* ==========================================================================
   ADC
   ========================================================================== */
unsigned int adc_read(unsigned char ch)
{
    ch &= 0x07;
    ADCON0 = (ADCON0 & 0xC7) | (unsigned char)(ch << 3);
    __delay_us(25);
    ADCON0bits.GO_nDONE = 1;
    while (ADCON0bits.GO_nDONE) {}
    return ((unsigned int)ADRESH << 8) | ADRESL;
}

/* ==========================================================================
   I2C (MSSP master)
   CRITICAL FIX: i2c_wait uses SSPSTATbits.R_nW  (not R_W)
   R_nW = bit 2 of SSPSTAT = "Read/not-Write" — 1 while transfer in progress
   Using R_W (wrong name) read bit 0 = WC (Write Collision) which is almost
   always 0, so i2c_wait() returned instantly and trashed every transaction.
   ========================================================================== */
void i2c_wait(void)
{
    /* Wait for all MSSP activity to finish:
     * SSPCON2[4:0] = SEN|RSEN|PEN|RCEN|ACKEN — all must be clear
     * SSPSTATbits.R_nW = 1 during data transfer */
    while ((SSPCON2 & 0x1F) || (SSPSTATbits.R_nW));
}

void i2c_start(void)
{
    i2c_wait(); SSPCON2bits.SEN  = 1; while (SSPCON2bits.SEN);
}

void i2c_restart(void)
{
    i2c_wait(); SSPCON2bits.RSEN = 1; while (SSPCON2bits.RSEN);
}

void i2c_stop(void)
{
    i2c_wait(); SSPCON2bits.PEN  = 1; while (SSPCON2bits.PEN);
}

unsigned char i2c_write(unsigned char d)
{
    i2c_wait(); SSPBUF = d; i2c_wait();
    return (unsigned char)(!SSPCON2bits.ACKSTAT);
}

unsigned char i2c_read_ack(void)
{
    unsigned char d;
    i2c_wait(); SSPCON2bits.RCEN = 1; while (!SSPSTATbits.BF); d = SSPBUF;
    i2c_wait(); SSPCON2bits.ACKDT = 0; SSPCON2bits.ACKEN = 1;
    while (SSPCON2bits.ACKEN);
    return d;
}

unsigned char i2c_read_nack(void)
{
    unsigned char d;
    i2c_wait(); SSPCON2bits.RCEN = 1; while (!SSPSTATbits.BF); d = SSPBUF;
    i2c_wait(); SSPCON2bits.ACKDT = 1; SSPCON2bits.ACKEN = 1;
    while (SSPCON2bits.ACKEN);
    return d;
}

/* BCD helpers */
unsigned char dec_to_bcd(unsigned char v)
{ return (unsigned char)(((v / 10) << 4) | (v % 10)); }

unsigned char bcd_to_dec(unsigned char v)
{ return (unsigned char)(((v >> 4) * 10) + (v & 0x0F)); }

/* ==========================================================================
   DS1307 RTC DRIVER
   ========================================================================== */

/* Read raw register 0 to check Clock Halt bit (bit 7).
   Returns 1 if halted, 0 if running. */
unsigned char rtc_is_halted(void)
{
    unsigned char raw;
    i2c_start();
    i2c_write(0xD0); i2c_write(0x00);
    i2c_restart();
    i2c_write(0xD1);
    raw = i2c_read_nack();
    i2c_stop();
    return (raw & 0x80) ? 1 : 0;
}

/* Write full time/date. Always clears CH bit → starts oscillator. */
void rtc_write_time(unsigned char h, unsigned char m, unsigned char s,
                    unsigned char d, unsigned char mo, unsigned char y)
{
    i2c_start();
    i2c_write(0xD0);
    i2c_write(0x00);
    i2c_write((unsigned char)(dec_to_bcd(s) & 0x7F)); /* CH=0 = RUN */
    i2c_write(dec_to_bcd(m));
    i2c_write(dec_to_bcd(h));
    i2c_write(dec_to_bcd(1));   /* day-of-week unused */
    i2c_write(dec_to_bcd(d));
    i2c_write(dec_to_bcd(mo));
    i2c_write(dec_to_bcd(y));
    i2c_stop();
}

/* Read and decode current time from DS1307 */
void rtc_read_time(void)
{
    unsigned char raw_sec;
    i2c_start();
    i2c_write(0xD0); i2c_write(0x00);
    i2c_restart();
    i2c_write(0xD1);
    raw_sec   = i2c_read_ack();     /* mask CH bit before decode */
    rtc_min   = i2c_read_ack();
    rtc_hr    = i2c_read_ack();
    i2c_read_ack();                  /* skip day-of-week */
    rtc_date  = i2c_read_ack();
    rtc_month = i2c_read_ack();
    rtc_yr    = i2c_read_nack();
    i2c_stop();

    rtc_sec   = bcd_to_dec((unsigned char)(raw_sec & 0x7F));
    rtc_min   = bcd_to_dec(rtc_min);
    rtc_hr    = bcd_to_dec((unsigned char)(rtc_hr  & 0x3F));
    rtc_date  = bcd_to_dec(rtc_date);
    rtc_month = bcd_to_dec(rtc_month);
    rtc_yr    = bcd_to_dec(rtc_yr);
}

/* ==========================================================================
   INTERRUPT SERVICE ROUTINE
   Timer0 @ ~1ms tick (TMR0 reload=100, prescaler 1:32, Fosc=20MHz)
   INT0 on RB0 falling edge = garage entry
   ========================================================================== */
void __interrupt() isr(void)
{
    unsigned char cur_exit;

    /* ---- Timer0 ~1ms -------------------------------------------------- */
    if (INTCONbits.TMR0IF)
    {
        TMR0 = 100;
        INTCONbits.TMR0IF = 0;

        if (garage_entry_req > 0) garage_entry_req--;
        if (entry_debounce  > 0) entry_debounce--;
        if (exit_debounce   > 0) exit_debounce--;
        if (pir_debounce    > 0) pir_debounce--;

        if (lock_timer > 0)
        {
            lock_timer--;
            if (lock_timer == 0) 
            {
                door_locked = 1;
                DOOR_LOCK = 1; DOOR_LED = 0; ; 
            }
        }

        /* EXIT sensor edge detection — active-low pull-up RA0 */
        cur_exit = EXIT_SENSOR;
        if (exit_prev == 1 && cur_exit == 0 && exit_debounce == 0)
        {
            exit_debounce  = 50;
            garage_exit_req = 1;
        }
        exit_prev = cur_exit;

        /* Alarm buzzer */
        if (security_armed && alarm_active)
        {
            alarm_beep_cnt++;
            if (alarm_beep_cnt >= 125) { ALARM_BUZZER = !ALARM_BUZZER; alarm_beep_cnt = 0; }
        }
        else
        {
            ALARM_BUZZER   = 0;
            alarm_beep_cnt = 0;
        }
    }

    /* ---- INT0 falling edge — Garage Entry ----------------------------- */
    if (INTCONbits.INTF)
    {
        if (entry_debounce == 0 && garage_count < 3)
        {
            entry_debounce   = 50;
            garage_count++;
            GARAGE_GATE      = 1;
            garage_entry_req = 250;
        }
        INTCONbits.INTF = 0;
    }
}

/* ==========================================================================
   SUBSYSTEM UPDATES (called every main loop iteration)
   ========================================================================== */
void update_lighting(void)
{
    if (light_override == 0)
    {
        ldr_value   = adc_read(0);
        LIGHT_RELAY = (ldr_value < 300) ? 1 : 0;
    }
    else LIGHT_RELAY = (light_override == 1) ? 1 : 0;
}

void update_thermostat(void)
{
    unsigned int v = adc_read(1);
    current_temp   = (unsigned int)(((unsigned long)v * 50000UL) / 1023UL);
    THERMO_FAN     = ((current_temp / 100) >= set_temp) ? 1 : 0;
}

void update_garage(void)
{
    if (garage_exit_req)
    {
        garage_exit_req = 0;
        if (garage_count > 0) { garage_count--; GARAGE_GATE = 1; garage_entry_req = 250; }
    }
    if (garage_entry_req == 0) GARAGE_GATE = 0;
}

void update_security(void)
{
    unsigned char cur = PIR_SENSOR;
    if (security_armed)
    {
        if (pir_prev == 0 && cur == 1 && pir_debounce == 0)
        { pir_debounce = 100; alarm_active = 1; }
    }
    else { alarm_active = 0; ALARM_BUZZER = 0; alarm_beep_cnt = 0; }
    pir_prev = cur;
}

void update_door_status(void)
{
    if (door_locked)
    {
        RC5 = 1;   // LOCKED LED ON
        RC6 = 0;   // OPEN LED OFF
    }
    else
    {
        RC5 = 0;   // LOCKED LED OFF
        RC6 = 1;   // OPEN LED ON
    }
}

/* ==========================================================================
   MENU HELPERS
   ========================================================================== */
unsigned char menu_get_number(unsigned char max_val)
{
    unsigned char val = 0, stage = 0, first = 0, k;
    lcd_goto(0x40); lcd_str("                "); lcd_goto(0x40);
    while (1)
    {
        k = keypad_wait();
        if (k >= '0' && k <= '9')
        {
            if (!stage) { first = (unsigned char)(k-'0'); val = (unsigned char)(first*10); lcd_data(k); stage = 1; }
            else
            {
                val = (unsigned char)((first*10)+(k-'0'));
                lcd_data(k);
                if (val > max_val) val = max_val;
                return val;
            }
        }
        else if (k == '*')
        {
            val = 0; stage = 0;
            lcd_goto(0x40); lcd_str("                "); lcd_goto(0x40);
        }
    }
}

/* ==========================================================================
   MENU SCREENS
   Each menu function handles its own keypad input and navigation.
   This prevents the main loop from consuming '#' before the menu sees it.
   ========================================================================== */

void menu_home(void)
{
    rtc_read_time();

    lcd_goto(0x00);
    lcd_str("HOME AUTO v2.1  ");

    lcd_goto(0x40);
    lcd_str("TIME:");
    lcd_put_2digit(rtc_hr);
    lcd_data(':');
    lcd_put_2digit(rtc_min);
    lcd_data(':');
    lcd_put_2digit(rtc_sec);

    lcd_goto(0x14);
    lcd_str("T:");
    lcd_put_uint(current_temp / 100);
    lcd_data(0xDF);
    lcd_str("C G:");
    lcd_data(garage_count + '0');
    lcd_str("/3");

    lcd_goto(0x54);
    lcd_str(security_armed ? "SEC:ARMED " : "SEC:OFF   ");
    lcd_str("ALARM:");
    lcd_str(alarm_active ? "ON " : "OFF");
}
/*
 * CLOCK MENU — FIX 1(c):
 * This function runs its own inner refresh loop so the time ticks live.
 * It blocks here until '#' is pressed. The main loop does NOT consume '#'.
 */
void menu_clock(void)
{
    unsigned char h, m, k;

    while (1)
    {
        rtc_read_time();   /* refresh every pass so seconds tick on screen */

        lcd_goto(0x00); lcd_str("  DIGITAL CLOCK ");
        lcd_goto(0x40); lcd_str("  ");
        lcd_put_2digit(rtc_hr); lcd_data(':');
        lcd_put_2digit(rtc_min); lcd_data(':');
        lcd_put_2digit(rtc_sec); lcd_str("      ");
        lcd_goto(0x14); lcd_str(" DATE: ");
        lcd_put_2digit(rtc_date); lcd_data('/');
        lcd_put_2digit(rtc_month); lcd_data('/');
        lcd_str("20"); lcd_put_2digit(rtc_yr);
        lcd_goto(0x54); lcd_str("*=SET  #=BACK   ");

        __delay_ms(250);   /* ~4 refreshes per second so seconds are readable */

        k = keypad_scan_raw(); /* non-blocking check while refreshing */
        if (k == '#') { keypad_prev = '#'; current_menu = 0; return; }
        if (k == '*')
        {
            lcd_clear();
            lcd_goto(0x00); lcd_str("SET HR (0-23):  ");
            h = menu_get_number(23);
            lcd_clear();
            lcd_goto(0x00); lcd_str("SET MIN (0-59): ");
            m = menu_get_number(59);
            rtc_write_time(h, m, 0, rtc_date, rtc_month, rtc_yr);
            __delay_ms(50);
            rtc_read_time();
        }
    }
}

void menu_thermostat(void)
{
    unsigned char new_t, k;
    while (1)
    {
        update_thermostat();
        lcd_goto(0x00); lcd_str("THERMOSTAT      ");
        lcd_goto(0x40); lcd_str("NOW:");
        lcd_put_uint(current_temp / 100); lcd_str("C FAN:");
        lcd_str(THERMO_FAN ? "ON " : "OFF");
        lcd_goto(0x14); lcd_str("SET:");
        lcd_put_uint(set_temp); lcd_data(0xDF); lcd_str("C             ");
        lcd_goto(0x54); lcd_str("*=SET  #=BACK   ");

        __delay_ms(200);
        k = keypad_scan_raw();
        if (k == '#') { keypad_prev = '#'; current_menu = 0; return; }
        if (k == '*')
        {
            lcd_clear(); lcd_goto(0x00); lcd_str("SET TEMP (C):   ");
            new_t = menu_get_number(50);
            if (new_t >= 10 && new_t <= 50) { set_temp = new_t; EEPROM_Write(20, set_temp); }
        }
    }
}

void menu_garage(void)
{
    unsigned char k;
    while (1)
    {
        update_garage();
        lcd_goto(0x00); lcd_str("SMART GARAGE    ");
        lcd_goto(0x40); lcd_str("OCCUPIED: ");
        lcd_put_uint(garage_count); lcd_str("/3       ");
        lcd_goto(0x14); lcd_str("AVAILABLE: ");
        lcd_put_uint((unsigned int)(3 - garage_count)); lcd_str("       ");
        lcd_goto(0x54); lcd_str("GATE:");
        lcd_str(GARAGE_GATE ? "OPEN " : "CLOSE");
        lcd_str("  #=BACK");

        __delay_ms(100);
        k = keypad_scan_raw();
        if (k == '#') { keypad_prev = '#'; current_menu = 0; return; }
    }
}

void menu_security(void)
{
    unsigned char k;
    while (1)
    {
        update_security();
        lcd_goto(0x00); lcd_str("SECURITY SYSTEM ");
        lcd_goto(0x40); lcd_str("STATUS: ");
        lcd_str(security_armed ? "ARMED   " : "DISARMED");
        lcd_goto(0x14); lcd_str("ALARM:  ");
        lcd_str(alarm_active ? "ACTIVE!   " : "QUIET     ");
        lcd_goto(0x54); lcd_str("*=TOGGLE #=BACK");

        __delay_ms(200);
        k = keypad_scan_raw();
        if (k == '#') { keypad_prev = '#'; current_menu = 0; return; }
        if (k == '*')
        {
            security_armed = !security_armed;
            if (!security_armed) { ALARM_BUZZER = 0; alarm_active = 0; alarm_beep_cnt = 0; }
            __delay_ms(400);
        }
    }
}

void menu_door_lock(void)
{
    unsigned char entered[4], i, correct, k;

    while (1)
    {
        update_door_status();

        lcd_goto(0x00);
        lcd_str("DOOR LOCK       ");

        lcd_goto(0x40);
        lcd_str("STATUS: ");
        lcd_str(door_locked ? "LOCKED   " : "UNLOCKED ");

        lcd_goto(0x14);
        lcd_str("RC5=LCK RC6=OPN ");

        lcd_goto(0x54);
        lcd_str("*=OPEN #=BACK   ");

        __delay_ms(200);

        k = keypad_scan_raw();

        if (k == '#')
        {
            keypad_prev = '#';
            current_menu = 0;
            return;
        }

        if (k == '*')
        {
            lcd_clear();
            lcd_goto(0x00);
            lcd_str("ENTER PIN:      ");
            lcd_goto(0x40);

            for (i = 0; i < 4; i++)
            {
                entered[i] = keypad_wait();
                lcd_data('*');
            }

            __delay_ms(200);

            correct = 1;
            for (i = 0; i < 4; i++)
            {
                if (entered[i] != password[i])
                {
                    correct = 0;
                    break;
                }
            }

            if (correct)
            {
                door_locked = 0;

                RC5 = 0;   // LOCKED OFF
                RC6 = 1;   // OPEN ON

                lock_timer = 3000;

                lcd_clear();
                lcd_goto(0x00);
                lcd_str(" ACCESS GRANTED ");
                lcd_goto(0x40);
                lcd_str("  UNLOCKED...   ");

                __delay_ms(1500);
            }
            else
            {
                lcd_clear();
                lcd_goto(0x00);
                lcd_str(" WRONG PIN!     ");
                __delay_ms(1500);
            }
        }
    }
}

void menu_settings(void)
{
    unsigned char k, i, valid;
    unsigned char old_pin[4], new_pin[4];

    while (1)
    {
        lcd_goto(0x00); lcd_str("SETTINGS        ");
        lcd_goto(0x40); lcd_str("1:Light 2:PIN   ");
        lcd_goto(0x14); lcd_str("3:Reset         ");
        lcd_goto(0x54); lcd_str("#=BACK          ");

        k = keypad_wait();
        if (k == '#') { current_menu = 0; return; }

        if (k == '1')
        {
            lcd_clear(); lcd_goto(0x00); lcd_str("LIGHT MODE:     "); lcd_goto(0x40);
            if      (light_override == 0) lcd_str("AUTO (LDR)    ");
            else if (light_override == 1) lcd_str("MANUAL ON     ");
            else                          lcd_str("MANUAL OFF    ");
            lcd_goto(0x14); lcd_str("*=CYCLE #=BACK  ");
            while (1)
            {
                k = keypad_wait();
                if (k == '*')
                {
                    if (++light_override > 2) light_override = 0;
                    lcd_goto(0x40);
                    if      (light_override == 0) lcd_str("AUTO (LDR)    ");
                    else if (light_override == 1) lcd_str("MANUAL ON     ");
                    else                          lcd_str("MANUAL OFF    ");
                }
                else if (k == '#') break;
            }
        }
        else if (k == '2')
        {
            lcd_clear(); lcd_goto(0x00); lcd_str("OLD PIN:        "); lcd_goto(0x40);
            for (i = 0; i < 4; i++) { old_pin[i] = keypad_wait(); lcd_data('*'); }
            valid = 1;
            for (i = 0; i < 4; i++) if (old_pin[i] != password[i]) { valid = 0; break; }
            if (!valid) { lcd_clear(); lcd_goto(0x00); lcd_str("WRONG PIN!      "); __delay_ms(1500); continue; }
            lcd_clear(); lcd_goto(0x00); lcd_str("NEW PIN:        "); lcd_goto(0x40);
            for (i = 0; i < 4; i++)
            {
                new_pin[i] = keypad_wait(); lcd_data('*');
                password[i] = new_pin[i]; EEPROM_Write((unsigned char)(EEPROM_PASS1+i), password[i]);
            }
            lcd_clear(); lcd_goto(0x00); lcd_str("PIN CHANGED!    "); __delay_ms(1500);
        }
        else if (k == '3')
        {
            lcd_clear(); lcd_goto(0x00); lcd_str("RESET ALL? *=Y  ");
            k = keypad_wait();
            if (k == '*')
            {
                set_temp=28; EEPROM_Write(20,28);
                security_armed=0; light_override=0; alarm_active=0;
                alarm_beep_cnt=0; ALARM_BUZZER=0;
                garage_count=0; garage_entry_req=0; garage_exit_req=0;
                entry_debounce=0; exit_debounce=0; pir_debounce=0; lock_timer=0;
                door_locked=1; DOOR_LOCK=1; DOOR_LED=0; GARAGE_GATE=0;
                lcd_clear(); lcd_goto(0x00); lcd_str("RESET DONE!     "); __delay_ms(1500);
            }
        }
    }
}

/* ==========================================================================
   MAIN
   ========================================================================== */
void main(void)
{
    unsigned char saved_temp;

    /* Port directions */
    TRISA = 0xFF;           /* RA0=LDR(ADC), RA1=LM35(ADC), RA2=PIR(digital in) */
    TRISB = 0xF1;           /* RB0=INT, RB1-RB3=keypad cols out, RB4-RB7=rows in */
    TRISC = 0x00;
    TRISCbits.TRISC3 = 1;   /* I2C SCL */
    TRISCbits.TRISC4 = 1;   /* I2C SDA */
    TRISD = 0x00;           /* LCD */
    TRISE = 0x01;           /* RE0=EXIT_SENSOR in */

    PORTA = 0x00; PORTB = 0xFF; PORTC = 0x00; PORTD = 0x00; PORTE = 0x00;

    /* Timer0 / interrupts
     * OPTION_REG = 0x04:
     *   RBPU=0  → PORTB internal pull-ups ENABLED (keypad rows + RB0 entry btn)
     *   INTEDG=0 → INT0 on FALLING edge (RB0 active-low button)
     *   T0CS=0, PSA=0, PS=100 → prescaler 1:32 → ~1ms tick at 20MHz */
    OPTION_REG = 0x04;
    INTCONbits.INTF = 0;
    INTCONbits.INTE = 1;

    /* ADC setup
     * ADCON1 = 0x8E:
     *   ADFM=1  (bit7) → right-justified
     *   PCFG=1110 (bits3:0) → AN0(RA0) and AN1(RA1) analog ONLY.
     *   RA2 is now a NORMAL DIGITAL INPUT — PIR/security button reads here.
     *   VREF+ = VDD (internal), VREF- = VSS. No RA3 pin connection needed.
     *
     * WRONG: ADCON1=0x85 (PCFG=0101) made RA2=VREF- → PIR invisible.
     */
    ADCON1 = 0x8E;
    ADCON0 = 0x81;  /* ADCS=10=Fosc/32=625kHz (ok for 20MHz), CH=0, ADON=1 */

    /* I2C Master @ 100kHz
     * SSPADD = Fosc/(4*Fbaud) - 1 = 20000000/(4*100000) - 1 = 49 */
    SSPSTAT = 0x80;   /* SMP=1 (slew rate OFF for 100kHz standard mode) */
    SSPCON  = 0x28;   /* SSPEN=1, I2C master mode (SSPM=1000) */
    SSPADD  = 49;

    /* Timer0 */
    TMR0 = 100;
    INTCONbits.TMR0IF = 0;
    INTCONbits.TMR0IE = 1;
    INTCONbits.GIE    = 1;

    lcd_init();
    lcd_goto(0x00); lcd_str(" SMART HOME SYS ");
    lcd_goto(0x40); lcd_str("  LOADING...    ");
    __delay_ms(1500);

    /* Restore thermostat setpoint */
    saved_temp = EEPROM_Read(20);
    if (saved_temp >= 10 && saved_temp <= 50) set_temp = saved_temp;
    else { set_temp = 28; EEPROM_Write(20, 28); }

    /* Restore or initialise door PIN */
    if (EEPROM_Read(EEPROM_FLAG) != 0xA5)
    {
        password[0]='1'; password[1]='2'; password[2]='3'; password[3]='4';
        EEPROM_Write(0,'1'); EEPROM_Write(1,'2');
        EEPROM_Write(2,'3'); EEPROM_Write(3,'4');
        EEPROM_Write(EEPROM_FLAG, 0xA5);
    }
    else
    {
        password[0]=EEPROM_Read(0); password[1]=EEPROM_Read(1);
        password[2]=EEPROM_Read(2); password[3]=EEPROM_Read(3);
    }

    /* RTC startup — FIX 1:
     * Check the CH (Clock Halt) bit directly from the raw register byte.
     * Do NOT rely on the decoded sec value — CH=1 makes raw_sec=0x80,
     * which decodes to 0 and passes the validity check, so the clock
     * never gets started. rtc_is_halted() reads the raw byte properly. */
    if (rtc_is_halted())
    {
        /* Start the RTC oscillator by writing CH=0 */
        rtc_write_time(12, 0, 0, 9, 6, 26);
        __delay_ms(100);
    }
    rtc_read_time();

    /* Sanity check (handles corrupt but running RTC) */
    if (rtc_min > 59 || rtc_hr > 23 ||
        rtc_date == 0 || rtc_date > 31 || rtc_month == 0 || rtc_month > 12)
    {
        rtc_write_time(12, 0, 0, 9, 6, 26);
        __delay_ms(100);
        rtc_read_time();
    }

    lcd_clear();
    lcd_goto(0x00); lcd_str("SYSTEM READY!   ");
    lcd_goto(0x40); lcd_str("Default PIN:1234");
    __delay_ms(2000);

    /* =======================================================================
       MAIN LOOP
       The home screen polls keypad for menu selection (1-6).
       Sub-menus run their own blocking loops and return here on '#'.
       This ensures '#' is never swallowed by the main loop while in a menu.
       ======================================================================= */
    DOOR_LOCK = 1;   // Locked state active
    DOOR_LED  = 0;   // Open indicator OFF
    door_locked = 1;
    
    while (1)
    {
        unsigned char k;

        update_lighting();
        update_thermostat();
        update_garage();
        update_security();
        update_door_status();

        if (current_menu == 0)
        {
            /* Home screen — refresh every ~20 * 2ms = ~40ms */
            if (lcd_tick++ >= 20)
            {
                lcd_tick = 0;
                menu_home();
            }

            /* Check for menu selection key */
            k = keypad_getkey();
            if      (k == '1') { lcd_clear(); current_menu = 1; menu_clock();      current_menu = 0; lcd_clear(); }
            else if (k == '2') { lcd_clear(); current_menu = 2; menu_thermostat(); current_menu = 0; lcd_clear(); }
            else if (k == '3') { lcd_clear(); current_menu = 3; menu_garage();     current_menu = 0; lcd_clear(); }
            else if (k == '4') { lcd_clear(); current_menu = 4; menu_security();   current_menu = 0; lcd_clear(); }
            else if (k == '5') { lcd_clear(); current_menu = 5; menu_door_lock();  current_menu = 0; lcd_clear(); }
            else if (k == '6') { lcd_clear(); current_menu = 6; menu_settings();   current_menu = 0; lcd_clear(); }
        }

        __delay_ms(2);
    }
}
