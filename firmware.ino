//sbeen a while, so good to do it again
#include <LiquidCrystal.h>
#include <Wire.h>

#define encoder0PinA 2
#define encoder0PinB 3
// 0x4B is the default i2c address
#define MAX9744_I2CADDR 0x4B

volatile int encoder0Pos = 0;
 
// We'll track the volume level in this variable.
int8_t thevol = 31;
byte speaker[8] = {
  B00011,
  B00111,
  B11111,
  B11111,
  B00111,
  B00011,
  B00000,
};

LiquidCrystal lcd(12, 11, 5, 4, 9, 8);//3,2->9,8 last 4 digits is data


void setup(){
  pinMode(13, OUTPUT);
  // set up the LCD's number of columns and rows:
  lcd.createChar(0, speaker);
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
  
  if (encoder0Pos != enchange){
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
  delay(100);
  
}

void updatelcd(int encoder){
    lcd.setCursor(0, 1);
    lcd.print("                ");
    lcd.setCursor(0,1);
    lcd.print("Vol: ");
    lcd.print(encoder);
    lcd.setCursor(14,1);
    lcd.write(byte(0));//custom character 0(typecast it with byte)
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


