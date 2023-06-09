
// ---------------------------------------------------------------------
// Microchip 12F675/TM1637 ADC and display code written/adapted by Steve Williams
// for Microchip's MPLAB XC8 compiler. The TM1637 display routines were originally
// written by electro-dan for the BoostC compiler as part of project, the ADC
// related coding was written by me.
// See also: https://github.com/electro-dan/PIC12F_TM1637_Thermometer
// The code reads a single ADC channel AN0(pin7) and could be adapted to read more
// The main ADC read loop is non-blocking though the TM1637 display writes are not
// The ADC is read at 1 second intervals. The code leaves it turned on, not power optimised
// Rounding adds significant overhead, could save program memory by removing if needed
// 
// No warranty is implied and the code is for test use at users own risk. 
// 
// Hardware configuration for the PIC 12F675:
// GP0 = ADC input (pin 7)
// GP1 = OUT: N/C
// GP2 = in original code IN/OUT for DS18B20 (not used by this demo)
// GP3 = OUT: N/C
// GP4 = IN/OUT: TM1637 DIO
// GP5 = IN/OUT: TM1637 CLK
// -----------------------------------------------------------------------


#include <xc.h>

// CONFIG as generated by Microchip IDE:
#pragma config FOSC = INTRCIO   // Oscillator Selection bits (INTOSC oscillator: I/O function on GP4/OSC2/CLKOUT pin, I/O function on GP5/OSC1/CLKIN)
#pragma config WDTE = OFF       // Watchdog Timer Enable bit (WDT disabled)
#pragma config PWRTE = ON       // Power-Up Timer Enable bit (PWRT enabled)
#pragma config MCLRE = OFF      // GP3/MCLR pin function select (GP3/MCLR pin function is digital I/O, MCLR internally tied to VDD)
#pragma config BOREN = ON       // Brown-out Detect Enable bit (BOD enabled)
#pragma config CP = OFF         // Code Protection bit (Program Memory code protection is disabled)
#pragma config CPD = OFF        // Data Code Protection bit (Data memory code protection is disabled)

#define _XTAL_FREQ 4000000      // Define clock frequency used by xc8 __delay(time) functions

// Set the TM1637 module data and clock pins:
#define trisConfiguration 0b00110000; // TM1637 GP4/5 pins are inputs, TM1637 module pullups 
                                      // will take these high. Configuration here of display pins ONLY 
#define tm1637dio GP4                 // Set the i/o ports names for TM1637 data and clock here
#define tm1637dioTrisBit 4            // This is the bit shift to set TRIS for GP4
#define tm1637clk GP5
#define tm1637clkTrisBit 5

//Timer1 definitions:
#define T1PRESCALE 01                  // 2 bits control, 01 = 1:2
#define T1CLK 1                        // If set T1 uses internal clock
#define TIMER1ON 0x01                  // Used to set bit0 T1CON = Timer1 ON
// Timer1 setup values for 1ms interrupt using a preload:
#define TIMER1LOWBYTE 0xFF             // 50000 cycles @ 1:2 prescale == 100ms.Preload no delays = 65536-50000
#define TIMER1HIGHBYTE 0x20            // = 15536 = 0x3C60.Tho lower preload gave accurate time

//General global variables:
volatile uint8_t timer1Flag = 0;               // Flag is set by Timer 1 ISR every 100ms
uint8_t ADCreadcounter = 0;                    // Counts intervals for ADC task in 100ms increments
uint8_t ADCreadStatus = 0;                     // Stage of ADC conversion task, 0 = not started
uint8_t LEDcounter = 0;                        // Used to time non-blocking LED flash in 100ms increments
uint8_t LEDonTime = 0;                         // If true LED flash routine is called, flashes N x 100ms 

//ADC definitions:
#define NOCONVERSION 0
#define STARTADCREAD 1
#define CONVERTING 2

//ADC variables:
const uint16_t RefmV = 5000;          // Specify Vref in mV
const uint8_t ADCinputConfig = 0x01;  // Setting bit 0..3 enables ADC inputs 0..3, used to set TRISIO and ANSEL
const uint8_t ADCchannel = 0;         // Active ADC channel, AN0 = 0..AN3 = 3

//Display variables:
const uint8_t tm1637ByteSetData = 0x40;        // 0x40 [01000000] = Indicate command to display data
const uint8_t tm1637ByteSetAddr = 0xC0;        // 0xC0 [11000000] = Start address write out all display bytes 
const uint8_t tm1637ByteSetOn = 0x88;          // 0x88 [10001000] = Display ON, plus brightness
const uint8_t tm1637ByteSetOff = 0x80;         // 0x80 [10000000] = Display OFF 
const uint8_t tm1637MaxDigits = 4;
const uint8_t tm1637RightDigit = tm1637MaxDigits - 1;
// Used to output the segment data for numbers 0..9 :
const uint8_t tm1637DisplayNumtoSeg[] = {0x3f, 0x06, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x07, 0x7f, 0x6f};
uint8_t tm1637Brightness = 5;         // Range 0 to 7
uint8_t tm1637Data[] = {0, 0, 0, 0};  // Digit numeric data to display,array elements are for digits 0..3
uint8_t decimalPointPos = 99;         // Flag for decimal point (digits counted from left),if > MaxDigits dp off
uint8_t zeroBlanking = 0;             // If set true blanks leading zeros
uint8_t numDisplayedDigits = 3;       // Limits total displayed digits, used after rounding a decimal value

// ISR Handles Timer1 interrupt:
void __interrupt() ISR(void);  // Note XC8 interrupt function setup syntax using __interrupt() + myisr()
void initialise(void);
void LEDflash(void);
uint16_t readADC(void);        // Returns ADC Vin in mV, ie 5000 max if Vref if Vref = 5V
void tm1637StartCondition(void);
void tm1637StopCondition(void);
uint8_t tm1637ByteWrite(uint8_t bWrite);
void tm1637UpdateDisplay(void);
void tm1637DisplayOn(void);
void tm1637DisplayOff(void);
uint8_t getDigits(unsigned int number);   //Extracts decimal digits from integer, populates tm1637Data array
void roundDigits(void);


void main(void)
{
  uint16_t displayedInt=0;       // Beware 65K limit if larger than 4 digit display,consider using uint32_t
  uint16_t ctr = 0;
  _delay(100);
  initialise();
  zeroBlanking = 0;              // Don't blank leading zeros
  decimalPointPos = 0;           // Display 0-5000mV as n.nnn volts, digit 0 = leftmost
  getDigits(displayedInt);
  tm1637UpdateDisplay();         // Display zero then start timed conversions, updating display as completed
  ADCreadcounter = 0;            // Start with timing counts at zero, both ADC read and timer1 flags
  timer1Flag = 0; 
  T1CON |= TIMER1ON;
  while(1)
    {
      if (timer1Flag)
        {
           ADCreadcounter ++;                    // Update task interval and LED timing flags
           LEDcounter ++;                           
           timer1Flag = 0;                       // Clear the 100ms timing flag
        }
      
      if (ADCreadcounter >= 10)                  // Start ADC read process every 100 ms
        { 
           ADCreadcounter = 0;
           ADCreadStatus = STARTADCREAD;         // Setting to 1 = start of ADC read 
        }
      
      switch (ADCreadStatus)             // The ADC read/display task is managed by ADCreadStatus control flag
      {
              case NOCONVERSION:
                  break;
              case STARTADCREAD:                 // nb. must only start ADC conversions after Taq since last
                  ADCON0 |= 0x02;                // Set GO/DONE, bit 1, to start conversion
                  ADCreadStatus = CONVERTING;
                  LEDcounter = 0;                // Zero the LED time counter, note counts 100ms increments
                  LEDonTime = 1;                 // Sets up a 500ms LED flash
                  break;
              case CONVERTING:                   // Polls GO/DONE for completed conversion,COULD ADD TIMEOUT?
                  if (!(ADCON0 & 0x20))
                  {
                      displayedInt = readADC();  // Get the ADC data and convert to integer, Vin in mV
                      getDigits(displayedInt);   // Extract digit data from integer into 4x uint8_t array 
                      roundDigits();             // Apply rounding to the array data if <4 digits displayed
                      tm1637UpdateDisplay();
                      ADCreadStatus = NOCONVERSION;  // Consider adding a timed delay before reset this flag
                  }
                  break;
      }
             
      if (LEDonTime)                              // Call the LED flash function if a count is set
          LEDflash();   
    }                       //while(1)
}                           //main



//***************************************************************************************
// Interrupt service routine:
//***************************************************************************************

void ISR(void)
{ 
    //GP2 = 1;  //LED on
    if (PIR1 & 0x01)                  // Check Timer1 interrupt flag bit 0 is set
    {
        PIR1 &= 0xFE;                 // Clear interrupt flag bit 0
                                     // Reset Timer1 preload for 100ms overflow/interrupt(nb running timer)
        TMR1H = TIMER1HIGHBYTE;       // Note some timing inaccuracy due to interrupt latency + reload time
        TMR1L = TIMER1LOWBYTE;        // Correction was therefore applied to low byte to improve accuracy 
        timer1Flag = 1;               
        
    }
}

//*******************************************************************************************
//Functions: 
//*******************************************************************************************

void LEDflash(void)
{
    if (LEDcounter <= LEDonTime)
    {
        GP2 = 1;                      // LED on
    }
    else
    {
        GP2 = 0;                      // LED off
        LEDonTime = 0;                // Stop the flash
    }
}

//********************************************************************************************
// readADC() reads the 10 bit ratiometric ADCvalue (Vin/Vref). Integer arithmetic is used 
// for conversion to a 32 bit unsigned integer value which is Vin in mV.Note XC8 integers
// are limited to 32 bit. This method suitable for 10 bit ADC conversions only and gives 
// minimal loss of resolution given that we get 5mV per bit @ 5V Vref.
//********************************************************************************************

uint16_t readADC(void)                  // Returns a 16 bit unsigned integer, Vin in mV
{
    uint32_t ADCmVfractional = 0;       // ADC -> mV calculation will require 32 bit integer arithmetic
    uint16_t ADCval = ADRESL;           // ADC result is a 10 bit number, read lower 8 bits
    ADCval |= (uint16_t)ADRESH << 8;    // Get bits 8/9 of the result,storing as as 16 bit integer
    uint32_t ADCmV = (uint32_t)RefmV * (uint32_t)ADCval; // nb need a 32 bit value RHS to get 32bit arithmetic
    ADCmVfractional = ADCmV & 0x000003FF;                // Store the lower 16 bit binary part before division
    ADCmV >>= 10;                       // Divide by 1024, result is the whole mV part of conversion
    // The binary rounding here is optional, uses code space and adds limited additional accuracy:
    if (ADCmVfractional>0x200)    // Next look at the fractional binary part, round up if >0.5 mV
      ADCmV += 1;                 
    return((uint16_t)ADCmV);      // OK to cast result to 16 bit as value is less than 2^16(65536)  
}


/*********************************************************************************************
 tm1637UpdateDisplay()
 Publish the tm1637Data array to the display
*********************************************************************************************/
void tm1637UpdateDisplay()
{   
    uint8_t tm1637DigitSegs = 0;
    uint8_t ctr;
    uint8_t stopBlanking = !zeroBlanking;            // Allow blanking of leading zeros if flag set
            
    // Write 0x40 [01000000] to indicate command to display data - [Write data to display register]:
    tm1637StartCondition();
    tm1637ByteWrite(tm1637ByteSetData);
    tm1637StopCondition();

    // Specify the display address 0xC0 [11000000] then write out all 4 bytes:
    tm1637StartCondition();
    tm1637ByteWrite(tm1637ByteSetAddr);
    for (ctr = 0; ctr < tm1637MaxDigits; ctr ++)
    {
        tm1637DigitSegs = tm1637DisplayNumtoSeg[tm1637Data[ctr]];
        if (!stopBlanking && (tm1637Data[ctr]==0))  // Blank leading zeros if stop blanking flag not set
            {
               if (ctr < tm1637RightDigit)          // Never blank the rightmost digit
                  tm1637DigitSegs = 0;              // Segments set 0x00 gives blanked display numeral
            }
        else
        {
           stopBlanking = 1;                    // Stop blanking if have reached a non-zero digit
           if (ctr==decimalPointPos)            // Flag for presence of decimal point, digits 0..3
           {                                    // No dp display if decimalPointPos is set > Maxdigits
               tm1637DigitSegs |= 0b10000000;   // High bit of segment data is decimal point, set to display
           }
        }
        if (ctr>(numDisplayedDigits-1))
            tm1637DigitSegs = 0;             // Segments set 0x00 blanks,limits displayed digits left to right
        
        tm1637ByteWrite(tm1637DigitSegs);       // Finally write out the segment data for each digit
    }
    tm1637StopCondition();

    // Write 0x80 [10001000] - Display ON, plus brightness
    tm1637StartCondition();
    tm1637ByteWrite((tm1637ByteSetOn + tm1637Brightness));
    tm1637StopCondition();
}


/*********************************************************************************************
 tm1637DisplayOn()
 Send display on command
*********************************************************************************************/
void tm1637DisplayOn(void)
{
    tm1637StartCondition();
    tm1637ByteWrite((tm1637ByteSetOn + tm1637Brightness));
    tm1637StopCondition();
}


/*********************************************************************************************
 tm1637DisplayOff()
 Send display off command
*********************************************************************************************/
void tm1637DisplayOff(void)
{
    tm1637StartCondition();
    tm1637ByteWrite(tm1637ByteSetOff);
    tm1637StopCondition();
}

/*********************************************************************************************
 tm1637StartCondition()
 Send the start condition
*********************************************************************************************/
void tm1637StartCondition(void) 
{
    TRISIO &= ~(1<<tm1637dioTrisBit);  //Clear data tris bit
    tm1637dio = 0;                     //Data output set low
    __delay_us(100);
}


/*********************************************************************************************
 tm1637StopCondition()
 Send the stop condition
*********************************************************************************************/
void tm1637StopCondition() 
{
    TRISIO &= ~(1<<tm1637dioTrisBit);   // Clear data tris bit
    tm1637dio = 0;                      // Data low
    __delay_us(100);
    TRISIO |= 1<<tm1637clkTrisBit;      // Set tris to release clk
    //tm1637clk = 1;
    __delay_us(100);
    // Release data
    TRISIO |= 1<<tm1637dioTrisBit;      // Set tris to release data
    __delay_us(100);
}


/*********************************************************************************************
 tm1637ByteWrite(char bWrite)
 Write one byte
*********************************************************************************************/
uint8_t tm1637ByteWrite(uint8_t bWrite) {
    for (uint8_t i = 0; i < 8; i++) {
        // Clock low
        TRISIO &= ~(1<<tm1637clkTrisBit);   // Clear clk tris bit
        tm1637clk = 0;
        __delay_us(100);
        
        // Test bit of byte, data high or low:
        if ((bWrite & 0x01) > 0) {
            TRISIO |= 1<<tm1637dioTrisBit;      // Set data tris 
        } else {
            TRISIO &= ~(1<<tm1637dioTrisBit);   // Clear data tris bit
            tm1637dio = 0;
        }
        __delay_us(100);

        // Shift bits to the left:
        bWrite = (bWrite >> 1);
        TRISIO |= 1<<tm1637clkTrisBit;      // Set tris so clk goes high
        __delay_us(100);
    }

    // Wait for ack, send clock low:
    TRISIO &= ~(1<<tm1637clkTrisBit);      // Clear clk tris bit
    tm1637clk = 0;
    TRISIO |= 1<<tm1637dioTrisBit;         // Set data tris, makes input
    tm1637dio = 0;
    __delay_us(100);
    
    TRISIO |= 1<<tm1637clkTrisBit;         // Set tris so clk goes high
    __delay_us(100);
    uint8_t tm1637ack = tm1637dio;
    if (!tm1637ack)
    {
        TRISIO &= ~(1<<tm1637dioTrisBit);  // Clear data tris bit
        tm1637dio = 0;
    }
    __delay_us(100);
    TRISIO &= ~(1<<tm1637clkTrisBit);      // Clear clk tris bit, set clock low
    tm1637clk = 0;
    __delay_us(100);

    return 1;
}


/*********************************************************************************************
  Function called once only to initialise variables and
  setup the PIC registers
*********************************************************************************************/
void initialise()
{
    GPIO = 0b00000000;             // all pins low by default
    TRISIO = trisConfiguration;    // All pins set as digital outputs other than GP 4/5(TM1637)
    TRISIO |= ADCinputConfig;      // Setting bit 0..3 sets digital i/o 0..3 to input(high impedance)
    CMCON = 7;                     // comparator off
    ANSEL = 0x10;                  // Init ADC with 8Tosc ADC conversion time
    ANSEL |= ADCinputConfig;       // Setup analogue inputs ANS3..0, bit 0..3 set enables each analogue input
    ADCON0 = 0x81;                 // ADC initialised for right justified data, ADC turned on (bit 0)
    ADCON0 |= ADCchannel<<2;       // Set the active ADC channel, bits 2/3 CHS0 CHS1, 0 = AN0 ..3 = AN3
    T1CON = 0;                     // Clear T1 control bits
    T1CON |= (T1PRESCALE<<4);      // Bits 4-5 set prescale, 01 = 1:2
    T1CON |= (T1CLK<<2);           // Bit 2 set enables disables external clock input 
    TMR1L = TIMER1LOWBYTE;         // Set Timer1 preload for 1ms overflow/interrupt
    TMR1H = TIMER1HIGHBYTE; 
    PIE1 = 0x01;                   // Timer 1 interrupt enable bit 1 set, other interrupts disabled
    PIR1 &= 0xFE;                  // Clear Timer1 interrupt flag bit 1
    INTCON |= 0xC0;                // Enable interrupts, general - bit 7 plus peripheral - bit 6 
}


/*************************************************************************************************
 getDigits extracts decimal digit numbers from an integer for the display, note max displayed value is 
 9999 for 4 digit display, truncation of larger numbers. Larger displays: note maximum 65K as coded with 
 16 bit parameter - probable need to declare number as uint32_t if coding for a 6 digit display  
 ************************************************************************************************/

uint8_t getDigits(uint16_t number)
{ 
    int8_t ctr = (tm1637RightDigit);            // Start processing for the rightmost displayed digit
    for (uint8_t ctr2 = 0; ctr2 < tm1637MaxDigits; ctr2++)
    {
        tm1637Data[ctr2]=0;      //Initialise the display data array with 0s
    }
    while(number > 0)            //Do if number greater than 0, ie. until all number's digits processed
    {
        if (ctr >= 0)
        {
           uint16_t modulus = number % 10;      // Split last digit from number
           tm1637Data[ctr] = (uint8_t)modulus;  // Update display character array
           number = number / 10;                // Divide number by 10
           ctr --;                              // Decrement digit counter to process number from right to left
        }
        else
        {
           number = 0;                          // Stop processing if have exceeded display's no of digits
        }
    }
    return 1;
}

//*****************************************************************************************
// roundDigits applies decimal rounding to digit data stored in tm1637Data array
// processing the digit data from right (least significant) to left. As written 
// the function can only round for removal of single rightmost digit. Could be developed.
//*****************************************************************************************

void roundDigits(void)
{
    int8_t digit = tm1637RightDigit;        // Current digit being processed, 0..3 L->R
    uint8_t carry = 0;                      // Carry is set to add 1 to previous digit
    for (digit = tm1637RightDigit; digit >= 0; digit -- )
    {
        if (digit == tm1637RightDigit)
        {
            if (tm1637Data[digit]>5)                  // Round up if rightmost digit >5 by setting carry
                carry = 1;
            tm1637Data[digit] = 0;                    // Processed digit is set to zero
        }
        
        if (digit < tm1637RightDigit)
        {
            tm1637Data[digit] += carry;               // Add any carry back from previous rounding
            if (tm1637Data[digit]>9)
            {
                tm1637Data[digit]=0;
                carry = 1;                            // Set a carry back to previous digit
            }
            else
                carry = 0;
        }
    }
}
