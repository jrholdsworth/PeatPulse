#include <SPI.h>

//Pressure Sensor Global Variables
RTC_DATA_ATTR int pwmChannel = 1;     // Selects channel 0
RTC_DATA_ATTR int frequence = 32768;  // PWM frequency of 1 KHz
RTC_DATA_ATTR int resolution = 8;     // 8-bit resolution, 256 possible values
RTC_DATA_ATTR int pwmPin = 25;

void resetsensor()  //this function keeps the sketch a little shorter
{
  SPI.setDataMode(SPI_MODE0);
  SPI.transfer(0x15);
  SPI.transfer(0x55);
  SPI.transfer(0x40);
}

float getPressure() {
  SPI.begin();  //see SPI library details on arduino.cc for details
  SPI.setBitOrder(MSBFIRST);
  SPI.setClockDivider(SPI_CLOCK_DIV32);  //divide 16 MHz to communicate on 500 kHz

  delay(100);

  // Configuration of channel 0 with the chosen frequency and resolution
  ledcSetup(pwmChannel, frequence, resolution);

  // Assigns the PWM channel to pin 23
  ledcAttachPin(pwmPin, pwmChannel);

  // Create the selected output voltage
  ledcWriteTone(pwmChannel, frequence);

  long c1 = 23092;
  long c2 = 1881;
  long c3 = 731;
  long c4 = 648;
  long c5 = 1244;
  long c6 = 25;

  resetsensor();  //resets the sensor

  //Temperature:
  unsigned int tempMSB = 0;  //first byte of value
  unsigned int tempLSB = 0;  //last byte of value
  unsigned int D2 = 0;
  SPI.transfer(0x0F);            //send first byte of command to get temperature value
  SPI.transfer(0x20);            //send second byte of command to get temperature value
  delay(35);                     //wait for conversion end
  SPI.setDataMode(SPI_MODE1);    //change mode in order to listen
  tempMSB = SPI.transfer(0x00);  //send dummy byte to read first byte of value
  tempMSB = tempMSB << 8;        //shift first byte
  tempLSB = SPI.transfer(0x00);  //send dummy byte to read second byte of value
  D2 = tempMSB | tempLSB;        //combine first and second byte of value
  resetsensor();                 //resets the sensor

  //Pressure:
  unsigned int presMSB = 0;  //first byte of value
  unsigned int presLSB = 0;  //last byte of value
  unsigned int D1 = 0;
  SPI.transfer(0x0F);            //send first byte of command to get pressure value
  SPI.transfer(0x40);            //send second byte of command to get pressure value
  delay(35);                     //wait for conversion end
  SPI.setDataMode(SPI_MODE1);    //change mode in order to listen
  presMSB = SPI.transfer(0x00);  //send dummy byte to read first byte of value
  presMSB = presMSB << 8;        //shift first byte
  presLSB = SPI.transfer(0x00);  //send dummy byte to read second byte of value
  D1 = presMSB | presLSB;
  const long UT1 = (c5 * 8) + 20224;
  const long dT = (D2 - UT1);
  const long TEMP = 200 + ((dT * (c6 + 50)) / 1024);
  const long OFF = (c2 * 4) + (((c4 - 512) * dT) / 4096);
  const long SENS = c1 + ((c3 * dT) / 1024) + 24576;
  long PCOMP = ((((SENS * (D1 - 7168)) / 16384) - OFF) / 32) + 250;
  float TEMPREAL = TEMP / 10;
  // Serial.print("pressure = ");
  // Serial.print(PCOMP);
  // Serial.println(" mbar");
  const long dT2 = dT - ((dT >> 7 * dT >> 7) >> 3);
  const float TEMPCOMP = (200 + (dT2 * (c6 + 100) >> 11)) / 10;
  // Serial.print("temperature = ");
  // Serial.print(TEMPCOMP);
  // Serial.println(" °C");
  // Serial.println("************************************");

  ledcDetachPin(pwmPin);
  pinMode(pwmPin, OUTPUT);
  digitalWrite(pwmPin, LOW);  // or high?
  return(PCOMP);
}
