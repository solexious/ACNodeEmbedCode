//////////////////////////////////////////////////
//                                              //
// Author: Charles Yarnold                      //
// Licence: GPL v3                              //
// Project page: http://bit.ly/yXnZMH           //
//                                              //
// Change Log:                                  //
//   v0.1 Initial code                          //
//                                              //
//////////////////////////////////////////////////

// Includes
#include <Wire.h>
#include "eepromi2c.h"
#include <SL018.h>
#include <SPI.h>
#include <Ethernet.h>

// Pin assignments
const int buttonPin = 4;
const int relayPin = 2;
const int statusLED[3] = { 
  3, 5, 6 };
const int RFIDsense = 14;

// Inits
SL018 rfid;
byte mac[] = {  
  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress server(173,194,34,88); // Google
EthernetClient client;

// Global Vars
byte returnedDBID[8];
byte readRFID[8];
byte supervisorRFID[8];
byte dbsyncRFID[8];
int toolStatusMethod;
int nodeID = 0;
char hostName[] = "babbage";

void setup(){

  Wire.begin();
  Serial.begin(9600);

  pinMode(RFIDsense, INPUT);
  pinMode(statusLED[0], OUTPUT);
  pinMode(statusLED[1], OUTPUT);
  pinMode(statusLED[2], OUTPUT);
  pinMode(relayPin, OUTPUT);


  setRGB(0,0,255);
  if (Ethernet.begin(mac) == 0) {
    Serial.println("Failed to configure Ethernet using DHCP");
    // no point in carrying on, so do nothing forevermore:
    for(;;)
      ;
  }
  setRGB(255,255,0);
  toolStatus(false);


  rfid.seekTag();

}

void loop(){

  if(getRFID()){

    if(checkID()){

      Serial.println("correct");
      setRGB(255,0,255);
      toolStatus(true);


    }
    else{

      Serial.println("no match");
      setRGB(0,255,255);
      toolStatus(false);

    }
    while(!digitalRead(RFIDsense)){
    }
    setRGB(255,255,0);
    toolStatus(false);
  }

}

bool returnDBID(int offset){

  eeRead((offset*8),returnedDBID);

  bool ffcheck = false;
  for(int x = 0; x < 8 && !ffcheck; x++){

    if(returnedDBID[x]!=255){
      ffcheck = true;
    }
  }

  //Serial.println(offset);

  return ffcheck;
}

bool checkID(){

  bool IDcheck = true;
  bool IDmatch = false;

  // Loop through DB, if found or out of IDs quit
  for(int x = 0; IDcheck == true && IDmatch == false; x++){

    if(returnDBID(x)){
      // ID has been retrived, now check it
      bool matchCheck = true;
      for(int y = 0; y < 8 && matchCheck; y++){
        // Loop through array and check
        if(returnedDBID[y]!=readRFID[y]){
          matchCheck = false;
        }
      }
      IDmatch = matchCheck;
    }
    else{
      IDcheck = false;
    }
  }
  return IDmatch;
}

bool getRFID(){

  rfid.seekTag();
  if(rfid.available()){
    byte* gotRFID = rfid.getTagNumber();

    // check len and pad if needed
    int IDLen = rfid.getTagLength();
    for(int x = 0; x<8; x++){
      // Shift across id to global and pad if needed
      if(x<IDLen){
        readRFID[x] = gotRFID[x];
      }
      else{
        readRFID[x] = 0;
      }
    }
    readRFID[7] = IDLen;

    return true;
  }
  else{
    return false;
  }
}


bool setRGB(int r, int g, int b){

  analogWrite(statusLED[0], r);
  analogWrite(statusLED[1], g);
  analogWrite(statusLED[2], b);

  return true;

}

bool toolStatus(bool stat){

  if(toolStatusMethod==0){
    // Use relay
    digitalWrite(relayPin, stat);
  }

  return true;

}

bool networkCheckCard(){

  // Check readRFID with the server
  if (client.connect(server, 80)) {
    // Make a HTTP request:
    client.print("GET /");
    client.print(nodeID);
    client.print("/card/");

    char msg[2];
    for(int x;x<readRFID[7];x++){
      sprintf(msg, "%02x", readRFID[x]);
      client.print(msg);
    }

    client.println(" HTTP/1.0");
    client.print("Host: ");
    client.println(hostName);
    client.println("Accept: text/plain");
    client.println();

    // ADD RESPONCE HANDLING HERE

  }
  else{
    return false;
  }
} 

bool networkAddCard(){

  // Check readRFID with the server
  if (client.connect(server, 80)) {
    // Make a HTTP request:
    client.print("POST /");
    client.print(nodeID);
    client.print("/card/ HTTP/1.0");
    client.print("Host: ");
    client.println(hostName);
    client.println("Content-Type: text/plain");

    char msg[2];
    for(int x;x<readRFID[7];x++){
      sprintf(msg, "%02x", readRFID[x]);
      client.print(msg);
    }

    client.print(",");

    for(int x;x<supervisorRFID[7];x++){
      sprintf(msg, "%02x", supervisorRFID[x]);
      client.print(msg);
    }

    client.println();
    client.println();

    // ADD RESPONCE HANDLING HERE

  }
  else{
    return false;
  }
} 

bool networkSyncDB(byte syncID[8]){

  // Check readRFID with the server
  if (client.connect(server, 80)) {
    // Make a HTTP request:
    client.print("GET /");
    client.print(nodeID);
    client.print("/sync/");

    if(syncID[7]!=0){
      char msg[2];
      for(int x;x<readRFID[7];x++){
        sprintf(msg, "%02x", readRFID[x]);
        client.print(msg);
      }
    }

    client.println(" HTTP/1.0");
    client.print("Host: ");
    client.println(hostName);
    client.println("Accept: text/plain");
    client.println();

    // ADD RESPONCE HANDLING HERE

  }
  else{
    return false;
  }
} 

bool networkReportToolStatus(bool stat){

  // Check readRFID with the server
  if (client.connect(server, 80)) {
    // Make a HTTP request:
    client.print("PUT /");
    client.print(nodeID);
    client.print("/status/ HTTP/1.0");
    client.print("Host: ");
    client.println(hostName);
    client.println("Content-Type: text/plain");

    client.println((int)stat);

    client.println();

    // ADD RESPONCE HANDLING HERE

  }
  else{
    return false;
  }
} 

bool networkCheckToolStatus(){

  // Check readRFID with the server
  if (client.connect(server, 80)) {
    // Make a HTTP request:
    char msg[2];
    client.print("GET /");
    client.print(nodeID);
    client.print("/status/ HTTP/1.0");
    client.print("Host: ");
    client.println(hostName);
    client.println("Accept: text/plain");
    client.println();

    // ADD RESPONCE HANDLING HERE

  }
  else{
    return false;
  }
} 

bool networkReportToolUse(bool stat){

  // Check readRFID with the server
  if (client.connect(server, 80)) {
    // Make a HTTP request:
    client.print("PUT /");
    client.print(nodeID);
    client.print("/tooluse/ HTTP/1.0");
    client.print("Host: ");
    client.println(hostName);
    client.println("Content-Type: text/plain");

    client.print((int)stat);
    client.print(",");

    char msg[2];
    for(int x;x<readRFID[7];x++){
      sprintf(msg, "%02x", readRFID[x]);
      client.print(msg);
    }

    client.println();
    client.println();

    // ADD RESPONCE HANDLING HERE

  }
  else{
    return false;
  }
} 

bool networkReportToolTime(long timeUsed){

  // Check readRFID with the server
  if (client.connect(server, 80)) {
    // Make a HTTP request:
    client.print("POST /");
    client.print(nodeID);
    client.print("/tooluse/time/ HTTP/1.0");
    client.print("Host: ");
    client.println(hostName);
    client.println("Content-Type: text/plain");

    client.print(timeUsed);
    client.print(",");

    char msg[2];
    for(int x;x<readRFID[7];x++){
      sprintf(msg, "%02x", readRFID[x]);
      client.print(msg);
    }

    client.println();
    client.println();

    // ADD RESPONCE HANDLING HERE

  }
  else{
    return false;
  }
} 

bool networkCaseAlert(bool stat){

  // Check readRFID with the server
  if (client.connect(server, 80)) {
    // Make a HTTP request:
    client.print("PUT /");
    client.print(nodeID);
    client.print("/case/ HTTP/1.0");
    client.print("Host: ");
    client.println(hostName);
    client.println("Content-Type: text/plain");

    client.println((int)stat);

    client.println();

    // ADD RESPONCE HANDLING HERE

  }
  else{
    return false;
  }
} 

