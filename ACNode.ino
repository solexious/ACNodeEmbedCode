//////////////////////////////////////////////////
//                                              //
// Author: Charles Yarnold                      //
// Licence: GPL v3                              //
// Project page: http://bit.ly/yXnZMH           //
//                                              //
// Change Log:                                  //
//   v0.1 Initial code                          //
//                                              //
//                                              //
//////////////////////////////////////////////////

// Includes
#include <Wire.h>
#include "eepromi2c.h"
#include <SL018.h>
#include <SPI.h>
#include <Ethernet.h>
#include <EEPROM.h>

// Pin assignments
const int buttonPin = 4;
const int relayPin = 2;
const int statusLED[3] = { 
  3, 5, 6 };
const int RFIDsense = 14;

// Inits
SL018 rfid;
byte mac[] = {  
  0xDE, 0xFF, 0xBE, 0xEF, 0xFE, 0xFF };
IPAddress server(172,31,24,5);
int serverPort = 1234;
EthernetClient client;

// Global Vars
byte returnedDBID[8];
byte readRFID[8];
byte supervisorRFID[8];
byte dbsyncRFID[8];
byte cardToAddID[8];
int toolStatusMethod;
int nodeID = 1;
char acHostName[] = "babbage";
bool toolStatusSetting = true;
bool networkStatus = true;

long previousMillis2 = 0;

void setup(){

  Wire.begin();
  Serial.begin(9600);

  pinMode(RFIDsense, INPUT);
  pinMode(statusLED[0], OUTPUT);
  pinMode(statusLED[1], OUTPUT);
  pinMode(statusLED[2], OUTPUT);
  pinMode(relayPin, OUTPUT);
  pinMode(buttonPin,INPUT);
  digitalWrite(buttonPin, HIGH);

  toolStatusSetting = EEPROM.read(100);

  byte mooID[8] = {
    4,71,30,234,242,44,1,7  };
  byte blankmooID[8] = {
    255,255,255,255,255,255,255,255          };

  //i2ceeWrite(0,mooID);
  i2ceeWrite(0,blankmooID);

  i2ceeRead(0, mooID);

  Serial.println(int(mooID[7]));

  char msg[2];  
  for(int x;x<7;x++){
    sprintf(msg, "%02X", mooID[x]);
    Serial.print(msg);
  }
  
  Serial.println();
  
  i2ceeRead(8, mooID);

  Serial.println(int(mooID[7]));

  char msgg[2];  
  for(int x;x<7;x++){
    sprintf(msgg, "%02X", mooID[x]);
    Serial.print(msgg);
  }
  
  Serial.println();
  

  setRGB(0,0,255);
  if (Ethernet.begin(mac) == 0) {
    Serial.println("Failed to configure Ethernet using DHCP");
    // no point in carrying on, so do nothing forevermore:
    for(;;)
      ;
  }

  bool toolStatusSettingNetwork = networkCheckToolStatus();
  if((networkStatus)&&(toolStatusSetting!=toolStatusSettingNetwork)){
    toolStatusSetting = toolStatusSettingNetwork;
    EEPROM.write(100, toolStatusSetting);
  }

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
    Serial.println("autoset true");
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
      Serial.println("id check is false");
      Serial.println(x);
      if (location){
        location -= 1;
      }
      byte endID[] = { 
        255, 255, 255, 255, 255, 255, 255, 255                               };
      i2ceeWrite(((location+1)*8),endID);
      //i2ceeWrite(16,endID);
      delay(10);
    }
  }

  Serial.println(sizeof(readRFID));

  i2ceeWrite((location*8),addID);

  delay(10);
}

void loop(){

  if(getRFID()){ // Read a card at the acnode
    if(!digitalRead(buttonPin)){
      
      supervisorRFID[0] = readRFID[0];
      supervisorRFID[1] = readRFID[1];
      supervisorRFID[2] = readRFID[2];
      supervisorRFID[3] = readRFID[3];
      supervisorRFID[4] = readRFID[4];
      supervisorRFID[5] = readRFID[5];
      supervisorRFID[6] = readRFID[6];
      supervisorRFID[7] = readRFID[7];
      
      setRGB(0,255,0);
      
      while(!digitalRead(RFIDsense)){
      }
      
      while(!getRFID()){
      }
      
      setRGB(0,0,255);
      
      networkAddCard();
      
    }
    else{
      // Set led to "thinking"
      setRGB(0,0,255);
      
      // If we seem to have a connection to the server, refresh tool status
      if(networkStatus){
        updateNetworkToolStatus();
      }
      
      // If we still seem to have a connection to the server, refresh the card status
      if(networkStatus){
        refreshNetworkCardDetails();
      }
      
  
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
          setRGB(0,0,255);
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
        }
        else{ // Card not found on network, solid red led untill card removed
          setRGB(0,255,255);
          toolOperation(false);
          while(!digitalRead(RFIDsense)){
          }
        }
      }
    }
  }
  if(toolStatusSetting){
    if(networkStatus){
      setRGB(255,255,0);
    }
    else{
      setRGB(0,0,255);
    }
  }
  else{
    setRGB(0,255,255);
  }

  // Check tool status every 60 seconds
  long currentMillis2 = millis();
  if(currentMillis2 - previousMillis2 > 60000) {
    previousMillis2 = currentMillis2;
    updateNetworkToolStatus();
  }

}

void refreshNetworkCardDetails(){
  
  if(networkCheckCard()){ // Card found add to db
    if(!checkID()){
      addCardToDB(true, readRFID);
      toolOperation(false);
      long previousMillis = 0;
      int ledState = LOW;
    }
  }
  else{ // Card not found on network, solid red led untill card removed
    // Remove from db
  }
}

void updateNetworkToolStatus(){
  
  bool toolStatusSettingNetwork = networkCheckToolStatus();
  if((networkStatus)&&(toolStatusSetting!=toolStatusSettingNetwork)){
    toolStatusSetting = toolStatusSettingNetwork;
    EEPROM.write(100, toolStatusSetting);
  }
}

bool returnDBID(int offset){

  i2ceeRead((offset*8),returnedDBID);

  bool ffcheck = false;
  for(int x = 0; x < 8 && !ffcheck; x++){

    if(returnedDBID[x]!=255){
      ffcheck = true;
    }
  }


  Serial.println(offset);

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
  if (client.connect(server, serverPort)) {
    // Make a HTTP request:
    client.print("GET /");
    client.print(nodeID);
    client.print("/card/");

    char msg[2];
    for(int x;x<readRFID[7];x++){
      sprintf(msg, "%02X", readRFID[x]);
      client.print(msg);
    }

    client.println(" HTTP/1.1");
    client.print("Host: ");
    client.println(acHostName);
    client.println("Accept: text/plain");
    client.println();

    // ADD RESPONCE HANDLING HERE
    while(!client.available()){
    }
    char httpResponce[250];
    int y = 0;
    while(client.available() && y<251) {
      httpResponce[y] = client.read();
      Serial.print(httpResponce[y]);
      y++;
    }
    client.stop();
    httpResponce[y] = 0;

    //Serial.println(httpResponce);

    char* r1 = httpResponce;
    char* r2 = strstr(r1, "HTTP/1.1 200");
    if(r2){
      char* r3 = strstr(r1, "\r\n\r\n")+4;
      Serial.print("card status returned:");
      Serial.println(r3);
      networkStatus = true;
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
      networkStatus = false;
      return 0;
    }


  }
  else{
    networkStatus = false;
    return 0;
  }
} 

bool networkAddCard(){

  // Check readRFID with the server
  if (client.connect(server, serverPort)) {
    // Make a HTTP request:
    client.print("POST /");
    client.print(nodeID);
    client.print("/grant-to-card/");
    char msg[2];
    for(int x;x<readRFID[7];x++){
      sprintf(msg, "%02X", readRFID[x]);
      client.print(msg);
    }
    client.print("/by-card/");
    for(int x;x<supervisorRFID[7];x++){
      sprintf(msg, "%02X", supervisorRFID[x]);
      client.print(msg);
    }
    client.println("/ HTTP/1.1");
    client.print("Host: ");
    client.println(acHostName);
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

    char* r1 = httpResponce;
    char* r2 = strstr(r1, "HTTP/1.1 200");
    if(r2){
      char* r3 = strstr(r1, "\r\n\r\n")+4;
      networkStatus = true;
      if(strstr(r3, "OK")){
        return true;
      }
      else{
        return false;
      }
    }
    else{
      networkStatus = false;
      return false;
    }

  }
  else{
    networkStatus = false;
    return false;
  }
} 

bool networkSyncDB(byte syncID[8]){

  // Check readRFID with the server
  if (client.connect(server, serverPort)) {
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

    client.println(" HTTP/1.1");
    client.print("Host: ");
    client.println(acHostName);
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
    char* r2 = strstr(r1, "HTTP/1.1 200");
    if(r2){
      char* r3 = strstr(r1, "\r\n\r\n")+4;
      networkStatus = true;
      if(strstr(r3, "END")){
        return false;
      }
      else{
        // Extract the card ID TODO
        return true;
      }
    }
    else{
      networkStatus = false;
      return false;
    }
  }
  else{
    networkStatus = false;
    return false;
  }
} 

bool networkReportToolStatus(bool stat){

  // Check readRFID with the server
  if (client.connect(server, serverPort)) {
    // Make a HTTP request:
    client.print("POST /");
    client.print(nodeID);
    client.println("/status/ HTTP/1.1");
    client.print("Host: ");
    client.println(acHostName);
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
    char* r2 = strstr(r1, "HTTP/1.1 200");
    if(r2){
      char* r3 = strstr(r1, "\r\n\r\n")+4;
      networkStatus = true;
      if(strstr(r3, "OK")){
        return true;
      }
      else{
        return false;
      }
    }
    else{
      networkStatus = false;
      return false;
    }

  }
  else{
    networkStatus = false;
    return false;
  }
} 

bool networkCheckToolStatus(){

  // Check readRFID with the server
  if (client.connect(server, serverPort)) {
    // Make a HTTP request:
    client.print("GET /");
    client.print(nodeID);
    client.println("/status/ HTTP/1.1");
    client.print("Host: ");
    client.println(acHostName);
    client.println("Accept: text/plain");
    client.println();

    // ADD RESPONCE HANDLING HERE
    char msg[2];
    while(!client.available()){
    }
    char httpResponce[250];
    int y = 0;
    while(client.connected() && y<251) {
      if(client.available()){
        httpResponce[y] = client.read();
        y++;
      }
    }
    client.stop();
    httpResponce[y] = 0;

    Serial.println(httpResponce);

    char* r1 = httpResponce;
    char* r2 = strstr(r1, "HTTP/1.1 200");
    if(r2){
      char* r3 = strstr(r1, "\r\n\r\n")+4;
      networkStatus = true;
      if(strstr(r3, "1")){
        Serial.println("1");
        return true;
      }
      else{
        Serial.println("0");
        return false;
      }
    }
    else{
      Serial.println("netdown");
      Serial.println(r2);
      networkStatus = false;
      return false;
    }

  }
  else{
    networkStatus = false;
    return false;
  }
} 

bool networkReportToolUse(bool stat){

  // Check readRFID with the server
  if (client.connect(server, serverPort)) {
    // Make a HTTP request:
    client.print("POST /");
    client.print(nodeID);
    client.print("/tooluse/"); 
    client.print((int)stat);
    client.print("/");
    char msg[2];
    for(int x;x<readRFID[7];x++){
      sprintf(msg, "%02X", readRFID[x]);
      client.print(msg);
      //Serial.print(msg);
    }
    client.println(" HTTP/1.1");
    client.print("Host: ");
    client.println(acHostName);
    client.println("Accept: text/plain");



    //Serial.println();

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

    //Serial.println(httpResponce);

    char* r1 = httpResponce;
    char* r2 = strstr(r1, "HTTP/1.1 200");
    if(r2){
      char* r3 = strstr(r1, "\r\n\r\n")+4;
      networkStatus = true;
      if(strstr(r3, "OK")){
        return true;
      }
      else{
        return false;
      }
    }
    else{
      networkStatus = false;
      return false;
    }

  }
  else{
    networkStatus = false;
    return false;
  }
} 

bool networkReportToolTime(long timeUsed){

  // Check readRFID with the server
  if (client.connect(server, serverPort)) {
    // Make a HTTP request:
    client.print("POST /");
    client.print(nodeID);
    client.println("/tooluse/time/ HTTP/1.1");
    client.print("Host: ");
    client.println(acHostName);
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
    char* r2 = strstr(r1, "HTTP/1.1 200");
    if(r2){
      char* r3 = strstr(r1, "\r\n\r\n")+4;
      networkStatus = true;
      if(strstr(r3, "OK")){
        return true;
      }
      else{
        return false;
      }
    }
    else{
      networkStatus = false;
      return false;
    }

  }
  else{
    networkStatus = false;
    return false;
  }
} 

bool networkCaseAlert(bool stat){

  // Check readRFID with the server
  if (client.connect(server, serverPort)) {
    // Make a HTTP request:
    client.print("PUT /");
    client.print(nodeID);
    client.println("/case/ HTTP/1.1");
    client.print("Host: ");
    client.println(acHostName);
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
    char* r2 = strstr(r1, "HTTP/1.1 200");
    if(r2){
      char* r3 = strstr(r1, "\r\n\r\n")+4;
      networkStatus = true;
      if(strstr(r3, "OK")){
        return true;
      }
      else{
        return false;
      }
    }
    else{
      networkStatus = false;
      return false;
    }

  }
  else{
    networkStatus = false;
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
