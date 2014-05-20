/* DS3231 Real Time Clock Basic Example  
 by: Kris Winer
 date: May 20, 2014
 license: Beerware - Use this code however you'd like. If you 
 find it useful you can buy me a beer some time.
 
 Demonstrate DS3231 Real Time clock functions. Breakout board comes with an  ATMEL I2C Serial EEPROM with
 32K (4096 x 8) bits of memory., which we will figure out how to use. 
 Both main devices support 400 kHz I2C, which we will use.
 Output time, date, year etc. to Nokia 5100 Display and QDSP-6064 bubble display.
 
 SDA and SCL should have external pull-up resistors (to 3.3V).
 4700 resistors are on the breakout ZS-042 board.
 
 Hardware setup:
 DS3231 Breakout --------- Arduino
 3.3V --------------------- 3.3V
 SDA ----------------------- A4
 SCL ----------------------- A5
 GND ---------------------- GND
 
  Note: The DS3231 and AT24C32 devices use I2C and we will use the Arduino Wire library. 
  We are using a 3.3 V 8 MHz Pro Mini or a 3.3 V Teensy 3.1.
  We have disabled the internal pull-ups used by the Wire library in the Wire.h/twi.c utility file.
  We are also using the 400 kHz fast I2C mode by setting the TWI_FREQ  to 400000L /twi.h utility file.
 */
 
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_PCD8544.h>

// Using NOKIA 5110 monochrome 84 x 48 pixel display
// pin 9 - Serial clock out (SCLK)
// pin 8 - Serial data out (DIN)
// pin 7 - Data/Command select (D/C)
// pin 5 - LCD chip select (CS)
// pin 6 - LCD reset (RST)
Adafruit_PCD8544 display = Adafruit_PCD8544(9, 8, 7, 5, 6);

// Define registers per Extremely Accurate I2C Integrated RTC/TCXO/Crystal data sheet 
// http://datasheets.maximintegrated.com/en/ds/DS3231.pdf
//
#define SECOND             0x00                
#define MINUTE             0x01                                                                          
#define HOUR               0x02
#define DAY                0x03  
#define DATE               0x04
#define MONTH              0x05
#define YEAR               0x06  
#define ALARM_1_SECONDS    0x07
#define ALARM_1_MINUTES    0x08
#define ALARM_1_HOURS      0x09
#define ALARM_1_DAY_DATE   0x0A
#define ALARM_2_MINUTES    0x0B
#define ALARM_2_HOURS      0x0C
#define ALARM_2_DAY_DATE   0x0D    
#define CONTROL            0x0E
#define STATUS             0x0F
#define AGING_OFFSET       0x10
#define MSB_TEMP           0x11
#define LSB_TEMP           0x12

#define AT24C32_ADDRESS    0x57  // On-board 32k byte EEPROM; 128 pages of 32 bytes each

// Using the GY-521 breakout board, I set ADO to 0 by grounding through a 4k7 resistor
// Seven-bit device address is 110100 for ADO = 0 and 110101 for ADO = 1
#define ADO 0
#if ADO
#define DS3231_ADDRESS 0x69  // Device address when ADO = 1
#else
#define DS3231_ADDRESS 0x68  // Device address when ADO = 0
#endif

// Pin definitions
#define intPin    0  // Hardware interrupt 0 is Pro Mini pin #2
#define blinkPin 13  // Blink LED on Teensy or Pro Mini when updating
#define outPin1  10  // Use this pin to output to light LED when Alarm 1 or 2 is triggered
#define outPin2  11  // Use this output pin for sound
boolean blinkOn = false;

uint8_t seconds, minutes, hours, date, day, month, year, century;
boolean PM;
float temperature;

uint32_t delt_t = 0; // used to control display output rate
uint32_t count = 0;  // used to control display output rate

void setup()
{
  Wire.begin();
  Serial.begin(38400);
  
  // Set up the interrupt pin, which is active low, open-drain
  attachInterrupt(intPin, alarmChange, FALLING); // activate interrupt when pin goes low
  pinMode(blinkPin, OUTPUT);                       // blink LED on pin 13 when timer is operating
  digitalWrite(blinkPin, HIGH);                    // start out with LED 13 on
  pinMode(outPin1, OUTPUT);                         // output on this pin when alarm 1 triggered
  pinMode(outPin2, OUTPUT);                         // output on this pin when alarm 1 triggered
  digitalWrite(outPin1, LOW);                       // start out with the output pin low
  analogWrite(outPin2, 0);                       // start out with the output pin low
  
  display.begin();         // Initialize the display
  display.setContrast(60); // Set the contrast
  display.setRotation(0);  //  0 or 2) width = width, 1 or 3) width = height, swapped etc.
  
// Start device display with ID of sensor
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(0,0); display.print("DS3231");
  display.setTextSize(1);
  display.setCursor(20, 20); display.print("RTC");
  display.setCursor(0, 30); display.print("time/date");
  display.setCursor(0, 40); display.print("temperature");
  display.display();
  delay(2000);

// Set up for data display
  display.setTextSize(1);      // Set text size to normal, 2 is twice normal etc.
  display.setTextColor(BLACK); // Set pixel color; 1 on the monochrome screen
  display.clearDisplay();      // clears the screen and buffer

/*
  // Once the time is set, there is no need to continue to set it since the batteries,
  // either external or internal will keep time even when not connected to a microcontroller.
  // Set minute
  writeByte(DS3231_ADDRESS, MINUTE, 0x26);
  // Set hour, AM/PM (PM = bit 5 = 1), 12-hour display (bit 6 = 1) or 24-hour display (bit 6 = 0)
  writeByte(DS3231_ADDRESS, HOUR, 0x05 | 0x20 | 0x40);
  // Set date
  writeByte(DS3231_ADDRESS, DATE, 0x19);
  // Set day
  writeByte(DS3231_ADDRESS, DAY, 0x07);
  // Set month
  writeByte(DS3231_ADDRESS, MONTH, 0x05);
  // Set Year
  writeByte(DS3231_ADDRESS, YEAR, 0x0E); 
*/ 
  // Configure interrupt and Alarms
  writeByte(DS3231_ADDRESS, STATUS, readByte(DS3231_ADDRESS, STATUS) & ~0x01); // clear Alarm 1 flag
  writeByte(DS3231_ADDRESS, STATUS, readByte(DS3231_ADDRESS, STATUS) & ~0x02); // clear Alarm 2 flag
  
  // Configure Alarms , interrupt, and square wave output
  byte c = readByte(DS3231_ADDRESS, CONTROL);
  writeByte(DS3231_ADDRESS, CONTROL, 0x07); // Enable interrupt and Alarm 1 and Alarm 2
  
// Configure Alarm 1 time and mask bits  
  c = readByte(DS3231_ADDRESS, ALARM_1_SECONDS);
  writeByte(DS3231_ADDRESS,ALARM_1_SECONDS, 0x30);   // Set Alarm 1 when seconds match 30 seconds
  c = readByte(DS3231_ADDRESS, ALARM_1_MINUTES);
  writeByte(DS3231_ADDRESS, ALARM_1_MINUTES, c | 0x80);
  c = readByte(DS3231_ADDRESS, ALARM_1_HOURS);
  writeByte(DS3231_ADDRESS, ALARM_1_HOURS, c | 0x80);
  c = readByte(DS3231_ADDRESS, ALARM_1_DAY_DATE);
  writeByte(DS3231_ADDRESS, ALARM_1_DAY_DATE, c | 0x80);
  
  // Configure Alarm 2 time and mask bits  
  c = readByte(DS3231_ADDRESS, ALARM_2_MINUTES);
  writeByte(DS3231_ADDRESS, ALARM_2_MINUTES, c | 0x80); // Set alarm 2 to trigger every minute
  c = readByte(DS3231_ADDRESS, ALARM_2_HOURS);
  writeByte(DS3231_ADDRESS, ALARM_2_HOURS, c | 0x80);
  c = readByte(DS3231_ADDRESS, ALARM_2_DAY_DATE);
  writeByte(DS3231_ADDRESS, ALARM_2_DAY_DATE, c | 0x80);
  
 /*
This sketch uses a speaker to play songs when the alarm triggers or the hours strike.
The Arduino's tone() command will play notes of a given frequency.
We'll provide a function that takes in note characters (a-g),
and returns the corresponding frequency from this table:

  note 	frequency
  c     131 Hz
  d     147 Hz
  e     165 Hz
  f     174 Hz
  g     196 Hz
  a     220 Hz
  b     247 Hz
 only notes up to 255 Hz can be encoded in a single byte!
 The AT24C32 can store 128 32-byte songs, one for each 24 hours in the day and plenty left over for alarms!

For more information, see http://arduino.cc/en/Tutorial/Tone
 
  // Test to see if we can write a 32-byte page to the 4K-byte EEPROM and read it again 
  // Once the data are on the EEPROM, they don't have tp be written again, but can be read over and over.
  uint8_t song1[32] = {131, 131, 131, 131, 147, 147, 147, 147, 196, 196, 196, 196, 220, 220, 220, 220, 131, 131, 131, 131, 147, 147, 147, 147, 196, 196, 196, 196, 131, 131, 131, 131};
  // Test to see if we can write a 32-byte page to the 4K-byte EEPROM and read it again 
  uint8_t page = 1;  // write to page 64 out of 127 (0 - 127 pages available)
  uint8_t entry = 0;  // start with entry 0 of 31 ( 0 - 31 byte locations per page)
  for (int ii = 0; ii < 32; ii++) {  // write the whole page
  writeEEPROM(AT24C32_ADDRESS, page, entry, song1[ii]);  
  entry++;
  }
  for (int entry = 0; entry < 32; entry++) {
  uint8_t data = readEEPROM(AT24C32_ADDRESS, page, entry);
  Serial.print("page = "); Serial.print(page); Serial.print("  entry = "); Serial.print(entry); Serial.print("  data = "); Serial.println(data); 
  }
 */ 
}

void loop()
{  
   // If device is not busy, read time, date, and temperature
   byte c = readByte(DS3231_ADDRESS, STATUS) & 0x04;
   if(!c) {  // if device not busy

  // These interrupts require constant polling of the STATUS register which takes a lot of time, 
  // which the microcontroller might not be able to spare. If the micrcontroller has a lot of other things to do
  // or we want to save power by only waking up the microcontroller when something requires its attention, use the 
  // hardware interrupt routine alarmChange().
   c = readByte(DS3231_ADDRESS, STATUS);           // Read STATUS register of DS3231 RTC
   if(c & 0x01) {                                  // If Alarm1 flag set, take some action
     digitalWrite(outPin1, HIGH);                  // Turn on extenal LED

  // play song1
     for (uint8_t entry = 0; entry < 32; entry++) {
       uint8_t data = readEEPROM(AT24C32_ADDRESS, 1, entry);
       tone(outPin2, data, 50);  delay(50);
     }
     noTone(outPin2);                              // End song1

     digitalWrite(outPin1, LOW);                   // Turn off external LED
     writeByte(DS3231_ADDRESS, STATUS, c & ~0x01); // clear Alarm 1 flag if already set
   }
   if(c & 0x02) {                                  // If Alarm 2 flag set, take some action
     digitalWrite(outPin1, HIGH);                  // Turn on extenal LED

  // play song1
     for (uint8_t entry = 0; entry < 32; entry++) {
       uint8_t data = readEEPROM(AT24C32_ADDRESS, 1, entry);
       tone(outPin2, data, 50); delay(50); 
     }
     noTone(outPin2);                              // End song1
     
     digitalWrite(outPin1, LOW);                   // Turn off extenal LED
     writeByte(DS3231_ADDRESS, STATUS, c & ~0x02); // clear Alarm 2 flag if already set
   }

    // get time
    seconds = readSeconds();
    minutes = readMinutes();
    hours   = readHours();
    PM      = readPM();
    
    day     = readDay();
    date    = readDate();
    month   = readMonth();
    year    = readYear();
    century = readCentury();
   
    // get temperature
    temperature = readTempData();  // Read the temperature
   }  

    // Serial print and/or display at 0.5 s rate independent of data rates
    delt_t = millis() - count;
    if (delt_t > 300) { // update LCD once per half-second independent of read rate
    digitalWrite(blinkPin, blinkOn);
    
    display.clearDisplay();
  
    display.setCursor(0, 0); display.print("DS3231 RTC");
    
    display.setCursor(0, 10); 
    if(hours < 10)   {display.print("0"); display.print(hours);}   else display.print(hours);
    display.print(":"); 
    if(minutes < 10) {display.print("0"); display.print(minutes);} else display.print(minutes);
    display.print(":"); 
    if(seconds < 10) {display.print("0"); display.print(seconds);} else display.print(seconds);
    if(PM) display.print(" PM"); else display.print(" AM"); 
    
    display.setCursor(0, 20); display.print(month); display.print("/"); display.print(date); display.print("/20"); display.print(year); 

    display.setCursor(0, 30); 
    if(day == 1) display.print("Monday");   
    if(day == 2) display.print("Tuesday");  
    if(day == 3) display.print("Wednesday");
    if(day == 4) display.print("Thursday");   
    if(day == 5) display.print("Friday");  
    if(day == 6) display.print("Saturday");  
    if(day == 7) display.print("Sunday");   
    
    display.setCursor(0, 40); display.print("T = "); display.print(temperature, 2); display.print(" C"); 
    display.display();
    
    blinkOn = ~blinkOn;
    count = millis();  
}
  }

//===================================================================================================================
//====== Set of useful function to access acceleratio, gyroscope, and temperature data
//===================================================================================================================
// Write one byte to the EEPROM
  void writeEEPROM(uint8_t EEPROMaddress, uint8_t page, uint8_t entry, uint8_t data)
  {
    // Construct EEPROM address from page and entry input
    // There are 128 pages and 32 entries (bytes) per page
    // EEPROM address are 16-bit (2 byte) address where the MS four bits are zero (or don't care)
    // the next seven MS bits are the page and the last five LS bits are the entry location on the page
    uint16_t pageEntryAddress = (uint16_t) ((uint16_t) page << 5) | entry;
    uint8_t  highAddressByte  = (uint8_t) (pageEntryAddress >> 8);  // byte with the four MSBits of the address
    uint8_t  lowAddressByte   = (uint8_t) (pageEntryAddress - ((uint16_t) highAddressByte << 8)); // byte with the eight LSbits of the address
    
    Wire.beginTransmission(EEPROMaddress);    // Initialize the Tx buffer
    Wire.write(highAddressByte);              // Put slave register address 1 in Tx buffer
    Wire.write(lowAddressByte);               // Put slave register address 2 in Tx buffer
    Wire.write(data);                         // Put data in Tx buffer
    delay(10);                                // maximum write cycle time per data sheet
    Wire.endTransmission();                   // Send the Tx buffer
    delay(10);
  }

// Read one byte from the EEPROM
    uint8_t readEEPROM(uint8_t EEPROMaddress, uint8_t page, uint8_t entry)
{
    uint16_t pageEntryAddress = (uint16_t) ((uint16_t) page << 5) | entry;
    uint8_t  highAddressByte  = (uint8_t) (pageEntryAddress >> 8);  // byte with the four MSBits of the address
    uint8_t  lowAddressByte   = (uint8_t) (pageEntryAddress - ((uint16_t)highAddressByte << 8)); // byte with the eight LSbits of the address

    uint8_t data;                                    // `data` will store the register data	 
    Wire.beginTransmission(EEPROMaddress);           // Initialize the Tx buffer
    Wire.write(highAddressByte);                     // Put slave register address 1 in Tx buffer
    Wire.write(lowAddressByte);                      // Put slave register address 2 in Tx buffer
    Wire.endTransmission(false);                     // Send the Tx buffer, but send a restart to keep connection alive
    Wire.requestFrom(EEPROMaddress, (uint8_t) 1);    // Read one byte from slave register address 
    delay(10);                                       // maximum write cycle time per data sheet
    data = Wire.read();                              // Fill Rx buffer with result
    delay(10);
    return data;                                     // Return data read from slave register
}

   void alarmChange()  // If either Alarm is triggered, take some action
   {
   display.setCursor(66,0); display.print("AL"); 
   display.display();
   }
     
    uint8_t readSeconds()
  {
    uint8_t data;
    data = readByte(DS3231_ADDRESS, SECOND);
    return ((data >> 4) * 10) + (data & 0x0F);
  }
    
  uint8_t readMinutes()
  {
    uint8_t data;
    data = readByte(DS3231_ADDRESS, MINUTE);
    return ((data >> 4) * 10) + (data & 0x0F);
  }
  
  uint8_t readHours()
  {
    uint8_t data;
    data = readByte(DS3231_ADDRESS, HOUR);
    return (((data & 0x10) >> 4) * 10) + (data & 0x0F);
  }
  
  boolean readPM()
  {
    uint8_t data;
    data = readByte(DS3231_ADDRESS, HOUR);
    return (data & 0x20);
  }
  
  uint8_t readDay()
  {
    uint8_t data;
    data = readByte(DS3231_ADDRESS, DAY);
    return data;
  }
  
  uint8_t readDate()
  {
    uint8_t data;
    data = readByte(DS3231_ADDRESS, DATE);
    return ((data >> 4) * 10) + (data & 0x0F);
  }

  uint8_t readYear()
  {
    uint8_t data;
    data = readByte(DS3231_ADDRESS, YEAR);
    return ((data >> 4) * 10) + (data & 0x0F);
  }

  uint8_t readMonth()
  {
    uint8_t data;
    data = readByte(DS3231_ADDRESS, MONTH);
    return (((data & 0x10) >> 4) * 10) + (data & 0x0F);
  }
  
  uint8_t readCentury()
  {
    uint8_t data;
    data = readByte(DS3231_ADDRESS, MONTH);
    return data >> 7;
  }
    
  float readTempData()
{
  uint8_t rawData[2];  // x/y/z gyro register data stored here
  readBytes(DS3231_ADDRESS, MSB_TEMP, 2, &rawData[0]);  // Read the two raw data registers sequentially into data array 
  return ((float) ((int8_t)rawData[0])) + ((float)((int8_t)rawData[1] >> 6) * 0.25) ;  // Construct the temperature in degrees C
}


        void writeByte(uint8_t address, uint8_t subAddress, uint8_t data)
{
	Wire.beginTransmission(address);  // Initialize the Tx buffer
	Wire.write(subAddress);           // Put slave register address in Tx buffer
	Wire.write(data);                 // Put data in Tx buffer
	Wire.endTransmission();           // Send the Tx buffer
}

        uint8_t readByte(uint8_t address, uint8_t subAddress)
{
	uint8_t data; // `data` will store the register data	 
	Wire.beginTransmission(address);         // Initialize the Tx buffer
	Wire.write(subAddress);	                 // Put slave register address in Tx buffer
	Wire.endTransmission(false);             // Send the Tx buffer, but send a restart to keep connection alive
	Wire.requestFrom(address, (uint8_t) 1);  // Read one byte from slave register address 
	data = Wire.read();                      // Fill Rx buffer with result
	return data;                             // Return data read from slave register
}

        void readBytes(uint8_t address, uint8_t subAddress, uint8_t count, uint8_t * dest)
{  
	Wire.beginTransmission(address);   // Initialize the Tx buffer
	Wire.write(subAddress);            // Put slave register address in Tx buffer
	Wire.endTransmission(false);       // Send the Tx buffer, but send a restart to keep connection alive
	uint8_t i = 0;
        Wire.requestFrom(address, count);  // Read bytes from slave register address 
	while (Wire.available()) {
        dest[i++] = Wire.read(); }         // Put read results in the Rx buffer
}
