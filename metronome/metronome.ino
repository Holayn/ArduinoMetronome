/**
* Kai Wong
* Metronome implemented with an Arduino. Comes with beat emphasis and tuning notes!
* CMPT422 Instructor Brian Gormanly
* 29 Nov 2017
* Note: import https://github.com/netlabtoolkit/VarSpeedServo into library
*/
#include <LiquidCrystal.h>
#include <VarSpeedServo.h> // https://github.com/netlabtoolkit/VarSpeedServo
// ---------------------SPEAKER-------------------------
#define NOTE_A5 880
#define NOTE_AS5 932
#define NOTE_B5  988
#define NOTE_C6  1047
#define NOTE_CS6 1109
#define NOTE_D6  1175
#define NOTE_DS6 1245
#define NOTE_E6  1319
#define NOTE_F6  1397
#define NOTE_FS6 1480
#define NOTE_G6  1568
#define NOTE_GS6 1661
#define NOTE_F5  698
const int speakerPin = 11; // pin for speaker
const int emphasis[4] = {1, 2, 3, 4}; // different beat emphasis for the speaker
int currentEmphasis = 0; // ptr for what emphasis we're on
int beepCtr = 0; // counter for what beat we're on
int tuneCtr = 0; // counter for what tuning note we're on
int tunes[] = {NOTE_A5, NOTE_AS5, NOTE_B5, NOTE_C6, NOTE_CS6, NOTE_D6, NOTE_DS6, NOTE_E6, NOTE_F6, NOTE_FS6, NOTE_G6, NOTE_GS6};
// ------------------------LCD -----------------------
const int contrast = 20; // the contrast for the LCD
const int dseven = 13; // seven display pin
const int dsix = 12; // six display pin
const int dfive = 10; // five display pin
const int dfour = 9; // four display pin
const int vzero = 6; // vzero pin
const int rs = 7; // register select pin
const int en = 4; // enabler pin
LiquidCrystal lcd(rs, en, dfour, dfive, dsix, dseven);
// ------------------------BUTTONS---------------------------
const int leftbut = 2; // interrupt pin for left button
const int rightbut = 3; // interrupt pin for right button
volatile int leftButtonState = 0; // used to keep track of the left button state
volatile int rightButtonState = 0; // used to keep track of the right button state
boolean bothPressed = false; // used to keep track if both buttons are pressed
boolean leftPressed = false; // used to keep track if the left button is pressed down
boolean leftReleased = false; // used to keep track if the left button has been released
boolean rightPressed = false; // used to keep track if the right button is pressed down
boolean rightReleased = false; // used to keep track if the right button has been released
boolean rightBothReleased = false; // used to keep track if both buttons were pressed and the right button has been released
boolean leftBothReleased = false; // used to keep track if both buttons were pressed and the left button has been released
// ------------------------SERVO----------------------------
VarSpeedServo metroServo;
const int metroPin = 5; // pin for metronome servo
int angle; // for the servo
int currentPos = 90; // Keeps track of stick position
boolean goingRight = true;
long previousMillis = 0; // will store last time stick was updated
float bpm = 80; // beats per minute for metronome
float rate; // this is the interval we need to wait to change angle (in milliseconds). Calculated off of BPM.
// ---------------------------------------------------------
const int led = 8; // led
long prevMillisSound; // will store last time sound was played
int soundDelay = 10; // delay to stop the sound playing
boolean hitMiddle = false; // boolean for stick at 90 degrees
boolean off = true; // boolean for turning light on or off

void setup() {
  metroServo.attach(metroPin); // servo setup
  Serial.begin(9600);
  // Set LEDs
  pinMode(led, OUTPUT);
  // Set the contrast
  pinMode(vzero, OUTPUT);
  analogWrite(vzero, contrast);
  // Print initial LCD
  lcd.begin(16, 2);
  lcd.clear();
  lcd.print("BPM: ");
  lcd.print((int)bpm);
  lcd.print (" 1/");
  lcd.print(emphasis[currentEmphasis]);
  lcd.setCursor(0,1);
  lcd.print("Adj. w/ btns");
  // Setup buttons
  pinMode(leftbut,INPUT);
  pinMode(rightbut,INPUT);
  // Attach interrupts to the ISR vector
  attachInterrupt(0, pin_leftbut_ISR, CHANGE); // issue interrrupt on state change
  attachInterrupt(1, pin_rightbut_ISR, CHANGE); // issue interrupt on state change
  metroServo.write(currentPos); // Move servo to current position
}

void loop() {
  metronome();
  // If break out of metronome, reset everything
  reset();
  delay(500);
}

// Performs the function of a metronome
void metronome() {
  prevMillisSound = millis();
  previousMillis = millis();
  updateBPMDisplay();
  while(true){
    unsigned long currentMillis = millis();
    // rate = (( 60.0 / bpm )*1000.0) / 180.0;
    rate = 60/bpm*1000;
    if(leftReleased){         // left button press
      resetFlags();
      stopBeep();             // stop any beeping
      // decrease bpm by 5
      if(bpm != 0){           // min BPM
        bpm = bpm - 5.0;
      }
      updateBPMDisplay();
      break;
    }
    if(rightReleased){        // right button press
      resetFlags();
      stopBeep();             // stop any beeping
      // increase bpm by 5
      if(bpm != 255){         // max BPM
        bpm = bpm + 5.0;
      }
      updateBPMDisplay();
      break;
    }
    if(bothPressed){          // if both buttons are pressed, go to next timing. If no more timings, break and go to tone screen
      resetFlags();
      stopBeep();             // stop any beeping
      beepCtr = 0; // reset the beat we're on
      if(currentEmphasis < 3){
        // go to next beat emphasis
        currentEmphasis++;
        updateBPMDisplay();
      }
      else{
         currentEmphasis = 0; // reset back to normal beat emphasis
         tuning(); // Enter tuning loop
         break;
      }
    }
    // Stop the beep after a delay
    if(currentMillis - prevMillisSound > soundDelay){
      prevMillisSound = currentMillis;
      stopBeep();
    }
    // Beep and light LED when servo in vertical position
    if((currentMillis - previousMillis >= rate/2) && !hitMiddle){
      light();
      beep();
      prevMillisSound = currentMillis;
      hitMiddle = true;
    }
    // Switch direction of servo
    // This rate is time between all the way left to all the way right. Therefore, we get screwed if speeds are fast.
    // Therefore, we need to calculate the min/max angle to write to when speeds are higher
    if(currentMillis - previousMillis > rate) {
      previousMillis = currentMillis;
      rotate(bpm);
      hitMiddle = false;
    }
  }
}

// Gives tuning tones
void tuning() {
  digitalWrite(led,HIGH);
  updateTuneDisplay();
  while(true){
    // Play tune
    tone(speakerPin, tunes[tuneCtr]);
    if(leftReleased){         // left button press. go to previous tuning note
      resetFlags();
      if(tuneCtr > 0){
        tuneCtr--;
      }
      updateTuneDisplay();
    }
    if(rightReleased){        // right button press. go to next tuning note
      resetFlags();
      if(tuneCtr < 11){
        tuneCtr++;
      }
      updateTuneDisplay();
    }
    if(bothPressed){          // if both buttons are pressed, break out of tuning
      resetFlags();
      noTone(speakerPin);
      break;
    }
  }
}

// Resets the servo to a 90 degree position
void reset() {
  metroServo.write(90,255);
  hitMiddle = false;
}

// Performs the servo rotation
// Changes speed and angle to write to based on BPM
void rotate(int bpm) {
  int minPos;
  int maxPos;
  int speed;
  if(bpm > 100){
    minPos = 45;
    maxPos = 135;
    speed = 100;
  }
  if(bpm >= 200){
    minPos = 60;
    maxPos = 120;
    speed = 100;
  }
  if(bpm <= 100){
    minPos = 30;
    maxPos = 150;
    speed = 70;
  }
  if(bpm <= 60){
    minPos = 0;
    maxPos = 180;
    speed = 70;
  }
  if(goingRight){
    metroServo.write(minPos, speed);
    goingRight = false;
  }
  else{
    metroServo.write(maxPos, speed);
    goingRight = true;
  }
  
}

// Emits a beep, with emphasis selection
void beep() {
  // Normal emphasis (quarter)
  if(emphasis[currentEmphasis] == 1){
    tone(speakerPin, NOTE_A5);
  }
  // Every 2 quarters
  if(emphasis[currentEmphasis] == 2){
    if(beepCtr == 0){
      tone(speakerPin, NOTE_A5);
      beepCtr++;
    }
    else{
      tone(speakerPin, NOTE_F5);
      beepCtr = 0;
    }
  }
  // Every 3 quarters
  if(emphasis[currentEmphasis] == 3){
    if(beepCtr == 0){
      tone(speakerPin, NOTE_A5);
      beepCtr++;
    }
    else if(beepCtr < 2){
      tone(speakerPin, NOTE_F5);
      beepCtr++;
    }
    else{
      tone(speakerPin, NOTE_F5);
      beepCtr = 0;
    }
  }
  // Every 4 quarters
  if(emphasis[currentEmphasis] == 4){
    if(beepCtr == 0){
      tone(speakerPin, NOTE_A5);
      beepCtr++;
    }
    else if(beepCtr < 3){
      tone(speakerPin, NOTE_F5);
      beepCtr++;
    }
    else{
      tone(speakerPin, NOTE_F5);
      beepCtr = 0;
    }
  }
}

// Stops the beep from playing
void stopBeep() {
  noTone(speakerPin);
}

// Blinks LED
void light() {
  digitalWrite(led,LOW);
  if(off){
    digitalWrite(led,HIGH);
    off = false;
  }
  else{
    digitalWrite(led,LOW);
    off = true;
  }
}

// Writes current BPM and timing to display when BPM is changed
void updateBPMDisplay() {
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("BPM: ");
  lcd.print((int)bpm);
  lcd.print(" ");
  lcd.print("1/");
  lcd.print(emphasis[currentEmphasis]);
  lcd.setCursor(0,1);
  lcd.print("Adj. w/ btns");
}

// Writes tuning note to display
void updateTuneDisplay() {
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Tuning Note: ");
  switch(tuneCtr) {
    case 0:
      lcd.print("A");
      break;
    case 1:
      lcd.print("A#");
      break;
    case 2:
      lcd.print("B");
      break;
    case 3:
      lcd.print("C");
      break;
    case 4:
      lcd.print("C#");
      break;
    case 5:
      lcd.print("D");
      break;
    case 6:
      lcd.print("D#");
      break;
    case 7:
      lcd.print("E");
      break;
    case 8:
      lcd.print("F");
      break;
    case 9:
      lcd.print("F#");
      break;
    case 10:
      lcd.print("G");
      break;
    case 11:
      lcd.print("G#");
      break;
  }
  lcd.setCursor(0,1);
  lcd.print("Adj. w/ btns");
}

// Method dedicated to resetting the booleans used for button press detection
// Resets buttons to their 'default' state typically after a button press is registered
// Implemented for cleaner-looking code, even if it might introduce some redundancies
void resetFlags() {
  leftPressed = false;
  rightPressed = false;
  leftReleased = false;
  rightReleased = false;
  bothPressed = false;
  leftBothReleased = false;
  rightBothReleased = false;
}

// Interrupt for left button press on button state rising
void pin_leftbut_ISR() {
  Serial.print("Left button interrupt");
  leftButtonState = digitalRead(leftbut);
  if(leftButtonState == HIGH){
    leftPressed = true;
  }
  // Register change from pressed to not pressed
  if(leftButtonState == LOW && leftPressed){
    if(rightPressed && !rightBothReleased){
      // The right button is still being pressed, so we're in a both button pressed situation
      // Must not register any button presses until all buttons are released!
      leftBothReleased = true;
    }
    else if(rightBothReleased){
      // The right button was also released, so we can set bothPressed = true
      bothPressed = true;
    }
    else{
      leftReleased = true;
    }
    leftPressed = false;
  }
}
// Interrupt for right button press on button state rising
void pin_rightbut_ISR() {
  Serial.print("Right button interrupt");
  rightButtonState = digitalRead(rightbut);
  if(rightButtonState == HIGH){
    rightPressed = true;
  }
  // Register change from pressed to not pressed
  if(rightButtonState == LOW && rightPressed){
    if(leftPressed && !leftBothReleased){
      // The left button is still being pressed, so we're in a both button pressed situation
      // Must not register any button presses until all buttons are released!
      rightBothReleased = true;
    }
    else if(leftBothReleased){
      // The left button was also released, so we can set bothPressed = true
      bothPressed = true;
    }
    else{
      rightReleased = true;
    }
    rightPressed = false;
  }
}













