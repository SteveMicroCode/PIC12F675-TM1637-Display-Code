# PIC12F675-TM1637-Display-Code
Demo C code which drives a TM1637 display using a PIC 12F675, compiles using MPLAB XC8 compiler
A port of copywrited code written by Dan C: https://github.com/electro-dan/PIC12F_TM1637_Thermometer
which was originally compiled with the Boost C compiler. Uploaded with original author permission.
Developed and compiled using the MPLAB X IDE v6.05
The PIC12F675 is a small chip ideal for driving commercially available TM1637 modules with minimal
additional board footprint or additional components. As coded timing is using the on-chip oscillator.
With appropriate adaptation it should be possible to drive displays using more than the 4 digits 
used for test purposes by me. Memory resources used by the C code approximate to 50% though there is
headroom to add for example ADC code for analogue inputs or I2C code for interface with other chips.
