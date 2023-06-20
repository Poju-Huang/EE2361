/* 
 * File:   Final_Testing_main_v001.c
 *
 * Created on April 9, 2023, 2:28 PM
 */

#include <xc.h>
#include <stdio.h>
#include <stdlib.h>
#include "Final_Testing_asmLib_v001.h"
#include "stdint.h"
#include "string.h"
// CW1: FLASH CONFIGURATION WORD 1 (see PIC24 Family Reference Manual 24.1)
#pragma config ICS = PGx1          // Comm Channel Select (Emulator EMUC1/EMUD1 pins are shared with PGC1/PGD1)
#pragma config FWDTEN = OFF        // Watchdog Timer Enable (Watchdog Timer is disabled)
#pragma config GWRP = OFF          // General Code Segment Write Protect (Writes to program memory are allowed)
#pragma config GCP = OFF           // General Code Segment Code Protect (Code protection is disabled)
#pragma config JTAGEN = OFF        // JTAG Port Enable (JTAG port is disabled)


// CW2: FLASH CONFIGURATION WORD 2 (see PIC24 Family Reference Manual 24.1)
#pragma config I2C1SEL = PRI       // I2C1 Pin Location Select (Use default SCL1/SDA1 pins)
#pragma config IOL1WAY = OFF       // IOLOCK Protection (IOLOCK may be changed via unlocking seq)
#pragma config OSCIOFNC = ON       // Primary Oscillator I/O Function (CLKO/RC15 functions as I/O pin)
#pragma config FCKSM = CSECME      // Clock Switching and Monitor (Clock switching is enabled, 
                                       // Fail-Safe Clock Monitor is enabled)
#pragma config FNOSC = FRCPLL      // Oscillator Select (Fast RC Oscillator with PLL module (FRCPLL))

#define BUFSIZE 1024
#define NUMSAMPLES 128

void lcd_cmd(char command);
void lcd_init(void);
void pic24_init();
void adc_init();
void initBuffer();
void timer1_init();
void lcd_printChar(char myChar);
void lcd_printStr(const char s[]);
void delay(int delay_in_1_ms);
void putVal(uint16_t ADCvalue);
void lcd_setCursor(char x, char y);

void ADC_SelectInput();

volatile char contrast = 0b01110000;  //contrast Low = 0b01110000
uint16_t adc_buffer0[BUFSIZE];
volatile int buffer_index0 = 0;
uint16_t adc_buffer1[BUFSIZE];
volatile int buffer_index1 = 0;
volatile int Channel = 1;

/*
 * 
 */

void delay(int delay_in_1_ms){
    int i = 0; //initializes i
    while (i < delay_in_1_ms){ //sets the loop to run equal to the value represented by delay_in_ms
        delay_1ms(); //delays 100us for each iteration
        i++; //iterates i
    }
}

void ADC_SelectInput(){
    if(Channel == 0){
        AD1CHS = 0b00000001;
        Channel = 1;
    }
    else{
        AD1CHS = 0b00000000;
        Channel = 0;
    }
}

void initBuffer(){
    int i;
    for(i=0;i<BUFSIZE;i++){
        adc_buffer0[i] = 0;
        adc_buffer1[i] = 0;
    }
}

void putVal0(uint16_t ADCvalue){
    adc_buffer0[buffer_index0++] = ADCvalue;
    if(buffer_index0 >= BUFSIZE){
        buffer_index0 = 0;
    }
}

void putVal1(uint16_t ADCvalue){
    adc_buffer1[buffer_index1++] = ADCvalue;
    if(buffer_index1 >= BUFSIZE){
        buffer_index1 = 0;
    }
}

void pic24_init(){
    CLKDIVbits.RCDIV = 0;
    AD1PCFG = 0xffff;
}

void lcd_init(){
    I2C2CONbits.I2CEN = 0; //Disable I2C Mode
    I2C2BRG = 157; //set to clock frequency of 100 kHz
    IFS3bits.MI2C2IF = 0; //Clear Interrupt Flag
    I2C2CONbits.I2CEN = 1; //Enable I2C Mode
    
    delay(40);
    
    lcd_cmd(0b00111000); // function set
    lcd_cmd(0b00111001); //function set, advance instruction mode
    lcd_cmd(0b00010100); //interval osc
    lcd_cmd(contrast); //contrast
    lcd_cmd(0b01010110);
    lcd_cmd(0b01101100); //follower control
    
    delay(200);
    
    lcd_cmd(0b00111000); //function set
    lcd_cmd(0b00001100); //Display On
    lcd_cmd(0b00000001); // Clear Display
    
    delay(1);   
}

void adc_init(){
    TRISAbits.TRISA0 = 1;
    TRISAbits.TRISA1 = 1;
    
    AD1PCFGbits.PCFG0 = 0; //setup I/O
    AD1PCFGbits.PCFG1 = 0; //setup I/O
    
    AD1CON2bits.VCFG = 0b000;  //Use AVDD (3.3V) and AVSS (0V) as max/min)
    AD1CON3bits.ADCS = 0b00000001;  //You want TAD >= 75ns (Tcy = 62.5ns)
    AD1CON1bits.SSRC = 0b010;  //sample on timer3 events
    AD1CON3bits.SAMC = 0b00001;  //You want at least 1 auto sample time bit
    AD1CON1bits.FORM = 0b00; //data output form --recommended unsigned int
                            //unsigned:0V = 0b0000000000, 3.3V = 0b1111111111
                            //signed: 0V = 0b1000000000, 3.3V = 0b011111111
    AD1CON1bits.ASAM = 0b1;  //read reference manual
    AD1CON2bits.SMPI = 0b0000;  //read reference manual
    
    _AD1IF = 0; //clear IF
    _AD1IE = 1; //enable interrupts
    AD1CHS = 0b00000001; //for pin AN0, 0b00000001 for AN1
    AD1CON1bits.ADON = 1;  //turn on ADC
    
    TMR3 = 0; //setup timer3
    T3CON = 0;
    T3CONbits.TCKPS = 0b10;
    PR3 = 7812;  //15624/2
    T3CONbits.TON = 1;
}

void timer1_init(){
    T1CONbits.TON = 0;
    T1CONbits.TCKPS = 0b01;
    PR1 = 49999;
    _T1IE = 0;
    T1CONbits.TON = 1;
}

void lcd_cmd(char command){
    I2C2CONbits.SEN = 1;	//Initiate Start condition
    while(I2C2CONbits.SEN == 1);  // SEN will clear when Start Bit is complete
    IFS3bits.MI2C2IF = 0;
    I2C2TRN = 0b01111100; // 8-bits consisting of the slave address and the R/nW bit
    while(IFS3bits.MI2C2IF == 0); // *Refer to NOTE below*
    IFS3bits.MI2C2IF = 0;
    I2C2TRN = 0b00000000; // 8-bits consisting of control byte
    while (IFS3bits.MI2C2IF == 0);
    IFS3bits.MI2C2IF = 0;
    I2C2TRN = command; // 8-bits consisting of the data byte
    while(IFS3bits.MI2C2IF == 0);
    IFS3bits.MI2C2IF = 0;
    I2C2CONbits.PEN = 1;
    while(I2C2CONbits.PEN==1); // PEN will clear when Stop bit is complete
}

void lcd_printChar(char Package) {
    I2C2CONbits.SEN = 1;	//Initiate Start condition
    while(I2C2CONbits.SEN == 1); // SEN will clear when Start Bit is complete
    IFS3bits.MI2C2IF = 0;
    I2C2TRN = 0b01111100; // 8-bits consisting of the slave address and the R/nW bit
    while(IFS3bits.MI2C2IF == 0);
    IFS3bits.MI2C2IF = 0;
    I2C2TRN = 0b01000000; // 8-bits consisting of control byte /w RS=1
    while(IFS3bits.MI2C2IF == 0);
    IFS3bits.MI2C2IF = 0;
    I2C2TRN = Package; // 8-bits consisting of the data byte
    while(IFS3bits.MI2C2IF == 0);
    I2C2CONbits.PEN = 1;
    while(I2C2CONbits.PEN == 1); // PEN will clear when Stop bit is complete
}

void lcd_printStr(const char s[]){
    I2C2CONbits.SEN = 1;	//Initiate Start condition
    while(I2C2CONbits.SEN == 1); // SEN will clear when Start Bit is complete
    IFS3bits.MI2C2IF = 0;
    I2C2TRN = 0b01111100; // 8-bits consisting of the slave address and the R/nW bit
    while(IFS3bits.MI2C2IF == 0);
    IFS3bits.MI2C2IF = 0;
    
    int string_length = strlen(s);
    string_length = string_length - 1;
    int i = 0;
    while(i <= string_length){
        IFS3bits.MI2C2IF = 0;
        I2C2TRN = 0b11000000; // Control Byte: CO = 1 RS = 1
        while(IFS3bits.MI2C2IF == 0);
        IFS3bits.MI2C2IF = 0;
        I2C2TRN = s[i]; // 8-bits consisting of the data byte
        while(IFS3bits.MI2C2IF == 0);
        i = i + 1;
    }
    //i = i + 1;
    IFS3bits.MI2C2IF = 0;
    I2C2TRN = 0b01000000; // Control Byte: CO = 0 RS = 1 "last byte"
    while(IFS3bits.MI2C2IF == 0);
    IFS3bits.MI2C2IF = 0;
    I2C2TRN = s[i]; // 8-bits consisting of the data byte
    while(IFS3bits.MI2C2IF == 0);
    IFS3bits.MI2C2IF = 0;
    I2C2CONbits.PEN = 1;
    while(I2C2CONbits.PEN == 1); // PEN will clear when Stop bit is complete
}

void lcd_setCursor(char x, char y){
    int pos_num = (0x40 * y) + x;
    char DDRAMaddress = (0b10000000 | pos_num);
    lcd_cmd(DDRAMaddress);
}

void loop(){
    char adStr0[8];
    uint32_t total_for_avg0 = 0;
    int index_check0 = 0;
    uint32_t avg0 = 0;
    int pos0;
    char adStr1[8];
    uint32_t total_for_avg1 = 0;
    int index_check1 = 0;
    uint32_t avg1 = 0;
    int pos1;
    while(1){
        total_for_avg0 = 0;
        index_check0 = 0;
        avg0 = 0;
        total_for_avg1 = 0;
        index_check1 = 0;
        avg1 = 0;
//        int adValue;
        if(_T1IF == 1){
            _T1IF = 0;
            pos0 = buffer_index0;
            pos1 = buffer_index1;
            for(int i = 0; i < NUMSAMPLES; i++){
                index_check0 = pos0 - i -1;
                index_check1 = pos1 - i -1;
                if(index_check0 < 0){
                    index_check0 = BUFSIZE + index_check0;
                }
                if(index_check1 < 0){
                    index_check1 = BUFSIZE + index_check1;
                }
                total_for_avg0 = total_for_avg0 + adc_buffer0[index_check0];
                total_for_avg1 = total_for_avg1 + adc_buffer1[index_check1];
            }
            avg0 = total_for_avg0/NUMSAMPLES;
            avg1 = total_for_avg1/NUMSAMPLES;
            sprintf(adStr0, "%6.4f V", (3.3/1024)*avg0);
            sprintf(adStr1, "%6.4f V", (3.3/1024)*avg1);
            lcd_setCursor(0,0);
            lcd_printStr(adStr0);
            lcd_setCursor(0,1);
            lcd_printStr(adStr1);
        }
    }
}

void __attribute__((interrupt, auto_psv)) _ADC1Interrupt(){
    _AD1IF = 0;
    if(Channel == 0){
        putVal0(ADC1BUF0);
        ADC_SelectInput();
    }
    else{
        putVal1(ADC1BUF0);
        ADC_SelectInput();
    }
}

int main(void) {
    pic24_init();
    lcd_init();
    adc_init();
    timer1_init(); //init a timer for 100 ms. 
    initBuffer();
    
    loop(); //if the timer overflows print the average voltage value
              //from the buffer screen (converting a value to a string
              //then reset timer interrupt flag)

    return 0;
}

