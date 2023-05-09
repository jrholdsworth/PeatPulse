#include <WiFi.h>            //Built-in
#include <ESP32WebServer.h>  //https://github.com/Pedroalbuquerque/ESP32WebServer download and place in your Libraries folder
#include <ESPmDNS.h>

#include "CSS.h"  //Includes headers of the web and de style file
#include <SD.h>
#include <SPI.h>

//Global Variables for the Webserver
ESP32WebServer server(80);
bool SD_present = false;  //Controls if the SD card is present or not

/*********  Declaring Functions so Compilier doesn't throw a hissy fit  **********/
//
void SD_dir();
void File_Upload();
void Device_Recover();
void BuzzerOn();
void BuzzerOff();
void printDirectory(const char* dirname, uint8_t levels);
void SD_file_download(String filename);
void handleFileUpload();
void SD_file_delete(String filename);
void SendHTML_Header();
void SendHTML_Content();
void SendHTML_Stop();
void ReportSDNotPresent();
void ReportFileNotPresent(String target);
void ReportCouldNotCreateFile(String target);
void refreshButtons();
String file_size(int bytes);

//


/*********  FUNCTIONS  **********/
//Initial page of the server web, list directory and give you the chance of deleting and uploading
void SD_dir() {
  if (SD_present) {
    //Action acording to post, dowload or delete, by MC 2022
    if (server.args() > 0)  //Arguments were received, ignored if there are not arguments
    {
      Serial.println(server.arg(0));

      String Order = server.arg(0);
      Serial.println(Order);

      if (Order.indexOf("download_") >= 0) {
        Order.remove(0, 9);
        SD_file_download(Order);
        Serial.println(Order);
      }

      if ((server.arg(0)).indexOf("delete_") >= 0) {
        Order.remove(0, 7);
        SD_file_delete(Order);
        Serial.println(Order);
      }
    }

    File root = SD.open("/");
    if (root) {
      root.rewindDirectory();
      SendHTML_Header();
      webpage += F("<table align='center'>");
      webpage += F("<tr><th>Name/Type</th><th style='width:20%'>Type File/Dir</th><th>File Size</th></tr>");
      printDirectory("/", 0);
      webpage += F("</table>");
      SendHTML_Content();
      root.close();
    } else {
      SendHTML_Header();
      webpage += F("<h3>No Files Found</h3>");
    }
    append_page_footer();
    SendHTML_Content();
    SendHTML_Stop();  //Stop is needed because no content length was sent
  } else ReportSDNotPresent();
}

//Upload a file to the SD
void File_Upload() {
  append_page_header();
  webpage += F("<h3>Select File to Upload</h3>");
  webpage += F("<FORM action='/fupload' method='post' enctype='multipart/form-data'>");
  webpage += F("<input class='buttons' style='width:25%' type='file' name='fupload' id = 'fupload' value=''>");
  webpage += F("<button class='buttons' style='width:10%' type='submit'>Upload File</button><br><br>");
  webpage += F("<a href='/'>[Back]</a><br><br>");
  append_page_footer();
  server.send(200, "text/html", webpage);
}

// Auxiliar variables to store the current output state
String recoverAllState = "off";
String Node1Recovery = "off";
String Node2Recovery = "off";

void Device_Recover() {

  userScheduler.deleteTask(taskSendMessageRecoveryOFF);
  taskSendMessageRecoveryOFF.disable();
  userScheduler.deleteTask(taskSendMessageRecoveryOn);
  taskSendMessageRecoveryOn.disable();

  userScheduler.deleteTask(taskSendMessageRecoveryOFFNode1);
  taskSendMessageRecoveryOFFNode1.disable();
  userScheduler.deleteTask(taskSendMessageRecoveryOnNode1);
  taskSendMessageRecoveryOnNode1.disable();

  refreshButtons();
  server.send(200, "text/html", webpage);
}
void refreshButtons() {
  append_page_header();

  webpage += F("<h3>Select Device To Recover</h3>");
  // CSS to style the on/off buttons
  // Feel free to change the background-color and font-size attributes to fit your preferences
  webpage += F("<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: left;}");
  webpage += F(".button { background-color: #555555; border: none; color: white; padding: 16px 40px;");
  webpage += F("text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}");
  webpage += F(".button2 {background-color: #4CAF50;}</style></head>");

  webpage += F("<p>All Device Recovery - State ");
  webpage += recoverAllState;
  webpage += F("</p>");

  // If the recoverAllState is off, it displays the ON button
  if (recoverAllState == "off") {
    webpage += F("<p><a href=\"/recover/on\"><button class=\"button\">OFF</button></a></p>");
  } else {
    webpage += F("<p><a href=\"/recover/off\"><button class=\"button button2\">ON</button></a></p>");
  }


  webpage += F("<p>Node 1 Recovery - State ");
  webpage += Node1Recovery;
  webpage += F("</p>");

  if (Node1Recovery == "off") {
    webpage += F("<p><a href=\"/recover/Node1on\"><button class=\"button\">OFF</button></a></p>");
  } else {
    webpage += F("<p><a href=\"/recover/Node1off\"><button class=\"button button2\">ON</button></a></p>");
  }
  append_page_footer();
  server.send(200, "text/html", webpage);
}

void BuzzerOn() {
  recoverAllState = "on";
  Serial.println("Buzzer On Called");  // Debugging
  userScheduler.deleteTask(taskSendMessageRecoveryOFF);
  taskSendMessageRecoveryOFF.disable();

  userScheduler.addTask(taskSendMessageRecoveryOn);  //.deleteTask
  taskSendMessageRecoveryOn.enable();
  Serial.println("Task Enabled");

  refreshButtons();
}

void BuzzerOff() {
  recoverAllState = "off";

  Serial.println("Buzzer off called");  // Debugging
  userScheduler.deleteTask(taskSendMessageRecoveryOn);
  taskSendMessageRecoveryOn.disable();

  userScheduler.addTask(taskSendMessageRecoveryOFF);  //.deleteTask
  taskSendMessageRecoveryOFF.enable();
  Serial.println("Task Enabled");  // Debugging

  refreshButtons();
}

void Node1BuzzerOn() {
  Node1Recovery = "on";
  Serial.println("Node 1 Recovery Called");  // Debugging
  userScheduler.deleteTask(taskSendMessageRecoveryOFFNode1);
  taskSendMessageRecoveryOFFNode1.disable();

  userScheduler.addTask(taskSendMessageRecoveryOnNode1);  //.deleteTask
  taskSendMessageRecoveryOnNode1.enable();
  Serial.println("Task Enabled");

  refreshButtons();
}


void Node1BuzzerOff() {
  Node1Recovery = "off";

  Serial.println("Node 1 Recovery off called");  // Debugging
  userScheduler.deleteTask(taskSendMessageRecoveryOnNode1);
  taskSendMessageRecoveryOnNode1.disable();

  userScheduler.addTask(taskSendMessageRecoveryOFFNode1);  //.deleteTask
  taskSendMessageRecoveryOFFNode1.enable();
  Serial.println("Task Enabled");  // Debugging

  refreshButtons();
}

//Prints the directory, it is called in void SD_dir()
void printDirectory(const char* dirname, uint8_t levels) {

  File root = SD.open(dirname);

  if (!root) {
    return;
  }
  if (!root.isDirectory()) {
    return;
  }
  File file = root.openNextFile();

  int i = 0;
  while (file) {
    if (webpage.length() > 1000) {
      SendHTML_Content();
    }
    if (file.isDirectory()) {
      webpage += "<tr><td>" + String(file.isDirectory() ? "Dir" : "File") + "</td><td>" + String(file.name()) + "</td><td></td></tr>";
      printDirectory(file.name(), levels - 1);
    } else {
      webpage += "<tr><td>" + String(file.name()) + "</td>";
      webpage += "<td>" + String(file.isDirectory() ? "Dir" : "File") + "</td>";
      int bytes = file.size();
      String fsize = "";
      if (bytes < 1024) fsize = String(bytes) + " B";
      else if (bytes < (1024 * 1024)) fsize = String(bytes / 1024.0, 3) + " KB";
      else if (bytes < (1024 * 1024 * 1024)) fsize = String(bytes / 1024.0 / 1024.0, 3) + " MB";
      else fsize = String(bytes / 1024.0 / 1024.0 / 1024.0, 3) + " GB";
      webpage += "<td>" + fsize + "</td>";
      webpage += "<td>";
      webpage += F("<FORM action='/' method='post'>");
      webpage += F("<button type='submit' name='download'");
      webpage += F("' value='");
      webpage += "download_" + String(file.name());
      webpage += F("'>Download</button>");
      webpage += "</td>";
      webpage += "<td>";
      webpage += F("<FORM action='/' method='post'>");
      webpage += F("<button type='submit' name='delete'");
      webpage += F("' value='");
      webpage += "delete_" + String(file.name());
      webpage += F("'>Delete</button>");
      webpage += "</td>";
      webpage += "</tr>";
    }
    file = root.openNextFile();
    i++;
  }
  file.close();
}

//Download a file from the SD, it is called in void SD_dir()
void SD_file_download(String filename) {
  if (SD_present) {
    File download = SD.open("/" + filename);
    if (download) {
      server.sendHeader("Content-Type", "text/text");
      server.sendHeader("Content-Disposition", "attachment; filename=" + filename);
      server.sendHeader("Connection", "close");
      server.streamFile(download, "application/octet-stream");
      download.close();
    } else ReportFileNotPresent("download");
  } else ReportSDNotPresent();
}

//Handles the file upload a file to the SD
File UploadFile;
//Upload a new file to the Filing system
void handleFileUpload() {
  HTTPUpload& uploadfile = server.upload();  //See https://github.com/esp8266/Arduino/tree/master/libraries/ESP8266WebServer/srcv
                                             //For further information on 'status' structure, there are other reasons such as a failed transfer that could be used
  if (uploadfile.status == UPLOAD_FILE_START) {
    String filename = uploadfile.filename;
    if (!filename.startsWith("/")) filename = "/" + filename;
    Serial.print("Upload File Name: ");
    Serial.println(filename);
    SD.remove(filename);                         //Remove a previous version, otherwise data is appended the file again
    UploadFile = SD.open(filename, FILE_WRITE);  //Open the file for writing in SD (create it, if doesn't exist)
    filename = String();
  } else if (uploadfile.status == UPLOAD_FILE_WRITE) {
    if (UploadFile) UploadFile.write(uploadfile.buf, uploadfile.currentSize);  // Write the received bytes to the file
  } else if (uploadfile.status == UPLOAD_FILE_END) {
    if (UploadFile)  //If the file was successfully created
    {
      UploadFile.close();  //Close the file again
      Serial.print("Upload Size: ");
      Serial.println(uploadfile.totalSize);
      webpage = "";
      append_page_header();
      webpage += F("<h3>File was successfully uploaded</h3>");
      webpage += F("<h2>Uploaded File Name: ");
      webpage += uploadfile.filename + "</h2>";
      webpage += F("<h2>File Size: ");
      webpage += file_size(uploadfile.totalSize) + "</h2><br><br>";
      webpage += F("<a href='/'>[Back]</a><br><br>");
      append_page_footer();
      server.send(200, "text/html", webpage);
    } else {
      ReportCouldNotCreateFile("upload");
    }
  }
}

//Delete a file from the SD, it is called in void SD_dir()
void SD_file_delete(String filename) {
  if (SD_present) {
    SendHTML_Header();
    File dataFile = SD.open("/" + filename, FILE_READ);  //Now read data from SD Card
    if (dataFile) {
      if (SD.remove("/" + filename)) {
        Serial.println(F("File deleted successfully"));
        webpage += "<h3>File '" + filename + "' has been erased</h3>";
        webpage += F("<a href='/'>[Back]</a><br><br>");
      } else {
        webpage += F("<h3>File was not deleted - error</h3>");
        webpage += F("<a href='/'>[Back]</a><br><br>");
      }
    } else ReportFileNotPresent("delete");
    append_page_footer();
    SendHTML_Content();
    SendHTML_Stop();
  } else ReportSDNotPresent();
}

//SendHTML_Header
void SendHTML_Header() {
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "-1");
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/html", "");  //Empty content inhibits Content-length header so we have to close the socket ourselves.
  append_page_header();
  server.sendContent(webpage);
  webpage = "";
}

//SendHTML_Content
void SendHTML_Content() {
  server.sendContent(webpage);
  webpage = "";
}

//SendHTML_Stop
void SendHTML_Stop() {
  server.sendContent("");
  server.client().stop();  //Stop is needed because no content length was sent
}

//ReportSDNotPresent
void ReportSDNotPresent() {
  SendHTML_Header();
  webpage += F("<h3>No SD Card present</h3>");
  webpage += F("<a href='/'>[Back]</a><br><br>");
  append_page_footer();
  SendHTML_Content();
  SendHTML_Stop();
}

//ReportFileNotPresent
void ReportFileNotPresent(String target) {
  SendHTML_Header();
  webpage += F("<h3>File does not exist</h3>");
  webpage += F("<a href='/");
  webpage += target + "'>[Back]</a><br><br>";
  append_page_footer();
  SendHTML_Content();
  SendHTML_Stop();
}

//ReportCouldNotCreateFile
void ReportCouldNotCreateFile(String target) {
  SendHTML_Header();
  webpage += F("<h3>Could Not Create Uploaded File (write-protected?)</h3>");
  webpage += F("<a href='/");
  webpage += target + "'>[Back]</a><br><br>";
  append_page_footer();
  SendHTML_Content();
  SendHTML_Stop();
}

//File size conversion
String file_size(int bytes) {
  String fsize = "";
  if (bytes < 1024) fsize = String(bytes) + " B";
  else if (bytes < (1024 * 1024)) fsize = String(bytes / 1024.0, 3) + " KB";
  else if (bytes < (1024 * 1024 * 1024)) fsize = String(bytes / 1024.0 / 1024.0, 3) + " MB";
  else fsize = String(bytes / 1024.0 / 1024.0 / 1024.0, 3) + " GB";
  return fsize;
}