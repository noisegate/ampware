/*
firmware for TransistorLove Industries class D amplifier.
Copyright (C) 2015 Marcell Marosvolgyi

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as publisgithed by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include <LiquidCrystal.h>
#include <Wire.h>
#include <avr/pgmspace.h>

//encoder code http://playground.arduino.cc/Main/RotaryEncoders
#define encoder0PinA 2
#define encoder0PinB 3
#define mutebuttonPin 6
#define mutesignalPin 7

//took some code from adafruit 
//https://learn.adafruit.com/adafruit-20w-stereo-audio-amplifier-class-d-max9744/digital-control
// 0x4B is the default i2c address
#define MAX9744_I2CADDR 0x4B

//https://www.arduino.cc/en/Reference/PROGMEM
//const dataType variableName[] PROGMEM = {};   // use this form

//data from max9744 datasheet
const float decibel[] PROGMEM = {
  -92.8,
  -90.3,
  -86.8,
  -84.3,
  -80.8,
  -78.3,
  -74.7,
  -72.2,
  -68.7,
  -66.2,
  -62.2,
  -60.2,
  -56.7,
  -54.2,
  -50.6,
  -48.1,
  -45.6,
  -43.7,
  -42.1,
  -39.6,
  -37.6,
  -36.0,
  -33.4,
  -31.5,
  -29.8,
  -27.2,
  -25.2,
  -23.5,
  -21.6,
  -19.7,
  -17.5,
  -16.4,
  -15.4,
  -14.4,
  -13.1,
  -12.0,
  -10.9,
  -9.9,
  -8.9,
  -7.1,
  -6.0,
  -5.0,
  -3.4,
  -1.9,
  -0.5,
  0.5,
  1.2,
  1.6,
  2.0,
  2.4,
  2.9,
  3.4,
  3.9,
  4.4,
  4.9,
  5.4,
  5.9,
  6.5,
  7.0,
  7.6,
  8.2,
  8.8,
  9.5
};

volatile int encoder0Pos = 0;
volatile boolean standby=false; 
// We'll track the volume level in this variable.
int8_t thevol = 31;
byte speaker[8] = {
  B00010,
  B00110,
  B11110,
  B11110,
  B11110,
  B00110,
  B00010,
};

byte loud[8]={
   B11111,
   B00000,
   B11111,
   B00000,
   B11111,
   B00000,
   B11111,
   B00000,
};

LiquidCrystal lcd(12, 11, 5, 4, 9, 8);//3,2->9,8 last 4 digits is data


void setup(){
  pinMode(13, OUTPUT);
  pinMode(mutesignalPin, OUTPUT);
  pinMode(mutebuttonPin, INPUT);digitalWrite(mutebuttonPin, HIGH);//pullup
  
  digitalWrite(mutesignalPin, LOW);
    // set up the LCD's number of columns and rows:
  lcd.createChar(0, speaker);
  lcd.createChar(1, loud);
  lcd.begin(16, 2);
  // Print a message to the LCD.
  lcd.print("power amp on");
  
  //encoder stuff
  pinMode(encoder0PinA, INPUT); 
  pinMode(encoder0PinB, INPUT);   
  attachInterrupt(1, doEncoderA, CHANGE);
  attachInterrupt(0, doEncoderB, CHANGE);  

  Wire.begin();//i2c stuff
  
  //fun stuff
  
  updatelcd(0);
}

void loop(){
  // set the cursor to column 0, line 1
  // (note: line 1 is the second row, since counting begins with 0):
  static int enchange=0;
  
  boolean temp;
  
  if ((encoder0Pos != enchange) && !standby){
    //we do this here and use enchange so the interrupt 
    //cant change the value between the if statement and the update
    //setvolume
    enchange = encoder0Pos;
    if (enchange>63){
      enchange = 63;
      encoder0Pos = 63;
    }
    if (enchange<0){
      enchange=0;
      encoder0Pos = 0;
    }
    updatelcd(enchange);
    if (!setvolume(int8_t(enchange)))
      msglcd("I2C ERROR");
  }
  
 // temp = digitalRead(mutebuttonPin);
  
  if (digitalRead(mutebuttonPin)==LOW){
    standby=!standby;
    //digitalWrite(13,HIGH);
    if (standby){
      //mute and go standby
      digitalWrite(13, HIGH);
      for (int i=enchange; i>0; i--){
        updatelcd(i);
        if (!setvolume(int8_t(i)))
          msglcd("I2C ERROR");
        delay(10);
      }
      digitalWrite(mutesignalPin, HIGH);
      mutelcd();
    }else{
      digitalWrite(13, LOW);
      digitalWrite(mutesignalPin, LOW);
      encoder0Pos = enchange; //if vol knob turned during standby, dismiss result
      //wakeup. do it slowly, we dont want to blow our tweeters
      for (int i=0; i<enchange; i++){
        updatelcd(i);
        if (!setvolume(int8_t(i)))
          msglcd("I2C ERROR");
        delay(10);
      }
      
    }
    while(digitalRead(mutebuttonPin)==LOW){};
    delay(1000);//software debounce
  }
  //delay(100);
  
}

void mutelcd(){
  lcd.clear();
  lcd.print("Standby");
}

void updatelcd(int encoder){
    float attenuation;
    //lcd.setCursor(0,0);
    //lcd.print ("                ");
    //lcd.setCursor(0, 1);
    //lcd.print ("                ");
    //lcd.setCursor(0,0);
    lcd.clear();
    lcd.print("Vol: ");
    lcd.print((int)(encoder/63.0*100));
    lcd.setCursor(10,0);
    lcd.print(" %");
    attenuation = pgm_read_float_near(decibel+encoder);
    lcd.setCursor(0,1);
    lcd.print("Att: ");
    lcd.print(attenuation);
    lcd.setCursor(10,1);
    lcd.print(" dB");
    lcd.setCursor(14,0);
    lcd.write(byte(0));//custom character 0(typecast it with byte)
    lcd.setCursor(15,0);
    if(encoder!=0)
      lcd.write(byte(1));
    
} 

void msglcd(String m){
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print (m);
}

boolean setvolume(int8_t v) {
  // cant be higher than 63 or lower than 0
  if (v > 63) v = 63;
  if (v < 0) v = 0;
  
  Wire.beginTransmission(MAX9744_I2CADDR);
  Wire.write(v);
  if (Wire.endTransmission() == 0) 
    return true;
  else
    return false;
}

void doEncoderA(){

  // look for a low-to-high on channel A
  if (digitalRead(encoder0PinA) == HIGH) { 
    // check channel B to see which way encoder is turning
    if (digitalRead(encoder0PinB) == LOW) {  
      encoder0Pos = encoder0Pos + 1;         // CW
    } 
    else {
      encoder0Pos = encoder0Pos - 1;         // CCW
    }
  }
  else   // must be a high-to-low edge on channel A                                       
  { 
    // check channel B to see which way encoder is turning  
    if (digitalRead(encoder0PinB) == HIGH) {   
      encoder0Pos = encoder0Pos + 1;          // CW
    } 
    else {
      encoder0Pos = encoder0Pos - 1;          // CCW
    }
  }
}

void doEncoderB(){

  // look for a low-to-high on channel B
  if (digitalRead(encoder0PinB) == HIGH) {   
   // check channel A to see which way encoder is turning
    if (digitalRead(encoder0PinA) == HIGH) {  
      encoder0Pos = encoder0Pos + 1;         // CW
    } 
    else {
      encoder0Pos = encoder0Pos - 1;         // CCW
    }
  }
  // Look for a high-to-low on channel B
  else { 
    // check channel B to see which way encoder is turning  
    if (digitalRead(encoder0PinA) == LOW) {   
      encoder0Pos = encoder0Pos + 1;          // CW
    } 
    else {
      encoder0Pos = encoder0Pos - 1;          // CCW
    }
  }
}


