# PIC12F675-TM1637-Display-Code
Demo C code which drives a TM1637 display using a PIC 12F675, compiles using MPLAB XC8 compiler
A port of part of copyright code written by Dan C: https://github.com/electro-dan/PIC12F_TM1637_Thermometer
which was originally compiled with the Boost C compiler. Uploaded with original author permission.
Developed and compiled using the MPLAB X IDE v6.05. All code is in a single file, no header dependencies 
apart from the MPLAB xc.h. It should port to other compilers pretty easily, I have tried to minimise
MPLAB XC8 dependencies which are mainly PIC register definitions and use of the MPLAB __delay inline function.

The PIC12F675 is a small chip ideal for driving commercially available TM1637 modules with minimal
additional board footprint or additional components. As coded timing is using the on-chip oscillator.
With appropriate adaptation it should be possible to drive displays using more than the 4 digits as 
were used for test purposes. Memory resources used by the C code approximate to 50% for display only 
though this allows some headroom to add for example ADC code for analogue inputs or I2C code for 
interface with other chips.

My most recent commit added example code for PIC12F675 analogue input and TM1637 display of the data.
A 0-5V signal is converted using the PIC's 10 bit ADC and displayed on the TM1637 rounded to 3 digits.
Integer maths is used for scaling of the raw ADC data, floating point is not really an option on this 
chip. Note that rounding adds overhead and as coded in C my ADC example code uses approx 90% of 
program memory. I am sure the code could be further optimised or the rounding code simply removed
if not needed.

Beware of in circuit programming issues coding for this small PIC given that the programming pins are almost
inevitably shared with inputs or other circuit components. In particular connecting AN0/ICSPDAT to the
Vcc power rail via a fairly low impedance will cause programmer errors. This happens if you use a lowish value
potentiometer as an input voltage source for example. Failed programming can trash the PICs calibration data 
permanently, you have been warned! Isolate ICSPDAT and ICSPCLK from the power rails when programming.

Note that the TM1637 module used can be made to communicate faster than the speed used in the demo code, see
my description .pdf file

Steve 6/2023
