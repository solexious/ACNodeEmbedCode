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

// Inits

SL018 rfid;

// Global Vars
byte returnedDBID[7];
byte readRFID[7];

void setup(){

  Wire.begin();
  Serial.begin(9600);
  rfid.seekTag();
  byte insertID[7] = { 
    4, 66, 41, 226, 208, 35, 128  };
  eeWrite(0,insertID);
  insertID = { 
    71,254,62,180,0,0,0  };
  eeWrite(7,insertID);
  insertID = { 
    255, 255, 255, 255, 255, 255, 255   };
  eeWrite(7+7,insertID);

}

void loop(){

  if(getRFID()){

    if(checkID()){

      Serial.println("correct");

    }
    else{

      Serial.println("no match");

    }
    delay(5000);
  }

}

bool returnDBID(int offset){

  eeRead((offset*7),returnedDBID);

  bool ffcheck = false;
  for(int x = 0; x < 7 && !ffcheck; x++){

    if(returnedDBID[x]!=255){
      ffcheck = true;
    }
  }

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
      for(int y = 0; y < 7 && matchCheck; y++){
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
    for(int x = 0; x<7; x++){
      // Shift across id to global and pad if needed
      if(x<IDLen){
        readRFID[x] = gotRFID[x];
      }
      else{
        readRFID[x] = 0;
      }
    }
    return true;
  }
  else{
    return false;
  }
}



