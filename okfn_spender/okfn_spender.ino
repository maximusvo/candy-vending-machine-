#include <Servo.h>
#include "HX711.h"
#include <LiquidCrystal_I2C.h>
#include <PciManager.h>
#include <Debouncer.h>
#include <Rotary.h>

#define calibration_factor  -7050.0   
// **pin config**
#define DOUT                5         // LCD
#define CLK                 4
#define start_input         8

#define vibrator            7         // actors
#define servo               9
#define ROT_PIN_A           A0        // encoder
#define ROT_PIN_B           A1
#define ROT_PUSH            A2

int     pos = 0;                      // variable to store the servo position
char    weight[8];                    // buffer to store scale input
boolean flag;
int     millis_start = 0;
int     millis_startup = 0;
int     current_min = 0;
int     state = 0;
int     stated_weight = 100;
int     scale_step = 10;
int     mode = 1;
int     time_to_start = 60;

Servo myservo;                        // create servo object to control a servo
LiquidCrystal_I2C lcd(0x27, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);  // LCD I2C Adresse setzen Pin A4 & A5
HX711 scale(DOUT, CLK);

// define method signatures for encoder
void onRotate(short direction, Rotary* rotary);
void onRotPushPress();
void onRotPushRelease(unsigned long pressTime);

Rotary r(ROT_PIN_A, ROT_PIN_B, onRotate, true);
Debouncer rotPushDebouncer(ROT_PUSH, MODE_CLOSE_ON_PUSH, onRotPushPress, onRotPushRelease, true);

void setup() {
  PciManager.registerListener(ROT_PUSH, &rotPushDebouncer);
  Serial.begin(9600);
  lcd.begin(16,2);                    // activate LCD for 16 chars and 2 lines
  lcd.backlight();
  myservo.attach(servo);              // attaches the servo to the servo object
  pinMode(start_input, INPUT_PULLUP);
  pinMode(vibrator, OUTPUT);

  scale.set_scale(calibration_factor); 
  scale.tare();                       //Assuming there is no weight on the scale at start up, reset the scale to 0
  
  Serial.println("System startup finished");
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.write("** Startup");
  lcd.setCursor(0,1);
  lcd.write("** finished");
  delay(3000);
  lcd.clear();
}

void loop() {
  //encoder
   SoftTimer.run();
   int corrent_millis =  millis();
   if(corrent_millis - millis_startup >= 60000)
   {
    millis_startup = corrent_millis;
    if(current_min <= 480)
    {
      current_min += 1;
    }
    else
    {
      current_min = 0;
    }
    Serial.println(current_min);
   }
  
  if((state == 0 & digitalRead(start_input) == LOW & (mode == 1 | mode == 3)) | (state == 0 & time_to_start == current_min & (mode == 2 | mode == 3)))
  {
    state=1;
  }
  
  if(state==1)
  { 
    if((digitalRead(vibrator) == LOW & scale.get_units()*100 <= 10) | flag == true) // start vibrator and check the weight
    {
      flag = false;
      digitalWrite(vibrator, HIGH);
      dtostrf(fabsf(scale.get_units()*100), 6, 2, weight);                          // converts the float scale output to char
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.write(weight);
      lcd.write("g POWER");
    }
    
    if(digitalRead(vibrator) == HIGH & scale.get_units()*100 >= scale_step)         // if the scale_step is reached, stop vibrator
    {
      digitalWrite(vibrator, LOW);
      scale_step += 10;
      millis_start = millis();
    }
                                                                                    // wait 1 second for more load and start the vibrator again, if the stated_weight isn't reached
    if(digitalRead(vibrator) == LOW & corrent_millis - millis_start >= 1000 & !(scale.get_units()*100 >= stated_weight))  
    {
      millis_start = millis();
      flag = true;
    }
                                                                                    // if the stated_weight is reached, stop the vibrator
    if(scale.get_units()*100 >= stated_weight)
    {
      flag = false;
      digitalWrite(vibrator, LOW);
      millis_start = millis();
    }
                                                                                    // wait 1 second and start the payout
    if(corrent_millis - millis_start >= 1000 & scale.get_units()*100 >= stated_weight)
    {
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.write("Ready to eat!");
      
      for (pos = 0; pos <= 180; pos += 1) 
      {                                                                              // goes from 0 degrees to 180 degrees - in steps of 1 degree
        myservo.write(pos);                                                          // tell servo to go to position in variable 'pos'
      }
      millis_start = millis();
    }
    
    if(pos >= 180 & corrent_millis - millis_start >= 1000)                           // if servo reached 180 degrees wait 1 second and pull the servo back
    {
      for (pos = 180; pos >= 0; pos -= 1) 
      {                                         
        myservo.write(pos);                                                          
      }
      current_min = 0;
      state = 0;                                                                        // process finished
    }
  }
}

void onRotate(short direction, Rotary* rotary) {
  if(mode == 5)
  {
    if(direction == DIRECTION_CW) {
      if(stated_weight < 100)
      {
        stated_weight += 10;
      }
      dtostrf(stated_weight, 6, 0, weight);
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.write("Change max load:");
      lcd.setCursor(0,1);
      lcd.write(weight);
      lcd.write("g");
    }
    if(direction == DIRECTION_CCW) {
      if(stated_weight >= 20)
      {
        stated_weight -= 10;
      }
      dtostrf(stated_weight, 6, 0, weight);
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.write("Change max load:");
      lcd.setCursor(0,1);
      lcd.write(weight);
      lcd.write("g");
    }
  }
  if(mode == 4)
    {
      if(direction == DIRECTION_CW) {
        if(time_to_start < 480)
        {
          time_to_start += 10;
        }
        dtostrf(time_to_start, 6, 0, weight);
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.write("Startup every/:");
        lcd.setCursor(0,1);
        lcd.write(weight);
        lcd.write("min");
      }
      if(direction == DIRECTION_CCW) {
        if(time_to_start >= 10)
        {
          time_to_start -= 10;
        }
        dtostrf(time_to_start, 6, 0, weight);
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.write("Startup every/:");
        lcd.setCursor(0,1);
        lcd.write(weight);
        lcd.write("min");
      }
    }
}

void onRotPushPress() {
    if(mode <= 5)
    {
      mode += 1;
    }
    switch(mode) 
    {
      case 1: 
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.write("Modus:");
        lcd.setCursor(0,1);
        lcd.write("Button");
        break;
      case 2: 
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.write("Modus:");
        lcd.setCursor(0,1);
        lcd.write("Time");
        current_min = 0;
        break;
      case 3: 
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.write("Modus:");
        lcd.setCursor(0,1);
        lcd.write("Button and Time");
        current_min = 0;
        break;
      case 4: 
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.write("Modus:");
        lcd.setCursor(0,1);
        lcd.write("Change time");
        break;
      case 5: 
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.write("Modus:");
        lcd.setCursor(0,1);
        lcd.write("Change loade");
        break;
      default: break;
    }
    if(mode > 5)
    {
      mode = 0;
    }
}
void onRotPushRelease(unsigned long pressTime) {
  
}

