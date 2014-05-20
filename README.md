DS3231RTC
=========

Basic sketch for the DS3231 Real Time Clock

This is the version of the Maxim Integrated Extremely Accurate Real Time Clock that uses the I2C communication protocol. The DS3234 uses SPI.

The claimed accuracy is +/- 2 ppm in the 0 to 40 C temperature interval, which is just over one minute per year.

This sketch shows how to read from and write to the device registers, how to set the time, date, and year, how to read time, date, and year, how to read temperature in degrees Centigrade, and how to display all on a Nokia 5110 68 x 84 pixel display. Also shown is the use of the two RTC alarms with either status register polling or hardware interrupt. There is an ATMEL 32K byte EEPROM on the ZS-042 breakout board used here that can be used for data logging and/or to store alarm sound. 

Added EEPROM read/write functions so sounds can be stored for later playing upon the hour or when alarms are triggered. Making use of the Arduino tone() function to drive a speaker to play the sounds read from and stored in the EEPROM as sound frequency (0 - 255 Hz, which covers more than one octave) bytes. The 128 pages (each of 32 bytes) capacity of the EEPROM is more than enough memory to store sounds for all occasions needed by even the most elaborate clock.
