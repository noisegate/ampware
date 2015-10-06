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

#include <Adafruit_NeoPixel.h>
#ifdef __AVR__
  #include <avr/power.h>
#endif

#define MAXMODES       3  //max number of blinking modes
#define NUMPIXELS      16
#define MAXPIXEL       NUMPIXELS-1 //the zerobased highest address

#define RXD 0           //atmel pin 2_PD0
#define TXD 1           //atmel pin 3_PD1
//encoder code http://playground.arduino.cc/Main/RotaryEncoders
//cant change these, should be 2 & 3 for interrupt

#define encoder0PinA 2   //atmel pin 4_PD2
#define encoder0PinB 3   //atmel pin 5_PD3

//other functionality
#define MUTEBUTTONPIN 9  //atmel pin 15_PB1
#define BACKLIGHT 11     //atmel pin 17_PB3 must be pwm
#define NEOPIN 10        //atmel pin 16_PB2 must be pwm
#define MUTESIGNALPIN 4  //atmel pin 06_PD4

#define RS    13        //atmel pin 19_PB5 LCD pin 4
#define EN    12        //atmel pin 18_PB4 LCD pin 6
#define D4     5        //atmel pin 11_PD5 LCD pin 11
#define D5     6        //atmel pin 12_PD6 LCD pin 12   
#define D6     7        //atmel pin 13_PD7 LCD pin 13
#define D7     8        //atmel pin 14_PB0 LCD pin 14

#define SPEAKER 0       //custom character
#define LOUD 1          //custom character

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

const byte redmap[] PROGMEM =   {5,5,5,5,5,5,5,5,5,5,5,10,10,10,10,10};
const byte greenmap[] PROGMEM = {5,5,5,5,5,5,5,5,5,5,5,0,0,0,0,0};
const byte bluemap[] PROGMEM =  {5,5,5,5,5,5,5,5,5,5,5,0,0,0,0,0};

volatile int encoder0Pos = 0;
volatile boolean standby=true;// I think is good 2b standby when ya turn it on 
volatile byte blinkmode=0;

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

//IU objects
LiquidCrystal lcd(RS, EN,D4,D5,D6,D7);
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NUMPIXELS, NEOPIN, NEO_GRB + NEO_KHZ800);

void setup(){
  //pcbpinMode(13, OUTPUT);
  pinMode(MUTESIGNALPIN, OUTPUT);
  pinMode(MUTEBUTTONPIN, INPUT_PULLUP);
  pinMode(BACKLIGHT, OUTPUT);
  digitalWrite(MUTESIGNALPIN, LOW);
    // set up the LCD's number of columns and rows:
  lcd.createChar(SPEAKER, speaker);
  lcd.createChar(LOUD, loud);
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
  digitalWrite(BACKLIGHT, HIGH);  
  updatelcd(0);


  #if defined (__AVR_ATtiny85__)
  if (F_CPU == 16000000) clock_prescale_set(clock_div_1);
  #endif
  // End of trinket special code

  pixels.begin(); // This initializes the NeoPixel library.

  updatering(0);

  standbystate();
}

void loop(){

  static int enchange=0;
  
  boolean temp;
  
  if (standby)
    blinkring();
  else{  
    if ((encoder0Pos != enchange)){
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
      updatering(enchange);
      if (!setvolume(int8_t(enchange)))
        msglcd("I2C ERROR");
    }
  }  
   
  if (digitalRead(MUTEBUTTONPIN)==LOW){

    unsigned long t0;
    boolean clp;//check longpress

    t0 = millis();
    clp = true;

    flashring();//alwayz feeback!!

     
    while(digitalRead(MUTEBUTTONPIN)==LOW){
      //dont just wait till release but decide what if
      //it is a long press...
      if (((millis()-t0)>1000)&&clp){
        //handle long press
        flashring();//need some feedback!!
        flashring();//twice @ longpress
        blinkmode++;
        if (blinkmode>MAXMODES) blinkmode=0;
        clp=false;//dont repeat during longpress
        if (!standby)
          updatering(enchange);
      }
    };

    if (clp){
      //if clp still true, there was no longpress so toggle function
      //and do whatever has to be done
      standby=!standby;
      
      if (standby){
        //mute and go standby
        for (int i=enchange; i>0; i--){
          //updatelcd(i);
          updatering(i);
          if (!setvolume(int8_t(i)))
            msglcd("I2C ERROR");
          analogWrite(BACKLIGHT, (byte)(255.0*(i+1)/(enchange+1)));
          delay(10);
        }
        updatelcd(0);
        standbystate();
      }else{
        digitalWrite(MUTESIGNALPIN, LOW);
        encoder0Pos = enchange; //if vol knob turned during standby, dismiss result
        //wakeup. do it slowly, we dont want to blow our tweeters
        for (int i=0; i<=enchange; i++){
          //updatelcd(i);
          updatering(i);
          if (!setvolume(int8_t(i)))
            msglcd("I2C ERROR");
          analogWrite(BACKLIGHT, (byte)(255.0*(i+1)/(enchange+1)));   
          delay(10);
        }
        updatelcd(enchange);
        //awakestate();
      }
    }
    
    delay(500);//software debounce
  }
  
}

void standbystate(){
  digitalWrite(MUTESIGNALPIN, LOW);
  analogWrite(BACKLIGHT, 0);
  mutelcd();
}

void awakestate(){
  digitalWrite(MUTESIGNALPIN, HIGH);
  analogWrite(BACKLIGHT, 255);
}

void mutelcd(){
  lcd.clear();
  lcd.print("Standby");
}

void updatelcd(int encoder){
    float attenuation;

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

void updatering(int encoder){

  for(int i=0;i<NUMPIXELS;i++){
    pixels.setPixelColor(i, pixels.Color(0, 0, 0)); // clear
  }

  pixels.setPixelColor(MAXPIXEL-encoder/4, 
                        pixels.Color(pgm_read_byte_near(redmap+(encoder/4)),
                                     pgm_read_byte_near(greenmap+(encoder/4)),
                                     pgm_read_byte_near(bluemap+(encoder/4))));
  
  pixels.show(); // This sends the updated pixel color to the hardware.
}

void blinkring(){
  static byte bouncy = 1;
  static unsigned int delaycnt = 10000;
  static int steps = 1;
  static byte phase =0;
  static byte cycles = 0;
  delaycnt++;
  
  if (delaycnt > 10001){
    delaycnt=0;
    if (cycles == 0){
      for (int i=0; i<NUMPIXELS; i++){
        pixels.setPixelColor(i, pixels.Color(0, 0, 0));
      }
      pixels.setPixelColor(0+(phase%8), pixels.Color(10,10,10));
      pixels.setPixelColor(8+(phase%8), pixels.Color(10,10,10));
      
      phase++;
      if (phase>63) {
      phase=0;
      cycles=1;
      }
    }
    else{
  
      bouncy += steps;
      if (bouncy>6) steps=-1;
      if (bouncy<2) {
        //cycles++;
        steps=1;
      }
      
      
      for (int i=0; i<NUMPIXELS; i++){
        if (blinkmode==0)
          pixels.setPixelColor(i, pixels.Color(bouncy, bouncy, bouncy));
        if (blinkmode==1)
          pixels.setPixelColor(i, wheel((i+phase)*16));
        if (blinkmode==2)
          pixels.setPixelColor(i, pixels.Color(0, 0, bouncy));
      }
      phase++;
    }
    pixels.show();
  }
}

void flashring(void){
  //flash entire ring once. 
  //I think direct feedback is a good thing, 
  //you should know if the device got your message
  for (int i=0; i<NUMPIXELS;i++){
    pixels.setPixelColor(i, pixels.Color(10, 10, 10));
  }
  pixels.show();
  delay(200);
  for (int i=0; i<NUMPIXELS;i++){
    pixels.setPixelColor(i, pixels.Color(0, 0, 0));
  }  
  pixels.show();
  delay(200);
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


uint32_t wheel(byte WheelPos) {
  // Input a value 0 to 255 to get a color value.
  // The colours are a transition r - g - b - back to r.
  // code with small adaptation from 
  // https://learn.adafruit.com/florabrella/test-the-neopixel-strip
  // 85 = 256/3 256 = full circle 


  if(WheelPos < 85) {
   return pixels.Color((WheelPos * 3)/20, (255 - WheelPos * 3)/20, 0);
  } 
  else if(WheelPos < 170) {
   WheelPos -= 85;
   return pixels.Color((255 - WheelPos * 3)/20, 0, (WheelPos * 3)/20);
  } 
  else {
   WheelPos -= 170;
   return pixels.Color(0, (WheelPos * 3)/20, (255 - WheelPos * 3)/20);
  }
}

