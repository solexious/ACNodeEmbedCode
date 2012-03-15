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
//                                              //
// Working functions:                           //
// networkCheckCard() - no supervisors          //
// networkAddCard()                             //
// networkReportToolStatus()                    //
// networkCheckToolStatus()                     //
// networkReportToolUse() - no ID passed        //
// networkCaseAlert()                           //
//                                              //
// Non-working functions:                       //
// networkSyncDB() - get ID from message body   //
// networkReportToolTime() - needs server side  //
//                                              //
// Functions to write:                          //
// syncDB() - use addCardToDB to force resync   //
//                                              //
// Functions to test:                           //
// addCardToDB() - add card to location provided//
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
IPAddress server(192,168,5,93); // Google
EthernetClient client;

// Global Vars
byte returnedDBID[8];
byte readRFID[8];
byte supervisorRFID[8];
byte dbsyncRFID[8];
byte cardToAddID[8];
int toolStatusMethod;
int nodeID = 1;
char hostName[] = "";
bool toolStatusSetting = true;

long previousMillis2 = 0;

void setup(){

  Wire.begin();
  Serial.begin(9600);

  pinMode(RFIDsense, INPUT);
  pinMode(statusLED[0], OUTPUT);
  pinMode(statusLED[1], OUTPUT);
  pinMode(statusLED[2], OUTPUT);
  pinMode(relayPin, OUTPUT);

  byte mooID[] = {
    4,48,121,34,228,34,128,7    };
  byte blankmooID[] = {
    255,255,255,255,255,255,255,255    };

  eeWrite(8,mooID);
  eeWrite(0,blankmooID);

  setRGB(0,0,255);
  if (Ethernet.begin(mac) == 0) {
    Serial.println("Failed to configure Ethernet using DHCP");
    // no point in carrying on, so do nothing forevermore:
    for(;;)
      ;
  }

  toolStatusSetting = networkCheckToolStatus();

  if(toolStatusSetting){
    setRGB(255,255,0);
  }
  else{
    setRGB(0,255,255);
  }
  toolOperation(false);


  rfid.seekTag();

}

bool addCardToDB(bool autoSet, byte addID[8], long location = 0){

  if(autoSet){

    // Search for free location
    bool IDcheck = true;
    bool IDmatch = false;
    long x = 0;
    for(; IDcheck == true && IDmatch == false; x++){

      if(returnDBID(x)){
        // ID has been retrived, now check it
        bool matchCheck = true;
        for(int y = 0; y < 8 && matchCheck; y++){
          // Loop through array and check
          if(returnedDBID[y]!=0){
            matchCheck = false;
          }
        }
        IDmatch = matchCheck;
      }
      else{
        IDcheck = false;
      }
    }

    location = x;

    // If IDcheck is false x is the last location in the db, else have found a empty spot in the db
    if(!IDcheck){
      location -= 1;
      byte endID[] = { 
        225, 225, 255, 255, 255, 255, 255, 255             };
      eeWrite(((location+1)*8),addID);
    }
  }
  eeWrite((location*8),addID);
}

void loop(){

  if(getRFID()){ // Read a card at the acnode
    // Set led to "thinking"
    setRGB(0,0,255);

    if(checkID()){ // Card is in the database
      bool clearToUse = true;
      if(!toolStatusSetting){ // Tool out of service, check if supervisor
        if(networkCheckCard()!=2){ // Is not a supervisor
          clearToUse = false;
        }
      }
      if(clearToUse){ // Ok to use the device, turn on and log
        setRGB(255,0,255);
        toolOperation(true);
        networkReportToolUse(true);
        long useTimeStart = millis();
        while(!digitalRead(RFIDsense)){ // Wait for card to be removed
        }
        toolOperation(false);
        networkReportToolUse(false);
        //networkReportToolTime(millis()-useTimeStart);
      }
      else{ // Not ok to use, disable
        setRGB(0,255,255);
        toolOperation(false);
        while(!digitalRead(RFIDsense)){
        } 
      }
    }
    else{ // Card not in DB, check with the network
      if(networkCheckCard()){ // Card found add to db
        addCardToDB(true, readRFID);
        toolOperation(false);
        long previousMillis = 0;
        int ledState = LOW;
        while(!digitalRead(RFIDsense)){ // Flash led green and wait for card to be removed for revalidation
          unsigned long currentMillis = millis();
          if(currentMillis - previousMillis > 200) {
            previousMillis = currentMillis;
            ledState = !ledState;
            if (ledState == LOW){
              setRGB(255,255,255);
            }
            else{
              setRGB(255,0,255);
            }
          }
        }
        /*setRGB(0,0,0);
         while(!digitalRead(RFIDsense)){
         }*/
      }
      else{ // Card not found on network, solid red led untill card removed
        setRGB(0,255,255);
        toolOperation(false);
        while(!digitalRead(RFIDsense)){
        }
      }
    }
  }
  if(toolStatusSetting){
    setRGB(255,255,0);
  }
  else{
    setRGB(0,255,255);
  }

  // Check tool status
  long currentMillis2 = millis();
  if(currentMillis2 - previousMillis2 > 60000) {
    previousMillis2 = currentMillis2;
    toolStatusSetting = networkCheckToolStatus();
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
  for(long x = 0; IDcheck == true && IDmatch == false; x++){

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

bool toolOperation(bool stat){

  if(toolStatusMethod==0){
    // Use relay
    digitalWrite(relayPin, stat);
  }

  return true;

}



// NETOWRK FUNCTIONS

int networkCheckCard(){

  // Check readRFID with the server
  if (client.connect(server, 8071)) {
    // Make a HTTP request:
    client.print("GET /");
    client.print(nodeID);
    client.print("/card/");

    char msg[2];
    for(int x;x<readRFID[7];x++){
      sprintf(msg, "%02X", readRFID[x]);
      client.print(msg);
    }

    client.println(" HTTP/1.0");
    client.print("Host: ");
    client.println(hostName);
    client.println("Accept: text/plain");
    client.println();

    // ADD RESPONCE HANDLING HERE
    while(!client.available()){
    }
    char httpResponce[250];
    int y = 0;
    while(client.available() && y<251) {
      httpResponce[y] = client.read();
      y++;
    }
    client.stop();
    httpResponce[y] = 0;

    Serial.println(httpResponce);

    char* r1 = httpResponce;
    char* r2 = strstr(r1, "HTTP/1.0 200 OK");
    if(r2){
      char* r3 = strstr(r1, "\r\n\r\n")+4;
      if(strstr(r3, "1")){
        return 1;
      }
      else if(strstr(r3, "2")){
        return 2;
      }
      else{
        return 0;
      }
    }
    else{
      return 0;
    }


  }
  else{
    return 0;
  }
} 

bool networkAddCard(){

  // Check readRFID with the server
  if (client.connect(server, 8071)) {
    // Make a HTTP request:
    client.print("POST /");
    client.print(nodeID);
    client.println("/card/ HTTP/1.0");
    client.print("Host: ");
    client.println(hostName);
    client.println("Accept: text/plain");
    client.print("Content-Length: ");
    client.println((int)((readRFID[7]*2)+(supervisorRFID[7]*2)+1));
    client.println("Content-Type: text/plain");
    client.println();

    char msg[2];
    for(int x;x<readRFID[7];x++){
      sprintf(msg, "%02X", readRFID[x]);
      client.print(msg);
    }

    client.print(",");

    for(int x;x<supervisorRFID[7];x++){
      sprintf(msg, "%02X", supervisorRFID[x]);
      client.print(msg);
    }

    client.println();
    client.println();

    // ADD RESPONCE HANDLING HERE
    while(!client.available()){
    }
    char httpResponce[250];
    int y = 0;
    while(client.available() && y<251) {
      httpResponce[y] = client.read();
      y++;
    }
    client.stop();
    httpResponce[y] = 0;

    char* r1 = httpResponce;
    char* r2 = strstr(r1, "HTTP/1.0 200 OK");
    if(r2){
      char* r3 = strstr(r1, "\r\n\r\n")+4;
      if(strstr(r3, "OK")){
        return true;
      }
      else{
        return false;
      }
    }
    else{
      return false;
    }

  }
  else{
    return false;
  }
} 

bool networkSyncDB(byte syncID[8]){

  // Check readRFID with the server
  if (client.connect(server, 8071)) {
    // Make a HTTP request:
    client.print("GET /");
    client.print(nodeID);
    client.print("/sync/");

    if(syncID[7]!=0){
      char msg[2];
      for(int x;x<readRFID[7];x++){
        sprintf(msg, "%02X", readRFID[x]);
        client.print(msg);
      }
    }

    client.println(" HTTP/1.0");
    client.print("Host: ");
    client.println(hostName);
    client.println("Accept: text/plain");
    client.println();

    // ADD RESPONCE HANDLING HERE
    while(!client.available()){
    }
    char httpResponce[250];
    int y = 0;
    while(client.available() && y<251) {
      httpResponce[y] = client.read();
      y++;
    }
    client.stop();
    httpResponce[y] = 0;

    // TODO
    char* r1 = httpResponce;
    char* r2 = strstr(r1, "HTTP/1.0 200 OK");
    if(r2){
      char* r3 = strstr(r1, "\r\n\r\n")+4;
      if(strstr(r3, "END")){
        return false;
      }
      else{
        // Extract the card ID TODO
        return true;
      }
    }
    else{
      return false;
    }
  }
  else{
    return false;
  }
} 

bool networkReportToolStatus(bool stat){

  // Check readRFID with the server
  if (client.connect(server, 8071)) {
    // Make a HTTP request:
    client.print("PUT /");
    client.print(nodeID);
    client.println("/status/ HTTP/1.0");
    client.print("Host: ");
    client.println(hostName);
    client.println("Accept: text/plain");
    client.println("Content-Length: 1");
    client.println("Content-Type: text/plain");
    client.println();

    client.println((int)stat);

    client.println();

    // ADD RESPONCE HANDLING HERE
    while(!client.available()){
    }
    char httpResponce[250];
    int y = 0;
    while(client.available() && y<251) {
      httpResponce[y] = client.read();
      y++;
    }
    client.stop();
    httpResponce[y] = 0;

    char* r1 = httpResponce;
    char* r2 = strstr(r1, "HTTP/1.0 200 OK");
    if(r2){
      char* r3 = strstr(r1, "\r\n\r\n")+4;
      if(strstr(r3, "OK")){
        return true;
      }
      else{
        return false;
      }
    }
    else{
      return false;
    }

  }
  else{
    return false;
  }
} 

bool networkCheckToolStatus(){

  // Check readRFID with the server
  if (client.connect(server, 8071)) {
    // Make a HTTP request:
    client.print("GET /");
    client.print(nodeID);
    client.println("/status/ HTTP/1.0");
    client.print("Host: ");
    client.println(hostName);
    client.println("Accept: text/plain");
    client.println();

    // ADD RESPONCE HANDLING HERE
    char msg[2];
    while(!client.available()){
    }
    char httpResponce[250];
    int y = 0;
    while(client.available() && y<251) {
      httpResponce[y] = client.read();
      y++;
    }
    client.stop();
    httpResponce[y] = 0;

    char* r1 = httpResponce;
    char* r2 = strstr(r1, "HTTP/1.0 200 OK");
    if(r2){
      char* r3 = strstr(r1, "\r\n\r\n")+4;
      if(strstr(r3, "1")){
        return true;
      }
      else{
        return false;
      }
    }
    else{
      return false;
    }

  }
  else{
    Serial.println("Can't connect");
    return false;
  }
} 

bool networkReportToolUse(bool stat){

  // Check readRFID with the server
  if (client.connect(server, 8071)) {
    // Make a HTTP request:
    client.print("PUT /");
    client.print(nodeID);
    client.println("/tooluse/ HTTP/1.0");
    client.print("Host: ");
    client.println(hostName);
    client.println("Accept: text/plain");
    client.print("Content-Length: ");
    client.println((int)((readRFID[7]*2)+2));
    //client.println("1");
    client.println("Content-Type: text/plain");
    client.println();

    client.print((int)stat);
    client.print(",");

    char msg[2];
    for(int x;x<readRFID[7];x++){
      sprintf(msg, "%02X", readRFID[x]);
      client.print(msg);
      Serial.print(msg);
    }
    Serial.println();

    client.println();
    client.println();

    // ADD RESPONCE HANDLING HERE
    while(!client.available()){
    }
    char httpResponce[250];
    int y = 0;
    while(client.available() && y<251) {
      httpResponce[y] = client.read();
      y++;
    }
    client.stop();
    httpResponce[y] = 0;
    
    Serial.println(httpResponce);

    char* r1 = httpResponce;
    char* r2 = strstr(r1, "HTTP/1.0 200 OK");
    if(r2){
      char* r3 = strstr(r1, "\r\n\r\n")+4;
      if(strstr(r3, "OK")){
        return true;
      }
      else{
        return false;
      }
    }
    else{
      return false;
    }

  }
  else{
    return false;
  }
} 

bool networkReportToolTime(long timeUsed){

  // Check readRFID with the server
  if (client.connect(server, 8071)) {
    // Make a HTTP request:
    client.print("POST /");
    client.print(nodeID);
    client.println("/tooluse/time/ HTTP/1.0");
    client.print("Host: ");
    client.println(hostName);
    client.println("Accept: text/plain");
    client.print("Content-Length: ");
    client.println((int)(getLength(timeUsed)+1+(readRFID[7]*2)));
    client.println("Content-Type: text/plain");
    client.println();

    client.print(timeUsed);
    client.print(",");

    char msg[2];
    for(int x;x<readRFID[7];x++){
      sprintf(msg, "%02X", readRFID[x]);
      client.print(msg);
    }

    client.println();
    client.println();

    // ADD RESPONCE HANDLING HERE
    while(!client.available()){
    }
    char httpResponce[250];
    int y = 0;
    while(client.available() && y<251) {
      httpResponce[y] = client.read();
      y++;
    }
    client.stop();
    httpResponce[y] = 0;

    char* r1 = httpResponce;
    char* r2 = strstr(r1, "HTTP/1.0 200 OK");
    if(r2){
      char* r3 = strstr(r1, "\r\n\r\n")+4;
      if(strstr(r3, "OK")){
        return true;
      }
      else{
        return false;
      }
    }
    else{
      return false;
    }

  }
  else{
    return false;
  }
} 

bool networkCaseAlert(bool stat){

  // Check readRFID with the server
  if (client.connect(server, 8071)) {
    // Make a HTTP request:
    client.print("PUT /");
    client.print(nodeID);
    client.println("/case/ HTTP/1.0");
    client.print("Host: ");
    client.println(hostName);
    client.println("Accept: text/plain");
    client.println("Content-Length: 1");
    client.println("Content-Type: text/plain");
    client.println();

    client.println((int)stat);
    //client.println("3");

    client.println();

    // ADD RESPONCE HANDLING HERE
    while(!client.available()){
    }
    char httpResponce[250];
    int y = 0;
    while(client.available() && y<251) {
      httpResponce[y] = client.read();
      y++;
    }
    client.stop();
    httpResponce[y] = 0;

    char* r1 = httpResponce;
    char* r2 = strstr(r1, "HTTP/1.0 200 OK");
    if(r2){
      char* r3 = strstr(r1, "\r\n\r\n")+4;
      if(strstr(r3, "OK")){
        return true;
      }
      else{
        return false;
      }
    }
    else{
      return false;
    }

  }
  else{
    return false;
  }
} 

int getLength(long someValue) {
  // there's at least one byte:
  int digits = 1;
  // continually divide the value by ten, 
  // adding one to the digit count for each
  // time you divide, until you're at 0:
  int dividend = someValue /10;
  while (dividend > 0) {
    dividend = dividend /10;
    digits++;
  }
  // return the number of digits:
  return digits;
}









