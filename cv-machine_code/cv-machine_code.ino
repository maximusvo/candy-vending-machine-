#include <Servo.h>
#include "HX711.h"
#include <LiquidCrystal_I2C.h>
#include <PciManager.h>
#include <Debouncer.h>
#include <Rotary.h>

#define calibration_factor  -252006   // scale calibration factor  
// **pin config**
#define DOUT                5         // scale
#define CLK                 4
#define start_input         8         // input for signal to start mechanism
#define vibrator            12        // actors
#define servo               9
#define ROT_PIN_A           A0        // encoder
#define ROT_PIN_B           A1
#define ROT_PUSH            A2
#define reset               11

int     pos = 0;                      // variable to store the servo position
char    char_bucket[8];               // bucket to store floating numbers
boolean flag_vibrator_running = false;
long    millis_process = 0;
long    millis_startup = 0;
long    current_millis;
long    process_watch;
int     current_min = 0;
float   stated_weight = 10.0;
float   scale_status_begin = 0.0;
int     mode = 1;
int     time_to_start = 60;

Servo myservo;                                                  // create servo object to control a servo
LiquidCrystal_I2C lcd(0x27, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);  // sets LCD at pin A4 & A5 and I2C adress 
HX711 scale(DOUT, CLK);                                         // create scale object

// define method signatures for encoder
void onRotate(short direction, Rotary* rotary);
void onRotPushPress();
void onRotPushRelease(unsigned long pressTime);
// define methods for mechanism
void start_giveLoad(Task* me);
void giveLoad(Task* me);
void time_last_startup(Task* me);
void servo_turn_up(Task* me);
void servo_turn_down(Task* me);
void vibrator_step(Task* task);
// define method to clear the lcd
boolean clear_lcd(Task* task);
boolean false_vibrator_running(Task* task);

Task t_start_giveLoad(100, start_giveLoad);
Task t_giveLoad(100, giveLoad);
Task t_time_last_startup(50, time_last_startup);
Task t_servo_turn_up(10, servo_turn_up);
Task t_servo_turn_down(10, servo_turn_down);
Task t_vibrator_step(10, vibrator_step);
DelayRun t_clear_lcd(3000, clear_lcd);
DelayRun t_false_vibrator_running(2000, false_vibrator_running);

Rotary r(ROT_PIN_A, ROT_PIN_B, onRotate, true);
Debouncer rotPushDebouncer(ROT_PUSH, MODE_CLOSE_ON_PUSH, onRotPushPress, onRotPushRelease, true);

void setup() {
  // activate serial communication
  Serial.begin(9600);                 
  // activate LCD for 16 chars and 2 lines
  lcd.begin(16,2);                    
  lcd.backlight();
  // activate timers and navigation hardware
  SoftTimer.add(&t_start_giveLoad);  
  SoftTimer.add(&t_time_last_startup);
  PciManager.registerListener(ROT_PUSH, &rotPushDebouncer);
  pinMode(start_input, INPUT_PULLUP); // define buzzer/startup input
  pinMode(vibrator, OUTPUT);          // define vibrator output
  pinMode(reset, INPUT);
  
  myservo.attach(servo);              // attaches the servo to the servo object and make clear there is no weight on the scale by moving the servo ones
  for (pos = 0; pos < 180; pos += 1) 
  {                                                                             
    myservo.write(pos);
    delay(10);                                                          
  }
  for (pos = 180; pos > 0; pos -= 1) 
  {                                                                             
    myservo.write(pos);  
    delay(10);                                                         
  }

  // scale config
  scale.set_scale(calibration_factor); 
  scale.tare();                       // assuming there is no weight on the scale at start up, reset the scale to 0
  
  Serial.println("System startup finished");
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.write("** Startup  **");
  lcd.setCursor(0,1);
  lcd.write("** finished **");
  t_clear_lcd.startDelayed();
}

void servo_turn_up(Task* me)
{
  if(pos != 0)
  {
    myservo.write(pos);                                                          // tell servo to go to position in variable 'pos'
    pos -= 1;
  }
  else
  {
    SoftTimer.remove(&t_servo_turn_up);
  }
}

void servo_turn_down(Task* me)
{
  if(pos != 180)
  {
    myservo.write(pos);                                                          // tell servo to go to position in variable 'pos'
    pos += 1;
  }
  else
  {
    SoftTimer.remove(&t_servo_turn_down);
  }
}

boolean clear_lcd(Task* me)
{
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.write("Let's get");
  lcd.setCursor(0,1);
  lcd.write("some sweet");
  return true;
}

boolean false_vibrator_running(Task* me)
{
  flag_vibrator_running = LOW;
  return true;
}
// method to run the vibrator in steps. the method stops the vibrator after the scale status has increased to 3g.
void vibrator_step(Task* me)
{
  float scale_status = scale.get_units()*100;
  digitalWrite(vibrator, HIGH);
  flag_vibrator_running = true;
  
  if (scale_status - scale_status_begin >= 3.0)                                 
  {
    digitalWrite(vibrator, LOW);
    t_false_vibrator_running.startDelayed();
    SoftTimer.remove(&t_vibrator_step);
  }
  
  if (current_millis - process_watch >= 20000.0)                                  // force a reset after 20 seconds of running the vibrator
  {
    pinMode(reset, OUTPUT);
    digitalWrite(reset, LOW);
  }
}
// method to check the time
void time_last_startup(Task* me)
{
   current_millis = millis();
   if(current_millis - millis_startup >= 60000.0)
   {
    millis_startup = current_millis;
    if(current_min <= 480)
    {
      current_min += 1;
    }
    else
    {
      current_min = 0;
    }
   }
}
// method to start mechanism
void start_giveLoad(Task* me)
{
   if((digitalRead(start_input) == LOW & (mode == 1 | mode == 3)) | (current_min >= time_to_start & (mode == 2 | mode == 3)))
   {
     lcd.clear();
     process_watch = current_millis;
     // process finished
     SoftTimer.add(&t_giveLoad);
     SoftTimer.remove(&t_start_giveLoad);
   }

}
// mechanism
void giveLoad(Task* me)
{
  float scale_status = scale.get_units()*100;
  
  if (!flag_vibrator_running & scale_status < stated_weight & pos == 0)
  {
    scale_status_begin = scale_status;
    SoftTimer.add(&t_vibrator_step);
  }
  
  if (scale_status >= stated_weight & !flag_vibrator_running)
  {
    SoftTimer.remove(&t_vibrator_step);
  }
  else
  {
    dtostrf(fabsf(scale_status), 6, 1, char_bucket);                              // converts the float scale output to char
    lcd.setCursor(0,0);
    lcd.write("Prepairing...");
    lcd.setCursor(0,1);
    lcd.write(char_bucket);
    lcd.write("g Energy");
    millis_process = current_millis;
  }
                                                                                  // wait 3 seconds and start the payout
  if(current_millis - millis_process >= 3000.0 & !flag_vibrator_running & pos == 0)
  {
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.write("Ready to eat!");
    SoftTimer.add(&t_servo_turn_down);
  }
  if(pos >= 180)                                                                  // if servo reached 180 degrees wait 1,5 second and pull the servo back
  {
    SoftTimer.add(&t_servo_turn_up);
    current_min = 0;
    // process finished        
    t_clear_lcd.startDelayed();                                                                       
    SoftTimer.add(&t_start_giveLoad);
    SoftTimer.remove(&t_giveLoad);
  }
}
  
void onRotate(short direction, Rotary* rotary) {
  
  if(mode == 5)
  {
    if(direction == DIRECTION_CW) {
      if(stated_weight < 100.0)
      {
        stated_weight += 10.0;
      }
    }
    if(direction == DIRECTION_CCW) {
      if(stated_weight >= 20.0)
      {
        stated_weight -= 10.0;
      }
    }
    dtostrf(stated_weight, 6, 0, char_bucket);
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.write("Change max load:");
    lcd.setCursor(0,1);
    lcd.write(char_bucket);
    lcd.write("g");
  }
  
  if(mode == 4)
  {
    if(direction == DIRECTION_CW) {
      if(time_to_start < 480)
      {
        time_to_start += 5;
      }
    }
    if(direction == DIRECTION_CCW) {
      if(time_to_start >= 10)
      {
        time_to_start -= 5;
      }
    }
    dtostrf(time_to_start, 6, 0, char_bucket);
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.write("Startup every/:");
    lcd.setCursor(0,1);
    lcd.write(char_bucket);
    lcd.write("min");
  }
  t_clear_lcd.startDelayed();
}

void onRotPushPress() {
    if(mode <= 5)
    {
      mode += 1;
    }
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.write("Mode:");
    lcd.setCursor(0,1);
    switch(mode) 
    {
      case 1: 
        lcd.write("Input");
        break;
      case 2: 
        lcd.write("Time");
        current_min = 0;
        break;
      case 3: 
        lcd.write("Input and Time");
        current_min = 0;
        break;
      case 4: 
        lcd.write("Change time");
        break;
      case 5: 
        lcd.write("Change payload");
        break;
      default: break;
    }
    if(mode > 5)
    {
      lcd.clear();
      mode = 0;
    }
}

void onRotPushRelease(unsigned long pressTime) {
  t_clear_lcd.startDelayed();
}

