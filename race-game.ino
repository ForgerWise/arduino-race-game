#include <Wire.h>
#include "Adafruit_GFX.h"
#include "Adafruit_LEDBackpack.h"
#include <TM1637Display.h>
#define LVCLK 5
#define LVDIO 6
#define SCCLK 12
#define SCDIO 13

Adafruit_LEDBackpack matrix1 = Adafruit_LEDBackpack();
Adafruit_LEDBackpack matrix2 = Adafruit_LEDBackpack();
Adafruit_LEDBackpack matrix3 = Adafruit_LEDBackpack();
Adafruit_LEDBackpack matrix4 = Adafruit_LEDBackpack();

// * Create a display object of type TM1637Display
TM1637Display LVDisplay = TM1637Display(LVCLK, LVDIO);
TM1637Display SCDisplay = TM1637Display(SCCLK, SCDIO);


int redLed = 4;
int yellowLed = 3;
int greenLed = 2;
int leftButton = 8;
int rightButton = 9;
int speedButton = 11;
int slowButton = 10;

bool lose = false;
int leftButtonState = 0;
int rightButtonState = 0;
int speedButtonState = 0;
int slowButtonState = 0;
int lastLeftButtonState = 0;
int lastRightButtonState = 0;
int lastSpeedButtonState = 0;
int lastSlowButtonState = 0;

float staticMovingTime = 1000;
int leftRightShiftDecider = 0;
int sameDirectionContinue = 0;
int lastTimeShift = 0;
int roadLevelGlobal = 0;

uint8_t originalLeftRoad = 0b00010000;
uint8_t originalRightRoad = 0b00001000;
uint8_t noneRoad = 0b00000000;
uint8_t leftRoad[16] = {};
uint8_t rightRoad[16] = {};
// * 0b12345678   <---   0b18765432
int roadEdges[16][2]; // * Minimum 0, Maximum 15
int carPosition[2] = {7, 8}; // * 0 ~ 15

uint8_t L = 0b00111000; // * L
uint8_t V = 0b10111110; // * V
uint8_t O = 0b00111111; // * O
uint8_t S = 0b01101101; // * S
uint8_t E = 0b01111001; // * E
uint8_t D = 0b01011110; // * D
uint8_t N = 0b01010100; // * N
uint8_t NONE = 0b00000000; // * NONE
uint8_t LVDisplayList[4];


/*
! This part is for function
*/
bool startGameLed();
bool isButtonPushed(int buttonState, int lastButtonState);
bool isBigButtonPushed(int buttonState, int lastButtonState, long lastDebounceTime);
void showLVDisplay(int level);
void showLVDisplayLose();
void showLVDisplayDone();
void showDisplayNone();
void decideLeftRightShiftDecider(int gameLevel);
uint8_t flipBinary(uint8_t originalBin);
uint8_t setCarBin(uint8_t binary, int carPosition);
bool isAbleToShift(int shift);
uint8_t shiftRoad(uint8_t binary, int shift);
uint8_t fixBinary(uint8_t binary);
bool isCrash();
void decideNextRoad(int gameLevel);
void upgradeRoad(int roadLevel);
void updateRoad();
void putRoadIntoMatrix();
void putNoRoadIntoMatrix();
void displayBoard();


void setup() {
  pinMode(redLed, OUTPUT); // * set the pin with red LED as “output”
  pinMode(yellowLed, OUTPUT); // * set the pin with yellow LED as “output”
  pinMode(greenLed, OUTPUT); // * set the pin with green LED as “output”
  pinMode(leftButton, INPUT);
  pinMode(rightButton, INPUT);
  pinMode(speedButton, INPUT);
  pinMode(slowButton, INPUT);
  randomSeed(analogRead(0));
  matrix1.begin(0x70);
  matrix2.begin(0x71);
  matrix3.begin(0x72);
  matrix4.begin(0x74);
  // 0x70  0x71(reverse)
  // 0x72  0x74(reverse)
  // matrix1 matrix2
  // matrix3 matrix4
  Serial.begin(9600);
  LVDisplay.setBrightness(5);
  SCDisplay.setBrightness(5);
}


void loop(){
  // * Initialize the variables
  bool lose = false;
  long lastTimeLeftButtonPushed = 0;
  long lastTimeRightButtonPushed = 0;
  long lastTimeSpeedButtonPushed = 0;
  long lastTimeSlowButtonPushed = 0;
  int gameLevel = 1; // * 1 ~ 20
  int score = 0; // * Maximum 9999
  int roadLevel = 0; // * Maximum 4, every level up means the road's width will decrease 1 unit 
  float movingTime = 1000;
  long lastTimeCarMoved = 0;
  long lastTimeShining = 0;
  long shiningTime = 1000;
  bool isShining = false;
  leftRightShiftDecider = 0; // * Initialize the leftRightShiftDecider

  // * Initialize the game
  for (int i = 0; i < 16; i ++){
    leftRoad[i] = originalLeftRoad;
    rightRoad[i] = originalRightRoad;
    roadEdges[i][0] = 3;
    roadEdges[i][1] = 12;
  }
  // * Initialize the car position
  carPosition[0] = 7;
  carPosition[1] = 8;
  // * Initialize the road level global
  roadLevelGlobal = 0;

  putRoadIntoMatrix();
  displayBoard();
  showLVDisplay(gameLevel);
  SCDisplay.showNumberDec(score);
  decideLeftRightShiftDecider(gameLevel);

  // * Start the game
  lose = startGameLed();
  while (!lose){
    // * While moving the car
    lastTimeCarMoved = millis();
    while (millis() - lastTimeCarMoved < movingTime){
      // * Check if the big button is pushed
      // * If the speed button is pushed, the level will increase
      speedButtonState = digitalRead(speedButton);
      if (isBigButtonPushed(speedButtonState, lastSpeedButtonState, lastTimeSpeedButtonPushed)){
        lastTimeSpeedButtonPushed = millis();
        gameLevel = min(gameLevel + 1, 20);
        showLVDisplay(gameLevel);
        movingTime = staticMovingTime / gameLevel;
      }
      // * If the slow button is pushed, the level will decrease
      slowButtonState = digitalRead(slowButton);
      if (isBigButtonPushed(slowButtonState, lastSlowButtonState, lastTimeSlowButtonPushed)){
        lastTimeSlowButtonPushed = millis();
        gameLevel = max(gameLevel - 1, 1 + score / 500);
        showLVDisplay(gameLevel);
        movingTime = staticMovingTime / gameLevel;
      }

      // * Check if the left button or right button is pushed
      // * If the left button is pushed, the car will move to the left and check if the car will crash
      leftButtonState = digitalRead(leftButton);
      if (isButtonPushed(leftButtonState, lastLeftButtonState, lastTimeLeftButtonPushed)){
        lastTimeLeftButtonPushed = millis();
        carPosition[0] --;
        carPosition[1] --;
        putRoadIntoMatrix();
        displayBoard();
        if (isCrash()){
          lose = true;
          break;
        }
      }
      // * If the right button is pushed, the car will move to the right and check if the car will crash
      rightButtonState = digitalRead(rightButton);
      if (isButtonPushed(rightButtonState, lastRightButtonState, lastTimeRightButtonPushed)){
        lastTimeRightButtonPushed = millis();
        carPosition[0] ++;
        carPosition[1] ++;
        putRoadIntoMatrix();
        displayBoard();
        if (isCrash()){
          lose = true;
          break;
        }
      }

      // * Update last button state
      lastLeftButtonState = leftButtonState;
      lastRightButtonState = rightButtonState;
      lastSpeedButtonState = speedButtonState;
      lastSlowButtonState = slowButtonState;

      if (isCrash()){
        lose = true;
        break;
      }
    }

    // * If the player lose, break the loop
    if (lose) {
      break;
    }
    
    // * Update the score
    score  += gameLevel * (roadLevel + 1);

    // * Update minimum game level
    gameLevel = max(gameLevel, 1 + score / 500);
    showLVDisplay(gameLevel);

    // * If the score is greater than 1000, 3000, 5000, 7000, the roadLevel will increase, the road's width will decrease 1 unit
    if (score > 7000 && roadLevel < 4) {
      roadLevel = 4;
      upgradeRoad(roadLevel);
    } else if (score > 5000 && roadLevel < 3) {
      roadLevel = 3;
      upgradeRoad(roadLevel);
    } else if (score > 3000 && roadLevel < 2) {
      roadLevel = 2;
      upgradeRoad(roadLevel);
    } else if (score > 1000 && roadLevel < 1) {
      roadLevel = 1;
      upgradeRoad(roadLevel);
    } else {
      // * If don't need to upgrade the road, just decide the next road
      // * Decide the next road
      // * Check if the road is able to shift to left or right
      decideNextRoad(gameLevel);
    }

    roadLevelGlobal = roadLevel;
    

    // * Put car in the road
    putRoadIntoMatrix();
    displayBoard();

    // * Show the score
    if (score > 9999){
      score = 9999;
      SCDisplay.showNumberDec(score);
      showLVDisplayDone();
      break;
    }
    SCDisplay.showNumberDec(score);
  }

  delay(500);

  while (true){
    // * If lose, let LV display show "LOSE", SC display show the score, LED show the light when lose.
    // * All of them should be shining until the player push either of the big buttons
    if (millis() - lastTimeShining > shiningTime){
      isShining = !isShining;
      lastTimeShining = millis();
    } else {
      if (isShining) {
        if (lose) {
          showLVDisplayLose();
        } else {
          showLVDisplayDone();
        }
        SCDisplay.showNumberDec(score);
        putRoadIntoMatrix();
        displayBoard();
      } else {
        showDisplayNone();
        putNoRoadIntoMatrix();
        displayBoard();
      }
    }

    // * Wait for the player to push the big button to restart the game
    speedButtonState = digitalRead(speedButton);
    slowButtonState = digitalRead(slowButton);
    if (isBigButtonPushed(speedButtonState, lastSpeedButtonState, lastTimeSpeedButtonPushed) || isBigButtonPushed(slowButtonState, lastSlowButtonState, lastTimeSlowButtonPushed)){
      lastTimeSpeedButtonPushed = millis();
      lastTimeSlowButtonPushed = millis();
      lose = false;
      break;
    }
    lastSpeedButtonState = speedButtonState;
    lastSlowButtonState = slowButtonState;
  }
}

/*
! This part is for 3 color LED
*/

// * Start the game with LED
bool startGameLed(){
  long delayTime = random(1000, 3001);
  bool lose = false;

  digitalWrite(redLed, HIGH);
  delay(1000);
  digitalWrite(yellowLed, HIGH);
  delay(1000);
  digitalWrite(greenLed, HIGH);

  // * Check if the big button is pushed before all the LED is turned off
  // * If the big button is pushed, the player lose.
  long time_now = millis();
  while (millis() - time_now < delayTime){
    speedButtonState = digitalRead(speedButton);
    if (speedButtonState == HIGH && speedButtonState != lastSpeedButtonState){
      lose = true;
      break;
    }
    lastSpeedButtonState = speedButtonState;
  }
  digitalWrite(redLed, LOW);
  digitalWrite(yellowLed, LOW);
  digitalWrite(greenLed, LOW);
  return lose;
}


/*
! This part is for button
*/

// * Check if the button is pushed (100ms) (for left and right button)
bool isButtonPushed(int buttonState, int lastButtonState, long lastDebounceTime){
  return (buttonState != lastButtonState) && (buttonState == HIGH) && (millis() - lastDebounceTime > 20);
}

// * Avoid chattering of the big button (500ms) (for speed and slow button)
bool isBigButtonPushed(int buttonState, int lastButtonState, long lastDebounceTime){
  return (buttonState != lastButtonState) && (buttonState == HIGH) && (millis() - lastDebounceTime > 200);
}


/*
! This part is for 4 digit display
*/

// * Show the LV display
void showLVDisplay(int level){
  int ten = level / 10;
  int one = level % 10;
  LVDisplayList[0] = L;
  LVDisplayList[1] = V;
  LVDisplayList[2] = LVDisplay.encodeDigit(ten);
  LVDisplayList[3] = LVDisplay.encodeDigit(one);
  LVDisplay.setSegments(LVDisplayList);
}

// * Show the LV display as "LOSE"
void showLVDisplayLose(){
  LVDisplayList[0] = L;
  LVDisplayList[1] = O;
  LVDisplayList[2] = S;
  LVDisplayList[3] = E;
  LVDisplay.setSegments(LVDisplayList);
}

// * Show the LV display as "dOnE"
void showLVDisplayDone(){
  LVDisplayList[0] = D;
  LVDisplayList[1] = O;
  LVDisplayList[2] = N;
  LVDisplayList[3] = E;
  LVDisplay.setSegments(LVDisplayList);
}

// * Show the SC, LV Display as None
void showDisplayNone(){
  uint8_t displayList[4] = {};
  for (int i = 0; i < 4; i ++){
    displayList[i] = NONE;
  }
  LVDisplay.setSegments(displayList);
  SCDisplay.setSegments(displayList);
}

/*
! This part is for the board
*/

// * Decide the leftRightShiftDecider
void decideLeftRightShiftDecider(int gameLevel) {
  if (leftRightShiftDecider == 0){
    leftRightShiftDecider = random(-5 * gameLevel, 5 * gameLevel + 1);
  } else if (leftRightShiftDecider > 0){
    leftRightShiftDecider = random(-5 * gameLevel, 1);
  } else {
    leftRightShiftDecider = random(0, 5 * gameLevel + 1);
  }
}

// * Flip the binary at 0x71 and 0x74, these two are reversed when installed
uint8_t flipBinary(uint8_t originalBin) {
  uint8_t flipBin = 0;
  for (int i = 0; i < 8; i ++){
    if (originalBin & (1 << i)){
      flipBin |= (1 << (7 - i));
    }
  }
  return flipBin;
}

// * Set the car at the specific position
uint8_t setCarBin(uint8_t binary, int carPosition) {
  uint8_t roadWithCar = binary;
  roadWithCar |= (1 << carPosition);
  return roadWithCar;
}

// * Check if the road is able to shift to left or right
bool isAbleToShift(int shift) {
  if (shift == -1 && roadEdges[0][0] == 0){
    return false;
  } else if (shift == 1 && roadEdges[0][1] == 15){
    return false;
  }
  return true;
}

// * Shift the road to left or right, exp. 0b00010000 -> 0b00100000 (left)
uint8_t shiftRoad(uint8_t binary, int shift) { // * shift = -1: left, shift = 1: right, shift = 0: no shift
  if (shift == -1) {
    binary = (binary << 1);
  } else if (shift == 1) {
    binary = (binary >> 1);
  }
  return binary;
}

// * Fix the binary to display correctly, from 0b18765432 to 0b12345678
uint8_t fixBinary(uint8_t binary){
  uint8_t fixedBinary = 0;
  fixedBinary |= (binary & (1 << 7));
  
  for (int i = 0; i < 7; i++) {
    if (binary & (1 << i)) {
      fixedBinary |= (1 << (6 - i));
    } else {
      fixedBinary &= ~(1 << (6 - i));
    }
  }
  
  return fixedBinary;
}

// * Check if the car will crash or not
bool isCrash(){
  for (int i = 12; i < 15; i ++) {
    if ((roadEdges[i][0] >= carPosition[0] || roadEdges[i][1] <= carPosition[1])){
      return true;
    }
  }
  return false;
}

// * Decide the next road
void decideNextRoad(int gameLevel){
  int shift = 0; // * -1: left, 0: no shift, 1: right, original: 0
  int direction = random(-100, 101);
  bool decide = true;

  // * If the road is not able to shift to left or right, decide the shift to the opposite direction or 0
  if (!isAbleToShift(-1)){
    shift = random(0, 2);
    decideLeftRightShiftDecider(gameLevel);
    decide = false;
  } else if (!isAbleToShift(1)){
    shift = random(-1, 1);
    decideLeftRightShiftDecider(gameLevel);
    decide = false;
  }

  // * If the direction is the same for 3 times, let shift = 0 and do not decide the shift
  if (roadLevelGlobal == 4 && sameDirectionContinue >= 3) { 
    decide = false;
  }

  if (decide){
    direction += leftRightShiftDecider;
    // * < -20: left, > 20: right, -20 ~ 20: no shift, which means to stay variable same
    if (direction < -20){
      shift = -1;
    } else if (direction > 20){
      shift = 1;
    } 
  }

  // * If the road level is 4, check if the shift is the same as the last time
  if (roadLevelGlobal == 4) {
    if (shift == lastTimeShift && shift != 0){ // * If the shift is the same as the last time and is not 0, sameDirectionContinue ++
      sameDirectionContinue ++;
    } else if (shift != lastTimeShift && shift != 0){ // * If the shift is not the same as the last time and is not 0, sameDirectionContinue = 1
      sameDirectionContinue = 2;
    } else { // * If the shift is 0, sameDirectionContinue = 0
      sameDirectionContinue = 0;
    }

    lastTimeShift = shift;
  }

  updateRoad();

  // * Shift the road to left or right
  if (leftRoad[0] == 0b00000001 && shift == 1) {
    leftRoad[0] = 0b00000000;
    rightRoad[0] = shiftRoad(rightRoad[0], shift) | 0b10000000;
  } else if (rightRoad[0] == 0b10000000 && shift == -1) {
    rightRoad[0] = 0b00000000;
    leftRoad[0] = shiftRoad(leftRoad[0], shift) | 0b00000001;
  } else if ((leftRoad[0] & 1) && shift == 1) {
    rightRoad[0] = 0b10000000;
    leftRoad[0] = shiftRoad(leftRoad[0], shift);
  } else if (((rightRoad[0] >> 7) & 1) && shift == -1) {
    leftRoad[0] = 0b00000001;
    rightRoad[0] = shiftRoad(rightRoad[0], shift);
  } else {
    leftRoad[0] = shiftRoad(leftRoad[0], shift);
    rightRoad[0] = shiftRoad(rightRoad[0], shift);
  }

  // * Update the road edges
  roadEdges[0][0] += shift;
  roadEdges[0][1] += shift;
}

// * Upgrade the road
void upgradeRoad(int roadLevel) {
  // * Update the road
  updateRoad();

  if (roadLevel == 1) {
    if (leftRoad[0] == 0b00000001){
      rightRoad[0] |= 0b10000000;
    }
    leftRoad[0] = shiftRoad(leftRoad[0], 1);
    roadEdges[0][0] ++;
  } else if (roadLevel == 2) {
    if (rightRoad[0] == 0b10000000){
      leftRoad[0] |= 0b00000001;
    }
    rightRoad[0] = shiftRoad(rightRoad[0], -1);
    roadEdges[0][1] --;
  } else if (roadLevel == 3) {
    if (leftRoad[0] == 0b00000001){
      rightRoad[0] |= 0b10000000;
      leftRoad[0] = shiftRoad(leftRoad[0], 1);
    } else if (leftRoad[0] == 0b10000001) {
      leftRoad[0] = 0b01000000;
    } else {
      leftRoad[0] = shiftRoad(leftRoad[0], 1);
    }
    roadEdges[0][0] ++;
  } else if (roadLevel == 4) {
    if (rightRoad[0] == 0b10000000){
      leftRoad[0] |= 0b00000001;
      rightRoad[0] = shiftRoad(rightRoad[0], -1);
    } else if (rightRoad[0] == 0b10000010) {
      rightRoad[0] = 0b10000100;
    } else {
      rightRoad[0] = shiftRoad(rightRoad[0], -1);
    }
    roadEdges[0][1] --;
  }
}


// * Update the road
void updateRoad(){
  for (int i = 15; i > 0; i --){
    leftRoad[i] = leftRoad[i - 1];
    rightRoad[i] = rightRoad[i - 1];
    roadEdges[i][0] = roadEdges[i - 1][0];
    roadEdges[i][1] = roadEdges[i - 1][1];
  }
}

// * Put road into matrix
void putRoadIntoMatrix(){
  for (int i = 0; i < 8; i ++){
    matrix1.displaybuffer[i] = fixBinary(leftRoad[i]);
    matrix2.displaybuffer[7 - i] = fixBinary(flipBinary(rightRoad[i]));
    if (i < 4 || i == 7){
      matrix3.displaybuffer[i] = fixBinary(leftRoad[i + 8]);
      matrix4.displaybuffer[7 - i] = fixBinary(flipBinary(rightRoad[i + 8]));
    } else { // * Put the car in the road
      if (carPosition[1] < 8){
        matrix3.displaybuffer[i] = fixBinary(setCarBin(setCarBin(leftRoad[i + 8], 7 - carPosition[0]), 7 - carPosition[1]));
        matrix4.displaybuffer[7 - i] = fixBinary(flipBinary(rightRoad[i + 8]));
      } else if (carPosition[0] > 7){
        matrix3.displaybuffer[i] = fixBinary(leftRoad[i + 8]);
        matrix4.displaybuffer[7 - i] = fixBinary(flipBinary(setCarBin(setCarBin(rightRoad[i + 8], 15 - carPosition[0]), 15 - carPosition[1])));
      } else {
        matrix3.displaybuffer[i] = fixBinary(setCarBin(leftRoad[i + 8], 0));
        matrix4.displaybuffer[7 - i] = fixBinary(flipBinary(setCarBin(rightRoad[i + 8], 7)));
      }
    }
  }
}

// * Put No road and car into matrix
void putNoRoadIntoMatrix(){
  for (int i = 0; i < 8; i ++){
    matrix1.displaybuffer[i] = NONE;
    matrix2.displaybuffer[7 - i] = NONE;
    matrix3.displaybuffer[i] = NONE;
    matrix4.displaybuffer[7 - i] = NONE;
  }
}


// * Display all the matrix
void displayBoard(){
  matrix1.writeDisplay();
  matrix2.writeDisplay();
  matrix3.writeDisplay();
  matrix4.writeDisplay();
}