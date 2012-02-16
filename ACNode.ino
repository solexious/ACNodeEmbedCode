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

// Pin assignments

const int buttonPin = 4;
const int relayPin = 2;
const int statusLED[3] = { 
  3, 5, 6 };
const int RFIDsense = 14;

// Inits

SL018 rfid;

// Global Vars
byte returnedDBID[8];
byte readRFID[8];
int toolStatusMethod;

void setup(){

  Wire.begin();
  Serial.begin(9600);

  pinMode(RFIDsense, INPUT);
  pinMode(statusLED[0], OUTPUT);
  pinMode(statusLED[1], OUTPUT);
  pinMode(statusLED[2], OUTPUT);
  pinMode(relayPin, OUTPUT);


  setRGB(255,255,0);
  digitalWrite(relayPin, LOW);


  rfid.seekTag();
  byte insertID[8] = { 
    4, 66, 41, 226, 208, 35, 128, 7        };
  eeWrite(0,insertID);
  insertID = { 
    4, 66, 41, 226, 208, 35, 128, 7        };
  eeWrite(1*8,insertID);
  insertID = { 
    4, 66, 41, 226, 208, 35, 128, 7        };
  eeWrite(3*8,insertID);

  for(int y=4; y<99; y++){
    eeWrite(y*8,insertID);
  }

  insertID = { 
    71,254,62,180,0,0,0,4        };
  eeWrite(792,insertID);
  insertID = { 
    255, 255, 255, 255, 255, 255, 255, 255         };
  eeWrite(100*8,insertID);

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
