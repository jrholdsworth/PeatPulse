//******************************//
//  Peat Bog Slave Code  //
//  Contributed by
//  James Holdsworth
//******************************///

//BUGS
//Need to Fix Sleeping on Master
//Now Sleeps when it needs to
//But Has Random Crashes on reboot
//Seems to be fixed by removing RTC ADDR from painless mesh

//Need to Fix enabling recovery mode
//Seems to Be Fixed - Bug when going from On recovery to Off recovery mode
//Fixed

//Need to Get wifi server working on master again
// fixed but no long range mode :( --Someone else to look into?
//seems to Be fixed but need long range mode test

//TODO
//Button on website for recovery mode individual and all
//Button to stop sending out recovery mode signal

//******************************//
//  Includes //
//******************************//
#include "painlessMesh.h"  // For Painless Mesh
#include <esp_wifi.h>      //For esp_wifi_set_protocol( WIFI_IF_STA, WIFI_PROTOCOL_LR );
#include "RTClib.h"        //Has to be done in this order to work

//Creating a Mesh
//Must be done here as the header files below require this declaration
painlessMesh mesh;

Scheduler userScheduler;  // Used in the Webserver to control the Recoverymode messages
void sendRecoveryModeMessageOn();
void sendRecoveryModeMessageOFF();
Task taskSendMessageRecoveryOn(TASK_SECOND * 5, TASK_FOREVER, &sendRecoveryModeMessageOn);
Task taskSendMessageRecoveryOFF(TASK_SECOND * 5, TASK_FOREVER, &sendRecoveryModeMessageOFF);

void sendRecoveryModeMessageOnNode1();
void sendRecoveryModeMessageOFFNode1();
Task taskSendMessageRecoveryOnNode1(TASK_SECOND * 5, TASK_FOREVER, &sendRecoveryModeMessageOnNode1);
Task taskSendMessageRecoveryOFFNode1(TASK_SECOND * 5, TASK_FOREVER, &sendRecoveryModeMessageOFFNode1);

#include "SDCARDFUNCTIONS.h"
#include "WEBSERVERFUNCTIONS.h"

//******************************//
//  Defines
//******************************//

//Defines for Deep Sleep
#define uS_TO_S_FACTOR 1000000 /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP 5        /* Time ESP32 will go to sleep (in seconds) */
//End defines for Deep sleep

//Defines for SD Card and Webserver
#define servername "MCserver"  //Define the name to your server...
#define SD_pin 14              //G16 in my case
//End Defines for SD Card and Webserver

//Defines for Painless Mesh
#define MESH_PREFIX "PeatPulseNet"
#define MESH_PASSWORD "123456789"
#define MESH_PORT 5555

//Deep Sleep Defines
#define uS_TO_S_FACTOR 1000000 /* Conversion factor for micro seconds to seconds */
//Defning Off timer Time
#define sleepTimeOveride 300000  //Defines 5 mins for override to go to sleep

//How Often the interval Deivices should Go to sleep
#define sleepinterval 60  //Defning Sleep interval (3600 is an hour)

#define numberOfNodes 1  //Defines Number of nodes


//******************************//
//  Global Variables
//******************************//

//Global Variable for RTC
RTC_DS3231 rtc;

//Variables for local sleeping state
RTC_DATA_ATTR bool readyToSleep = false;       //Ready to sleep with data from master
RTC_DATA_ATTR bool readyToSleeplocal = false;  //Ready to sleep with data send from node

//Delays to control sending the time to the network
//Delays between sending the message to the nodes and going into power off
int powerOffDelay = 2000;  //2 Second Power Off Delay
unsigned long powerOffDelayTimer = 0;

//Delays between sending the message again and again (if needed i.e. nodecount isn't equal to 0)
int sendMessageDelay = 8000;  // Second Power Off Delay
unsigned long sendMessageDelayTimer = 0;
//Note the sendMessageDelay must always be greater than the powerOffDelay
//Otherwise the message will get sent again, resetting the powerOffDelay timer

//Stores the node ID of the incomming message (for use with the SD card)
int incommingID = 0;

//Counter for the number of nodes
int nodeCount = 0;  //Keeps track of the nodes so the master knows if to shut down or not

//Used to calculate timing delays for the nodes on the system
int seconds = 0;
int minutes = 0;

//Holds the data for the  powerOffTimer
unsigned long OffTimer = 0;

//SETUPMODE FOR PUSH BUTTON
RTC_DATA_ATTR int devicemode = 0;  //Need to know which state device is in after reboot
const int BUTTON_PIN = 27;
//END SETUP


void setup() {
  //initialize serial communication at 115200 bits per second:
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);  //Setting button input at Pullup
  delay(1000);

  //Will always Need the mesh network so will be setup irrespective of master mode:
  //Mesh Setup Start
  mesh.setDebugMsgTypes(ERROR|CONNECTION);                                      //Sets the diagnostics messages
  mesh.init(MESH_PREFIX, MESH_PASSWORD, MESH_PORT, WIFI_AP_STA, 5);  //Initialises the mesh

  
  mesh.setRoot(true);  //This is the Master/ Root node
  mesh.setContainsRoot(true); //tells The mesh that a root Exists
  //Setting up function calls on speicifc action
  mesh.onReceive(&receivedCallback);
  mesh.onNewConnection(&newConnectionCallback);
  mesh.onDroppedConnection(&droppedConnectionCallback);
  //End Mesh Setup

  //Will always need RTC
  rtc.begin();  //Sets up the RTC on the I2C bus

  //INSIDE BUTTON STATEMENT
  //Mode 1 Website Mode
  if (digitalRead(BUTTON_PIN) == 0) {
    WiFi.softAP("PeatPulseRecoveryNetwork", "12345678");  //Network and password for the access point genereted by ESP32
                                            //Set your preferred server name, if you use "mcserver" the address would be http://mcserver.local/
    if (!MDNS.begin(servername)) {
      Serial.println(F("Error setting up MDNS responder!"));
      ESP.restart();
    }

    Serial.print(F("Initializing SD card..."));

    //see if the card is present and can be initialised.
    //Note: Using the ESP32 and SD_Card readers requires a 1K to 4K7 pull-up to 3v3 on the MISO line, otherwise may not work
    if (!SD.begin(SD_pin)) {
      Serial.println(F("Card failed or not present, no SD Card data logging possible..."));
      SD_present = false;
    } else {
      Serial.println(F("Card initialised... file access enabled..."));
      SD_present = true;
    }

    /*********  Server Commands  **********/
    server.on("/", SD_dir);
    server.on("/upload", File_Upload);
    server.on(
      "/fupload", HTTP_POST, []() {
        server.send(200);
      },
      handleFileUpload);
    server.on("/recover", Device_Recover);
    server.on("/recover/on", BuzzerOn);
    server.on("/recover/off", BuzzerOff);
    server.on("/recover/Node1on", Node1BuzzerOn);
    server.on("/recover/Node1off", Node1BuzzerOff);

    server.begin();

    Serial.println("HTTP server started");
    devicemode = 0;
  }
  //END BUTTON STATEMENT

  //Mode 2
  else if (digitalRead(BUTTON_PIN) == 1) {
    //esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_LR);              //Wifi long range mode
    SD.begin(SD_pin);           //The Chip select pin the SD card is connected to
    nodeCount = numberOfNodes;  //Sets the number of nodes to the known number in the network on startup
    OffTimer = millis();        //Starts the 5 miniute power down timer.
    devicemode = 1;
  }
}

void loop() {

  mesh.update();  //Always have mesh so always needed

  //Mode 1
  if (devicemode == 0) {
    server.handleClient();  //Listen for client connections
    mesh.update();
    userScheduler.execute();
    mesh.update();
  }

  //Mode 2
  else if (devicemode == 1) {
    //maybe remove ready to sleep clause
    if (readyToSleep == true && millis() - sendMessageDelayTimer >= sendMessageDelay) {  //Only sends the time to the nodes if data has first been recieved and the message timer is ready
      //Send time to all nodes
      //Gets the current minutes from Rtc
      DateTime now = rtc.now();
      //Converts to string to be sent
      minutes = now.minute();
      String DataString = "minutes";
      DataString += ":";
      DataString += String(minutes);
      DataString += "     ";

      //Gets the current seconds from Rtc
      //Converts to string to be sent
      seconds = now.second();
      DataString += "Seconds";
      DataString += ":";
      DataString += String(seconds);
      DataString += "     ";

      mesh.sendBroadcast(DataString);  //Broadcasts to all nodes on the network
      Serial.println("Sent Message");  // Debugging

      powerOffDelayTimer = millis();     //STARTS the power off delay timer to allow message to fully send
      sendMessageDelayTimer = millis();  //Resets the send message timer.
      readyToSleeplocal = true;
    }

    if ((readyToSleeplocal == true && readyToSleep == true && nodeCount == 0 && millis() - powerOffDelayTimer >= powerOffDelay) || millis() - OffTimer >= sleepTimeOveride) {
      //This runs if:
      //Either  :   A message has been recieved + a message has been send + no nodes are connected + the power down timer of 2 seconds has finished
      //OR      :   The 5 miniute override timer finishes up.

      mesh.stop();  //Ends the Mesh connections
      //WiFi.mode(WIFI_OFF);
      WiFi.disconnect();
      delay(1);

      //Gets the Adjusted time the master needs to sleep for
      int adjustedTimeSleep = timeAdjuster();

      //resets the sleep flags for wakeup
      readyToSleeplocal = false;
      readyToSleep = false;
      //enables timer for asjusted time and goes to sleep
      esp_sleep_enable_timer_wakeup(adjustedTimeSleep * uS_TO_S_FACTOR);
      Serial.print("going to sleep for ");
      Serial.println(adjustedTimeSleep);
      Serial.flush();
      esp_deep_sleep_start();
    }
  }
}

//Called when a message is recieved
void receivedCallback(uint32_t from, String &msg) {
  //Debugging
  Serial.printf("logServer: Received from %u msg=%s\n", from, msg.c_str());
  //In here convert the incomming data so it can be stored on the SD card
  Serial.println("Incomming Message");
  //Debugging ends

  //Converts the incommming string and find the ID in the string
  const char *p = msg.c_str();  //Pointer to string
  while (*p) {
    if (*p++ == ':') {
      incommingID = atoi(p);
      break;
    }
  }

  //Debugging
  Serial.println("incommingID");
  Serial.println(incommingID);
  //Debugging end

  //TODO
  WriteToSd(incommingID, msg);         //Removes a node from the node count
                        //Each node only broadcasts once so can do this
  readyToSleep = true;  //Ready to sleep now (not locally) data has been recieved from a node
}

//Called on a New Connection
void newConnectionCallback(size_t nodeId) {
  Serial.printf("New Connection %u\n", nodeId);
}

//Called on a Dropped Connection
void droppedConnectionCallback(size_t nodeId) {
  Serial.printf("Dropped Connection %u\n", nodeId);
  nodeCount--; 
}

//Based from current time what time the device should next wake up
//Will wake up on the hour each time
int timeAdjuster() {
  //3600 Seconds in an hour
  //The internal clock isnt accurate over a long time but over an hour it will be accurate enough

  //Get current seconds and minutes from again here as time may have changed from sending the message RTC

  //minutes and seconds ....
  //seconds.nowseconds
  DateTime now = rtc.now();
  //Converts to string to be sent
  minutes = now.minute();
  seconds = now.second();

  //NEED TO FIX TIME ADJUSTMENT BUT NEARLY THERE
  //These Two statements calculate the next power on if for the nearest minute
  if (sleepinterval <= 60 && sleepinterval < seconds)
    return (60 - seconds);
  else if (sleepinterval <= 60)
    return (sleepinterval - seconds);

  //these statements calculate the power on for the nearest hour
  if (sleepinterval <= 3600 && sleepinterval < (seconds + 60 * minutes))
    return (3600 - seconds - 60 * minutes);
  else if (sleepinterval <= 3600)
    return ((sleepinterval) - (minutes * 60) - (seconds));
}

void sendRecoveryModeMessageOn() {
  mesh.update();                   //Always have mesh so always needed
  bool RecoveryModeActive = true;  //To be updated i.e. the selected button on the webserver
  String DataString = "recovery mode ";
  DataString += String(RecoveryModeActive);
  DataString += "   ";
  mesh.sendBroadcast(DataString);  //Broadcasts to all nodes on the network
  Serial.println("Sent Message");  // Debugging
}


void sendRecoveryModeMessageOFF() {
  mesh.update();  //Always have mesh so always needed
  //Send time to all nodes
  //Gets the current minutes from Rtc
  DateTime now = rtc.now();
  //Converts to string to be sent
  minutes = now.minute();
  String DataString = "minutes";
  DataString += ":";
  DataString += String(minutes);
  DataString += "   ";

  //Gets the current seconds from Rtc
  //Converts to string to be sent
  seconds = now.second();
  DataString += "Seconds";
  DataString += ":";
  DataString += String(seconds);
  DataString += "   ";

  bool RecoveryModeActive = false;  //To be updated i.e. the selected button on the webserver
  DataString += "recovery mode ";
  DataString += String(RecoveryModeActive);
  DataString += "   ";
  mesh.sendBroadcast(DataString);  //Broadcasts to all nodes on the network
  Serial.println("Sent Message");  // Debugging
}


void sendRecoveryModeMessageOnNode1() {
  mesh.update();                   //Always have mesh so always needed
  bool RecoveryModeActive = true;  //To be updated i.e. the selected button on the webserver
  String DataString = "recovery mode ";
  DataString += String(RecoveryModeActive);
  DataString += "   ";
  // mesh.sendBroadcast(DataString);  //Broadcasts to all nodes on the network //Change to sendsingle
  Serial.println("Sent Message");  // Debugging
}


void sendRecoveryModeMessageOFFNode1() {
  mesh.update();  //Always have mesh so always needed
  //Send time to all nodes
  //Gets the current minutes from Rtc
  DateTime now = rtc.now();
  //Converts to string to be sent
  minutes = now.minute();
  String DataString = "minutes";
  DataString += ":";
  DataString += String(minutes);
  DataString += "   ";

  //Gets the current seconds from Rtc
  //Converts to string to be sent
  seconds = now.second();
  DataString += "Seconds";
  DataString += ":";
  DataString += String(seconds);
  DataString += "   ";

  bool RecoveryModeActive = false;  //To be updated i.e. the selected button on the webserver
  DataString += "recovery mode ";
  DataString += String(RecoveryModeActive);
  DataString += "   ";
  // mesh.sendBroadcast(DataString);  //Broadcasts to all nodes on the network //Change to send single
  Serial.println("Sent Message");  // Debugging
}

void WriteToSd(int Id, String &msg) {  //Called When Data comes in from the network

  //NEEDS FIXING TO NOT OVERWRITE FILES BUT OTHERWISE IS DONE
  Serial.println("Writing to SD Card!!!");

  //Change this a bit
  char specific_Filepath[] = "/Node_";
  char nodeIDString[10];                    // declare character array with enough space for integer value
  sprintf(nodeIDString, "%d", Id);          // format integer value as string
  strcat(specific_Filepath, nodeIDString);  // concatenate string to specific_Filepath

  if (!SD.exists(specific_Filepath)) {  //If the file doesn't exist make it
    writeFile(SD, specific_Filepath, "Node ID ");
    String IdString = String(Id);
    appendFile(SD, specific_Filepath, IdString.c_str());
  } else {  //If the file exists then append to it
    appendFile(SD, specific_Filepath, "Node ID ");
    String IdString = String(Id);
    appendFile(SD, specific_Filepath, IdString.c_str());
  }

  DateTime now = rtc.now();

  //Write timing
  appendFile(SD, specific_Filepath, "\t");
  appendFile(SD, specific_Filepath, "Year ");
  String yearString = String(now.year());
  appendFile(SD, specific_Filepath, yearString.c_str());

  appendFile(SD, specific_Filepath, "\t");
  appendFile(SD, specific_Filepath, "Month ");
  String monthString = String(now.month());
  appendFile(SD, specific_Filepath, monthString.c_str());

  appendFile(SD, specific_Filepath, "\t");
  appendFile(SD, specific_Filepath, "Day ");
  String dayString = String(now.day());
  appendFile(SD, specific_Filepath, dayString.c_str());

  appendFile(SD, specific_Filepath, "\t");
  appendFile(SD, specific_Filepath, "Hour ");
  String hourString = String(now.hour()+1);
  appendFile(SD, specific_Filepath, hourString.c_str());

  appendFile(SD, specific_Filepath, "\t");
  appendFile(SD, specific_Filepath, "Minute ");
  String minuteString = String(now.minute());
  appendFile(SD, specific_Filepath, minuteString.c_str());

  appendFile(SD, specific_Filepath, "\t");
  appendFile(SD, specific_Filepath, "Second ");
  String secondString = String(now.second());
  appendFile(SD, specific_Filepath, secondString.c_str());
  //Finish Writing Time

  appendFile(SD, specific_Filepath, "\t");
  appendFile(SD, specific_Filepath, msg.c_str());

  appendFile(SD, specific_Filepath, "\t");
  appendFile(SD, specific_Filepath, "\n");

  Serial.println("");
  Serial.println("Finished Writing to SD Card");
  Serial.flush();
  return;
}
