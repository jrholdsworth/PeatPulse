//******************************//
//  Peat Bog Slave Code  //
//  Contributed by
//  James Holdsworth
//  Wiliam Schlatt
//******************************//

//******************************//
//  Includes //
//******************************//
//Indludes For TOF sensor//
#include "Arduino.h"
#include "Wire.h"
#include "DFRobot_VL53L0X.h"
//End TOF sensor Includes

//Temperature Sensor Includes
#include "DHT.h"
//End Temperature Sensor includes

//Pressure Sensor Includes
#include "PressureSens.h"
//End Pressure Sensor Includes

//Includes
#include "painlessMesh.h"  //For Painless Mesh
#include <esp_wifi.h>      //For esp_wifi_set_protocol( WIFI_IF_STA, WIFI_PROTOCOL_LR );

//For String Conversions
#include <stdbool.h>
#include <string.h>

//******************************//
//  Defines
//******************************//

//Defined for Deep Sleep
//Deep Sleep Defines
#define uS_TO_S_FACTOR 1000000ULL /* Conversion factor for micro seconds to seconds */

//Defning Off timer Time
#define sleepTimeOveride 180000  //Defines 3 mins for override to go to sleep

#define sleepinterval 60  //Defning Sleep interval (3600 is an hour)
//End defines for Deep sleep

//Defines
#define MESH_PREFIX "PeatPulseNet"
#define MESH_PASSWORD "123456789"
#define MESH_PORT 5555

//Temperature sensor defines
#define DHTPIN 15     // Digital pin connected to the DHT sensor
#define DHTTYPE DHT22   // DHT 22  
//End Temperature sensor defines

//******************************//
//  Global Variables
//******************************//

//Temperature Sensor Gloval Variables
DHT dht(DHTPIN, DHTTYPE);

//TOF sensor Global Variables
DFRobot_VL53L0X sensor;

//Declaring a Mesh
painlessMesh mesh;  //Creates and instance of painless mesh
                    //Stores isn RTC

//Variables for sleeping
RTC_DATA_ATTR bool readyToSleep = false;       //Ready to sleep with data from master
RTC_DATA_ATTR bool readyToSleeplocal = false;  //Ready to sleep with data send from node
RTC_DATA_ATTR bool recoveryModeEnable;

//Node Info
RTC_DATA_ATTR int messageCount = 0;  //Keep track of each message, i.e. first set of readings is value 1, etc
int nodeId = 1;

RTC_DATA_ATTR size_t MasterId = 3185316401;  //Holds the masternode aka Logserver Id
                                             // The Id is related to the esp's chip so never changes

//Variables to Control Sending a message
RTC_DATA_ATTR int period = 2000;  //Allow for 20 Seconds for the nodes to power up
RTC_DATA_ATTR unsigned long time_now = 0;


//5 Minute timer variables
//Declare here but this only gets run on reset
//Will call millis in setup which is run whenever deepsleep wakes
//Dont want to put it in RTC memory as want the value to reset
unsigned long OffTimer = 0;

//Variables to Calculate Sleep Time
unsigned long timeMsgRecieved = 0;
unsigned long ElapsedTime = 0;
unsigned long timeSinceLastMessage = 0;
int timeArr[2];  // timeArr[0] = minutes, timeArr[1] = seconds

//The Data Variables
RTC_DATA_ATTR float temperature = 0;
RTC_DATA_ATTR float humidity = 0;
RTC_DATA_ATTR float pressure = 0;
RTC_DATA_ATTR float height = 0;
RTC_DATA_ATTR float voltage = 3.2;
RTC_DATA_ATTR char diagnostics[56];
RTC_DATA_ATTR bool diagIssue = false;



void setup() {
  //initialize serial communication at 115200 bits per second:
  Serial.begin(115200);
  OffTimer = millis();
  dht.begin();
  delay(2000 *nodeId);  //Power on Delay
  //TOF SENSOR SETUP//
  //join i2c bus (address optional for master)
  Wire.begin();
  //Set I2C sub-device address
  sensor.begin(0x50);

  //Set to Back-to-back mode and high precision mode
  sensor.setMode(sensor.eContinuous, sensor.eHigh);
  //Laser rangefinder begins to work
  sensor.start();
  //END TOF SENSOR SETUP

  getReadings();

  mesh.setDebugMsgTypes(ERROR);                                      //Set level of Debug
                                                                     // Only using Error to not clutter up Serial monitor
  mesh.init(MESH_PREFIX, MESH_PASSWORD, MESH_PORT, WIFI_AP_STA, 5);  //Initialising without Task Schedule, easier to control task flow.
  mesh.setContainsRoot(true);         //Tells the mesh that there is a Root node
  mesh.onReceive(&receivedCallback);  //Sets the functions to be called when data recieved
  //Starts the override sleep timer
}

void loop() {
  //Important - Needed for Mesh to work
  mesh.update();
  if (!recoveryModeEnable) {
    pinMode(2, OUTPUT);
    digitalWrite(2, LOW);  //Set Low - Off

    if (millis() >= time_now + period && readyToSleeplocal == false && mesh.isConnected(MasterId) == true) {  //Do I really need the millis delay in there? Maybe to help control checking rate?
      time_now += period;
      //Call function to get Data
      memset(diagnostics, 0, sizeof(diagnostics));
      //Diagnostics
      if (temperature == -218.40|| temperature == -273.00) {  //To Check on Disconnect
        strcat(diagnostics, " TEMP NR");
        diagIssue = true;
      }

      if (humidity == 0.00) {  //To Check on disconnect
        strcat(diagnostics, " HUM  NR");
        diagIssue = true;
      }

      if (pressure == 7880) {  //To Check on disconnect
        strcat(diagnostics, " PRES NR ");
        diagIssue = true;
      }

      if (height == 16383.75) {  //To Check on disconnect
        strcat(diagnostics, " HEI NR ");
        diagIssue = true;
      }

      if (voltage <= 3.0) {
        strcat(diagnostics, "LOW PWR");
        diagIssue = true;
      }

      if (diagIssue == false)
        strcat(diagnostics, "NORM");
      diagIssue = false;



      //Package Data ready to be sent
      messageCount++;
      String DataString = "Node ID";
      DataString += ":";
      DataString += String(nodeId);
      DataString += "\t";
      DataString += "Node Message Number";
      DataString += ":";
      DataString += String(messageCount);
      DataString += "\t";
      DataString += "Temperature";
      DataString += ":";
      DataString += String(temperature);
      DataString += "\t";
      DataString += "Humidity";
      DataString += ":";
      DataString += String(humidity);
      DataString += "\t";
      DataString += "Pressure";
      DataString += ":";
      DataString += String(pressure);
      DataString += "\t";
      DataString += "Height";
      DataString += ":";
      DataString += String(height);
      DataString += "\t";
      DataString += "Voltage";
      DataString += ":";
      DataString += String(voltage);
      DataString += "\t";
      DataString += "Diagnostics";
      DataString += " --> ";
      DataString += diagnostics;

      //Send the data
      mesh.sendSingle(MasterId, DataString);
      Serial.println("Sent Message");
      //Get ready to sleep
      readyToSleeplocal = true;
    }

    if ((readyToSleeplocal == true && readyToSleep == true) || millis() - OffTimer >= sleepTimeOveride) {  //If node has send message out
      mesh.stop();
      WiFi.disconnect();

      if (millis() - OffTimer >= sleepTimeOveride) {
        readyToSleeplocal = false;
        readyToSleep = false;
        //go to sleep here
        esp_sleep_enable_timer_wakeup(3420 * uS_TO_S_FACTOR); //Go to sleep for 57 mins if override go to sleep
        Serial.print("Override ");
        Serial.println("Going to sleep for 3420 seconds");
        esp_deep_sleep_start();

      } else {      
        ElapsedTime = millis();
        timeSinceLastMessage = ElapsedTime - timeMsgRecieved;
        int adjustedTimeSleep = timeAdjuster();  //Will be used to work out the actual time the device must sleep for

        //Set flags to flase for next run
        readyToSleeplocal = false;
        readyToSleep = false;
        //go to sleep here
        esp_sleep_enable_timer_wakeup(adjustedTimeSleep * uS_TO_S_FACTOR);
        Serial.print("going to sleep for ");
        Serial.println(adjustedTimeSleep);
        esp_deep_sleep_start();
      }
    }
  }

  else if (recoveryModeEnable) {
    pinMode(17, OUTPUT);
    digitalWrite(17, HIGH);  //Set High
    //Reset Message Count?
  }
}

void receivedCallback(uint32_t from, String& msg) {
  timeMsgRecieved = millis();  //populates the timeAdjuster variable with the time the message was recieved from the master

  Serial.printf("logClient: Received from %u msg=%s\n", from, msg.c_str());

  recoveryModeEnable = strstr(msg.c_str(), "recovery mode 1") != NULL;
  Serial.print("recovery mode on? ");
  Serial.println(recoveryModeEnable);

  if (readyToSleeplocal == true) {
    Serial.println("Incomming Message");
    const char* p = msg.c_str();  //Pointer to string
    int i = 0;
    while (*p) {
      if (*p++ == ':') {
        timeArr[i] = atoi(p);  //Convert froms string to int
        i++;
      }
    }
    Serial.println("minutes");
    Serial.println(timeArr[0]);
    Serial.println("seconds");
    Serial.println(timeArr[1]);
    readyToSleep = true;
  }
}

//Works from current time what time the device should next wake up
int timeAdjuster() {
  //3600 Seconds in an hour //We get the time from the master
  //This means we can tell node to sleep until the next hour
  //The internal clock isnt accurate over a long time but over an hour it will be accurate enough
  //int adjustedSeconds = sleepinterval - (timeArr[0] * 60) - (timeArr[1]) - (timeSinceLastMessage / 1000);
  //Takes the incomming time as minutes

  //NEED TO FIX TIME ADJUSTMENT BUT ALMOST DONE
  //int adjustedSeconds = sleepinterval - (timeArr[0] * 60) - (timeArr[1]) - (timeSinceLastMessage / 1000);
  //return (adjustedSeconds);

  int timeDifference = millis() - timeMsgRecieved;

  if (sleepinterval <= 60 && sleepinterval < (timeArr[1] + timeDifference / 1000))
    return (60 - timeArr[1] - (timeDifference / 1000));
  else if (sleepinterval <= 60)
    return (sleepinterval - timeArr[1] - (timeDifference / 1000));

  //these statements calculate the power on for the nearest hour
  if (sleepinterval <= 3600 && sleepinterval < (timeArr[1] + 60 * timeArr[0]))
    return (3600 - timeArr[1] - 60 * timeArr[0]);
  else if (sleepinterval <= 3600)
    return ((sleepinterval) - (timeArr[0] * 60) - (timeArr[1]));
}

void getReadings() {
  temperature = 0;
  humidity = 0;
  pressure = 0;
  height = 0;
  voltage = 0;
  int readingWaitPeriod = 2000;
  unsigned long time_now = 0;

  for (int i = 0; i < 5; i++) {
    temperature +=  dht.readTemperature() / 5;
    humidity += dht.readHumidity() / 5;
    pressure += getPressure() / 5;
    height += sensor.getDistance() / 5;
    voltage += (analogReadMilliVolts(35)*2) / 5;  //ADC READ
    time_now = millis();
    while (millis() < time_now + readingWaitPeriod) {
      //wait approx. [readingWaitPeriod] ms
    }
  }
  Serial.println(temperature);
  Serial.println(humidity);
  Serial.println(pressure);
  Serial.println(height);
  Serial.println(voltage);
}
